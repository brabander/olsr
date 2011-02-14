
/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004-2009, the olsr.org team - see HISTORY file
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of olsr.org, olsrd nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Visit http://www.olsr.org for more information.
 *
 * If you find this software useful feel free to make a donation
 * to the project. For more information see the website or contact
 * the copyright holders.
 *
 */

#include <unistd.h>
#include <assert.h>
#include <stdlib.h>

#include "common/avl.h"
#include "common/avl_olsr_comp.h"
#include "scheduler.h"
#include "link_set.h"
#include "olsr.h"
#include "olsr_memcookie.h"
#include "os_net.h"
#include "os_time.h"
#include "olsr_logging.h"

/* Timer data */
static uint32_t now_times;             /* relative time compared to startup (in milliseconds */
static struct timeval first_tv;        /* timevalue during startup */
static struct timeval last_tv;         /* timevalue used for last olsr_times() calculation */

/* Hashed root of all timers */
static struct list_entity timer_wheel[TIMER_WHEEL_SLOTS];
static uint32_t timer_last_run;        /* remember the last timeslot walk */

/* Memory cookie for the timer manager */
struct avl_tree timerinfo_tree;
static struct olsr_memcookie_info *timer_mem_cookie = NULL;
static struct olsr_memcookie_info *timerinfo_cookie = NULL;

/* Head of all OLSR used sockets */
struct list_entity socket_head;

/* Prototypes */
static void walk_timers(uint32_t *);
static uint32_t calc_jitter(unsigned int rel_time, uint8_t jitter_pct, unsigned int random_val);

/*
 * A wrapper around times(2). Note, that this function has some
 * portability problems, so do not rely on absolute values returned.
 * Under Linux, uclibc and libc directly call the sys_times() located
 * in kernel/sys.c and will only return an error if the tms_buf is
 * not writeable.
 */
static uint32_t
olsr_times(void)
{
  struct timeval tv;
  uint32_t t;

  if (os_gettimeofday(&tv, NULL) != 0) {
    OLSR_ERROR(LOG_SCHEDULER, "OS clock is not working, have to shut down OLSR (%s)\n", strerror(errno));
    olsr_exit(1);
  }

  /* test if time jumped backward or more than 60 seconds forward */
  if (tv.tv_sec < last_tv.tv_sec || (tv.tv_sec == last_tv.tv_sec && tv.tv_usec < last_tv.tv_usec)
      || tv.tv_sec - last_tv.tv_sec > 60) {
    OLSR_WARN(LOG_SCHEDULER, "Time jump (%d.%06d to %d.%06d)\n",
              (int32_t) (last_tv.tv_sec), (int32_t) (last_tv.tv_usec), (int32_t) (tv.tv_sec), (int32_t) (tv.tv_usec));

    t = (last_tv.tv_sec - first_tv.tv_sec) * 1000 + (last_tv.tv_usec - first_tv.tv_usec) / 1000;
    t++;                        /* advance time by one millisecond */

    first_tv = tv;
    first_tv.tv_sec -= (t / 1000);
    first_tv.tv_usec -= ((t % 1000) * 1000);

    if (first_tv.tv_usec < 0) {
      first_tv.tv_sec--;
      first_tv.tv_usec += 1000000;
    }
    last_tv = tv;
    return t;
  }
  last_tv = tv;
  return (tv.tv_sec - first_tv.tv_sec) * 1000 + (tv.tv_usec - first_tv.tv_usec) / 1000;
}

/**
 * Returns a timestamp s seconds in the future
 */
uint32_t
olsr_timer_getAbsolute(uint32_t s)
{
  return now_times + s;
}

/**
 * Returns the number of milliseconds until the timestamp will happen
 */

int32_t
olsr_timer_getRelative(uint32_t s)
{
  uint32_t diff;
  if (s > now_times) {
    diff = s - now_times;

    /* overflow ? */
    if (diff > (1u << 31)) {
      return -(int32_t) (0xffffffff - diff);
    }
    return (int32_t) (diff);
  }

  diff = now_times - s;
  /* overflow ? */
  if (diff > (1u << 31)) {
    return (int32_t) (0xffffffff - diff);
  }
  return -(int32_t) (diff);
}

bool
olsr_timer_isTimedOut(uint32_t s)
{
  if (s > now_times) {
    return s - now_times > (1u << 31);
  }

  return now_times - s <= (1u << 31);
}

struct olsr_timer_info *
olsr_timer_add(const char *name, timer_cb_func callback, bool periodic) {
  struct olsr_timer_info *ti;

  ti = olsr_memcookie_malloc(timerinfo_cookie);
  ti->name = strdup(name);
  ti->node.key = ti->name;
  ti->callback = callback;
  ti->periodic = periodic;

  avl_insert(&timerinfo_tree, &ti->node);
  return ti;
}

/**
 * Add a socket and handler to the socketset
 * beeing used in the main select(2) loop
 * in listen_loop
 *
 *@param fd the socket
 *@param pf the processing function
 */
void
olsr_socket_add(int fd, socket_handler_func pf_imm, void *data, unsigned int flags)
{
  struct olsr_socket_entry *new_entry;

  if (fd < 0 || pf_imm == NULL) {
    OLSR_WARN(LOG_SCHEDULER, "Bogus socket entry - not registering...");
    return;
  }
  OLSR_DEBUG(LOG_SCHEDULER, "Adding OLSR socket entry %d\n", fd);

  new_entry = olsr_malloc(sizeof(*new_entry), "Socket entry");

  new_entry->fd = fd;
  new_entry->process_immediate = pf_imm;
  new_entry->data = data;
  new_entry->flags = flags;

  /* Queue */
  list_add_before(&socket_head, &new_entry->socket_node);
}

/**
 * Remove a socket and handler to the socketset
 * beeing used in the main select(2) loop
 * in listen_loop
 *
 *@param fd the socket
 *@param pf the processing function
 */
int
olsr_socket_remove(int fd, socket_handler_func pf_imm)
{
  struct olsr_socket_entry *entry, *iterator;

  if (fd < 0 || pf_imm == NULL) {
    OLSR_WARN(LOG_SCHEDULER, "Bogus socket entry - not processing...");
    return 0;
  }
  OLSR_DEBUG(LOG_SCHEDULER, "Removing OLSR socket entry %d\n", fd);

  OLSR_FOR_ALL_SOCKETS(entry, iterator) {
    if (entry->fd == fd && entry->process_immediate == pf_imm) {
      /* just mark this node as "deleted", it will be cleared later at the end of handle_fds() */
      entry->process_immediate = NULL;
      entry->flags = 0;
      return 1;
    }
  }
  return 0;
}

void
olsr_socket_enable(int fd, socket_handler_func pf_imm, unsigned int flags)
{
  struct olsr_socket_entry *entry, *iterator;

  OLSR_FOR_ALL_SOCKETS(entry, iterator) {
    if (entry->fd == fd && entry->process_immediate == pf_imm) {
      entry->flags |= flags;
    }
  }
}

void
olsr_socket_disable(int fd, socket_handler_func pf_imm, unsigned int flags)
{
  struct olsr_socket_entry *entry, *iterator;

  OLSR_FOR_ALL_SOCKETS(entry, iterator) {
    if (entry->fd == fd && entry->process_immediate == pf_imm) {
      entry->flags &= ~flags;
    }
  }
}

/**
 * Close and free all sockets.
 */
void
olsr_socket_cleanup(void)
{
  struct olsr_socket_entry *entry, *iterator;

  OLSR_FOR_ALL_SOCKETS(entry, iterator) {
    os_close(entry->fd);
    list_remove(&entry->socket_node);
    free(entry);
  }
}

static void
handle_fds(uint32_t next_interval)
{
  struct olsr_socket_entry *entry, *iterator;
  struct timeval tvp;
  int32_t remaining;

  /* calculate the first timeout */
  now_times = olsr_times();

  remaining = olsr_timer_getRelative(next_interval);
  if (remaining <= 0) {
    /* we are already over the interval */
    if (list_is_empty(&socket_head)) {
      /* If there are no registered sockets we do not call select(2) */
      return;
    }
    tvp.tv_sec = 0;
    tvp.tv_usec = 0;
  } else {
    /* we need an absolute time - milliseconds */
    tvp.tv_sec = remaining / MSEC_PER_SEC;
    tvp.tv_usec = (remaining % MSEC_PER_SEC) * USEC_PER_MSEC;
  }

  /* do at least one select */
  for (;;) {
    fd_set ibits, obits;
    int n, hfd = 0, fdsets = 0;
    FD_ZERO(&ibits);
    FD_ZERO(&obits);

    /* Adding file-descriptors to FD set */
    OLSR_FOR_ALL_SOCKETS(entry, iterator) {
      if (entry->process_immediate == NULL) {
        continue;
      }
      if ((entry->flags & OLSR_SOCKET_READ) != 0) {
        fdsets |= OLSR_SOCKET_READ;
        FD_SET((unsigned int)entry->fd, &ibits);        /* And we cast here since we get a warning on Win32 */
      }
      if ((entry->flags & OLSR_SOCKETPOLL_WRITE) != 0) {
        fdsets |= OLSR_SOCKETPOLL_WRITE;
        FD_SET((unsigned int)entry->fd, &obits);        /* And we cast here since we get a warning on Win32 */
      }
      if ((entry->flags & (OLSR_SOCKET_READ | OLSR_SOCKETPOLL_WRITE)) != 0 && entry->fd >= hfd) {
        hfd = entry->fd + 1;
      }
    }

    if (hfd == 0 && (long)remaining <= 0) {
      /* we are over the interval and we have no fd's. Skip the select() etc. */
      return;
    }

    do {
      n = os_select(hfd, fdsets & OLSR_SOCKET_READ ? &ibits : NULL, fdsets & OLSR_SOCKETPOLL_WRITE ? &obits : NULL, NULL, &tvp);
    } while (n == -1 && errno == EINTR);

    if (n == 0) {               /* timeout! */
      break;
    }
    if (n == -1) {              /* Did something go wrong? */
      OLSR_WARN(LOG_SCHEDULER, "select error: %s", strerror(errno));
      break;
    }

    /* Update time since this is much used by the parsing functions */
    now_times = olsr_times();
    OLSR_FOR_ALL_SOCKETS(entry, iterator) {
      int flags;
      if (entry->process_immediate == NULL) {
        continue;
      }
      flags = 0;
      if (FD_ISSET(entry->fd, &ibits)) {
        flags |= OLSR_SOCKET_READ;
      }
      if (FD_ISSET(entry->fd, &obits)) {
        flags |= OLSR_SOCKETPOLL_WRITE;
      }
      if (flags != 0) {
        entry->process_immediate(entry->fd, entry->data, flags);
      }
    }

    /* calculate the next timeout */
    remaining = olsr_timer_getRelative(next_interval);
    if (remaining <= 0) {
      /* we are already over the interval */
      break;
    }
    /* we need an absolute time - milliseconds */
    tvp.tv_sec = remaining / MSEC_PER_SEC;
    tvp.tv_usec = (remaining % MSEC_PER_SEC) * USEC_PER_MSEC;
  }

  OLSR_FOR_ALL_SOCKETS(entry, iterator) {
    if (entry->process_immediate == NULL) {
      /* clean up socket handler */
      list_remove(&entry->socket_node);
      free(entry);
    }
  }
}

/**
 * Main scheduler event loop. Polls at every
 * sched_poll_interval and calls all functions
 * that are timed out or that are triggered.
 * Also calls the olsr_process_changes()
 * function at every poll.
 *
 * @return nada
 */
void
olsr_scheduler(void)
{
  OLSR_INFO(LOG_SCHEDULER, "Scheduler started - polling every %u ms\n", olsr_cnf->pollrate);

  /* Main scheduler loop */
  while (app_state == STATE_RUNNING) {
    uint32_t next_interval;

    /*
     * Update the global timestamp. We are using a non-wallclock timer here
     * to avoid any undesired side effects if the system clock changes.
     */
    now_times = olsr_times();
    next_interval = olsr_timer_getAbsolute(olsr_cnf->pollrate);

    /* Process timers */
    walk_timers(&timer_last_run);

    /* Update */
    olsr_process_changes();

    /* Check for changes in topology */
    if (link_changes) {
      increase_local_ansn_number();
      OLSR_DEBUG(LOG_SCHEDULER, "ANSN UPDATED %d\n\n", get_local_ansn_number());
      link_changes = false;
    }

    /* Read incoming data and handle it immediately */
    handle_fds(next_interval);
  }
}

/**
 * Decrement a relative timer by a random number range.
 *
 * @param the relative timer expressed in units of milliseconds.
 * @param the jitter in percent
 * @param cached result of random() at system init.
 * @return the absolute timer in system clock tick units
 */
static uint32_t
calc_jitter(unsigned int rel_time, uint8_t jitter_pct, unsigned int random_val)
{
  unsigned int jitter_time;

  /*
   * No jitter or, jitter larger than 99% does not make sense.
   * Also protect against overflows resulting from > 25 bit timers.
   */
  if (jitter_pct == 0 || jitter_pct > 99 || rel_time > (1 << 24)) {
    return olsr_timer_getAbsolute(rel_time);
  }

  /*
   * Play some tricks to avoid overflows with integer arithmetic.
   */
  jitter_time = (jitter_pct * rel_time) / 100;
  jitter_time = random_val / (1 + RAND_MAX / (jitter_time + 1));

  OLSR_DEBUG(LOG_TIMER, "TIMER: jitter %u%% rel_time %ums to %ums\n", jitter_pct, rel_time, rel_time - jitter_time);

  return olsr_timer_getAbsolute(rel_time - jitter_time);
}

/**
 * Init datastructures for maintaining timers.
 */
void
olsr_init_timers(void)
{
  int idx;

  OLSR_INFO(LOG_SCHEDULER, "Initializing scheduler.\n");

  /* Grab initial timestamp */
  if (os_gettimeofday(&first_tv, NULL)) {
    OLSR_ERROR(LOG_TIMER, "OS clock is not working, have to shut down OLSR (%s)\n", strerror(errno));
    olsr_exit(1);
  }
  last_tv = first_tv;
  now_times = olsr_times();

  /* init lists */
  list_init_head(&socket_head);
  for (idx = 0; idx < TIMER_WHEEL_SLOTS; idx++) {
    list_init_head(&timer_wheel[idx]);
  }

  /*
   * Reset the last timer run.
   */
  timer_last_run = now_times;

  /* Allocate a cookie for the block based memory manager. */
  timer_mem_cookie = olsr_memcookie_add("timer_entry", sizeof(struct olsr_timer_entry));

  avl_init(&timerinfo_tree, avl_comp_strcasecmp, false, NULL);
  timerinfo_cookie = olsr_memcookie_add("timerinfo", sizeof(struct olsr_timer_info));
}

/**
 * Walk through the timer list and check if any timer is ready to fire.
 * Callback the provided function with the context pointer.
 */
static void
walk_timers(uint32_t * last_run)
{
  unsigned int total_timers_walked = 0, total_timers_fired = 0;
  unsigned int wheel_slot_walks = 0;

  /*
   * Check the required wheel slots since the last time a timer walk was invoked,
   * or check *all* the wheel slots, whatever is less work.
   * The latter is meant as a safety belt if the scheduler falls behind.
   */
  while ((*last_run <= now_times) && (wheel_slot_walks < TIMER_WHEEL_SLOTS)) {
    struct list_entity tmp_head_node;
    /* keep some statistics */
    unsigned int timers_walked = 0, timers_fired = 0;

    /* Get the hash slot for this clocktick */
    struct list_entity *timer_head_node;

    timer_head_node = &timer_wheel[*last_run & TIMER_WHEEL_MASK];

    /* Walk all entries hanging off this hash bucket. We treat this basically as a stack
     * so that we always know if and where the next element is.
     */
    list_init_head(&tmp_head_node);
    while (!list_is_empty(timer_head_node)) {
      /* the top element */
      struct olsr_timer_entry *timer;

      timer = list_first_element(timer_head_node, timer, timer_list);

      /*
       * Dequeue and insert to a temporary list.
       * We do this to avoid loosing our walking context when
       * multiple timers fire.
       */
      list_remove(&timer->timer_list);
      list_add_after(&tmp_head_node, &timer->timer_list);
      timers_walked++;

      /* Ready to fire ? */
      if (olsr_timer_isTimedOut(timer->timer_clock)) {
        OLSR_DEBUG(LOG_TIMER, "TIMER: fire %s timer %p, ctx %p, "
                   "at clocktick %u (%s)\n",
                   timer->timer_info->name,
                   timer, timer->timer_cb_context, (unsigned int)*last_run, olsr_timer_getWallclockString());

        /* This timer is expired, call into the provided callback function */
        timer->timer_in_callback = true;
        timer->timer_info->callback(timer->timer_cb_context);
        timer->timer_in_callback = false;
        timer->timer_info->changes++;

        /* Only act on actually running timers */
        if (timer->timer_running) {
          /*
           * Don't restart the periodic timer if the callback function has
           * stopped the timer.
           */
          if (timer->timer_period) {
            /* For periodical timers, rehash the random number and restart */
            timer->timer_random = random();
            olsr_timer_change(timer, timer->timer_period, timer->timer_jitter_pct);
          } else {
            /* Singleshot timers are stopped */
            olsr_timer_stop(timer);
          }
        }
        else {
          /* free memory */
          olsr_memcookie_free(timer_mem_cookie, timer);
        }

        timers_fired++;
      }
    }

    /*
     * Now merge the temporary list back to the old bucket.
     */
    list_merge(timer_head_node, &tmp_head_node);

    /* keep some statistics */
    total_timers_walked += timers_walked;
    total_timers_fired += timers_fired;

    /* Increment the time slot and wheel slot walk iteration */
    (*last_run)++;
    wheel_slot_walks++;
  }

  OLSR_DEBUG(LOG_TIMER, "TIMER: processed %4u/%d clockwheel slots, "
             "timers walked %4u/%u, timers fired %u\n",
             wheel_slot_walks, TIMER_WHEEL_SLOTS, total_timers_walked, timer_mem_cookie->ci_usage, total_timers_fired);

  /*
   * If the scheduler has slipped and we have walked all wheel slots,
   * reset the last timer run.
   */
  *last_run = now_times;
}

/**
 * Stop and delete all timers.
 */
void
olsr_flush_timers(void)
{
  struct olsr_timer_info *ti, *iterator;

  struct list_entity *timer_head_node;
  unsigned int wheel_slot = 0;

  for (wheel_slot = 0; wheel_slot < TIMER_WHEEL_SLOTS; wheel_slot++) {
    timer_head_node = &timer_wheel[wheel_slot & TIMER_WHEEL_MASK];

    /* Kill all entries hanging off this hash bucket. */
    while (!list_is_empty(timer_head_node)) {
      struct olsr_timer_entry *timer;

      timer = list_first_element(timer_head_node, timer, timer_list);
      olsr_timer_stop(timer);
    }
  }

  /* free all timerinfos */
  OLSR_FOR_ALL_TIMERS(ti, iterator) {
    avl_delete(&timerinfo_tree, &ti->node);
    free(ti->name);
    olsr_memcookie_free(timerinfo_cookie, ti);
  }

  /* release memory cookie for timers */
  olsr_memcookie_remove(timerinfo_cookie);
}

/**
 * Returns the difference between gmt and local time in seconds.
 * Use gmtime() and localtime() to keep things simple.
 *
 * taken and slightly modified from www.tcpdump.org.
 */
static int
olsr_get_timezone(void)
{
#define OLSR_TIMEZONE_UNINITIALIZED -1
  static int time_diff = OLSR_TIMEZONE_UNINITIALIZED;
  if (time_diff == OLSR_TIMEZONE_UNINITIALIZED) {
    int dir;
    const time_t t = time(NULL);
    const struct tm gmt = *gmtime(&t);
    const struct tm *loc = localtime(&t);

    time_diff = (loc->tm_hour - gmt.tm_hour) * 60 * 60 + (loc->tm_min - gmt.tm_min) * 60;

    /*
     * If the year or julian day is different, we span 00:00 GMT
     * and must add or subtract a day. Check the year first to
     * avoid problems when the julian day wraps.
     */
    dir = loc->tm_year - gmt.tm_year;
    if (!dir) {
      dir = loc->tm_yday - gmt.tm_yday;
    }

    time_diff += dir * 24 * 60 * 60;
  }
  return time_diff;
}

/**
 * Format an absolute wallclock system time string.
 * May be called upto 4 times in a single printf() statement.
 * Displays microsecond resolution.
 *
 * @return buffer to a formatted system time string.
 */
const char *
olsr_timer_getWallclockString(void)
{
  static char buf[sizeof("00:00:00.000000")];
  struct timeval now;
  int sec, usec;

  os_gettimeofday(&now, NULL);

  sec = (int)now.tv_sec + olsr_get_timezone();
  usec = (int)now.tv_usec;

  snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%06d", (sec % 86400) / 3600, (sec % 3600) / 60, sec % 60, usec);

  return buf;
}

/**
 * Format an relative non-wallclock system time string.
 * May be called upto 4 times in a single printf() statement.
 * Displays millisecond resolution.
 *
 * @param absolute time expressed in clockticks
 * @return buffer to a formatted system time string.
 */
const char *
olsr_timer_getClockString(uint32_t clk)
{
  static char buf[sizeof("00:00:00.000")];

  unsigned int msec = clk % 1000;
  unsigned int sec = clk / 1000;

  snprintf(buf, sizeof(buf), "%02u:%02u:%02u.%03u", sec / 3600, (sec % 3600) / 60, (sec % 60), (msec % MSEC_PER_SEC));

  return buf;
}

/**
 * Start a new timer.
 *
 * @param relative time expressed in milliseconds
 * @param jitter expressed in percent
 * @param timer callback function
 * @param context for the callback function
 * @return a pointer to the created entry
 */
struct olsr_timer_entry *
olsr_timer_start(unsigned int rel_time,
                 uint8_t jitter_pct, void *context, struct olsr_timer_info *ti)
{
  struct olsr_timer_entry *timer;

  assert(ti != 0);          /* we want timer cookies everywhere */
  assert(rel_time);
  assert(jitter_pct <= 100);

  timer = olsr_memcookie_malloc(timer_mem_cookie);

  /*
   * Compute random numbers only once.
   */
  if (!timer->timer_random) {
    timer->timer_random = random();
  }

  /* Fill entry */
  timer->timer_clock = calc_jitter(rel_time, jitter_pct, timer->timer_random);
  timer->timer_cb_context = context;
  timer->timer_jitter_pct = jitter_pct;
  timer->timer_running = true;

  /* The cookie is used for debugging to traceback the originator */
  timer->timer_info = ti;
  ti->usage++;
  ti->changes++;

  /* Singleshot or periodical timer ? */
  timer->timer_period = ti->periodic ? rel_time : 0;

  /*
   * Now insert in the respective timer_wheel slot.
   */
  list_add_before(&timer_wheel[timer->timer_clock & TIMER_WHEEL_MASK], &timer->timer_list);

  OLSR_DEBUG(LOG_TIMER, "TIMER: start %s timer %p firing in %s, ctx %p\n",
             ti->name, timer, olsr_timer_getClockString(timer->timer_clock), context);

  return timer;
}
#include "valgrind/valgrind.h"

/**
 * Delete a timer.
 *
 * @param the olsr_timer_entry that shall be removed
 * @return nada
 */
void
olsr_timer_stop(struct olsr_timer_entry *timer)
{
  /* It's okay to get a NULL here */
  if (timer == NULL) {
    return;
  }

  assert(timer->timer_info);     /* we want timer cookies everywhere */
  assert(timer->timer_list.next != NULL && timer->timer_list.prev != NULL);

  OLSR_DEBUG(LOG_TIMER, "TIMER: stop %s timer %p, ctx %p\n",
             timer->timer_info->name, timer, timer->timer_cb_context);


  /*
   * Carve out of the existing wheel_slot and free.
   */
  list_remove(&timer->timer_list);
  timer->timer_running = false;
  timer->timer_info->usage--;
  timer->timer_info->changes++;

  if (!timer->timer_in_callback) {
    olsr_memcookie_free(timer_mem_cookie, timer);
  }
}

/**
 * Change a olsr_timer_entry.
 *
 * @param olsr_timer_entry to be changed.
 * @param new relative time expressed in units of milliseconds.
 * @param new jitter expressed in percent.
 * @return nada
 */
void
olsr_timer_change(struct olsr_timer_entry *timer, unsigned int rel_time, uint8_t jitter_pct)
{
  /* Sanity check. */
  if (!timer) {
    return;
  }

  assert(timer->timer_info);     /* we want timer cookies everywhere */

  /* Singleshot or periodical timer ? */
  timer->timer_period = timer->timer_info->periodic ? rel_time : 0;

  timer->timer_clock = calc_jitter(rel_time, jitter_pct, timer->timer_random);
  timer->timer_jitter_pct = jitter_pct;

  /*
   * Changes are easy: Remove timer from the exisiting timer_wheel slot
   * and reinsert into the new slot.
   */
  list_remove(&timer->timer_list);
  list_add_before(&timer_wheel[timer->timer_clock & TIMER_WHEEL_MASK], &timer->timer_list);

  OLSR_DEBUG(LOG_TIMER, "TIMER: change %s timer %p, firing to %s, ctx %p\n",
             timer->timer_info->name, timer,
             olsr_timer_getClockString(timer->timer_clock), timer->timer_cb_context);
}

/*
 * This is the one stop shop for all sort of timer manipulation.
 * Depending on the paseed in parameters a new timer is started,
 * or an existing timer is started or an existing timer is
 * terminated.
 */
void
olsr_timer_set(struct olsr_timer_entry **timer_ptr,
               unsigned int rel_time,
               uint8_t jitter_pct, void *context, struct olsr_timer_info *ti)
{
  assert(ti);          /* we want timer cookies everywhere */
  if (rel_time == 0) {
    /* No good future time provided, kill it. */
    olsr_timer_stop(*timer_ptr);
    *timer_ptr = NULL;
  }
  else if ((*timer_ptr) == NULL) {
    /* No timer running, kick it. */
    *timer_ptr = olsr_timer_start(rel_time, jitter_pct, context, ti);
  }
  else {
    olsr_timer_change(*timer_ptr, rel_time, jitter_pct);
  }
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

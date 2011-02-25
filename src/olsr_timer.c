/*
 * olsr_timer.c
 *
 *  Created on: Feb 14, 2011
 *      Author: rogge
 */

#include <unistd.h>
#include <assert.h>
#include <stdlib.h>

#include "common/avl.h"
#include "common/avl_olsr_comp.h"
#include "olsr.h"
#include "olsr_logging.h"
#include "olsr_memcookie.h"
#include "olsr_socket.h"
#include "os_time.h"
#include "olsr_timer.h"

/* Hashed root of all timers */
static struct list_entity timer_wheel[TIMER_WHEEL_SLOTS];
static uint32_t timer_last_run;        /* remember the last timeslot walk */

/* Memory cookie for the timer manager */
struct list_entity timerinfo_list;
static struct olsr_memcookie_info *timer_mem_cookie = NULL;
static struct olsr_memcookie_info *timerinfo_cookie = NULL;

/* Prototypes */
static void walk_timers(uint32_t *);
static uint32_t calc_jitter(unsigned int rel_time, uint8_t jitter_pct, unsigned int random_val);
static int olsr_get_timezone(void);

/**
 * Init datastructures for maintaining timers.
 */
void
olsr_timer_init(void)
{
  int idx;

  OLSR_INFO(LOG_SCHEDULER, "Initializing scheduler.\n");

  /* init lists */
  for (idx = 0; idx < TIMER_WHEEL_SLOTS; idx++) {
    list_init_head(&timer_wheel[idx]);
  }

  /*
   * Reset the last timer run.
   */
  timer_last_run = olsr_clock_getNow();

  /* Allocate a cookie for the block based memory manager. */
  timer_mem_cookie = olsr_memcookie_add("timer_entry", sizeof(struct olsr_timer_entry));

  list_init_head(&timerinfo_list);
  timerinfo_cookie = olsr_memcookie_add("timerinfo", sizeof(struct olsr_timer_info));
}

/**
 * Cleanup timer scheduler, this stops and deletes all timers
 */
void
olsr_timer_cleanup(void)
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
    list_remove(&ti->node);
    free(ti->name);
    olsr_memcookie_free(timerinfo_cookie, ti);
  }

  /* release memory cookie for timers */
  olsr_memcookie_remove(timerinfo_cookie);
}

/**
 * Add a new group of timers to the scheduler
 * @param name
 * @param callback timer event function
 * @param periodic true if the timer is periodic, false otherwise
 * @return new timer info
 */
struct olsr_timer_info *
olsr_timer_add(const char *name, timer_cb_func callback, bool periodic) {
  struct olsr_timer_info *ti;

  ti = olsr_memcookie_malloc(timerinfo_cookie);
  ti->name = strdup(name);
  ti->callback = callback;
  ti->periodic = periodic;

  list_add_tail(&timerinfo_list, &ti->node);
  return ti;
}

/**
 * Main timer scheduler event loop.
 * Will call socket scheduler and olsr_process_changes()
 */
void
olsr_timer_scheduler(void)
{
  OLSR_INFO(LOG_SCHEDULER, "Scheduler started - polling every %u ms\n", olsr_cnf->pollrate);

  /* Main scheduler loop */
  while (app_state == STATE_RUNNING) {
    uint32_t next_interval;

    /*
     * Update the global timestamp. We are using a non-wallclock timer here
     * to avoid any undesired side effects if the system clock changes.
     */
    olsr_clock_update();
    next_interval = olsr_clock_getAbsolute(olsr_cnf->pollrate);

    /* Process timers */
    walk_timers(&timer_last_run);

    /* Update */
    olsr_process_changes();

    /* Read incoming data and handle it immediately */
    handle_sockets(next_interval);
  }
}

/**
 * Format an absolute wallclock system time string.
 * May be called upto 4 times in a single printf() statement.
 * Displays microsecond resolution.
 *
 * @return buffer to a formatted system time string.
 */
const char *
olsr_timer_getWallclockString(struct timeval_buf *buf)
{
  struct timeval now;
  int sec, usec;

  os_gettimeofday(&now, NULL);

  sec = (int)now.tv_sec + olsr_get_timezone();
  usec = (int)now.tv_usec;

  snprintf(buf->buf, sizeof(buf), "%02d:%02d:%02d.%06d",
      (sec % 86400) / 3600, (sec % 3600) / 60, sec % 60, usec);

  return buf->buf;
}

/**
 * Format an relative non-wallclock system time string.
 * Displays millisecond resolution.
 *
 * @param absolute timestamp
 * @return buffer to a formatted system time string.
 */
const char *
olsr_timer_getClockString(struct timeval_buf *buf, uint32_t clk)
{
  unsigned int msec = clk % 1000;
  unsigned int sec = clk / 1000;

  snprintf(buf->buf, sizeof(buf),
      "%02u:%02u:%02u.%03u", sec / 3600, (sec % 3600) / 60, (sec % 60), (msec % MSEC_PER_SEC));

  return buf->buf;
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
#if !defined(REMOVE_LOG_DEBUG)
  struct timeval_buf timebuf;
#endif

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
             ti->name, timer, olsr_timer_getClockString(&timebuf, timer->timer_clock), context);

  return timer;
}

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
#if !defined(REMOVE_LOG_DEBUG)
  struct timeval_buf timebuf;
#endif

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
             olsr_timer_getClockString(&timebuf, timer->timer_clock), timer->timer_cb_context);
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

/**
 * Decrement a relative timer by a random number range.
 *
 * @param the relative timer expressed in units of milliseconds.
 * @param the jitter in percent
 * @param cached result of random() at system init.
 * @return the absolute timer
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
    return olsr_clock_getAbsolute(rel_time);
  }

  /*
   * Play some tricks to avoid overflows with integer arithmetic.
   */
  jitter_time = (jitter_pct * rel_time) / 100;
  jitter_time = random_val / (1 + RAND_MAX / (jitter_time + 1));

  OLSR_DEBUG(LOG_TIMER, "TIMER: jitter %u%% rel_time %ums to %ums\n", jitter_pct, rel_time, rel_time - jitter_time);

  return olsr_clock_getAbsolute(rel_time - jitter_time);
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
  while ((*last_run <= olsr_clock_getNow()) && (wheel_slot_walks < TIMER_WHEEL_SLOTS)) {
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
      if (olsr_clock_isPast(timer->timer_clock)) {
#if !defined(REMOVE_LOG_DEBUG)
  struct timeval_buf timebuf;
#endif
        OLSR_DEBUG(LOG_TIMER, "TIMER: fire %s timer %p, ctx %p, "
                   "at clocktick %u (%s)\n",
                   timer->timer_info->name,
                   timer, timer->timer_cb_context, (unsigned int)*last_run,
                   olsr_timer_getWallclockString(&timebuf));

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
  *last_run = olsr_clock_getNow();
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



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


#ifndef _OLSR_SCHEDULER
#define _OLSR_SCHEDULER

#include "olsr_time.h"
#include "common/list.h"
#include "common/avl.h"

#include "olsr_types.h"

#include <time.h>

#define TIMER_WHEEL_SLOTS 1024
#define TIMER_WHEEL_MASK (TIMER_WHEEL_SLOTS - 1)

/* prototype for timer callback */
typedef void (*timer_cb_func) (void *);

/*
 * This struct defines a class of timers which have the same
 * type (periodic/non-periodic) and callback.
 */
struct olsr_timer_info {
  /* node of timerinfo tree */
  struct avl_node node;

  /* name of this timer class */
  char *name;

  /* callback function */
  timer_cb_func callback;

  /* true if this is a class of periodic timers */
  bool periodic;

  /* Stats, resource usage */
  uint32_t usage;

  /* Stats, resource churn */
  uint32_t changes;
};


/*
 * Our timer implementation is a based on individual timers arranged in
 * a double linked list hanging of hash containers called a timer wheel slot.
 * For every timer a olsr_timer_entry is created and attached to the timer wheel slot.
 * When the timer fires, the timer_cb function is called with the
 * context pointer.
 */
struct olsr_timer_entry {
  /* Wheel membership */
  struct list_entity timer_list;

  /* backpointer to timer info */
  struct olsr_timer_info *timer_info;

  /* when timer shall fire (absolute internal timerstamp) */
  uint32_t timer_clock;

  /* timeperiod between two timer events for periodical timers */
  uint32_t timer_period;

  /* the jitter expressed in percent */
  uint8_t timer_jitter_pct;

  /* true if timer is running at the moment */
  bool timer_running;

  /* true if timer is in callback at the moment */
  bool timer_in_callback;

  /* cache random() result for performance reasons */
  unsigned int timer_random;

  /* context pointer */
  void *timer_cb_context;
};

/* Timers */
extern struct avl_tree EXPORT(timerinfo_tree);
#define OLSR_FOR_ALL_TIMERS(ti, iterator) avl_for_each_element_safe(&timerinfo_tree, ti, node, iterator)

void olsr_init_timers(void);
void olsr_flush_timers(void);

uint32_t EXPORT(olsr_timer_getAbsolute) (uint32_t relative);
int32_t EXPORT(olsr_timer_getRelative) (uint32_t absolute);
bool EXPORT(olsr_timer_isTimedOut) (uint32_t s);

/**
 * Calculates the current time in the internal OLSR representation
 * @return current time
 */
static inline uint32_t olsr_timer_getNow(void) {
  return olsr_timer_getAbsolute(0);
}

void EXPORT(olsr_timer_set) (struct olsr_timer_entry **, uint32_t, uint8_t,
    void *, struct olsr_timer_info *);
struct olsr_timer_entry *EXPORT(olsr_timer_start) (uint32_t, uint8_t,
    void *, struct olsr_timer_info *);
void EXPORT(olsr_timer_change)(struct olsr_timer_entry *, uint32_t, uint8_t);
void EXPORT(olsr_timer_stop) (struct olsr_timer_entry *);

struct olsr_timer_info *EXPORT(olsr_timer_add)(const char *name, timer_cb_func callback, bool periodic);

/* Printing timestamps */
const char *EXPORT(olsr_timer_getClockString)(uint32_t);
const char *EXPORT(olsr_timer_getWallclockString)(void);

/* Main scheduler loop */
void olsr_scheduler(void);

/* flags for socket handler */
static const unsigned int OLSR_SOCKET_READ = 0x04;
static const unsigned int OLSR_SOCKETPOLL_WRITE = 0x08;

/* prototype for socket handler */
typedef void (*socket_handler_func) (int fd, void *data, unsigned int flags);

/* This struct represents a single registered socket handler */
struct olsr_socket_entry {
  /* list of socket handlers */
  struct list_entity socket_node;

  /* file descriptor of the socket */
  int fd;

  /* socket handler */
  socket_handler_func process_immediate;

  /* custom data pointer for sockets */
  void *data;

  /* flags (OLSR_SOCKET_READ and OLSR_SOCKET_WRITE) */
  unsigned int flags;
};

/* deletion safe macro for socket list traversal */
extern struct list_entity EXPORT(socket_head);
#define OLSR_FOR_ALL_SOCKETS(socket, iterator) list_for_each_element_safe(&socket_head, socket, socket_node, iterator)

void olsr_socket_cleanup(void);

void EXPORT(olsr_socket_add) (int fd, socket_handler_func pf_imm, void *data, unsigned int flags);
int EXPORT(olsr_socket_remove) (int fd, socket_handler_func pf_imm);

void EXPORT(olsr_socket_enable) (int fd, socket_handler_func pf_imm, unsigned int flags);
void EXPORT(olsr_socket_disable) (int fd, socket_handler_func pf_imm, unsigned int flags);

#endif

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

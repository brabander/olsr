/*
 * olsr_timer.h
 *
 *  Created on: Feb 14, 2011
 *      Author: rogge
 */

#ifndef OLSR_TIMER_H_
#define OLSR_TIMER_H_

#include "olsr_time.h"
#include "common/list.h"
#include "common/avl.h"

#include "olsr_types.h"

#define TIMER_WHEEL_SLOTS 1024
#define TIMER_WHEEL_MASK (TIMER_WHEEL_SLOTS - 1)

/* prototype for timer callback */
typedef void (*timer_cb_func) (void *);

/*
 * This struct defines a class of timers which have the same
 * type (periodic/non-periodic) and callback.
 */
struct olsr_timer_info {
  /* node of timerinfo list */
  struct list_entity node;

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

/* buffer for displaying absolute timestamps */
struct timeval_buf {
  char buf[sizeof("00:00:00.000000")];
};

/* Timers */
extern struct list_entity EXPORT(timerinfo_list);
#define OLSR_FOR_ALL_TIMERS(ti, iterator) list_for_each_element_safe(&timerinfo_list, ti, node, iterator)

void olsr_timer_init(void);
void olsr_timer_cleanup(void);
void olsr_timer_updateClock(void);

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

const char *EXPORT(olsr_timer_getClockString)(struct timeval_buf *, uint32_t);
const char *EXPORT(olsr_timer_getWallclockString)(struct timeval_buf *);

/* Main scheduler loop */
void olsr_timer_scheduler(void);

#endif /* OLSR_TIMER_H_ */

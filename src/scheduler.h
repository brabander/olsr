/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2004 Andreas Tønnesen (andreto@ifi.uio.no)
 *
 * This file is part of the olsr.org OLSR daemon.
 *
 * olsr.org is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * olsr.org is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with olsr.org; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 * 
 * $Id: scheduler.h,v 1.8 2004/11/12 21:20:23 kattemat Exp $
 *
 */




#ifndef _OLSR_SCHEDULER
#define _OLSR_SCHEDULER


/* List entries */

/* Timeout entry */

struct timeout_entry
{
  void (*function)(void);
  struct timeout_entry *next;
};

/* Event entry */

struct event_entry
{
  void (*function)(void *);
  void *param;
  float interval;
  float since_last;
  olsr_u8_t *trigger;
  struct event_entry *next;
};


/* Lists */
struct timeout_entry *timeout_functions;
struct event_entry *event_functions;

float will_int; /* Willingness update interval */
float max_jitter;

int
olsr_register_timeout_function(void (*)(void));

int
olsr_remove_timeout_function(void (*)(void));

int
olsr_register_scheduler_event(void (*)(void *), void *, float, float, olsr_u8_t *);

int
olsr_remove_scheduler_event(void (*)(void *), void *, float, float, olsr_u8_t *);

void
scheduler(void);

#endif

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
 */


#ifndef _OLSR_FUNCTIONS
#define _OLSR_FUNCTIONS

#include "olsr_protocol.h"
#include "interfaces.h"


/**
 * Process changes functions
 */

struct pcf
{
  int (*function)(int, int, int);
  struct pcf *next;
};

struct pcf *pcf_list;


olsr_8_t changes_topology;
olsr_8_t changes_neighborhood;
olsr_8_t changes_hna;

olsr_u16_t message_seqno;

#define TIMED_OUT(s1) \
        timercmp(s1, &now, <)

/*
 * Queueing macros
 */

/* First "argument" is NOT a pointer! */

#define QUEUE_ELEM(pre, new) \
        pre.next->prev = new; \
        new->next = pre.next; \
        new->prev = &pre; \
        pre.next = new

#define DEQUEUE_ELEM(elem) \
	elem->prev->next = elem->next; \
	elem->next->prev = elem->prev


void
register_pcf(int (*)(int, int, int));

inline void
olsr_process_changes();

inline void
olsr_init_timer(olsr_u32_t, struct timeval *);

inline void
olsr_get_timestamp(olsr_u32_t, struct timeval *);

void
init_msg_seqno();

inline olsr_u16_t
get_msg_seqno();

int
olsr_forward_message(union olsr_message *, 
		     union olsr_ip_addr *, 
		     olsr_u16_t, 
		     struct interface *, 
		     union olsr_ip_addr *);

int
buffer_forward(union olsr_message *, olsr_u16_t);

void
olsr_init_tables();

void
olsr_init_willingness();

void
olsr_update_willingness();

olsr_u8_t
olsr_calculate_willingness();

void
olsr_exit(const char *, int);

void *
olsr_malloc(size_t, const char *);

inline int
olsr_printf(int, char *, ...);

#endif

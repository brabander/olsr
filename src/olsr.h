/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004, Andreas Tønnesen(andreto@olsr.org)
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
 * $Id: olsr.h,v 1.16 2005/01/17 17:19:55 tlopatic Exp $
 */


#ifndef _OLSR_FUNCTIONS
#define _OLSR_FUNCTIONS

#include "olsr_protocol.h"
#include "interfaces.h"

#include <sys/time.h>

/**
 * Process changes functions
 */

struct pcf
{
  int (*function)(int, int, int);
  struct pcf *next;
};

struct pcf *pcf_list;


olsr_bool changes_topology;
olsr_bool changes_neighborhood;
olsr_bool changes_hna;

olsr_u16_t message_seqno;

/* Provides a timestamp s1 milliseconds in the future
   according to system ticks returned by times(2) */
#define GET_TIMESTAMP(s1) \
        now_times + ((s1) / system_tick_divider)

#define TIMED_OUT(s1) \
        ((s1) - now_times < 0)


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

void
olsr_process_changes(void);

inline void
olsr_init_timer(olsr_u32_t, struct timeval *);

inline void
olsr_get_timestamp(olsr_u32_t, struct timeval *);

void
init_msg_seqno(void);

inline olsr_u16_t
get_msg_seqno(void);

int
olsr_forward_message(union olsr_message *, 
		     union olsr_ip_addr *, 
		     olsr_u16_t, 
		     struct interface *, 
		     union olsr_ip_addr *);

void
set_buffer_timer(struct interface *);

void
olsr_init_tables(void);

void
olsr_init_willingness(void);

void
olsr_update_willingness(void *);

olsr_u8_t
olsr_calculate_willingness(void);

void
olsr_exit(const char *, int);

void *
olsr_malloc(size_t, const char *);

inline int
olsr_printf(int, char *, ...);

#endif

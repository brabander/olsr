
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
#ifndef _OLSR_MPRS_SET
#define _OLSR_MPRS_SET

#include "mantissa.h"
#include "defs.h"
#include "common/list.h"

#define OLSR_MPR_SEL_JITTER 5   /* percent */

struct mpr_selector {
  union olsr_ip_addr MS_main_addr;
  struct timer_entry *MS_timer;
  struct list_node mprs_list;
};

/* inline to recast from link_list back to link_entry */
LISTNODE2STRUCT(list2mpr, struct mpr_selector, mprs_list);

#define FOR_ALL_MPRS_ENTRIES(elem)  \
{ \
  struct list_node *elem_node, *next_elem_node; \
  for (elem_node = mprs_list_head.next;      \
       elem_node != &mprs_list_head; /* circular list */ \
       elem_node = next_elem_node) { \
    next_elem_node = elem_node->next; \
    elem = list2mpr(elem_node);

#define FOR_ALL_MPRS_ENTRIES_END(elem) }}


extern uint16_t ansn;

void olsr_init_mprs(void);

static INLINE uint16_t
get_local_ansn(void)
{
  return ansn;
}

static INLINE void
increase_local_ansn(void)
{
  ansn++;
}

struct mpr_selector *EXPORT(olsr_lookup_mprs_set) (const union olsr_ip_addr *);

int olsr_update_mprs_set(const union olsr_ip_addr *, olsr_reltime);

void
  olsr_print_mprs_set(void);

#endif

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */


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


#ifndef _OLSR_HNA
#define _OLSR_HNA

#include "olsr_types.h"
#include "olsr_protocol.h"
#include "duplicate_set.h"
#include "common/avl.h"

struct hna_net {
  struct avl_node hna_tc_node;         /* node in the per-tc hna tree */
  struct olsr_ip_prefix hna_prefix;    /* the prefix, key */
  struct timer_entry *hna_net_timer;   /* expiration timer */
  struct tc_entry *hna_tc;             /* backpointer to the owning tc entry */
  uint16_t tc_entry_seqno;             /* sequence number for cleanup */
};

#define OLSR_HNA_NET_JITTER 5   /* percent */

AVLNODE2STRUCT(hna_tc_tree2hna, hna_net, hna_tc_node)
#define OLSR_FOR_ALL_TC_HNA_ENTRIES(tc, hna_set) OLSR_FOR_ALL_AVL_ENTRIES(&tc->hna_tree, hna_tc_tree2hna, hna_set)
#define OLSR_FOR_ALL_TC_HNA_ENTRIES_END() OLSR_FOR_ALL_AVL_ENTRIES_END()

/* HNA msg input parser */
void olsr_input_hna(struct olsr_message *, const uint8_t *, const uint8_t *,
    struct interface *, union olsr_ip_addr *, enum duplicate_status);

void olsr_init_hna_set(void);
void olsr_flush_hna_nets(struct tc_entry *tc);
void olsr_print_hna_set(void);
void generate_hna(void *p);

#endif

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

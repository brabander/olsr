
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

#ifndef _OLSR_MID
#define _OLSR_MID

#include "defs.h"
#include "olsr_types.h"
#include "olsr_protocol.h"
#include "common/avl.h"
#include "duplicate_set.h"

struct mid_entry {
  struct avl_node mid_tc_node;         /* node in the per-tc mid tree */
  struct avl_node mid_node;            /* node in the global mid tree */
  union olsr_ip_addr mid_alias_addr;   /* key for both trees */
  struct tc_entry *mid_tc;             /* backpointer to owning tc entry */
  uint16_t mid_entry_seqno;            /* msg seq number for change tracking */
};

AVLNODE2STRUCT(global_tree2mid, struct mid_entry, mid_node);
AVLNODE2STRUCT(alias_tree2mid, struct mid_entry, mid_tc_node);

#define OLSR_FOR_ALL_MID_ENTRIES(mid_alias) \
{ \
  struct avl_node *mid_alias_node, *next_mid_alias_node; \
  for (mid_alias_node = avl_walk_first(&mid_tree); \
    mid_alias_node; mid_alias_node = next_mid_alias_node) { \
    next_mid_alias_node = avl_walk_next(mid_alias_node); \
    mid_alias = global_tree2mid(mid_alias_node);
#define OLSR_FOR_ALL_MID_ENTRIES_END(mid_alias) }}

#define OLSR_FOR_ALL_TC_MID_ENTRIES(tc, mid_alias) \
{ \
  struct avl_node *mid_alias_node, *next_mid_alias_node; \
  for (mid_alias_node = avl_walk_first(&tc->mid_tree); \
    mid_alias_node; mid_alias_node = next_mid_alias_node) { \
    next_mid_alias_node = avl_walk_next(mid_alias_node); \
    mid_alias = alias_tree2mid(mid_alias_node);
#define OLSR_FOR_ALL_TC_MID_ENTRIES_END(tc, mid_alias) }}

#define OLSR_MID_JITTER 5       /* percent */

extern struct avl_tree mid_tree;

/* MID msg input parser */
void olsr_input_mid(union olsr_message *, struct interface *, union olsr_ip_addr *, enum duplicate_status);

void olsr_init_mid_set(void);
void olsr_delete_mid_entry(struct mid_entry *);
void olsr_flush_mid_entries(struct tc_entry *);
union olsr_ip_addr *EXPORT(olsr_lookup_main_addr_by_alias) (const union olsr_ip_addr *);
struct mid_entry *olsr_lookup_tc_mid_entry(struct tc_entry *, const union olsr_ip_addr *);
void olsr_print_mid_set(void);
void generate_mid(void *p);
#endif

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

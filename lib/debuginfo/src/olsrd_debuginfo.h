
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

/*
 * Dynamic linked library for the olsr.org olsr daemon
 */

#ifndef _OLSRD_TXTINFO
#define _OLSRD_TXTINFO

#include "olsr_types.h"
#include "plugin.h"
#include "plugin_util.h"
#include "common/avl.h"

enum debug_msgtraffic_type {
  DTR_HELLO,
  DTR_TC,
  DTR_MID,
  DTR_HNA,
  DTR_OTHER,

  DTR_MESSAGES,
  DTR_MSG_TRAFFIC,

  /* this one must be the last one */
  DTR_MSG_COUNT
};

enum debug_pkttraffic_type {
  DTR_PACKETS,
  DTR_PACK_TRAFFIC,

  /* this one must be the last one */
  DTR_PKT_COUNT
};

struct debug_msgtraffic_count {
  uint32_t data[DTR_MSG_COUNT];
};

struct debug_pkttraffic_count {
  uint32_t data[DTR_PKT_COUNT];
};

struct debug_msgtraffic {
  struct avl_node node;
  union olsr_ip_addr ip;

  struct debug_msgtraffic_count total;
  struct debug_msgtraffic_count current;
  struct debug_msgtraffic_count traffic[0];
};

struct debug_pkttraffic {
  struct avl_node node;
  union olsr_ip_addr ip;
  char *int_name;

  struct debug_pkttraffic_count total;
  struct debug_pkttraffic_count current;
  struct debug_pkttraffic_count traffic[0];
};

#define OLSR_FOR_ALL_MSGTRAFFIC_ENTRIES(tr) \
{ \
  struct avl_node *tr_tree_node, *next_tr_tree_node; \
  for (tr_tree_node = avl_walk_first(&stat_msg_tree); \
    tr_tree_node; tr_tree_node = next_tr_tree_node) { \
    next_tr_tree_node = avl_walk_next(tr_tree_node); \
    tr = (struct debug_msgtraffic *)(tr_tree_node);
#define OLSR_FOR_ALL_MSGTRAFFIC_ENTRIES_END(tr) }}

#define OLSR_FOR_ALL_PKTTRAFFIC_ENTRIES(tr) \
{ \
  struct avl_node *tr_tree_node, *next_tr_tree_node; \
  for (tr_tree_node = avl_walk_first(&stat_pkt_tree); \
    tr_tree_node; tr_tree_node = next_tr_tree_node) { \
    next_tr_tree_node = avl_walk_next(tr_tree_node); \
    tr = (struct debug_pkttraffic *)(tr_tree_node);
#define OLSR_FOR_ALL_PKTTRAFFIC_ENTRIES_END(tr) }}

#endif

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

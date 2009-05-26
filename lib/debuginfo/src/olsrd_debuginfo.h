
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

enum debug_traffic_type {
  DTR_HELLO,
  DTR_TC,
  DTR_MID,
  DTR_HNA,
  DTR_OTHER,

  DTR_MESSAGES,
  DTR_MSG_TRAFFIC,

  DTR_PACKETS,
  DTR_PACK_TRAFFIC,

  /* this one must be the last one */
  DTR_COUNT
};

struct debug_traffic_count {
  uint32_t data[DTR_COUNT];
};

struct debug_traffic {
  struct avl_node node;
  union olsr_ip_addr ip;

  struct debug_traffic_count total;
  struct debug_traffic_count current;
  struct debug_traffic_count traffic[0];
};

#define OLSR_FOR_ALL_DEBUGTRAFFIC_ENTRIES(tr) \
{ \
  struct avl_node *tr_tree_node, *next_tr_tree_node; \
  for (tr_tree_node = avl_walk_first(&statistics_tree); \
    tr_tree_node; tr_tree_node = next_tr_tree_node) { \
    next_tr_tree_node = avl_walk_next(tr_tree_node); \
    tr = (struct debug_traffic *)(tr_tree_node);
#define OLSR_FOR_ALL_DEBUGTRAFFIC_ENTRIES_END(tc) }}

#endif

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

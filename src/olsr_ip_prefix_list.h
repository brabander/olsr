
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

#ifndef OLSR_IP_PREFIX_LIST_H_
#define OLSR_IP_PREFIX_LIST_H_

#include "defs.h"
#include "olsr_types.h"
#include "common/list.h"

struct ip_prefix_entry {
  struct list_node node;
  struct olsr_ip_prefix net;
};

/* inline to recast from node back to ip_prefix_entry */
LISTNODE2STRUCT(list2ipprefix, struct ip_prefix_entry, node);

/* deletion safe macro for ip_prefix traversal */
#define OLSR_FOR_ALL_IPPREFIX_ENTRIES(ipprefix_head, ipprefix_node) \
{ \
  struct list_node *link_head_node, *link_node, *next_link_node; \
  link_head_node = ipprefix_head; \
  for (link_node = link_head_node->next; \
    link_node != link_head_node; link_node = next_link_node) { \
    next_link_node = link_node->next; \
    ipprefix_node = list2ipprefix(link_node);
#define OLSR_FOR_ALL_IPPREFIX_ENTRIES_END() }}

//struct ip_prefix_list {
//  struct olsr_ip_prefix net;
//  struct ip_prefix_list *next;
//};

/*
 * List functions
 */
void EXPORT(ip_prefix_list_add) (struct list_node *, const union olsr_ip_addr *, uint8_t);
int EXPORT(ip_prefix_list_remove) (struct list_node *, const union olsr_ip_addr *, uint8_t, int);
void ip_prefix_list_flush(struct list_node *);
struct ip_prefix_entry *ip_prefix_list_find(struct list_node *, const union olsr_ip_addr *, uint8_t, int);


#endif /* OLSR_IP_PREFIX_LIST_H_ */

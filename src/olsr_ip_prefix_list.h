
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
  struct list_entity node;
  struct olsr_ip_prefix net;
};

/* deletion safe macro for ip_prefix traversal */
#define OLSR_FOR_ALL_IPPREFIX_ENTRIES(head, prefix_node, iterator) list_for_each_element_safe(head, prefix_node, node, iterator)

/*
 * List functions
 */
void EXPORT(ip_prefix_list_add) (struct list_entity *, const union olsr_ip_addr *, uint8_t);
int EXPORT(ip_prefix_list_remove) (struct list_entity *, const union olsr_ip_addr *, uint8_t, int);
void ip_prefix_list_flush(struct list_entity *);
struct ip_prefix_entry *ip_prefix_list_find(struct list_entity *, const union olsr_ip_addr *, uint8_t, int);


#endif /* OLSR_IP_PREFIX_LIST_H_ */

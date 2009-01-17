
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

#include "olsr_types.h"
#include "olsr.h"
#include "ipcalc.h"
#include "olsr_ip_prefix_list.h"

void
ip_prefix_list_flush(struct list_node *ip_prefix_head)
{
  struct ip_prefix_entry *entry;

  OLSR_FOR_ALL_IPPREFIX_ENTRIES(ip_prefix_head, entry) {
    free(entry);
  } OLSR_FOR_ALL_IPPREFIX_ENTRIES_END();
}

void
ip_prefix_list_add(struct list_node *ip_prefix_head, const union olsr_ip_addr *net, uint8_t prefix_len)
{
  struct ip_prefix_entry *new_entry = olsr_malloc(sizeof(*new_entry), "new ip_prefix");

  new_entry->net.prefix = *net;
  new_entry->net.prefix_len = prefix_len;

  /* Queue */
  list_add_before(ip_prefix_head, &new_entry->node);
}

int
ip_prefix_list_remove(struct list_node *ip_prefix_head, const union olsr_ip_addr *net, uint8_t prefix_len, int ip_version)
{
  struct ip_prefix_entry *h;

  OLSR_FOR_ALL_IPPREFIX_ENTRIES(ip_prefix_head, h) {
    if (ipequal(ip_version, net, &h->net.prefix) && h->net.prefix_len == prefix_len) {
      free(h);
      return 1;
    }
  }
  OLSR_FOR_ALL_IPPREFIX_ENTRIES_END();
  return 0;
}

struct ip_prefix_entry *
ip_prefix_list_find(struct list_node *ip_prefix_head, const union olsr_ip_addr *net, uint8_t prefix_len, int ip_version)
{
  struct ip_prefix_entry *h;

  OLSR_FOR_ALL_IPPREFIX_ENTRIES(ip_prefix_head, h) {
    if (prefix_len == h->net.prefix_len && ipequal(ip_version, net, &h->net.prefix)) {
      return h;
    }
  }
  OLSR_FOR_ALL_IPPREFIX_ENTRIES_END();
  return NULL;
}

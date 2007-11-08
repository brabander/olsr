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
 * $Id: local_hna_set.c,v 1.16 2007/11/08 22:47:41 bernd67 Exp $
 */

#include "defs.h"
#include "local_hna_set.h"
#include "olsr.h"
#include "net_olsr.h"

void
add_local_hna4_entry(const union olsr_ip_addr *net, const union olsr_ip_addr *mask)
{
  struct local_hna_entry *new_entry = olsr_malloc(sizeof(struct local_hna_entry), "Add local HNA entry 4");
  
  new_entry->net.prefix.v4 = net->v4;
  new_entry->net.prefix_len = olsr_netmask_to_prefix(mask);

  /* Queue */
  new_entry->next = olsr_cnf->hna_entries;
  olsr_cnf->hna_entries = new_entry;
}


void
add_local_hna6_entry(const union olsr_ip_addr *net, const olsr_u16_t prefix_len)
{
  struct local_hna_entry *new_entry = olsr_malloc(sizeof(struct local_hna_entry), "Add local HNA entry 6");
  
  memcpy(&new_entry->net, net, sizeof(struct in6_addr));
  new_entry->net.prefix_len = prefix_len;

  /* Queue */
  new_entry->next = olsr_cnf->hna_entries;
  olsr_cnf->hna_entries = new_entry;
}


int
remove_local_hna4_entry(const union olsr_ip_addr *net, const union olsr_ip_addr *mask)
{
  struct local_hna_entry *h = olsr_cnf->hna_entries, *prev = NULL;
  const olsr_u16_t prefix_len = olsr_netmask_to_prefix(mask);

  while(h)
    {
      if((net->v4.s_addr == h->net.prefix.v4.s_addr) && 
	 (mask->v4.s_addr == prefix_len))
	{
	  /* Dequeue */
	  if(prev == NULL)
	    olsr_cnf->hna_entries = h->next;
	  else
	    prev->next = h->next;

	  free(h);
	  return 1;
	}
      prev = h;
      h = h->next;
    }

  return 0;
}



int
remove_local_hna6_entry(const union olsr_ip_addr *net, const olsr_u16_t prefix_len)
{
  struct local_hna_entry *h = olsr_cnf->hna_entries, *prev = NULL;

  while (h)
    {
      if((memcmp(net, &h->net.prefix, olsr_cnf->ipsize) == 0) && 
	 (prefix_len == h->net.prefix_len))
	{
	  /* Dequeue */
	  if (prev == NULL)
	    olsr_cnf->hna_entries = h->next;
	  else
	    prev->next = h->next;
	  free(h);
	  return 1;
	}
      prev = h;
      h = h->next;
    }
  return 0;
}

struct local_hna_entry *
find_local_hna4_entry(const union olsr_ip_addr *net, const olsr_u32_t mask)
{
  struct local_hna_entry *h = olsr_cnf->hna_entries;
  const union olsr_ip_addr ip_addr = { .v4 = { .s_addr = mask } };
  const olsr_u16_t prefix_len = olsr_netmask_to_prefix(&ip_addr);
  while(h)
    {
      if((net->v4.s_addr == h->net.prefix.v4.s_addr) && 
	 (prefix_len == h->net.prefix_len))
	{
	  return h;
	}
      h = h->next;
    }

  return NULL;
}



struct local_hna_entry *
find_local_hna6_entry(const union olsr_ip_addr *net, const olsr_u16_t prefix_len)
{
  struct local_hna_entry *h = olsr_cnf->hna_entries;
  while(h)
    {
      if((memcmp(net, &h->net.prefix, olsr_cnf->ipsize) == 0) && 
	 (prefix_len == h->net.prefix_len))
	{
	  return h;
	}
      h = h->next;
    }

  return NULL;
}




int
check_inet_gw(void)
{
  if(olsr_cnf->ip_version == AF_INET)
    {
      struct local_hna_entry *h;
      for(h = olsr_cnf->hna_entries; h != NULL; h = h->next)
	{
	  if(h->net.prefix_len == 0 && h->net.prefix.v4.s_addr == 0)
	    return 1;
	}
    }
  return 0;

}

/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2003 Andreas Tønnesen (andreto@ifi.uio.no)
 *
 * This file is part of the olsr.org OLSR daemon.
 *
 * olsr.org is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * olsr.org is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with olsr.org; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 * 
 * $Id: local_hna_set.c,v 1.7 2004/10/19 19:23:00 kattemat Exp $
 *
 */


#include "local_hna_set.h"


void
add_local_hna4_entry(union olsr_ip_addr *net, union olsr_ip_addr *mask)
{
  struct hna4_entry *new_entry;

  new_entry = olsr_malloc(sizeof(struct hna4_entry), "Add local HNA entry 4");
  
  new_entry->net.v4 = net->v4;
  new_entry->netmask.v4 = mask->v4;

  /* Queue */
  new_entry->next = olsr_cnf->hna4_entries;
  olsr_cnf->hna4_entries = new_entry;
}


void
add_local_hna6_entry(union olsr_ip_addr *net, olsr_u16_t prefix_len)
{
  struct hna6_entry *new_entry;

  new_entry = olsr_malloc(sizeof(struct hna6_entry), "Add local HNA entry 6");
  
  memcpy(&new_entry->net, net, sizeof(struct in6_addr));
  prefix_len = prefix_len;

  /* Queue */
  new_entry->next = olsr_cnf->hna6_entries;
  olsr_cnf->hna6_entries = new_entry;
}


int
remove_local_hna4_entry(union olsr_ip_addr *net, union olsr_ip_addr *mask)
{
  struct hna4_entry *h4 = olsr_cnf->hna4_entries, *h4prev = NULL;

  while(h4)
    {
      if((net->v4 == h4->net.v4) && 
	 (mask->v4 == h4->netmask.v4))
	{
	  /* Dequeue */
	  if(h4prev == NULL)
	    olsr_cnf->hna4_entries = h4->next;
	  else
	    h4prev->next = h4->next;

	  free(h4);
	  return 1;
	}
      h4prev = h4;
      h4 = h4->next;
    }

  return 0;
}



int
remove_local_hna6_entry(union olsr_ip_addr *net, olsr_u16_t prefix_len)
{
  struct hna6_entry *h6 = olsr_cnf->hna6_entries, *h6prev = NULL;

  while(h6)
    {
      if((memcmp(net, &h6->net, ipsize) == 0) && 
	 (prefix_len == h6->prefix_len))
	{
	  /* Dequeue */
	  if(h6prev == NULL)
	    olsr_cnf->hna6_entries = h6->next;
	  else
	    h6prev->next = h6->next;

	  free(h6);
	  return 1;
	}
      h6prev = h6;
      h6 = h6->next;
    }

  return 0;
}



int
check_inet_gw()
{
  struct hna4_entry *h4 = olsr_cnf->hna4_entries;

  if(olsr_cnf->ip_version == AF_INET)
    {
      while(h4)
	{
	  if(h4->netmask.v4 == 0 && h4->net.v4 == 0)
	    return 1;
	  h4 = h4->next;
	}
      return 0;
    }
  return 0;

}

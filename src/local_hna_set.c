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
 * $Id: local_hna_set.c,v 1.5 2004/09/21 19:08:57 kattemat Exp $
 *
 */


#include "local_hna_set.h"


int
olsr_init_local_hna_set()
{

  inet_gw = 0;
  if(ipversion == AF_INET)
    {
      netmask_size = sizeof(olsr_u32_t);
    }
  else
    {
      netmask_size = sizeof(olsr_u16_t);
    }

  local_hna4_set.next = &local_hna4_set;
  local_hna4_set.prev = &local_hna4_set;
  local_hna6_set.next = &local_hna6_set;
  local_hna6_set.prev = &local_hna6_set;

  return 1;
}


void
add_local_hna4_entry(union olsr_ip_addr *net, union hna_netmask *mask)
{
  struct local_hna_entry *new_entry;

  if((net->v4 == 0) && (mask->v4 == 0))
    inet_gw = 1;

  new_entry = olsr_malloc(sizeof(struct local_hna_entry), "Add local HNA entry 4");

  memcpy(&new_entry->A_network_addr, net, sizeof(olsr_u32_t));
  memcpy(&new_entry->A_netmask, mask, sizeof(olsr_u32_t));

  /* Queue */

  local_hna4_set.next->prev = new_entry;
  new_entry->next = local_hna4_set.next;
  local_hna4_set.next = new_entry;
  new_entry->prev = &local_hna4_set;
}


void
add_local_hna6_entry(union olsr_ip_addr *net, union hna_netmask *mask)
{
  struct local_hna_entry *new_entry;

  new_entry = olsr_malloc(sizeof(struct local_hna_entry), "Add local HNA entry 6");

  memcpy(&new_entry->A_network_addr, net, sizeof(struct in6_addr));
  memcpy(&new_entry->A_netmask, mask, sizeof(olsr_u16_t));

  /* Queue */

  local_hna6_set.next->prev = new_entry;
  new_entry->next = local_hna6_set.next;
  local_hna6_set.next = new_entry;
  new_entry->prev = &local_hna6_set;
}


int
remove_local_hna4_entry(union olsr_ip_addr *net, union hna_netmask *mask)
{
  struct local_hna_entry *entry;

  if((net->v4 == 0) && (mask->v4 == 0))
    inet_gw = 0;

  for(entry = local_hna4_set.next; 
      entry != &local_hna4_set;
      entry = entry->next)
    {
      if((net->v4 == entry->A_network_addr.v4) && 
	 (mask->v4 == entry->A_netmask.v4))
	{
	  entry->prev->next = entry->next;
	  entry->next->prev = entry->prev;

	  free(entry);
	  return 1;
	}
    }
  return 0;
}



int
remove_local_hna6_entry(union olsr_ip_addr *net, union hna_netmask *mask)
{
  struct local_hna_entry *entry;

  for(entry = local_hna6_set.next; 
      entry != &local_hna6_set;
      entry = entry->next)
    {
      if((memcmp(net, &entry->A_network_addr, ipsize) == 0) && 
	 (mask->v6 == entry->A_netmask.v6))
	{
	  entry->prev->next = entry->next;
	  entry->next->prev = entry->prev;
	  
	  free(entry);
	  return 1;
	}
    }
  return 0;
}



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
 * $Id: hna_set.c,v 1.8 2004/11/05 11:52:54 kattemat Exp $
 *
 */

#include "defs.h"
#include "olsr.h"
#include "scheduler.h"


/**
 *Initialize the HNA set
 */
int
olsr_init_hna_set()
{

  int index;

  if(olsr_cnf->ip_version == AF_INET)
    {
      netmask_size = sizeof(olsr_u32_t);
    }
  else
    {
      netmask_size = sizeof(olsr_u16_t);
    }

  /* Since the holdingtime is assumed to be rather large for 
   * HNA entries, the timeoutfunction is only ran once every second
   */
  olsr_register_scheduler_event(&olsr_time_out_hna_set, NULL, 1, 0, NULL);

  for(index=0;index<HASHSIZE;index++)
    {
      hna_set[index].next = &hna_set[index];
      hna_set[index].prev = &hna_set[index];
    }

  return 1;
}




/**
 *Lookup a network entry in a networkentry list
 *
 *@param nets the network list to look in
 *@param net the network to look for
 *@param mask the netmask to look for
 *
 *@return the localted entry or NULL of not found
 */
struct hna_net *
olsr_lookup_hna_net(struct hna_net *nets, union olsr_ip_addr *net, union hna_netmask *mask)
{
  struct hna_net *tmp_net;


  /* Loop trough entrys */
  for(tmp_net = nets->next;
      tmp_net != nets;
      tmp_net = tmp_net->next)
    { 
      if(COMP_IP(&tmp_net->A_network_addr, net) &&
	 (memcmp(&tmp_net->A_netmask, mask, netmask_size) == 0))
	return tmp_net;
    }
  
  /* Not found */
  return NULL;
}




/**
 *Lookup a gateway entry
 *
 *@param gw the address of the gateway
 *
 *@return the located entry or NULL if not found
 */
struct hna_entry *
olsr_lookup_hna_gw(union olsr_ip_addr *gw)
{
  struct hna_entry *tmp_hna;
  olsr_u32_t hash;

  //olsr_printf(5, "TC: lookup entry\n");

  hash = olsr_hashing(gw);
  
  /* Check for registered entry */
  for(tmp_hna = hna_set[hash].next;
      tmp_hna != &hna_set[hash];
      tmp_hna = tmp_hna->next)
    {
      if(COMP_IP(&tmp_hna->A_gateway_addr, gw))
	return tmp_hna;
    }
  
  /* Not found */
  return NULL;
}



/**
 *Add a gatewayentry to the HNA set
 *
 *@param addr the address of the gateway
 *
 *@return the created entry
 */
struct hna_entry *
olsr_add_hna_entry(union olsr_ip_addr *addr)
{
  struct hna_entry *new_entry;
  olsr_u32_t hash;

  new_entry = olsr_malloc(sizeof(struct hna_entry), "New HNA entry");

  /* Fill struct */
  COPY_IP(&new_entry->A_gateway_addr, addr);

  /* Link nets */
  new_entry->networks.next = &new_entry->networks;
  new_entry->networks.prev = &new_entry->networks;

  /* queue */
  hash = olsr_hashing(addr);
  
  hna_set[hash].next->prev = new_entry;
  new_entry->next = hna_set[hash].next;
  hna_set[hash].next = new_entry;
  new_entry->prev = &hna_set[hash];

  return new_entry;

}



/**
 *Adds a ntework entry to a HNA gateway
 *
 *@param hna_gw the gateway entry to add the
 *network to
 *@param net the networkaddress to add
 *@param mask the netmask
 *
 *@return the newly created entry
 */
struct hna_net *
olsr_add_hna_net(struct hna_entry *hna_gw, union olsr_ip_addr *net, union hna_netmask *mask)
{
  struct hna_net *new_net;


  /* Add the net */
  new_net = olsr_malloc(sizeof(struct hna_net), "Add HNA net");
  
  /* Fill struct */
  COPY_IP(&new_net->A_network_addr, net);
  memcpy(&new_net->A_netmask, mask, netmask_size);

  /* Queue */
  hna_gw->networks.next->prev = new_net;
  new_net->next = hna_gw->networks.next;
  hna_gw->networks.next = new_net;
  new_net->prev = &hna_gw->networks;

  return new_net;
}




/**
 * Update a HNA entry. If it does not exist it
 * is created.
 * This is the only function that should be called 
 * from outside concerning creation of HNA entries.
 *
 *@param gw address of the gateway
 *@param net address of the network
 *@param mask the netmask
 *@param vtime the validitytime of the entry
 *
 *@return nada
 */
void
olsr_update_hna_entry(union olsr_ip_addr *gw, union olsr_ip_addr *net, union hna_netmask *mask, float vtime)
{
  struct hna_entry *gw_entry;
  struct hna_net *net_entry;

  if((gw_entry = olsr_lookup_hna_gw(gw)) == NULL)
    /* Need to add the entry */
    gw_entry = olsr_add_hna_entry(gw);
  
  if((net_entry = olsr_lookup_hna_net(&gw_entry->networks, net, mask)) == NULL)
    {
      /* Need to add the net */
      net_entry = olsr_add_hna_net(gw_entry, net, mask);
      changes_hna = OLSR_TRUE;
    }

  /* Update holdingtime */
  olsr_get_timestamp((olsr_u32_t) vtime*1000, &net_entry->A_time);

}






/**
 *Function that times out all entrys in the hna set and
 *deletes the timed out ones.
 *
 *@return nada
 */
void
olsr_time_out_hna_set(void *foo)
{
  int index;
  struct hna_entry *tmp_hna, *hna_to_delete;
  struct hna_net *tmp_net, *net_to_delete;

  for(index=0;index<HASHSIZE;index++)
    {
      tmp_hna = hna_set[index].next;
      /* Check all entrys */
      while(tmp_hna != &hna_set[index])
	{
	  /* Check all networks */
	  tmp_net = tmp_hna->networks.next;

	  while(tmp_net != &tmp_hna->networks)
	    {
	      if(TIMED_OUT(&tmp_net->A_time))
		{
		  net_to_delete = tmp_net;
		  tmp_net = tmp_net->next;
		  delete_hna_net(net_to_delete);
		  changes_hna = OLSR_TRUE;
		}
	      else
		tmp_net = tmp_net->next;
	    }

	  /* Delete gw entry if empty */
	  if(tmp_hna->networks.next == &tmp_hna->networks)
	    {
	      hna_to_delete = tmp_hna;
	      tmp_hna = tmp_hna->next;
	      delete_hna_entry(hna_to_delete);
	    }
	  else
	    tmp_hna = tmp_hna->next;
	}
    }

}






/**
 *Deletes and dequeues a HNA network entry
 *
 *@param net the entry to delete
 */
void
delete_hna_net(struct hna_net *net)
{

  /* Dequeue */
  net->prev->next = net->next;
  net->next->prev = net->prev;

  /* Delete */
  free(net);

}




/**
 *Deletes and dequeues a hna gw entry
 *NETWORKS MUST BE DELETED FIRST!
 *
 *@param entry the entry to delete
 */
void
delete_hna_entry(struct hna_entry *entry)
{
  /* Dequeue */
  entry->prev->next = entry->next;
  entry->next->prev = entry->prev;

  /* Delete */
  free(entry);
}






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
 * $Id: mid_set.c,v 1.8 2004/11/05 11:52:55 kattemat Exp $
 *
 */

#include "defs.h"
#include "two_hop_neighbor_table.h"
#include "mid_set.h"
#include "olsr.h"
#include "scheduler.h"
#include "neighbor_table.h"
#include "link_set.h"

/**
 * Initialize the MID set
 *
 */

int
olsr_init_mid_set()
{
  int index;

  olsr_printf(5, "MID: init\n");

  /* Since the holdingtime is assumed to be rather large for 
   * MID entries, the timeoutfunction is only ran once every second
   */
  olsr_register_scheduler_event(&olsr_time_out_mid_set, NULL, 1, 0, NULL);

  for(index=0;index<HASHSIZE;index++)
    {
      mid_set[index].next = &mid_set[index];
      mid_set[index].prev = &mid_set[index];
    }

  return 1;
}


/**
 *Insert a new interface alias to the interface association set.
 *If the main interface of the association is not yet registered
 *in the set a new entry is created.
 *
 *@param m_addr the main address of the node
 *@param alias the alias address to insert
 *
 *@return nada
 */

void 
insert_mid_tuple(union olsr_ip_addr *m_addr, struct addresses *alias, float vtime)
{
  struct mid_entry *tmp;
  struct addresses *tmp_adr;
  struct neighbor_2_entry *tmp_2_neighbor;
  olsr_u32_t hash;
  struct neighbor_entry *tmp_neigh, *real_neigh;

  //olsr_printf(5, "TC: lookup entry\n");

  hash = olsr_hashing(m_addr);



  /* Check for registered entry */
  for(tmp = mid_set[hash].next;
      tmp != &mid_set[hash];
      tmp = tmp->next)
    {
      if(COMP_IP(&tmp->main_addr, m_addr))
	break;
    }
	 
  /*If the address was registered*/ 
  if(tmp != &mid_set[hash])
    {
      tmp_adr = tmp->aliases;
      tmp->aliases = alias;
      alias->next = tmp_adr;
      olsr_get_timestamp((olsr_u32_t) vtime*1000, &tmp->ass_timer);
    }
      /*Create new node*/
  else
    {
      tmp = olsr_malloc(sizeof(struct mid_entry), "MID new alias");

      tmp->aliases = alias;
      COPY_IP(&tmp->main_addr, m_addr);
      olsr_get_timestamp((olsr_u32_t) vtime*1000, &tmp->ass_timer);
      /* Queue */
      QUEUE_ELEM(mid_set[hash], tmp);
      /*
      tmp->next = mid_set[hash].next;
      tmp->prev = &mid_set[hash];
      mid_set[hash].next->prev = tmp;
      mid_set[hash].next = tmp;
      */
    }
  


  /*
   * Delete possible duplicate entries in 2 hop set
   * and delete duplicate neighbor entries. Redirect
   * link entries to the correct neighbor entry.
   *
   *THIS OPTIMALIZATION IS NOT SPECIFIED IN RFC3626
   */

  tmp_adr = alias;

  while(tmp_adr)
    {

      /* Delete possible 2 hop neighbor */
      if((tmp_2_neighbor = olsr_lookup_two_hop_neighbor_table_mid(&tmp_adr->address)) != NULL)
	{
	  olsr_printf(1, "Deleting 2 hop node from MID: %s to ", olsr_ip_to_string(&tmp_adr->address));
	  olsr_printf(1, "%s\n", olsr_ip_to_string(m_addr));

	  olsr_delete_two_hop_neighbor_table(tmp_2_neighbor);

	  changes_neighborhood = OLSR_TRUE;
	}

      /* Delete a possible neighbor entry */
      if(((tmp_neigh = olsr_lookup_neighbor_table_alias(&tmp_adr->address)) != NULL)
	 && ((real_neigh = olsr_lookup_neighbor_table_alias(m_addr)) != NULL))

	{
	  olsr_printf(1, "[MID]Deleting bogus neighbor entry %s real ", olsr_ip_to_string(&tmp_adr->address));
	  olsr_printf(1, "%s\n", olsr_ip_to_string(m_addr));

	  replace_neighbor_link_set(tmp_neigh, real_neigh);

	  /* Delete the neighbor */
	  /* Dequeue */
	  DEQUEUE_ELEM(tmp_neigh);
	  //tmp_neigh->prev->next = tmp_neigh->next;
	  //tmp_neigh->next->prev = tmp_neigh->prev;
	  /* Delete */
	  free(tmp_neigh);

	  changes_neighborhood = OLSR_TRUE;
	}
      
      tmp_adr = tmp_adr->next;
    }


}


/**
 *Insert an alias address for a node.
 *If the main address is not registered
 *then a new entry is created.
 *
 *@param main_add the main address of the node
 *@param alias the alias address to insert
 *@param seq the sequence number to register a new node with
 *
 *@return nada
 */
void
insert_mid_alias(union olsr_ip_addr *main_add, union olsr_ip_addr *alias, float vtime)
{
  struct addresses *adr;

  adr = olsr_malloc(sizeof(struct addresses), "Insert MID alias");

  olsr_printf(1, "Inserting alias %s for ", olsr_ip_to_string(alias));
  olsr_printf(1, "%s\n", olsr_ip_to_string(main_add));

  COPY_IP(&adr->address, alias);
  adr->next = NULL;

  insert_mid_tuple(main_add, adr, vtime);

  /*
   *Recalculate topology
   */
  changes_neighborhood = OLSR_TRUE;
  changes_topology = OLSR_TRUE;

  //print_mid_list();
}




/**
 *Lookup the main address for a alias address
 *
 *@param adr the alias address to check
 *
 *@return the main address registered on the alias
 *or NULL if not found
 */
union olsr_ip_addr *
mid_lookup_main_addr(union olsr_ip_addr *adr)
{

  struct mid_entry *tmp_list;
  struct addresses *tmp_addr;
  int index;


  for(index=0;index<HASHSIZE;index++)
    {
      tmp_list = mid_set[index].next;
      /*Traverse MID list*/
      while(tmp_list != &mid_set[index])
	{
	  if(COMP_IP(&tmp_list->main_addr, adr))
	    return NULL;

	  tmp_addr = tmp_list->aliases;
	  /*And all aliases registered on them*/
	  while(tmp_addr)
	    {
	      if(COMP_IP(&tmp_addr->address, adr))
		return &tmp_list->main_addr;
	      tmp_addr = tmp_addr->next;
	    }

	  tmp_list = tmp_list->next;
	}
    }
  return NULL;

}




/*
 *Find all aliases for an address.
 *
 *@param adr the main address to search for
 *
 *@return a linked list of addresses structs
 */
struct addresses *
mid_lookup_aliases(union olsr_ip_addr *adr)
{
  struct mid_entry *tmp_list;
  olsr_u32_t hash;

  //olsr_printf(1, "MID: lookup entry...");

  hash = olsr_hashing(adr);

  /* Check all registered nodes...*/
  for(tmp_list = mid_set[hash].next;
      tmp_list != &mid_set[hash];
      tmp_list = tmp_list->next)
    {
      if(COMP_IP(&tmp_list->main_addr, adr))
	return tmp_list->aliases;
    }


  return NULL;
}


/**
 *Update the timer for an entry
 *
 *@param adr the main address of the entry
 *
 *@return 1 if the node was updated, 0 if not
 */
int
olsr_update_mid_table(union olsr_ip_addr *adr, float vtime)
{
  struct mid_entry *tmp_list = mid_set;
  olsr_u32_t hash;

  olsr_printf(3, "MID: update %s\n", olsr_ip_to_string(adr));
  hash = olsr_hashing(adr);

  /* Check all registered nodes...*/
  for(tmp_list = mid_set[hash].next;
      tmp_list != &mid_set[hash];
      tmp_list = tmp_list->next)
    {
      /*find match*/
      if(COMP_IP(&tmp_list->main_addr, adr))
	{
	  //printf("Updating timer for node %s\n",ip_to_string(&tmp_list->main_addr));
	  olsr_get_timestamp((olsr_u32_t) vtime*1000, &tmp_list->ass_timer);

	  return 1;
	}
    }
  return 0;
}



/**
 *Find timed out entries and delete them
 *
 *@return nada
 */
void
olsr_time_out_mid_set(void *foo)
{
  struct mid_entry *tmp_list;
  struct mid_entry *entry_to_delete;
  int index;


  for(index=0;index<HASHSIZE;index++)
    {
      tmp_list = mid_set[index].next;
      /*Traverse MID list*/
      while(tmp_list != &mid_set[index])
	{
	  /*Check if the entry is timed out*/
	  if(TIMED_OUT(&tmp_list->ass_timer))
	    {
	      entry_to_delete = tmp_list;
	      tmp_list = tmp_list->next;
#ifdef DEBUG
	      olsr_printf(1, "MID info for %s timed out.. deleting it\n", 
			  olsr_ip_to_string(&entry_to_delete->main_addr));
#endif
	      /*Delete it*/
	      mid_delete_node(entry_to_delete);
	    }
	  else
	      tmp_list = tmp_list->next;
	}
    }

  return;
}


/*
 *Delete an entry
 *
 *@param entry the entry to delete
 *
 *@return 1
 */
int
mid_delete_node(struct mid_entry *entry)
{
  struct addresses *aliases, *tmp_aliases;

  /* Free aliases */
  aliases = entry->aliases;
  while(aliases)
    {
      tmp_aliases = aliases;
      aliases = aliases->next;
      free(tmp_aliases);
    }
  /* Dequeue */
  DEQUEUE_ELEM(entry);
  //entry->prev->next = entry->next;
  //entry->next->prev = entry->prev;

  free(entry);
  
  return 0;
}


/**
 *Print all multiple interface info
 *For debuging purposes
 */
void
olsr_print_mid_set()
{

  struct mid_entry *tmp_list;
  struct addresses *tmp_addr;
  int index;

  olsr_printf(1, "mid set: %02d:%02d:%02d.%06lu\n",nowtm->tm_hour, nowtm->tm_min, nowtm->tm_sec, now.tv_usec);

  for(index=0;index<HASHSIZE;index++)
    {
      tmp_list = mid_set[index].next;
      /*Traverse MID list*/
      for(tmp_list = mid_set[index].next;
	  tmp_list != &mid_set[index];
	  tmp_list = tmp_list->next)
	{
	  olsr_printf(1, "%s: ", olsr_ip_to_string(&tmp_list->main_addr));
	  
	  tmp_addr = tmp_list->aliases;
	  
	  while(tmp_addr)
	    {
	      olsr_printf(1, " %s ", olsr_ip_to_string(&tmp_addr->address));
	      tmp_addr = tmp_addr->next;
	    }
	  olsr_printf(1, "\n");
	  
	}
    }

}






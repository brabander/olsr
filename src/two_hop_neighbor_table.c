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
 * $Id: two_hop_neighbor_table.c,v 1.8 2004/11/10 11:54:28 tlopatic Exp $
 *
 */



#include "defs.h"
#include "two_hop_neighbor_table.h"
#include "mid_set.h"
#include "mpr.h"


/**
 *Initialize 2 hop neighbor table
 */
int
olsr_init_two_hop_table()
{
  int index;

  for(index=0;index<HASHSIZE;index++)
    {
      two_hop_neighbortable[index].next = &two_hop_neighbortable[index];
      two_hop_neighbortable[index].prev = &two_hop_neighbortable[index];
    }
  return 1;
}


/**
 *Remove a one hop neighbor from a two hop neighbors
 *one hop list.
 *
 *@param two_hop_entry the two hop neighbor to remove the 
 *one hop neighbor from
 *@param address the address of the one hop neighbor to remove
 *
 *@return nada
 */

void
olsr_delete_neighbor_pointer(struct neighbor_2_entry *two_hop_entry, union olsr_ip_addr *address)
{
 struct	neighbor_list_entry *entry, *entry_to_delete;

 entry = two_hop_entry->neighbor_2_nblist.next;


 while(entry != &two_hop_entry->neighbor_2_nblist)
   {
     if(COMP_IP(&entry->neighbor->neighbor_main_addr, address))
       {
	 entry_to_delete = entry;
	 entry = entry->next;

	 /* dequeue */
	 DEQUEUE_ELEM(entry_to_delete);

	 free(entry_to_delete);
       }
     else
       entry = entry->next;
     
   }
}



/**
 *Delete an entry from the two hop neighbor table.
 *
 *@param two_hop_neighbor the two hop neighbor to delete.
 *
 *@return nada
 */
void
olsr_delete_two_hop_neighbor_table(struct neighbor_2_entry *two_hop_neighbor)
{
  struct neighbor_list_entry *one_hop_list, *entry_to_delete;
  struct neighbor_entry      *one_hop_entry;
  
  one_hop_list = two_hop_neighbor->neighbor_2_nblist.next;
  
  /* Delete one hop links */
  while(one_hop_list != &two_hop_neighbor->neighbor_2_nblist)
    {
      one_hop_entry = one_hop_list->neighbor;
      olsr_delete_neighbor_2_pointer(one_hop_entry, &two_hop_neighbor->neighbor_2_addr);
      
      entry_to_delete = one_hop_list;
      
      one_hop_list = one_hop_list->next;
      
      /* no need to dequeue */

      free(entry_to_delete);
    }
  
  /* dequeue */
  DEQUEUE_ELEM(two_hop_neighbor);
  
  free(two_hop_neighbor);
}



/**
 *Insert a new entry to the two hop neighbor table.
 *
 *@param two_hop_neighbor the entry to insert
 *
 *@return nada
 */
void
olsr_insert_two_hop_neighbor_table(struct neighbor_2_entry *two_hop_neighbor)
{
  olsr_u32_t              hash; 

  //printf("Adding 2 hop neighbor %s\n", olsr_ip_to_string(&two_hop_neighbor->neighbor_2_addr));

  hash = olsr_hashing(&two_hop_neighbor->neighbor_2_addr);

  /* Queue */  
  QUEUE_ELEM(two_hop_neighbortable[hash], two_hop_neighbor);
}


/**
 *Look up an entry in the two hop neighbor table.
 *
 *@param dest the IP address of the entry to find
 *
 *@return a pointer to a neighbor_2_entry struct
 *representing the two hop neighbor
 */
struct neighbor_2_entry *
olsr_lookup_two_hop_neighbor_table(union olsr_ip_addr *dest)
{

  struct neighbor_2_entry  *neighbor_2;
  olsr_u32_t               hash;
  struct addresses *adr;

  //printf("LOOKING FOR %s\n", olsr_ip_to_string(dest));
  hash = olsr_hashing(dest);

  
  for(neighbor_2 = two_hop_neighbortable[hash].next;
      neighbor_2 != &two_hop_neighbortable[hash];
      neighbor_2 = neighbor_2->next)
    {
      //printf("Checking %s\n", olsr_ip_to_string(dest));
      if (COMP_IP(&neighbor_2->neighbor_2_addr, dest))
	return neighbor_2;

      adr = mid_lookup_aliases(&neighbor_2->neighbor_2_addr);

      while(adr)
	{
	  if(COMP_IP(&adr->address, dest))
	    return neighbor_2;
	  adr = adr->next;
	} 
    }

  return NULL;
}



/**
 *Look up an entry in the two hop neighbor table.
 *NO CHECK FOR MAIN ADDRESS OR ALIASES!
 *
 *@param dest the IP address of the entry to find
 *
 *@return a pointer to a neighbor_2_entry struct
 *representing the two hop neighbor
 */
struct neighbor_2_entry *
olsr_lookup_two_hop_neighbor_table_mid(union olsr_ip_addr *dest)
{
  struct neighbor_2_entry  *neighbor_2;
  olsr_u32_t               hash;

  //printf("LOOKING FOR %s\n", olsr_ip_to_string(dest));
  hash = olsr_hashing(dest);
  
  for(neighbor_2 = two_hop_neighbortable[hash].next;
      neighbor_2 != &two_hop_neighbortable[hash];
      neighbor_2 = neighbor_2->next)
    {
      if (COMP_IP(&neighbor_2->neighbor_2_addr, dest))
	return neighbor_2;
    }

  return NULL;
}



/**
 *Print the two hop neighbor table to STDOUT.
 *
 *@return nada
 */
void
olsr_print_two_hop_neighbor_table()
{
  int i;
  struct neighbor_2_entry *neigh2;
  struct neighbor_list_entry *entry;
  struct neighbor_entry *neigh;
  olsr_bool first;
  double total_lq;

  olsr_printf(1, "\n--- %02d:%02d:%02d -------------------------- TWO-HOP NEIGHBORS\n\n",
              nowtm->tm_hour,
              nowtm->tm_min,
              nowtm->tm_sec,
              now.tv_usec);

  olsr_printf(1, "IP addr (2-hop)  IP addr (1-hop)  TLQ\n");

  for (i = 0; i < HASHSIZE; i++)
    {
      for (neigh2 = two_hop_neighbortable[i].next;
           neigh2 != &two_hop_neighbortable[i]; neigh2 = neigh2->next)
	{
          first = OLSR_TRUE;

	  for (entry = neigh2->neighbor_2_nblist.next;
               entry != &neigh2->neighbor_2_nblist; entry = entry->next)
	    {
              neigh = entry->neighbor;

              if (first)
                {
                  olsr_printf(1, "%-15s  ",
                              olsr_ip_to_string(&neigh2->neighbor_2_addr));
                  first = OLSR_FALSE;
                }

              else
                olsr_printf(1, "                 ");

#if defined USE_LINK_QUALITY
              total_lq = entry->full_link_quality;
#else
              total_lq = 0.0;
#endif
              olsr_printf(1, "%-15s  %5.3f\n",
                          olsr_ip_to_string(&neigh->neighbor_main_addr),
                          total_lq);
            }
	}
    }
}

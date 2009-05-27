
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
#include "two_hop_neighbor_table.h"
#include "olsr.h"
#include "ipcalc.h"
#include "defs.h"
#include "mid_set.h"
#include "neighbor_table.h"
#include "net_olsr.h"
#include "scheduler.h"
#include "olsr_logging.h"

#include <stdlib.h>

struct nbr2_entry two_hop_neighbortable[HASHSIZE];

/**
 *Initialize 2 hop neighbor table
 */
void
olsr_init_two_hop_table(void)
{
  int idx;

  OLSR_INFO(LOG_2NEIGH, "Initialize two-hop neighbortable...\n");

  for (idx = 0; idx < HASHSIZE; idx++) {
    two_hop_neighbortable[idx].next = &two_hop_neighbortable[idx];
    two_hop_neighbortable[idx].prev = &two_hop_neighbortable[idx];
  }
}

/**
 * A Reference to a two-hop neighbor entry has been added.
 * Bump the refcouunt.
 */
void
olsr_lock_nbr2(struct nbr2_entry *nbr2)
{
  nbr2->nbr2_refcount++;
}

/**
 * Unlock and free a neighbor 2 entry if the refcount has gone below 1.
 */
void
olsr_unlock_nbr2(struct nbr2_entry *nbr2)
{
  if (--nbr2->nbr2_refcount) {
    return;
  }

  /*
   * Nobody is interested in this nbr2 anymore.
   * Remove all references to it and free.
   */
  olsr_delete_two_hop_neighbor_table(nbr2);
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
olsr_delete_neighbor_pointer(struct nbr2_entry *two_hop_entry, struct nbr_entry *neigh)
{
  struct nbr_list_entry *entry = two_hop_entry->nbr2_nblist.next;
  while (entry != &two_hop_entry->nbr2_nblist) {
    if (entry->neighbor == neigh) {
      struct nbr_list_entry *entry_to_delete = entry;
      entry = entry->next;

      /* dequeue */
      DEQUEUE_ELEM(entry_to_delete);

      free(entry_to_delete);
    } else {
      entry = entry->next;
    }
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
olsr_delete_two_hop_neighbor_table(struct nbr2_entry *nbr2)
{
  struct nbr_entry *nbr;
  struct nbr_list_entry *nbr_list;
  struct nbr2_list_entry *nbr2_list;

  /*
   * Kill all references to this nbr2.
   */
  OLSR_FOR_ALL_NBR_ENTRIES(nbr) {
    OLSR_FOR_ALL_NBR2_LIST_ENTRIES(nbr, nbr2_list) {
      if (nbr2_list->nbr2 == nbr2) {
        olsr_delete_nbr2_list_entry(nbr2_list);
        break;
      }
    } OLSR_FOR_ALL_NBR2_LIST_ENTRIES_END(nbr, nbr2_list)
  } OLSR_FOR_ALL_NBR_ENTRIES_END(nbr);

  /*
   * Delete all the one hop backlinks hanging off this nbr2
   */
  while (nbr2->nbr2_nblist.next != &nbr2->nbr2_nblist) {
    nbr_list = nbr2->nbr2_nblist.next; 
    DEQUEUE_ELEM(nbr_list);
    free(nbr_list);
  }

  DEQUEUE_ELEM(nbr2);
  free(nbr2);
}

/**
 *Insert a new entry to the two hop neighbor table.
 *
 *@param two_hop_neighbor the entry to insert
 *
 *@return nada
 */
void
olsr_insert_two_hop_neighbor_table(struct nbr2_entry *two_hop_neighbor)
{
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif
  uint32_t hash = olsr_ip_hashing(&two_hop_neighbor->nbr2_addr);

  OLSR_DEBUG(LOG_2NEIGH, "Adding 2 hop neighbor %s\n", olsr_ip_to_string(&buf, &two_hop_neighbor->nbr2_addr));

  /* Queue */
  QUEUE_ELEM(two_hop_neighbortable[hash], two_hop_neighbor);
}

/**
 *Look up an entry in the two hop neighbor table.
 *
 *@param dest the IP address of the entry to find
 *
 *@return a pointer to a nbr2_entry struct
 *representing the two hop neighbor
 */
struct nbr2_entry *
olsr_lookup_two_hop_neighbor_table(const union olsr_ip_addr *dest)
{
  struct nbr2_entry *nbr2;
  uint32_t hash = olsr_ip_hashing(dest);

  for (nbr2 = two_hop_neighbortable[hash].next; nbr2 != &two_hop_neighbortable[hash]; nbr2 = nbr2->next) {
    struct tc_entry *tc;

    if (olsr_ipcmp(&nbr2->nbr2_addr, dest) == 0) {
      return nbr2;
    }
    /*
     * Locate the hookup point and check if this is a registered alias.
     */
    tc = olsr_locate_tc_entry(&nbr2->nbr2_addr);
    if (olsr_lookup_tc_mid_entry(tc, dest)) {
      return nbr2;
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
 *@return a pointer to a nbr2_entry struct
 *representing the two hop neighbor
 */
struct nbr2_entry *
olsr_lookup_two_hop_neighbor_table_mid(const union olsr_ip_addr *dest)
{
  struct nbr2_entry *nbr2;
  uint32_t hash = olsr_ip_hashing(dest);

  for (nbr2 = two_hop_neighbortable[hash].next; nbr2 != &two_hop_neighbortable[hash]; nbr2 = nbr2->next) {
    if (olsr_ipcmp(&nbr2->nbr2_addr, dest) == 0)
      return nbr2;
  }
  return NULL;
}


/**
 * Links a one-hop neighbor with a 2-hop neighbor.
 *
 * @param neighbor the 1-hop neighbor
 * @param two_hop_neighbor the 2-hop neighbor
 * @return nada
 */
void
olsr_link_nbr_nbr2(struct nbr_entry *nbr, struct nbr2_entry *nbr2, float vtime)
{
  struct nbr_list_entry *nbr_list;

  nbr_list = olsr_malloc(sizeof(struct nbr_list_entry), "Link entries 1");

  nbr_list->neighbor = nbr;

  nbr_list->second_hop_linkcost = LINK_COST_BROKEN;
  nbr_list->path_linkcost = LINK_COST_BROKEN;
  nbr_list->saved_path_linkcost = LINK_COST_BROKEN;

  /* Add nbr_list to nbr2 */
  nbr2->nbr2_nblist.next->prev = nbr_list;
  nbr_list->next = nbr2->nbr2_nblist.next;
  nbr2->nbr2_nblist.next = nbr_list;
  nbr_list->prev = &nbr2->nbr2_nblist;

  olsr_add_nbr2_list_entry(nbr, nbr2, vtime);
}


/**
 *Print the two hop neighbor table to STDOUT.
 *
 *@return nada
 */
void
olsr_print_two_hop_neighbor_table(void)
{
#if !defined REMOVE_LOG_INFO
  /* The whole function makes no sense without it. */
  int i;

  OLSR_INFO(LOG_2NEIGH, "\n--- %s ----------------------- TWO-HOP NEIGHBORS\n\n"
            "IP addr (2-hop)  IP addr (1-hop)  Total cost\n", olsr_wallclock_string());

  for (i = 0; i < HASHSIZE; i++) {
    struct nbr2_entry *neigh2;
    for (neigh2 = two_hop_neighbortable[i].next; neigh2 != &two_hop_neighbortable[i]; neigh2 = neigh2->next) {
      struct nbr_list_entry *entry;
      bool first = true;

      for (entry = neigh2->nbr2_nblist.next; entry != &neigh2->nbr2_nblist; entry = entry->next) {
        struct ipaddr_str buf;
        struct lqtextbuffer lqbuffer;

        OLSR_INFO_NH(LOG_2NEIGH, "%-15s  %-15s  %s\n",
                     first ? olsr_ip_to_string(&buf, &neigh2->nbr2_addr) : "",
                     olsr_ip_to_string(&buf, &entry->neighbor->neighbor_main_addr),
                     get_linkcost_text(entry->path_linkcost, false, &lqbuffer));

        first = false;
      }
    }
  }
#endif
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

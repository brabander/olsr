
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

/* Root of the two hop neighbor database */
struct avl_tree nbr2_tree;

/**
 * Initialize 2 hop neighbor table
 */
void
olsr_init_two_hop_table(void)
{
  OLSR_INFO(LOG_NEIGHTABLE, "Initializing neighbor2 tree.\n");
  avl_init(&nbr2_tree, avl_comp_default);

  /* XXX cookie allocation */
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

/*
 * Lookup a neighbor list. 
 */
static struct nbr_list_entry *
olsr_lookup_nbr_list_entry(struct nbr2_entry *nbr2, const union olsr_ip_addr *addr)
{
  struct avl_node *node;

  node = avl_find(&nbr2->nbr2_nbr_list_tree, addr);
  if (node) {
    return nbr_list_node_to_nbr_list(node);
  }
  return NULL;
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
olsr_delete_nbr_list_by_addr(struct nbr2_entry *nbr2, const union olsr_ip_addr *addr)
{
  struct nbr_list_entry *nbr_list;

  nbr_list = olsr_lookup_nbr_list_entry(nbr2, addr);
  if (!nbr_list) {
    return;
  }

  avl_delete(&nbr2->nbr2_nbr_list_tree, &nbr_list->nbr_list_node);
  free(nbr_list);
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
    }
    OLSR_FOR_ALL_NBR2_LIST_ENTRIES_END(nbr, nbr2_list)
  }
  OLSR_FOR_ALL_NBR_ENTRIES_END(nbr);

  /*
   * Delete all the one hop backlinks hanging off this nbr2
   */
  OLSR_FOR_ALL_NBR_LIST_ENTRIES(nbr2, nbr_list) {
    avl_delete(&nbr2->nbr2_nbr_list_tree, &nbr_list->nbr_list_node);
  }
  OLSR_FOR_ALL_NBR_LIST_ENTRIES_END(nbr2, nbr_list);

  avl_delete(&nbr2_tree, &nbr2->nbr2_node);
  free(nbr2);
}

/**
 * Insert a new entry to the two hop neighbor table.
 *
 * @param two_hop_neighbor the entry to insert
 *
 * @return nada
 */
struct nbr2_entry *
olsr_add_nbr2_entry(const union olsr_ip_addr *addr)
{
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif
  struct nbr2_entry *nbr2;

  /*
   * Check first if the entry exists.
   */
  nbr2 = olsr_lookup_two_hop_neighbor_table(addr);
  if (nbr2) {
    return nbr2;
  }

  OLSR_DEBUG(LOG_2NEIGH, "Adding 2 hop neighbor %s\n", olsr_ip_to_string(&buf, addr));

  nbr2 = olsr_malloc(sizeof(*nbr2), "Process HELLO");

  /* Init neighbor reference subtree */
  avl_init(&nbr2->nbr2_nbr_list_tree, avl_comp_default);

  nbr2->nbr2_refcount = 0;
  nbr2->nbr2_addr = *addr;

  /* Add to global neighbor 2 tree */
  nbr2->nbr2_node.key = &nbr2->nbr2_addr;
  avl_insert(&nbr2_tree, &nbr2->nbr2_node, AVL_DUP_NO);

  return nbr2;
}


/**
 * Lookup a neighbor2 entry in the neighbortable2 based on an address.
 *
 * @param addr the IP address of the neighbor to look up
 *
 * @return a pointer to the neighbor2 struct registered on the given
 *  address. NULL if not found.
 */
struct nbr2_entry *
olsr_lookup_nbr2_entry_alias(const union olsr_ip_addr *addr)
{
  struct avl_node *node;

  node = avl_find(&nbr2_tree, addr);
  if (node) {
    return nbr2_node_to_nbr2(node);
  }
  return NULL;
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
olsr_lookup_two_hop_neighbor_table(const union olsr_ip_addr *addr)
{
  const union olsr_ip_addr *main_addr;

  /*
   * Find main address of node
   */
  main_addr = olsr_lookup_main_addr_by_alias(addr);
  if (!main_addr) {
    main_addr = addr;
  }
  return olsr_lookup_nbr2_entry_alias(main_addr);
}

/*
 * Add a nbr_list reference to a nbr2 refernce subtree.
 */
static void
olsr_add_nbr_list_entry(struct nbr_entry *nbr, struct nbr2_entry *nbr2)
{
  struct nbr_list_entry *nbr_list;

  /*
   * Check if the entry exists.
   */
  nbr_list = olsr_lookup_nbr_list_entry(nbr2, &nbr->neighbor_main_addr);
  if (!nbr_list) {

    /*
     * Unknown, Create a fresh one.
     */
    nbr_list = olsr_malloc(sizeof(struct nbr_list_entry), "Link entries 1");
    nbr_list->neighbor = nbr;   /* XXX refcount */
    nbr_list->second_hop_linkcost = LINK_COST_BROKEN;
    nbr_list->path_linkcost = LINK_COST_BROKEN;
    nbr_list->saved_path_linkcost = LINK_COST_BROKEN;

    nbr_list->nbr_list_node.key = &nbr->neighbor_main_addr;
    avl_insert(&nbr2->nbr2_nbr_list_tree, &nbr_list->nbr_list_node, AVL_DUP_NO);
  }
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
  olsr_add_nbr_list_entry(nbr, nbr2);
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
  struct nbr2_entry *neigh2;
  struct nbr_list_entry *entry;
  struct ipaddr_str buf;
  struct lqtextbuffer lqbuffer;
  bool first;

  OLSR_INFO(LOG_2NEIGH, "\n--- %s ----------------------- TWO-HOP NEIGHBORS\n\n"
            "IP addr (2-hop)  IP addr (1-hop)  Total cost\n", olsr_wallclock_string());

  OLSR_FOR_ALL_NBR2_ENTRIES(neigh2) {
    first = true;
    OLSR_FOR_ALL_NBR_LIST_ENTRIES(neigh2, entry) {
      OLSR_INFO_NH(LOG_2NEIGH, "%-15s  %-15s  %s\n",
                   first ? olsr_ip_to_string(&buf, &neigh2->nbr2_addr) : "",
                   olsr_ip_to_string(&buf, &entry->neighbor->neighbor_main_addr),
                   get_linkcost_text(entry->path_linkcost, false, &lqbuffer));

      first = false;
    } OLSR_FOR_ALL_NBR_LIST_ENTRIES_END(neigh2, entry);
  } OLSR_FOR_ALL_NBR2_ENTRIES_END(neigh2);
#endif
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

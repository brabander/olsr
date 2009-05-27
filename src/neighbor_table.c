
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

#include "ipcalc.h"
#include "defs.h"
#include "two_hop_neighbor_table.h"
#include "mid_set.h"
#include "mpr.h"
#include "neighbor_table.h"
#include "olsr.h"
#include "scheduler.h"
#include "link_set.h"
#include "mpr_selector_set.h"
#include "net_olsr.h"
#include "olsr_logging.h"

#include <stdlib.h>

/* Root of the one hop neighbor database */
struct avl_tree nbr_tree;

/* Some cookies for stats keeping */
struct olsr_cookie_info *nbr2_list_timer_cookie = NULL;
struct olsr_cookie_info *nbr2_list_mem_cookie = NULL;
struct olsr_cookie_info *nbr_mem_cookie = NULL;

/*
 * Init neighbor tables.
 */
void
olsr_init_neighbor_table(void)
{
  OLSR_INFO(LOG_NEIGHTABLE, "Initializing neighbor tree.\n");
  avl_init(&nbr_tree, avl_comp_default);

  nbr2_list_timer_cookie = olsr_alloc_cookie("2-Hop Neighbor List", OLSR_COOKIE_TYPE_TIMER);

  nbr2_list_mem_cookie = olsr_alloc_cookie("2-Hop Neighbor List", OLSR_COOKIE_TYPE_MEMORY);
  olsr_cookie_set_memory_size(nbr2_list_mem_cookie, sizeof(struct nbr2_list_entry));

  nbr_mem_cookie = olsr_alloc_cookie("1-Hop Neighbor", OLSR_COOKIE_TYPE_MEMORY);
  olsr_cookie_set_memory_size(nbr_mem_cookie, sizeof(struct nbr_entry));
}


/**
 * Add a neighbor 2 reference to a neighbor.
 */
struct nbr2_list_entry *
olsr_add_nbr2_list_entry(struct nbr_entry *nbr, struct nbr2_entry *nbr2, float vtime)
{
  struct nbr2_list_entry *nbr2_list;

  /*
   * check first if the entry exists.
   */
  nbr2_list = olsr_lookup_nbr2_list_entry(nbr, &nbr2->nbr2_addr);
  if (nbr2_list) {

    /* 
     * Refresh timer.
     */
    olsr_change_timer(nbr2_list->nbr2_list_timer, vtime, OLSR_NBR2_LIST_JITTER, OLSR_TIMER_ONESHOT);
    return nbr2_list;
  }

  /*
   * Reference not found, allocate and init a fresh one.
   */
  nbr2_list = olsr_cookie_malloc(nbr2_list_mem_cookie);

  nbr2_list->nbr2 = nbr2;
  olsr_lock_nbr2(nbr2);

  nbr2_list->nbr2_nbr = nbr;    /* XXX nbr refcount protection */

  /*
   * Start the timer.
   */
  olsr_start_timer(vtime, OLSR_NBR2_LIST_JITTER, OLSR_TIMER_ONESHOT,
                   &olsr_expire_nbr2_list, nbr2_list, nbr2_list_timer_cookie->ci_id);

  /* Add to the nbr2 reference subtree */
  nbr2_list->nbr2_list_node.key = &nbr2->nbr2_addr;
  avl_insert(&nbr->nbr2_list_tree, &nbr2_list->nbr2_list_node, AVL_DUP_NO);

  return nbr2_list;
}


/**
 * Unlink, delete and free a nbr2_list entry.
 */
static void
olsr_delete_nbr2_list_entry(struct nbr2_list_entry *nbr2_list)
{
  struct nbr2_entry *nbr2;
  struct nbr_entry *nbr;

  nbr2 = nbr2_list->nbr2;
  nbr = nbr2_list->nbr2_nbr;

  /*
   * Kill running timers.
   */
  olsr_stop_timer(nbr2_list->nbr2_list_timer);
  nbr2_list->nbr2_list_timer = NULL;

  /* Remove from neighbor2 reference subtree */
  avl_delete(&nbr->nbr2_list_tree, &nbr2_list->nbr2_list_node);

  /* Remove reference to a two-hop neighbor, unlock */
  nbr2_list->nbr2 = NULL;
  olsr_unlock_nbr2(nbr2);

  olsr_cookie_free(nbr2_list_mem_cookie, nbr2_list);

  /* Set flags to recalculate the MPR set and the routing table */
  changes_neighborhood = true;
  changes_topology = true;
}

/**
 * Delete a two hop neighbor from a neighbors two hop neighbor list.
 *
 * @param neighbor the neighbor to delete the two hop neighbor from.
 * @param address the IP address of the two hop neighbor to delete.
 *
 * @return positive if entry deleted
 */
bool
olsr_delete_nbr2_list_entry_by_addr(struct nbr_entry *nbr, union olsr_ip_addr *addr)
{
  struct nbr2_list_entry *nbr2_list;

  nbr2_list = olsr_lookup_nbr2_list_entry(nbr, addr);

  if (nbr2_list) {
    olsr_delete_nbr2_list_entry(nbr2_list);
    return true;
  }

  return false;
}


/**
 * Check if a two hop neighbor is reachable via a given
 * neighbor.
 *
 * @param nbr neighbor-entry to check via
 * @param addr the addres of the two hop neighbor to find.
 *
 * @return a pointer to the nbr2_list_entry struct
 * representing the two hop neighbor if found. NULL if not found.
 */
struct nbr2_list_entry *
olsr_lookup_nbr2_list_entry(struct nbr_entry *nbr, const union olsr_ip_addr *addr)
{
  struct avl_node *node;

  node = avl_find(&nbr->nbr2_list_tree, addr);
  if (node) {
    return nbr2_list_node_to_nbr2_list(node);
  }
  return NULL;
}


/**
 * Delete a neighbor table entry.
 *
 * Remember: Deleting a neighbor entry results the deletion of its 2 hop neighbors list!!!
 * @param addr the neighbor entry to delete
 *
 * @return TRUE on success, FALSE otherwise.
 */
bool
olsr_delete_nbr_entry(const union olsr_ip_addr * addr)
{
  struct nbr2_list_entry *nbr2_list;
  struct nbr_entry *nbr;

#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif

  /*
   * Find neighbor entry
   */
  nbr = olsr_lookup_nbr_entry(addr);
  if (!nbr) {
    return false;
  }

  OLSR_DEBUG(LOG_NEIGHTABLE, "Delete 1-hop neighbor: %s\n", olsr_ip_to_string(&buf, addr));

  OLSR_FOR_ALL_NBR2_LIST_ENTRIES(nbr, nbr2_list) {
    olsr_delete_neighbor_pointer(nbr2_list->nbr2, nbr);
    olsr_delete_nbr2_list_entry(nbr2_list);
  } OLSR_FOR_ALL_NBR2_LIST_ENTRIES_END(nbr, nbr2_list);

  /* Remove from global neighbor tree */
  avl_delete(&nbr_tree, &nbr->nbr_node);

  olsr_cookie_free(nbr_mem_cookie, nbr);

  changes_neighborhood = true;

  return true;
}


/**
 * Insert a neighbor entry in the neighbor table.
 *
 * @param addr the main address of the new node
 * @return pointer to an already existting (or new created) neighbor entry
 */
struct nbr_entry *
olsr_add_nbr_entry(const union olsr_ip_addr *addr)
{
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif
  struct nbr_entry *nbr;

  /*
   * Check if neighbor entry exists.
   */
  nbr = olsr_lookup_nbr_entry(addr);
  if (nbr) {
    return nbr;
  }

  OLSR_DEBUG(LOG_NEIGHTABLE, "Add 1-hop neighbor: %s\n", olsr_ip_to_string(&buf, addr));

  nbr = olsr_cookie_malloc(nbr_mem_cookie);

  /* Set address, willingness and status */
  nbr->neighbor_main_addr = *addr;
  nbr->willingness = WILL_NEVER;
  nbr->status = NOT_SYM;

  /* Init subtree for nbr2 pointers */
  avl_init(&nbr->nbr2_list_tree, avl_comp_default);

  nbr->linkcount = 0;
  nbr->is_mpr = false;
  nbr->was_mpr = false;

  /* Add to the global neighbor tree */
  nbr->nbr_node.key = &nbr->neighbor_main_addr;
  avl_insert(&nbr_tree, &nbr->nbr_node, AVL_DUP_NO);

  return nbr;
}



/**
 * Lookup a neighbor entry in the neighbortable based on an address.
 * Unalias the passed in address before.
 * 
 * @param addr the IP address of the neighbor to look up
 *
 * @return a pointer to the neighbor struct registered on the given
 * address. NULL if not found.
 */
struct nbr_entry *
olsr_lookup_nbr_entry(const union olsr_ip_addr *addr)
{
  const union olsr_ip_addr *main_addr;

  /*
   * Find main address of node
   */
  main_addr = olsr_lookup_main_addr_by_alias(addr);
  if (!main_addr) {
    main_addr = addr;
  }

  return olsr_lookup_nbr_entry_alias(main_addr);
}


/**
 * Lookup a neighbor entry in the neighbortable based on an address.
 *
 * @param addr the IP address of the neighbor to look up
 *
 * @return a pointer to the neighbor struct registered on the given
 *  address. NULL if not found.
 */
struct nbr_entry *
olsr_lookup_nbr_entry_alias(const union olsr_ip_addr *addr)
{
  struct avl_node *node;

  node = avl_find(&nbr_tree, addr);
  if (node) {
    return nbr_node_to_nbr(node);
  }
  return NULL;
}


int
olsr_update_nbr_status(struct nbr_entry *entry, int lnk)
{
  /*
   * Update neighbor entry
   */

  if (lnk == SYM_LINK) {
    /* N_status is set to SYM */
    if (entry->status == NOT_SYM) {
      struct nbr2_entry *two_hop_neighbor;

      /* Delete posible 2 hop entry on this neighbor */
      if ((two_hop_neighbor = olsr_lookup_two_hop_neighbor_table(&entry->neighbor_main_addr)) != NULL) {
        olsr_delete_two_hop_neighbor_table(two_hop_neighbor);
      }

      changes_neighborhood = true;
      changes_topology = true;
      if (olsr_cnf->tc_redundancy > 1)
        signal_link_changes(true);
    }
    entry->status = SYM;
  } else {
    if (entry->status == SYM) {
      changes_neighborhood = true;
      changes_topology = true;
      if (olsr_cnf->tc_redundancy > 1)
        signal_link_changes(true);
    }
    /* else N_status is set to NOT_SYM */
    entry->status = NOT_SYM;
    /* remove neighbor from routing list */
  }

  return entry->status;
}


/**
 * Callback for the nbr2_list timer.
 */
void
olsr_expire_nbr2_list(void *context)
{
  struct nbr2_list_entry *nbr2_list;
  struct nbr_entry *nbr;
  struct nbr2_entry *nbr2;

  nbr2_list = (struct nbr2_list_entry *)context;
  nbr2_list->nbr2_list_timer = NULL;

  nbr = nbr2_list->nbr2_nbr;
  nbr2 = nbr2_list->nbr2;

  olsr_delete_neighbor_pointer(nbr2, nbr);
  olsr_unlock_nbr2(nbr2);

  olsr_delete_nbr2_list_entry(nbr2_list);
}


/**
 * Print the registered neighbors and two hop neighbors to STDOUT.
 *
 * @return nada
 */
void
olsr_print_neighbor_table(void)
{
#if !defined REMOVE_LOG_INFO
  /* The whole function doesn't do anything else. */

  const int ipwidth = olsr_cnf->ip_version == AF_INET ? 15 : 39;
  struct nbr_entry *nbr;
  struct link_entry *lnk;
  struct ipaddr_str buf;

  OLSR_INFO(LOG_NEIGHTABLE, "\n--- %s ------------------------------------------------ NEIGHBORS\n\n"
            "%*s  LQ    SYM   MPR   MPRS  will\n", olsr_wallclock_string(), ipwidth, "IP address");

  OLSR_FOR_ALL_NBR_ENTRIES(nbr) {

    lnk = get_best_link_to_neighbor(&nbr->neighbor_main_addr);
    if (!lnk) {
      continue;
    }

    OLSR_INFO_NH(LOG_NEIGHTABLE, "%-*s  %s  %s  %s  %d\n",
                 ipwidth, olsr_ip_to_string(&buf, &nbr->neighbor_main_addr),
                 nbr->status == SYM ? "YES " : "NO  ",
                 nbr->is_mpr ? "YES " : "NO  ",
                 olsr_lookup_mprs_set(&nbr->neighbor_main_addr) == NULL ? "NO  " : "YES ", nbr->willingness);
  } OLSR_FOR_ALL_NBR_ENTRIES_END(nbr);
#endif
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

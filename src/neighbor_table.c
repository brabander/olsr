
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
#include "mid_set.h"
#include "neighbor_table.h"
#include "olsr.h"
#include "scheduler.h"
#include "link_set.h"
#include "net_olsr.h"
#include "olsr_logging.h"

#include <stdlib.h>

/* Root of the one hop and two hop neighbor trees */
struct avl_tree nbr_tree;
struct avl_tree nbr2_tree;

/* Some cookies for stats keeping */
struct olsr_cookie_info *nbr2_mem_cookie = NULL;
struct olsr_cookie_info *nbr_mem_cookie = NULL;

struct olsr_cookie_info *nbr_connector_mem_cookie = NULL;
struct olsr_cookie_info *nbr_connector_timer_cookie = NULL;

static void olsr_expire_nbr_con(void *);

/*
 * Init neighbor tables.
 */
void
olsr_init_neighbor_table(void)
{
  OLSR_INFO(LOG_NEIGHTABLE, "Initializing neighbor tree.\n");
  avl_init(&nbr_tree, avl_comp_default);
  avl_init(&nbr2_tree, avl_comp_default);

  nbr_connector_timer_cookie = olsr_alloc_cookie("Neighbor connector", OLSR_COOKIE_TYPE_TIMER);
  nbr_connector_mem_cookie = olsr_alloc_cookie("Neighbor connector", OLSR_COOKIE_TYPE_MEMORY);
  olsr_cookie_set_memory_size(nbr_connector_mem_cookie, sizeof(struct nbr_con));

  nbr_mem_cookie = olsr_alloc_cookie("1-Hop Neighbor", OLSR_COOKIE_TYPE_MEMORY);
  olsr_cookie_set_memory_size(nbr_mem_cookie, sizeof(struct nbr_entry));

  nbr2_mem_cookie = olsr_alloc_cookie("2-Hop Neighbor", OLSR_COOKIE_TYPE_MEMORY);
  olsr_cookie_set_memory_size(nbr2_mem_cookie, sizeof(struct nbr2_entry));
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
  nbr = olsr_lookup_nbr_entry(addr, true);
  if (nbr) {
    return nbr;
  }

  OLSR_DEBUG(LOG_NEIGHTABLE, "Add 1-hop neighbor: %s\n", olsr_ip_to_string(&buf, addr));

  nbr = olsr_cookie_malloc(nbr_mem_cookie);

  /* Set address, willingness and status */
  nbr->nbr_addr = *addr;
  nbr->willingness = WILL_NEVER;
  nbr->is_sym = false;

  /* Init subtree for nbr2 connectors */
  avl_init(&nbr->con_tree, avl_comp_default);

  nbr->linkcount = 0;
  nbr->is_mpr = false;
  nbr->was_mpr = false;

  /* Add to the global neighbor tree */
  nbr->nbr_node.key = &nbr->nbr_addr;
  avl_insert(&nbr_tree, &nbr->nbr_node, false);

  return nbr;
}

/**
 * Delete a neighbor table entry.
 *
 * Remember: Deleting a neighbor entry results the deletion of its 2 hop neighbors list!!!
 * @param addr the neighbor entry to delete
 */
void
olsr_delete_nbr_entry(struct nbr_entry *nbr)
{
  struct nbr_con *connector;
  struct nbr2_entry *nbr2;

#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif

  OLSR_DEBUG(LOG_NEIGHTABLE, "Delete 1-hop neighbor: %s\n", olsr_ip_to_string(&buf, &nbr->nbr_addr));

  /*
   * Remove all references pointing to this neighbor.
   */
  OLSR_FOR_ALL_NBR_CON_ENTRIES(nbr, connector) {
    nbr2 = connector->nbr2;

    olsr_delete_nbr_con(connector);
    if (nbr2->con_tree.count == 0) {
      olsr_delete_nbr2_entry(nbr2);
    }
  } OLSR_FOR_ALL_NBR_CON_ENTRIES_END()

  /* Remove from global neighbor tree */
  avl_delete(&nbr_tree, &nbr->nbr_node);

  olsr_cookie_free(nbr_mem_cookie, nbr);

  changes_neighborhood = true;
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
olsr_lookup_nbr_entry(const union olsr_ip_addr *addr, bool lookupalias)
{
  const union olsr_ip_addr *main_addr = NULL;
  struct avl_node *node;

  /*
   * Find main address of node
   */
  if (lookupalias) {
    main_addr = olsr_lookup_main_addr_by_alias(addr);
  }
  if (main_addr == NULL) {
    main_addr = addr;
  }

  node = avl_find(&nbr_tree, addr);
  if (node) {
    return nbr_node_to_nbr(node);
  }
  return NULL;
}

int olsr_update_nbr_status(struct nbr_entry *entry, bool sym) {
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif
  /*
   * Update neighbor entry
   */

  if (sym) {
    /* N_status is set to SYM */
    if (!entry->is_sym) {
      struct nbr2_entry *two_hop_neighbor;

      /* Delete posible 2 hop entry on this neighbor */
      if ((two_hop_neighbor = olsr_lookup_nbr2_entry(&entry->nbr_addr, true)) != NULL) {
        olsr_delete_nbr2_entry(two_hop_neighbor);
      }

      changes_neighborhood = true;
      changes_topology = true;
      if (olsr_cnf->tc_redundancy > 1)
        signal_link_changes(true);
    }
    entry->is_sym = true;
    OLSR_DEBUG(LOG_NEIGHTABLE, "Neighbor %s is now symmetric\n", olsr_ip_to_string(&buf, &entry->nbr_addr));
  } else {
    if (entry->is_sym) {
      changes_neighborhood = true;
      changes_topology = true;
      if (olsr_cnf->tc_redundancy > 1)
        signal_link_changes(true);
    }
    /* else N_status is set to NOT_SYM */
    entry->is_sym = false;

    OLSR_DEBUG(LOG_NEIGHTABLE, "Neighbor %s is now non-symmetric\n", olsr_ip_to_string(&buf, &entry->nbr_addr));
  }

  return entry->is_sym;
}

/**
 * Insert a new entry to the two hop neighbor table.
 *
 * @param addr the entry to insert
 * @return nada
 */
struct nbr2_entry *
olsr_add_nbr2_entry(const union olsr_ip_addr *addr) {
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif
  struct nbr2_entry *nbr2;

  /*
   * Check first if the entry exists.
   */
  nbr2 = olsr_lookup_nbr2_entry(addr, true);
  if (nbr2) {
    return nbr2;
  }

  OLSR_DEBUG(LOG_2NEIGH, "Adding 2 hop neighbor %s\n", olsr_ip_to_string(&buf, addr));

  nbr2 = olsr_cookie_malloc(nbr2_mem_cookie);

  /* Init neighbor connector subtree */
  avl_init(&nbr2->con_tree, avl_comp_default);

  nbr2->nbr2_addr = *addr;

  /* Add to global neighbor 2 tree */
  nbr2->nbr2_node.key = &nbr2->nbr2_addr;
  avl_insert(&nbr2_tree, &nbr2->nbr2_node, false);

  return nbr2;
}

/**
 * Delete an entry from the two hop neighbor table.
 *
 * @param nbr2 the two hop neighbor to delete.
 */
void
olsr_delete_nbr2_entry(struct nbr2_entry *nbr2) {
  struct nbr_con *connector;

#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif

  OLSR_DEBUG(LOG_NEIGHTABLE, "Delete 2-hop neighbor: %s\n", olsr_ip_to_string(&buf, &nbr2->nbr2_addr));

  /*
   * Remove all references pointing to this two hop neighbor.
   */
  OLSR_FOR_ALL_NBR2_CON_ENTRIES(nbr2, connector) {
    olsr_delete_nbr_con(connector);
  } OLSR_FOR_ALL_NBR2_CON_ENTRIES_END();

  /* Remove from global neighbor tree */
  avl_delete(&nbr2_tree, &nbr2->nbr2_node);

  olsr_cookie_free(nbr2_mem_cookie, nbr2);
  changes_neighborhood = true;
}

struct nbr2_entry *
olsr_lookup_nbr2_entry(const union olsr_ip_addr *addr, bool lookupalias) {
  const union olsr_ip_addr *main_addr = NULL;
  struct avl_node *node;

  /*
   * Find main address of node
   */
  if (lookupalias) {
    main_addr = olsr_lookup_main_addr_by_alias(addr);
  }
  if (main_addr == NULL) {
    main_addr = addr;
  }

  node = avl_find(&nbr2_tree, addr);
  if (node) {
    return nbr2_node_to_nbr2(node);
  }
  return NULL;
}

/**
 * Links a one-hop neighbor with a 2-hop neighbor.
 *
 * @param nbr  the 1-hop neighbor
 * @param nbr2 the 2-hop neighbor
 * @param vtime validity time of the 2hop neighbor
 */
struct nbr_con *
olsr_link_nbr_nbr2(struct nbr_entry *nbr, const union olsr_ip_addr *nbr2_addr, uint32_t vtime) {
  struct nbr_con *connector;
  struct nbr2_entry *nbr2;

  /*
   * Check if connector entry exists.
   */
  connector = olsr_lookup_nbr_con_entry(nbr, nbr2_addr);
  if (connector) {
    olsr_change_timer(connector->nbr2_con_timer, vtime, OLSR_NBR2_LIST_JITTER, OLSR_TIMER_ONESHOT);
    return connector;
  }

  /*
   * Generate a fresh one.
   */
  nbr2 = olsr_add_nbr2_entry(nbr2_addr);

  connector = olsr_cookie_malloc(nbr_connector_mem_cookie);

  connector->nbr = nbr;
  connector->nbr2 = nbr2;
  connector->nbr_tree_node.key = &nbr2->nbr2_addr;
  connector->nbr2_tree_node.key = &nbr->nbr_addr;

  avl_insert(&nbr->con_tree, &connector->nbr_tree_node, false);
  avl_insert(&nbr2->con_tree, &connector->nbr2_tree_node, false);

  connector->path_linkcost = LINK_COST_BROKEN;

  connector->nbr2_con_timer = olsr_start_timer(vtime, OLSR_NBR2_LIST_JITTER,
      OLSR_TIMER_ONESHOT, &olsr_expire_nbr_con, connector, nbr_connector_timer_cookie);

  return connector;
}

/**
 * Unlinks an one-hop and a 2-hop neighbor.
 * Does NOT free the two-hop neighbor
 *
 * @param connector the connector between the neighbors
 */
void
olsr_delete_nbr_con(struct nbr_con *connector) {
  olsr_stop_timer(connector->nbr2_con_timer);
  connector->nbr2_con_timer = NULL;

  avl_delete(&connector->nbr->con_tree, &connector->nbr_tree_node);
  avl_delete(&connector->nbr2->con_tree, &connector->nbr2_tree_node);

  olsr_cookie_free(nbr_connector_mem_cookie, connector);
}

/**
 * Looks up the connection object of an one-hop neighbor to a
 * 2-hop neighbor ip address.
 *
 * @param nbr the one-hop neighbor
 * @param nbr2_addr the ip of the 2-hop neighbor
 * @return nbr_con, or NULL if not found
 */
struct nbr_con *
olsr_lookup_nbr_con_entry(struct nbr_entry *nbr, const union olsr_ip_addr *nbr2_addr) {
  struct avl_node *node;

  node = avl_find(&nbr->con_tree, nbr2_addr);
  if (node) {
    return nbr_con_node_to_connector(node);
  }
  return NULL;
}

/**
 * Looks up the connection object of an 2-hop neighbor to an
 * one-hop neighbor ip address.
 *
 * @param nbr2 the 2-hop neighbor
 * @param nbr_addr the ip of the one-hop neighbor
 * @return nbr_con, or NULL if not found
 */
struct nbr_con *
olsr_lookup_nbr2_con_entry(struct nbr2_entry *nbr2, const union olsr_ip_addr *nbr_addr) {
  struct avl_node *node;

  node = avl_find(&nbr2->con_tree, nbr_addr);
  if (node) {
    return nbr2_con_node_to_connector(node);
  }
  return NULL;
}

/*
 * Wrapper for the timer callback.
 */
static void
olsr_expire_nbr_con(void *context) {
  struct nbr_con *connector;

  connector = context;
  connector->nbr2_con_timer = NULL;

  olsr_delete_nbr_con(connector);
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

  const int ipwidth = olsr_cnf->ip_version == AF_INET ? INET_ADDRSTRLEN : INET6_ADDRSTRLEN;
  struct nbr_entry *nbr;
  struct link_entry *lnk;
  struct ipaddr_str buf;
  struct nbr2_entry *nbr2;
  struct nbr_con *connector;
  char lqbuffer[LQTEXT_MAXLENGTH];
  bool first;

  OLSR_INFO(LOG_NEIGHTABLE, "\n--- %s ------------------------------------------------ NEIGHBORS\n\n"
            "%-*s\tSYM\tMPR\tMPRS\twill\n", olsr_wallclock_string(), ipwidth, "IP address");

  OLSR_FOR_ALL_NBR_ENTRIES(nbr) {

    lnk = get_best_link_to_neighbor(&nbr->nbr_addr);
    if (!lnk) {
      continue;
    }

    OLSR_INFO_NH(LOG_NEIGHTABLE, "%-*s\t%s\t%s\t%s\t%d\n",
                 ipwidth, olsr_ip_to_string(&buf, &nbr->nbr_addr),
                 nbr->is_sym ? "YES" : "NO",
                 nbr->is_mpr ? "YES" : "NO",
                 nbr->mprs_count == 0  ? "NO  " : "YES ",
                 nbr->willingness);
  } OLSR_FOR_ALL_NBR_ENTRIES_END();

  OLSR_INFO(LOG_2NEIGH, "\n--- %s ----------------------- TWO-HOP NEIGHBORS\n\n"
            "IP addr (2-hop)  IP addr (1-hop)  Total cost\n", olsr_wallclock_string());

  OLSR_FOR_ALL_NBR2_ENTRIES(nbr2) {
    first = true;
    OLSR_FOR_ALL_NBR2_CON_ENTRIES(nbr2, connector) {
      OLSR_INFO_NH(LOG_2NEIGH, "%-*s  %-*s  %s\n",
                   ipwidth, first ? olsr_ip_to_string(&buf, &nbr2->nbr2_addr) : "",
                   ipwidth, olsr_ip_to_string(&buf, &connector->nbr->nbr_addr),
                   olsr_get_linkcost_text(connector->path_linkcost, false, lqbuffer, sizeof(lqbuffer)));

      first = false;
    } OLSR_FOR_ALL_NBR2_CON_ENTRIES_END()
  } OLSR_FOR_ALL_NBR2_ENTRIES_END()

#endif
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

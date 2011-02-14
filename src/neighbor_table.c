
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

#include <assert.h>
#include <stdlib.h>

/* Root of the one hop and two hop neighbor trees */
struct avl_tree nbr_tree;
struct avl_tree nbr2_tree;

/* memory management */
static struct olsr_memcookie_info *nbr2_mem_cookie = NULL;
static struct olsr_memcookie_info *nbr_mem_cookie = NULL;
static struct olsr_memcookie_info *nbr_connector_mem_cookie = NULL;

/* neighbor connection validity timer */
static struct olsr_timer_info *nbr_connector_timer_info = NULL;

static void olsr_expire_nbr_con(void *);
static void internal_delete_nbr_con(struct nbr_con *connector);

/*
 * Init neighbor tables.
 */
void
olsr_init_neighbor_table(void)
{
  OLSR_INFO(LOG_NEIGHTABLE, "Initializing neighbor tree.\n");
  avl_init(&nbr_tree, avl_comp_default, false, NULL);
  avl_init(&nbr2_tree, avl_comp_default, false, NULL);

  nbr_connector_timer_info = olsr_timer_add("Neighbor connector", &olsr_expire_nbr_con, false);
  nbr_connector_mem_cookie = olsr_memcookie_add("Neighbor connector", sizeof(struct nbr_con));

  nbr_mem_cookie = olsr_memcookie_add("1-Hop Neighbor", sizeof(struct nbr_entry));

  nbr2_mem_cookie = olsr_memcookie_add("2-Hop Neighbor", sizeof(struct nbr2_entry));
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

  nbr = olsr_memcookie_malloc(nbr_mem_cookie);

  /* Set address, willingness and status */
  nbr->nbr_addr = *addr;
  nbr->willingness = WILL_NEVER;
  nbr->is_sym = false;

  /* Init subtree for nbr2 connectors */
  avl_init(&nbr->con_tree, avl_comp_default, false, NULL);

  nbr->linkcount = 0;
  nbr->is_mpr = false;
  nbr->was_mpr = false;

  /* add tc_edge if necessary */
  assert(tc_myself);
  nbr->tc_edge = olsr_lookup_tc_edge(tc_myself, addr);
  if (nbr->tc_edge == NULL) {
    nbr->tc_edge = olsr_add_tc_edge_entry(tc_myself, addr, 0);
  }

  /* and connect it to this neighbor */
  nbr->tc_edge->neighbor = nbr;

  /* Add to the global neighbor tree */
  nbr->nbr_node.key = &nbr->nbr_addr;
  avl_insert(&nbr_tree, &nbr->nbr_node);

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
  struct nbr_con *connector, *iterator;
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif

  OLSR_DEBUG(LOG_NEIGHTABLE, "Delete 1-hop neighbor: %s\n", olsr_ip_to_string(&buf, &nbr->nbr_addr));

  /*
   * Remove all references pointing to this neighbor.
   */
  OLSR_FOR_ALL_NBR_CON_ENTRIES(nbr, connector, iterator) {
    olsr_delete_nbr_con(connector);
  }

  /* remove corresponding tc_edge if not already removed by olsr_delete_all_tc_entries() */
  if (nbr->tc_edge) {
    /* first clear the connection to this neighbor */
    nbr->tc_edge->neighbor = NULL;

    /* now try to kill the edge */
    olsr_delete_tc_edge_entry(nbr->tc_edge);
  }

  /* Remove from global neighbor tree */
  avl_delete(&nbr_tree, &nbr->nbr_node);

  olsr_memcookie_free(nbr_mem_cookie, nbr);

  changes_neighborhood = true;
  changes_topology = true;
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
  struct nbr_entry *nbr;

  /*
   * Find main address of node
   */
  if (lookupalias) {
    main_addr = olsr_lookup_main_addr_by_alias(addr);
  }
  if (main_addr == NULL) {
    main_addr = addr;
  }

  nbr = avl_find_element(&nbr_tree, addr, nbr, nbr_node);
  return nbr;
}

void olsr_update_nbr_status(struct nbr_entry *entry) {
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif
  struct link_entry *link;

  /* look for best symmetric link */
  link = get_best_link_to_neighbor(entry);

  /*
   * Update neighbor entry
   */
  if (link && lookup_link_status(link) == SYM_LINK) {
    /* N_status is set to SYM */
    if (!entry->is_sym) {
      struct nbr2_entry *two_hop_neighbor;

      /* Delete posible 2 hop entry on this neighbor */
      if ((two_hop_neighbor = olsr_lookup_nbr2_entry(&entry->nbr_addr, true)) != NULL) {
        olsr_delete_nbr2_entry(two_hop_neighbor);
      }

      changes_neighborhood = true;
      changes_topology = true;
      if (olsr_cnf->tc_redundancy > 1 || entry->is_mpr) {
        signal_link_changes(true);
      }
    }
    entry->is_sym = true;
    OLSR_DEBUG(LOG_NEIGHTABLE, "Neighbor %s is now symmetric\n", olsr_ip_to_string(&buf, &entry->nbr_addr));
  } else {
    if (entry->is_sym) {
      changes_neighborhood = true;
      if (olsr_cnf->tc_redundancy > 1 || entry->is_mpr) {
        signal_link_changes(true);
        changes_topology = true;
      }
    }
    /* else N_status is set to NOT_SYM */
    entry->is_sym = false;

    OLSR_DEBUG(LOG_NEIGHTABLE, "Neighbor %s is now non-symmetric\n", olsr_ip_to_string(&buf, &entry->nbr_addr));
  }
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

  nbr2 = olsr_memcookie_malloc(nbr2_mem_cookie);

  /* Init neighbor connector subtree */
  avl_init(&nbr2->con_tree, avl_comp_default, false, NULL);

  nbr2->nbr2_addr = *addr;

  /* Add to global neighbor 2 tree */
  nbr2->nbr2_node.key = &nbr2->nbr2_addr;
  avl_insert(&nbr2_tree, &nbr2->nbr2_node);

  return nbr2;
}

/**
 * Delete an entry from the two hop neighbor table.
 *
 * @param nbr2 the two hop neighbor to delete.
 */
void
olsr_delete_nbr2_entry(struct nbr2_entry *nbr2) {
  struct nbr_con *connector, *iterator;

#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif

  OLSR_DEBUG(LOG_NEIGHTABLE, "Delete 2-hop neighbor: %s\n", olsr_ip_to_string(&buf, &nbr2->nbr2_addr));

  /*
   * Remove all references pointing to this two hop neighbor.
   */
  OLSR_FOR_ALL_NBR2_CON_ENTRIES(nbr2, connector, iterator) {
    internal_delete_nbr_con(connector);
  }

  /* Remove from global neighbor tree */
  avl_delete(&nbr2_tree, &nbr2->nbr2_node);

  olsr_memcookie_free(nbr2_mem_cookie, nbr2);
  changes_neighborhood = true;
}

struct nbr2_entry *
olsr_lookup_nbr2_entry(const union olsr_ip_addr *addr, bool lookupalias) {
  const union olsr_ip_addr *main_addr = NULL;
  struct nbr2_entry *entry;

  /*
   * Find main address of node
   */
  if (lookupalias) {
    main_addr = olsr_lookup_main_addr_by_alias(addr);
  }
  if (main_addr == NULL) {
    main_addr = addr;
  }

  entry = avl_find_element(&nbr2_tree, addr, entry, nbr2_node);
  return entry;
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
    olsr_timer_change(connector->nbr2_con_timer, vtime, OLSR_NBR2_LIST_JITTER);
    return connector;
  }

  /*
   * Generate a fresh one.
   */
  nbr2 = olsr_add_nbr2_entry(nbr2_addr);

  connector = olsr_memcookie_malloc(nbr_connector_mem_cookie);

  connector->nbr = nbr;
  connector->nbr2 = nbr2;
  connector->nbr_tree_node.key = &nbr2->nbr2_addr;
  connector->nbr2_tree_node.key = &nbr->nbr_addr;

  avl_insert(&nbr->con_tree, &connector->nbr_tree_node);
  avl_insert(&nbr2->con_tree, &connector->nbr2_tree_node);

  connector->path_linkcost = LINK_COST_BROKEN;

  connector->nbr2_con_timer = olsr_timer_start(vtime, OLSR_NBR2_LIST_JITTER,
      connector, nbr_connector_timer_info);

  return connector;
}

/**
 * Deletes a nbr-connector without deleting the 2-hop neighbor
 * @param connector the connector between neighbors
 */
static void
internal_delete_nbr_con(struct nbr_con *connector) {
  olsr_timer_stop(connector->nbr2_con_timer);
  connector->nbr2_con_timer = NULL;

  avl_delete(&connector->nbr->con_tree, &connector->nbr_tree_node);
  avl_delete(&connector->nbr2->con_tree, &connector->nbr2_tree_node);

  olsr_memcookie_free(nbr_connector_mem_cookie, connector);
}

/**
 * Unlinks an one-hop and a 2-hop neighbor.
 *
 * @param connector the connector between the neighbors
 */
void
olsr_delete_nbr_con(struct nbr_con *connector) {
  struct nbr2_entry *nbr2;

  nbr2 = connector->nbr2;

  internal_delete_nbr_con(connector);

  if (nbr2->con_tree.count == 0) {
    olsr_delete_nbr2_entry(nbr2);
  }
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
  struct nbr_con *con;
  con = avl_find_element(&nbr->con_tree, nbr2_addr, con, nbr_tree_node);
  return con;
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
  struct nbr_con *con;
  con = avl_find_element(&nbr2->con_tree, nbr_addr, con, nbr2_tree_node);
  return con;
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
  struct nbr_entry *nbr, *nbr_iterator;
  struct link_entry *lnk;
  struct ipaddr_str buf, buf2;
  struct nbr2_entry *nbr2, *nbr2_iterator;
  struct nbr_con *connector, *con_iterator;
  char lqbuffer[LQTEXT_MAXLENGTH];
  bool first;

  OLSR_INFO(LOG_NEIGHTABLE, "\n--- %s ------------------------------------------------ NEIGHBORS\n\n"
            "%-*s\tSYM\tMPR\tMPRS\twill\n", olsr_timer_getWallclockString(), ipwidth, "IP address");

  OLSR_FOR_ALL_NBR_ENTRIES(nbr, nbr_iterator) {

    lnk = get_best_link_to_neighbor_ip(&nbr->nbr_addr);
    if (!lnk) {
      continue;
    }

    OLSR_INFO_NH(LOG_NEIGHTABLE, "%-*s\t%s\t%s\t%s\t%d\n",
                 ipwidth, olsr_ip_to_string(&buf, &nbr->nbr_addr),
                 nbr->is_sym ? "YES" : "NO",
                 nbr->is_mpr ? "YES" : "NO",
                 nbr->mprs_count == 0  ? "NO  " : "YES ",
                 nbr->willingness);
  }

  OLSR_INFO(LOG_2NEIGH, "\n--- %s ----------------------- TWO-HOP NEIGHBORS\n\n"
            "IP addr (2-hop)  IP addr (1-hop)  Total cost\n", olsr_timer_getWallclockString());

  OLSR_FOR_ALL_NBR2_ENTRIES(nbr2, nbr2_iterator) {
    first = true;
    OLSR_FOR_ALL_NBR2_CON_ENTRIES(nbr2, connector, con_iterator) {
      OLSR_INFO_NH(LOG_2NEIGH, "%-*s  %-*s  %s\n",
                   ipwidth, first ? olsr_ip_to_string(&buf, &nbr2->nbr2_addr) : "",
                   ipwidth, olsr_ip_to_string(&buf2, &connector->nbr->nbr_addr),
                   olsr_get_linkcost_text(connector->path_linkcost, false, lqbuffer, sizeof(lqbuffer)));

      first = false;
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

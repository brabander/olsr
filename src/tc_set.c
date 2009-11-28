
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
#include <assert.h>

#include "tc_set.h"
#include "olsr.h"
#include "lq_packet.h"
#include "net_olsr.h"
#include "link_set.h"
#include "mid_set.h"
#include "neighbor_table.h"
#include "olsr_logging.h"

static bool delete_outdated_tc_edges(struct tc_entry *);

/* Root of the link state database */
struct avl_tree tc_tree;
struct tc_entry *tc_myself = NULL;     /* Shortcut to ourselves */

/* Some cookies for stats keeping */
static struct olsr_cookie_info *tc_edge_gc_timer_cookie = NULL;
static struct olsr_cookie_info *tc_validity_timer_cookie = NULL;
struct olsr_cookie_info *spf_backoff_timer_cookie = NULL;
struct olsr_cookie_info *tc_mem_cookie = NULL;

static uint32_t relevantTcCount = 0;

/* the first 32 TCs are without Fisheye */
static int ttl_index = -32;

static uint16_t local_ansn_number = 0;

static void olsr_cleanup_tc_entry(struct tc_entry *tc);

/**
 * Add a new tc_entry to the tc tree
 *
 * @param (last)adr address of the entry
 * @return a pointer to the created entry
 */
static struct tc_entry *
olsr_add_tc_entry(const union olsr_ip_addr *adr)
{
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif
  struct tc_entry *tc;

  /*
   * Safety net against loss of the last main IP address.
   */
  if (olsr_ipcmp(&olsr_cnf->router_id, &all_zero) == 0) {
    return NULL;
  }

  OLSR_DEBUG(LOG_TC, "TC: add entry %s\n", olsr_ip_to_string(&buf, adr));

  tc = olsr_cookie_malloc(tc_mem_cookie);
  if (!tc) {
    return NULL;
  }

  /* Fill entry */
  tc->addr = *adr;
  tc->vertex_node.key = &tc->addr;

  tc->mid_seq = -1;
  tc->hna_seq = -1;
  tc->tc_seq = -1;
  /*
   * Insert into the global tc tree.
   */
  avl_insert(&tc_tree, &tc->vertex_node, false);
  olsr_lock_tc_entry(tc);

  /*
   * Initialize subtrees for edges, prefixes, HNAs and MIDs.
   */
  avl_init(&tc->edge_tree, avl_comp_default);
  avl_init(&tc->prefix_tree, avl_comp_prefix_origin_default);
  avl_init(&tc->mid_tree, avl_comp_default);
  avl_init(&tc->hna_tree, avl_comp_prefix_default);

  /*
   * Add a rt_path for ourselves.
   */
  olsr_insert_routing_table(adr, 8 * olsr_cnf->ipsize, adr, OLSR_RT_ORIGIN_TC);

  return tc;
}

/**
 * Initialize the topology set
 *
 */
void
olsr_init_tc(void)
{
  OLSR_INFO(LOG_TC, "Initialize topology set...\n");

  avl_init(&tc_tree, avl_comp_default);

  /*
   * Get some cookies for getting stats to ease troubleshooting.
   */
  tc_edge_gc_timer_cookie = olsr_alloc_cookie("TC edge GC", OLSR_COOKIE_TYPE_TIMER);
  tc_validity_timer_cookie = olsr_alloc_cookie("TC validity", OLSR_COOKIE_TYPE_TIMER);
  spf_backoff_timer_cookie = olsr_alloc_cookie("SPF backoff", OLSR_COOKIE_TYPE_TIMER);

  tc_mem_cookie = olsr_alloc_cookie("tc_entry", OLSR_COOKIE_TYPE_MEMORY);
  olsr_cookie_set_memory_size(tc_mem_cookie, sizeof(struct tc_entry));
}

/**
 * The main ip address has changed.
 * Do the needful.
 */
void
olsr_change_myself_tc(void)
{
  struct link_entry *entry;
  bool main_ip_change = false;

  if (tc_myself) {

    /*
     * Check if there was a change.
     */
    if (olsr_ipcmp(&tc_myself->addr, &olsr_cnf->router_id) == 0) {
      return;
    }

    /*
     * Flush our own tc_entry.
     */
    olsr_delete_tc_entry(tc_myself);

    /*
     * Clear the reference.
     */
    olsr_unlock_tc_entry(tc_myself);
    tc_myself = NULL;

    main_ip_change = true;
  }

  /*
   * The old entry for ourselves is gone, generate a new one and trigger SPF.
   */
  tc_myself = olsr_add_tc_entry(&olsr_cnf->router_id);
  olsr_lock_tc_entry(tc_myself);

  OLSR_FOR_ALL_LINK_ENTRIES(entry) {

    /**
     * check if a main ip change destroyed our TC entries
     */
    if (main_ip_change || entry->link_tc_edge == NULL) {
      struct nbr_entry *ne = entry->neighbor;
      entry->link_tc_edge = olsr_add_tc_edge_entry(tc_myself, &ne->nbr_addr, 0);
    }
  } OLSR_FOR_ALL_LINK_ENTRIES_END(link);
  changes_topology = true;
}

/**
 * Attempts to delete a tc entry. This will work unless there are virtual edges left
 * that cannot be removed
 *
 * @param entry the TC entry to delete
 *
 */
void
olsr_delete_tc_entry(struct tc_entry *tc)
{
  struct tc_edge_entry *tc_edge;

#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif
  OLSR_DEBUG(LOG_TC, "TC: del entry %s %u %s\n", olsr_ip_to_string(&buf, &tc->addr),
      tc->edge_tree.count, tc->is_virtual ? "true" : "false");

  /* we don't want to keep this node */
  tc->is_virtual = true;

  if (tc->edge_tree.count == 0) {
    olsr_cleanup_tc_entry(tc);
    return;
  }
  /* The delete all non-virtual edges, the last one will clean up the tc if possible */
  OLSR_FOR_ALL_TC_EDGE_ENTRIES(tc, tc_edge) {
    /* we don't need this edge for the tc, so let's try to remove it */
    olsr_delete_tc_edge_entry(tc_edge);
  } OLSR_FOR_ALL_TC_EDGE_ENTRIES_END();
}

/**
 * Delete a tc entry after all edges have been cleared.
 *
 * @param entry the TC entry to delete
 */
static void
olsr_cleanup_tc_entry(struct tc_entry *tc) {
  struct rt_path *rtp;
#if !defined(REMOVE_LOG_DEBUG)
  struct ipaddr_str buf;
#endif
  OLSR_DEBUG(LOG_TC, "TC: del entry %s %u\n", olsr_ip_to_string(&buf, &tc->addr), tc->refcount);
  assert (tc->edge_tree.count == 0);

  OLSR_FOR_ALL_PREFIX_ENTRIES(tc, rtp) {
    olsr_delete_rt_path(rtp);
  } OLSR_FOR_ALL_PREFIX_ENTRIES_END();

  /* Flush all MID aliases and kill the MID timer */
  olsr_flush_mid_entries(tc);

  /* Flush all HNA Networks and kill its timers */
  olsr_flush_hna_nets(tc);

  /* Stop running timers */
  olsr_stop_timer(tc->edge_gc_timer);
  tc->edge_gc_timer = NULL;
  olsr_stop_timer(tc->validity_timer);
  tc->validity_timer = NULL;

  avl_delete(&tc_tree, &tc->vertex_node);
  olsr_unlock_tc_entry(tc);
}

/**
 * Look up a entry from the TC tree based on address
 *
 * @param adr the address to look for
 * @return the entry found or NULL
 */
struct tc_entry *
olsr_lookup_tc_entry(const union olsr_ip_addr *adr)
{
  struct avl_node *node;

  node = avl_find(&tc_tree, adr);
  return node ? vertex_tree2tc(node) : NULL;
}

/*
 * Lookup a tc entry. Creates one if it does not exist yet.
 */
struct tc_entry *
olsr_locate_tc_entry(const union olsr_ip_addr *adr)
{
  struct tc_entry *tc = olsr_lookup_tc_entry(adr);

  return tc == NULL ? olsr_add_tc_entry(adr) : tc;
}

/**
 * Format tc_edge contents into a buffer.
 */
#if !defined REMOVE_LOG_DEBUG
static char *
olsr_tc_edge_to_string(struct tc_edge_entry *tc_edge)
{
  static char buf[128];
  struct ipaddr_str addrbuf, dstbuf;
  char lqbuffer[LQTEXT_MAXLENGTH];

  snprintf(buf, sizeof(buf),
           "%s > %s, cost %s",
           olsr_ip_to_string(&addrbuf, &tc_edge->tc->addr),
           olsr_ip_to_string(&dstbuf, &tc_edge->T_dest_addr),
           olsr_get_linkcost_text(tc_edge->cost, false, lqbuffer, sizeof(lqbuffer)));
  return buf;
}
#endif

/**
 * Wrapper for the timer callback.
 * A TC entry has not been refreshed in time.
 * Remove it from the link-state database.
 */
static void
olsr_expire_tc_entry(void *context)
{
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif
  struct tc_entry *tc = context;
  tc->validity_timer = NULL;

  OLSR_DEBUG(LOG_TC, "TC: expire node entry %s\n",
             olsr_ip_to_string(&buf, &tc->addr));

  olsr_delete_tc_entry(tc);
  changes_topology = true;
}

/**
 * Wrapper for the timer callback.
 * Does the garbage collection of older ansn entries after no edge addition to
 * the TC entry has happened for OLSR_TC_EDGE_GC_TIME.
 */
static void
olsr_expire_tc_edge_gc(void *context)
{
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif
  struct tc_entry *tc = context;

  OLSR_DEBUG(LOG_TC, "TC: expire edge GC for %s\n",
             olsr_ip_to_string(&buf, &tc->addr));

  tc->edge_gc_timer = NULL;

  if (delete_outdated_tc_edges(tc)) {
    changes_topology = true;
  }
}

/*
 * If the edge does not have a minimum acceptable link quality
 * set the etx cost to infinity such that it gets ignored during
 * SPF calculation.
 *
 * @return 1 if the change of the etx value was relevant
 */
bool
olsr_calc_tc_edge_entry_etx(struct tc_edge_entry *tc_edge)
{
  olsr_linkcost old = tc_edge->cost;
  tc_edge->cost = olsr_calc_tc_cost(tc_edge);
  tc_edge->common_cost = tc_edge->cost;
  if (tc_edge->edge_inv) {
    tc_edge->edge_inv->common_cost = tc_edge->cost;
  }
  return olsr_is_relevant_costchange(old, tc_edge->cost);
}

/**
 * Add a new tc_edge_entry to the tc_edge_tree
 *
 * @param (last)adr address of the entry
 * @return a pointer to the created entry
 */
struct tc_edge_entry *
olsr_add_tc_edge_entry(struct tc_entry *tc, union olsr_ip_addr *addr, uint16_t ansn)
{
  struct tc_entry *tc_neighbor;
  struct tc_edge_entry *tc_edge;
  struct tc_edge_entry *tc_edge_inv;
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif
  tc_edge = olsr_malloc_tc_edge_entry();
  if (!tc_edge) {
    return NULL;
  }

  /* Fill entry */
  tc_edge->T_dest_addr = *addr;
  tc_edge->ansn = ansn;
  tc_edge->edge_node.key = &tc_edge->T_dest_addr;

  /*
   * Insert into the edge tree.
   * Expectation is to have only one tc_edge per tc pair.
   * However we need duplicate key support for the case of local
   * parallel links where we add one tc_edge per link_entry.
   */
  avl_insert(&tc->edge_tree, &tc_edge->edge_node, true);
  olsr_lock_tc_entry(tc);

  /*
   * Connect backpointer.
   */
  tc_edge->tc = tc;

  /*
   * Check if the neighboring router and the inverse edge is in the lsdb.
   * Create short cuts to the inverse edge for faster SPF execution.
   */
  tc_neighbor = olsr_lookup_tc_entry(&tc_edge->T_dest_addr);
  if (tc_neighbor == NULL) {
    OLSR_DEBUG(LOG_TC, "TC:   creating neighbor tc_entry %s\n", olsr_ip_to_string(&buf, &tc_edge->T_dest_addr));
    tc_neighbor = olsr_add_tc_entry(&tc_edge->T_dest_addr);
    tc_neighbor->is_virtual = true;
  }

  /* don't create an inverse edge for a tc pointing to us ! */
  if (1 && tc_neighbor != tc_myself) {
    tc_edge_inv = olsr_lookup_tc_edge(tc_neighbor, &tc->addr);
    if (!tc_edge_inv ) {
      OLSR_DEBUG(LOG_TC, "TC:   creating inverse edge for %s\n", olsr_ip_to_string(&buf, &tc->addr));
      tc_edge_inv = olsr_add_tc_edge_entry(tc_neighbor, &tc->addr, 0);

      tc_edge_inv->is_virtual = 1;
    }

    /*
     * Connect the edges mutually.
     */
    tc_edge_inv->edge_inv = tc_edge;
    tc_edge->edge_inv = tc_edge_inv;
  }

  /*
   * Update the etx.
   */
  olsr_calc_tc_edge_entry_etx(tc_edge);

  OLSR_DEBUG(LOG_TC, "TC: add edge entry %s\n", olsr_tc_edge_to_string(tc_edge));

  return tc_edge;
}

/**
 * Delete a TC edge entry.
 *
 * @param tc the TC entry
 * @param tc_edge the TC edge entry
 * @return true if the tc entry was deleted, false otherwise
 */
void
olsr_delete_tc_edge_entry(struct tc_edge_entry *tc_edge)
{
  struct tc_entry *tc;
  struct tc_edge_entry *tc_edge_inv;
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif

  tc_edge->is_virtual = 1;

  tc_edge_inv = tc_edge->edge_inv;
  if (tc_edge_inv != NULL && tc_edge_inv->is_virtual == 0) {
    OLSR_DEBUG(LOG_TC, "TC: mark edge entry %s\n", olsr_tc_edge_to_string(tc_edge));
    return;
  }

  OLSR_DEBUG(LOG_TC, "TC: del edge entry %s\n", olsr_tc_edge_to_string(tc_edge));

  /*
   * Clear the backpointer of our inverse edge.
   */
  if (tc_edge_inv) {
    /* split the two edges */
    tc_edge_inv->edge_inv = NULL;
    tc_edge->edge_inv = NULL;

    if (tc_edge_inv->is_virtual) {
      /* remove the other side too because it's a virtual link */
      olsr_delete_tc_edge_entry(tc_edge_inv);
    }
  }

  tc = tc_edge->tc;

  /* remove edge from tc FIRST */
  avl_delete(&tc->edge_tree, &tc_edge->edge_node);
  OLSR_DEBUG(LOG_TC, "TC: %s down to %d edges\n", olsr_ip_to_string(&buf, &tc->addr), tc->edge_tree.count);

  /* now check if TC is virtual and has no edges left */
  if (tc->is_virtual && tc->edge_tree.count == 0) {
    /* cleanup virtual tc node */
    olsr_cleanup_tc_entry(tc);
  }
  olsr_unlock_tc_entry(tc);
  olsr_free_tc_edge_entry(tc_edge);
}

/**
 * Delete all destinations that have a lower ANSN.
 *
 * @param tc the entry to delete edges from
 * @return TRUE if any destinations were deleted, FALSE if not
 */
static bool
delete_outdated_tc_edges(struct tc_entry *tc)
{
  struct tc_edge_entry *tc_edge;
  bool retval = false;

  OLSR_DEBUG(LOG_TC, "TC: deleting outdated TC-edge entries\n");

  OLSR_FOR_ALL_TC_EDGE_ENTRIES(tc, tc_edge) {
    if (SEQNO_GREATER_THAN(tc->ansn, tc_edge->ansn)) {
      olsr_delete_tc_edge_entry(tc_edge);
      retval = true;
    }
  }
  OLSR_FOR_ALL_TC_EDGE_ENTRIES_END();

  if (retval)
    changes_topology = true;
  return retval;
}

/**
 * Delete all destinations that are inside the borders but
 * not updated in the last tc.
 *
 * @param tc the entry to delete edges from
 * @param ansn the advertised neighbor set sequence number
 * @return 1 if any destinations were deleted 0 if not
 */
static int
olsr_delete_revoked_tc_edges(struct tc_entry *tc, uint16_t ansn, union olsr_ip_addr *lower_border, union olsr_ip_addr *upper_border)
{
  struct tc_edge_entry *tc_edge;
  int retval = 0;
  bool passedLowerBorder = false;

  OLSR_DEBUG(LOG_TC, "TC: deleting revoked TCs\n");

  OLSR_FOR_ALL_TC_EDGE_ENTRIES(tc, tc_edge) {
    if (!passedLowerBorder) {
      if (avl_comp_default(lower_border, &tc_edge->T_dest_addr) <= 0) {
        passedLowerBorder = true;
      } else {
        continue;
      }
    }

    if (passedLowerBorder) {
      if (avl_comp_default(upper_border, &tc_edge->T_dest_addr) <= 0) {
        break;
      }
    }

    if (SEQNO_GREATER_THAN(ansn, tc_edge->ansn)) {
      olsr_delete_tc_edge_entry(tc_edge);
      retval = 1;
    }
  }
  OLSR_FOR_ALL_TC_EDGE_ENTRIES_END();

  if (retval)
    changes_topology = true;
  return retval;
}


/**
 * Update an edge registered on an entry.
 * Creates new edge-entries if not registered.
 * Bases update on a received TC message
 *
 * @param entry the TC entry to check
 * @pkt the TC edge entry in the packet
 * @return 1 if entries are added 0 if not
 */
static int
olsr_tc_update_edge(struct tc_entry *tc, uint16_t ansn, const unsigned char **curr, union olsr_ip_addr *neighbor)
{
  struct tc_edge_entry *tc_edge;
  int edge_change = 0;

  /*
   * Fetch the per-edge data
   */
  pkt_get_ipaddress(curr, neighbor);

  /* First check if we know this edge */
  tc_edge = olsr_lookup_tc_edge(tc, neighbor);
  if (!tc_edge) {
    /*
     * Yet unknown - create it.
     * Check if the address is allowed.
     */
    if (!olsr_validate_address(neighbor)) {
      return 0;
    }

    tc_edge = olsr_add_tc_edge_entry(tc, neighbor, ansn);

    olsr_deserialize_tc_lq_pair(curr, tc_edge);
    edge_change = 1;
  } else {
    /*
     * We know this edge - Update entry.
     */
    tc_edge->ansn = ansn;

    /*
     * Update link quality if configured.
     */
    olsr_deserialize_tc_lq_pair(curr, tc_edge);

    /*
     * Update the etx.
     */
    if (olsr_calc_tc_edge_entry_etx(tc_edge)) {
      if (tc->msg_hops <= olsr_cnf->lq_dlimit) {
        edge_change = 1;
      }
    }
    if (edge_change) {
      OLSR_DEBUG(LOG_TC, "TC:   chg edge entry %s\n", olsr_tc_edge_to_string(tc_edge));
    }
  }
  tc_edge->is_virtual = 0;
  tc->is_virtual = false;

  return edge_change;
}

/**
 * Lookup an edge hanging off a TC entry.
 *
 * @param entry the entry to check
 * @param dst_addr the destination address to check for
 * @return a pointer to the tc_edge found - or NULL
 */
struct tc_edge_entry *
olsr_lookup_tc_edge(struct tc_entry *tc, union olsr_ip_addr *edge_addr)
{
  struct avl_node *edge_node;

  edge_node = avl_find(&tc->edge_tree, edge_addr);

  return edge_node ? edge_tree2tc_edge(edge_node) : NULL;
}

/**
 * Print the topology table to stdout
 */
void
olsr_print_tc_table(void)
{
#if !defined REMOVE_LOG_INFO
  /* The whole function makes no sense without it. */
  struct tc_entry *tc;
  const int ipwidth = olsr_cnf->ip_version == AF_INET ? 15 : 30;

  OLSR_INFO(LOG_TC, "\n--- %s ------------------------------------------------- TOPOLOGY\n\n", olsr_wallclock_string());
  OLSR_INFO_NH(LOG_TC, "%-*s %-*s             %8s %8s\n", ipwidth,
               "Source IP addr", ipwidth, "Dest IP addr", olsr_get_linklabel(0), "(common)");

  OLSR_FOR_ALL_TC_ENTRIES(tc) {
    struct tc_edge_entry *tc_edge;
    OLSR_FOR_ALL_TC_EDGE_ENTRIES(tc, tc_edge) {
      struct ipaddr_str addrbuf, dstaddrbuf;
      char lqbuffer1[LQTEXT_MAXLENGTH], lqbuffer2[LQTEXT_MAXLENGTH];

      OLSR_INFO_NH(LOG_TC, "%-*s %-*s %-7s      %8s %8s\n",
                   ipwidth, olsr_ip_to_string(&addrbuf, &tc->addr),
                   ipwidth, olsr_ip_to_string(&dstaddrbuf,
                                              &tc_edge->T_dest_addr),
                   tc_edge->is_virtual ? "virtual" : "",
                   olsr_get_linkcost_text(tc_edge->cost, false, lqbuffer1, sizeof(lqbuffer1)),
                   olsr_get_linkcost_text(tc_edge->common_cost, false, lqbuffer2, sizeof(lqbuffer2)));

    } OLSR_FOR_ALL_TC_EDGE_ENTRIES_END();
  } OLSR_FOR_ALL_TC_ENTRIES_END();
#endif
}

/*
 * calculate the border IPs of a tc edge set according to the border flags
 *
 * @param lower border flag
 * @param pointer to lower border ip
 * @param upper border flag
 * @param pointer to upper border ip
 * @result 1 if lower/upper border ip have been set
 */
#if 0
static int
olsr_calculate_tc_border(uint8_t lower_border,
                         union olsr_ip_addr *lower_border_ip, uint8_t upper_border, union olsr_ip_addr *upper_border_ip)
{
  if (lower_border == 0 && upper_border == 0) {
    return 0;
  }
  if (lower_border == 0xff) {
    memset(lower_border_ip, 0, sizeof(*lower_border_ip));
  } else {
    int i;

    lower_border--;
    for (i = 0; i < lower_border / 8; i++) {
      lower_border_ip->v6.s6_addr[olsr_cnf->ipsize - i - 1] = 0;
    }
    lower_border_ip->v6.s6_addr[olsr_cnf->ipsize - lower_border / 8 - 1] &= (0xff << (lower_border & 7));
    lower_border_ip->v6.s6_addr[olsr_cnf->ipsize - lower_border / 8 - 1] |= (1 << (lower_border & 7));
  }

  if (upper_border == 0xff) {
    memset(upper_border_ip, 0xff, sizeof(*upper_border_ip));
  } else {
    int i;

    upper_border--;

    for (i = 0; i < upper_border / 8; i++) {
      upper_border_ip->v6.s6_addr[olsr_cnf->ipsize - i - 1] = 0;
    }
    upper_border_ip->v6.s6_addr[olsr_cnf->ipsize - upper_border / 8 - 1] &= (0xff << (upper_border & 7));
    upper_border_ip->v6.s6_addr[olsr_cnf->ipsize - upper_border / 8 - 1] |= (1 << (upper_border & 7));
  }
  return 1;
}
#endif
/*
 * Process an incoming TC or TC_LQ message.
 *
 * If the message is interesting enough, update our edges for it,
 * trigger SPF and finally flood it to all our 2way neighbors.
 *
 * The order for extracting data off the message does matter,
 * as every call to pkt_get increases the packet offset and
 * hence the spot we are looking at.
 */

void
olsr_input_tc(struct olsr_message * msg, const uint8_t *payload, const uint8_t *end,
    struct interface * input_if __attribute__ ((unused)),
    union olsr_ip_addr * from_addr, enum duplicate_status status)
{
  uint16_t ansn;
  uint8_t lower_border, upper_border;
  const uint8_t *curr;
  struct tc_entry *tc;
  bool relevantTc;
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif
  union olsr_ip_addr lower_border_ip, upper_border_ip;
  int borderSet = 0;

  /* We are only interested in TC message types. */
  if (msg->type != olsr_get_TC_MessageId()) {
    return;
  }

  /*
   * If the sender interface (NB: not originator) of this message
   * is not in the symmetric 1-hop neighborhood of this node, the
   * message MUST be discarded.
   */
  if (check_neighbor_link(from_addr) != SYM_LINK) {
    OLSR_DEBUG(LOG_TC, "Received TC from NON SYM neighbor %s\n", olsr_ip_to_string(&buf, from_addr));
    return;
  }

  curr = payload;
  pkt_get_u16(&curr, &ansn);

  /* Get borders */
  pkt_get_u8(&curr, &lower_border);
  pkt_get_u8(&curr, &upper_border);

  tc = olsr_lookup_tc_entry(&msg->originator);

  /* TCs can be splitted, so we are looking for ANSNs equal or higher */
  if (tc && status != RESET_SEQNO_OLSR_MESSAGE && tc->tc_seq != -1 && olsr_seqno_diff(ansn, tc->ansn) < 0) {
    /* this TC is too old, discard it */
    return;
  }

  /*
   * Generate a new tc_entry in the lsdb and store the sequence number.
   */
  if (!tc) {
    tc = olsr_add_tc_entry(&msg->originator);
  }

  /*
   * Update the tc entry.
   */
  tc->msg_hops = msg->hopcnt;
  tc->tc_seq = msg->seqno;
  tc->ansn = ansn;
  tc->ignored = 0;
  tc->err_seq_valid = false;
  tc->is_virtual = false;

  OLSR_DEBUG(LOG_TC, "Processing TC from %s, seq 0x%04x\n", olsr_ip_to_string(&buf, &msg->originator), tc->tc_seq);

  /*
   * Now walk the edge advertisements contained in the packet.
   */

  borderSet = 0;
  relevantTc = false;
  while (curr + olsr_cnf->ipsize + olsr_sizeof_TCLQ() <= end) {
    if (olsr_tc_update_edge(tc, ansn, &curr, &upper_border_ip)) {
      relevantTc = true;
    }

    if (!borderSet) {
      borderSet = 1;
      memcpy(&lower_border_ip, &upper_border_ip, sizeof(lower_border_ip));
    }
  }

  if (relevantTc) {
    relevantTcCount++;
    changes_topology = true;
  }

  /*
   * Calculate real border IPs.
   */
  assert(msg);
  if (borderSet) {
    // borderSet = olsr_calculate_tc_border(lower_border, &lower_border_ip, upper_border, &upper_border_ip);
  }

  /*
   * Set or change the expiration timer accordingly.
   */
  assert(msg);
  olsr_set_timer(&tc->validity_timer, msg->vtime,
                 OLSR_TC_VTIME_JITTER, OLSR_TIMER_ONESHOT, &olsr_expire_tc_entry, tc, tc_validity_timer_cookie);

  if (borderSet) {

    /*
     * Delete all old tc edges within borders.
     */
    olsr_delete_revoked_tc_edges(tc, ansn, &lower_border_ip, &upper_border_ip);
  } else {

    /*
     * Kick the the edge garbage collection timer. In the meantime hopefully
     * all edges belonging to a multipart neighbor set will arrive.
     */
    olsr_set_timer(&tc->edge_gc_timer, OLSR_TC_EDGE_GC_TIME,
                   OLSR_TC_EDGE_GC_JITTER, OLSR_TIMER_ONESHOT, &olsr_expire_tc_edge_gc, tc, tc_edge_gc_timer_cookie);
  }
}

uint32_t
getRelevantTcCount(void)
{
  return relevantTcCount;
}

void
olsr_delete_all_tc_entries(void) {
  struct tc_entry *tc;
  struct link_entry *link;

  /* then remove all tc entries */
  OLSR_FOR_ALL_TC_ENTRIES(tc) {
    tc->is_virtual = 0;
  } OLSR_FOR_ALL_TC_ENTRIES_END(tc)

  OLSR_FOR_ALL_TC_ENTRIES(tc) {
    if (tc != tc_myself) {
      olsr_delete_tc_entry(tc);
    }
  } OLSR_FOR_ALL_TC_ENTRIES_END(tc)

  /* kill all references in link_set */
  OLSR_FOR_ALL_LINK_ENTRIES(link) {
    link->link_tc_edge = NULL;
  } OLSR_FOR_ALL_LINK_ENTRIES_END(link)


  /* kill tc_myself */
  if (tc_myself) {
    olsr_delete_tc_entry(tc_myself);
    olsr_unlock_tc_entry(tc_myself);
    tc_myself = NULL;
  }
}

static uint8_t
calculate_border_flag(void *lower_border, void *higher_border)
{
  uint8_t *lower = lower_border;
  uint8_t *higher = higher_border;
  uint8_t bitmask;
  uint8_t part, bitpos;

  for (part = 0; part < olsr_cnf->ipsize; part++) {
    if (lower[part] != higher[part]) {
      break;
    }
  }

  if (part == olsr_cnf->ipsize) {       // same IPs ?
    return 0;
  }
  // look for first bit of difference
  bitmask = 0xfe;
  for (bitpos = 0; bitpos < 8; bitpos++, bitmask <<= 1) {
    if ((lower[part] & bitmask) == (higher[part] & bitmask)) {
      break;
    }
  }

  bitpos += 8 * (olsr_cnf->ipsize - part - 1);
  return bitpos + 1;
}

uint16_t
get_local_ansn_number(void) {
  return local_ansn_number;
}

void
increase_local_ansn_number(void) {
  local_ansn_number++;
}

static bool
olsr_output_lq_tc_internal(void *ctx  __attribute__ ((unused)), union olsr_ip_addr *nextIp, bool skip)
{
  static int ttl_list[] = { 2, 8, 2, 16, 2, 8, 2, MAX_TTL };
  struct interface *ifp;
  struct nbr_entry *nbr;
  struct link_entry *link;
  uint8_t msg_buffer[MAXMESSAGESIZE - OLSR_HEADERSIZE] __attribute__ ((aligned));
  uint8_t *curr = msg_buffer;
  uint8_t *length_field, *border_flags, *seqno, *last;
  bool sendTC = false, nextFragment = false;
  uint8_t ttl = 255;

  OLSR_INFO(LOG_PACKET_CREATION, "Building TC\n-------------------\n");

  pkt_put_u8(&curr, olsr_get_TC_MessageId());
  pkt_put_reltime(&curr, olsr_cnf->tc_params.validity_time);

  length_field = curr;
  pkt_put_u16(&curr, 0); /* put in real messagesize later */

  pkt_put_ipaddress(&curr, &olsr_cnf->router_id);

  if (olsr_cnf->lq_fish > 0) {
    /* handle fisheye */
    ttl_index++;
    if (ttl_index >= (int)ARRAYSIZE(ttl_list)) {
      ttl_index = 0;
    }
    if (ttl_index >= 0) {
      ttl = ttl_list[ttl_index];
    }
  }
  pkt_put_u8(&curr, ttl);

  /* hopcount */
  pkt_put_u8(&curr, 0);

  /* reserve sequence number lazy */
  seqno = curr;
  pkt_put_u16(&curr, 0);
  pkt_put_u16(&curr, get_local_ansn_number());

  /* border flags */
  border_flags = curr;
  pkt_put_u16(&curr, 0xffff);

  last = msg_buffer + sizeof(msg_buffer) - olsr_cnf->ipsize - olsr_sizeof_TCLQ();

  OLSR_FOR_ALL_NBR_ENTRIES(nbr) {
    /* allow fragmentation */
    if (skip) {
      struct nbr_entry *prevNbr;
      if (olsr_ipcmp(&nbr->nbr_addr, nextIp) != 0) {
        continue;
      }
      skip = false;

      /* rewrite lower border flag */
      prevNbr = nbr_node_to_nbr(nbr->nbr_node.prev);
      *border_flags = calculate_border_flag(&prevNbr->nbr_addr, &nbr->nbr_addr);
    }

    /* too long ? */
    if (curr > last) {
      /* rewrite upper border flag */
      struct nbr_entry *prevNbr = nbr_node_to_nbr(nbr->nbr_node.prev);

      *(border_flags+1) = calculate_border_flag(&prevNbr->nbr_addr, &nbr->nbr_addr);
      *nextIp = nbr->nbr_addr;
      nextFragment = true;
      break;
    }

    /*
     * TC redundancy 2
     *
     * Only consider symmetric neighbours.
     */
    if (!nbr->is_sym) {
      continue;
    }

    /*
     * TC redundancy 1
     *
     * Only consider MPRs and MPR selectors
     */
    if (olsr_cnf->tc_redundancy == 1 && !nbr->is_mpr && nbr->mprs_count == 0) {
      continue;
    }

    /*
     * TC redundancy 0
     *
     * Only consider MPR selectors
     */
    if (olsr_cnf->tc_redundancy == 0 && nbr->mprs_count == 0) {
      continue;
    }

    /* Set the entry's link quality */
    link = get_best_link_to_neighbor(&nbr->nbr_addr);
    if (!link) {
      /* no link ? */
      continue;
    }

    if (link->linkcost >= LINK_COST_BROKEN) {
      /* don't advertisebroken links */
      continue;
    }

    pkt_put_ipaddress(&curr, &nbr->nbr_addr);
    olsr_serialize_tc_lq(&curr, link);

    sendTC = true;
  } OLSR_FOR_ALL_NBR_ENTRIES_END()

  if (!sendTC && skip) {
    OLSR_DEBUG(LOG_TC, "Nothing to send for this TC...\n");
    return false;
  }

  /* late initialization of length and sequence number */
  pkt_put_u16(&seqno, get_msg_seqno());
  pkt_put_u16(&length_field, curr - msg_buffer);

  /* send to all interfaces */
  OLSR_FOR_ALL_INTERFACES(ifp) {
    if (net_outbuffer_bytes_left(ifp) < curr - msg_buffer) {
      net_output(ifp);
      set_buffer_timer(ifp);
    }
    net_outbuffer_push(ifp, msg_buffer, curr - msg_buffer);
  } OLSR_FOR_ALL_INTERFACES_END(ifp)
  return nextFragment;
}

void
olsr_output_lq_tc(void *ctx) {
  union olsr_ip_addr next;
  bool skip = false;

  memset(&next, 0, sizeof(next));

  while ((skip = olsr_output_lq_tc_internal(ctx, &next, skip)));
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

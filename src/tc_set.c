
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
#include <stdio.h>

#include "common/avl.h"
#include "common/avl_olsr_comp.h"
#include "tc_set.h"
#include "olsr.h"
#include "lq_packet.h"
#include "net_olsr.h"
#include "link_set.h"
#include "mid_set.h"
#include "neighbor_table.h"
#include "olsr_logging.h"

static bool delete_outdated_tc_edges(struct tc_entry *);
static void olsr_expire_tc_entry(void *context);
static void olsr_expire_tc_edge_gc(void *context);

/* Root of the link state database */
struct avl_tree tc_tree;
struct tc_entry *tc_myself = NULL;     /* Shortcut to ourselves */

/* Some cookies for stats keeping */
struct olsr_memcookie_info *tc_mem_cookie = NULL;
static struct olsr_timer_info *tc_edge_gc_timer_info = NULL;
static struct olsr_timer_info *tc_validity_timer_info = NULL;

static uint32_t relevantTcCount = 0;

/* the first 32 TCs are without Fisheye */
static int ttl_index = -32;

static uint16_t local_ansn_number = 0;

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

  tc = olsr_memcookie_malloc(tc_mem_cookie);
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
  avl_insert(&tc_tree, &tc->vertex_node);

  /*
   * Initialize subtrees for edges, prefixes, HNAs and MIDs.
   */
  avl_init(&tc->edge_tree, avl_comp_default, true, NULL);
  avl_init(&tc->prefix_tree, avl_comp_prefix_origin_default, false, NULL);
  avl_init(&tc->mid_tree, avl_comp_default, false, NULL);
  avl_init(&tc->hna_tree, avl_comp_prefix_default, false, NULL);

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

  avl_init(&tc_tree, avl_comp_default, false, NULL);

  /*
   * Get some cookies for getting stats to ease troubleshooting.
   */
  tc_edge_gc_timer_info = olsr_timer_add("TC edge GC", olsr_expire_tc_edge_gc, false);
  tc_validity_timer_info = olsr_timer_add("TC validity", &olsr_expire_tc_entry, false);

  tc_mem_cookie = olsr_memcookie_add("tc_entry", sizeof(struct tc_entry));

  /* start with a random answer set number */
  local_ansn_number = random() & 0xffff;
}

/**
 * The main ip address has changed.
 * Do the needful.
 */
void
olsr_change_myself_tc(void)
{
  struct nbr_entry *entry, *iterator;
  bool main_ip_change = false;

  if (tc_myself) {

    /*
     * Check if there was a change.
     */
    if (olsr_ipcmp(&tc_myself->addr, &olsr_cnf->router_id) == 0) {
      return;
    }

    /* flush local edges */
    OLSR_FOR_ALL_NBR_ENTRIES(entry, iterator) {
      if (entry->tc_edge) {
        /* clean up local edges if necessary */
        entry->tc_edge->neighbor = NULL;
        olsr_delete_tc_edge_entry(entry->tc_edge);
        entry->tc_edge = NULL;
      }
    }

    /*
     * Flush our own tc_entry.
     */
    olsr_delete_tc_entry(tc_myself);

    /*
     * Clear the reference.
     */
    tc_myself = NULL;

    main_ip_change = true;
  }

  /*
   * The old entry for ourselves is gone, generate a new one and trigger SPF.
   */
  tc_myself = olsr_add_tc_entry(&olsr_cnf->router_id);

  if (main_ip_change) {
    OLSR_FOR_ALL_NBR_ENTRIES(entry, iterator) {
    /**
     * check if a main ip change destroyed our TC entries
     */
    if (entry->tc_edge == NULL) {
        entry->tc_edge = olsr_add_tc_edge_entry(tc_myself, &entry->nbr_addr, 0);
        entry->tc_edge->neighbor = entry;
      }
    }
  }
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
  struct tc_edge_entry *tc_edge, *edge_iterator;
  struct rt_path *rtp, *rtp_iterator;

#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif
  OLSR_DEBUG(LOG_TC, "TC: del entry %s\n", olsr_ip_to_string(&buf, &tc->addr));

  /* The delete all non-virtual edges */
  OLSR_FOR_ALL_TC_EDGE_ENTRIES(tc, tc_edge, edge_iterator) {
    olsr_delete_tc_edge_entry(tc_edge);
  }

  /* Stop running timers */
  olsr_timer_stop(tc->validity_timer);
  tc->validity_timer = NULL;

  olsr_timer_stop(tc->edge_gc_timer);
  tc->edge_gc_timer = NULL;

  /* still virtual edges left, node has to stay in database */
  if (tc->edge_tree.count > 0) {
    tc->virtual = true;
    tc->ansn = 0;
    return;
  }

  OLSR_FOR_ALL_PREFIX_ENTRIES(tc, rtp, rtp_iterator) {
    olsr_delete_rt_path(rtp);
  }

  /* Flush all MID aliases and kill the MID timer */
  olsr_flush_mid_entries(tc);

  /* Flush all HNA Networks and kill its timers */
  olsr_flush_hna_nets(tc);

  avl_delete(&tc_tree, &tc->vertex_node);
  olsr_memcookie_free(tc_mem_cookie, tc);
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
  struct tc_entry *tc;

  tc = avl_find_element(&tc_tree, adr, tc, vertex_node);
  return tc;
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
           "%s > %s, cost %s, ansn %04x",
           olsr_ip_to_string(&addrbuf, &tc_edge->tc->addr),
           olsr_ip_to_string(&dstbuf, &tc_edge->T_dest_addr),
           olsr_get_linkcost_text(tc_edge->cost, false, lqbuffer, sizeof(lqbuffer)),
           tc_edge->ansn);
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

  OLSR_DEBUG(LOG_TC, "TC: expire node entry %s\n",
             olsr_ip_to_string(&buf, &tc->addr));

  tc->validity_timer = NULL;
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
  tc->edge_gc_timer = NULL;

  OLSR_DEBUG(LOG_TC, "TC: expire edge GC for %s\n",
             olsr_ip_to_string(&buf, &tc->addr));

  if (delete_outdated_tc_edges(tc)) {
    changes_topology = true;
  }
}

/**
 * Add a new tc_edge_entry to the tc_edge_tree
 *
 * @param (last)adr address of the entry
 * @return a pointer to the created entry
 */
struct tc_edge_entry *
olsr_add_tc_edge_entry(struct tc_entry *tc, const union olsr_ip_addr *addr, uint16_t ansn)
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
  avl_insert(&tc->edge_tree, &tc_edge->edge_node);

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
  }

  /* don't create an inverse edge for a tc pointing to us ! */
  tc_edge_inv = olsr_lookup_tc_edge(tc_neighbor, &tc->addr);
  if (tc_edge_inv == NULL) {
    OLSR_DEBUG(LOG_TC, "TC:   creating inverse edge for %s\n", olsr_ip_to_string(&buf, &tc->addr));
    tc_edge_inv = olsr_add_tc_edge_entry(tc_neighbor, &tc->addr, 0);

    /* mark edge as virtual */
    tc_edge_inv->virtual = true;
  }

  /*
   * Connect the edges mutually.
   */
  tc_edge_inv->edge_inv = tc_edge;
  tc_edge->edge_inv = tc_edge_inv;

  /* this is a real edge */
  tc_edge->virtual = false;

  /*
   * Update the etx.
   */
  tc_edge->cost = olsr_calc_tc_cost(tc_edge);

  OLSR_DEBUG(LOG_TC, "TC: add edge entry %s\n", olsr_tc_edge_to_string(tc_edge));

  return tc_edge;
}

static void
internal_delete_tc_edge_entry(struct tc_edge_entry *tc_edge) {
  struct tc_entry *tc = tc_edge->tc;
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif
  assert (tc_edge->edge_inv == NULL);

  avl_delete(&tc->edge_tree, &tc_edge->edge_node);
  OLSR_DEBUG(LOG_TC, "TC: %s down to %d edges\n", olsr_ip_to_string(&buf, &tc->addr), tc->edge_tree.count);

  olsr_free_tc_edge_entry(tc_edge);
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
  struct tc_entry *tc, *tc_inv;
  struct tc_edge_entry *tc_edge_inv;
  bool was_real = false;

  assert(tc_edge->edge_inv);

  /* cache tc_entry pointer */
  tc = tc_edge->tc;

  /* get reverse edge */
  tc_edge_inv = tc_edge->edge_inv;
  tc_inv = tc_edge_inv->tc;

  if (!tc_edge_inv->virtual || tc_edge_inv->neighbor != NULL) {
    /* mark this edge as virtual and correct tc_entry realedge_count */
    tc_edge->virtual = true;
    OLSR_DEBUG(LOG_TC, "TC: mark edge entry %s as virtual\n", olsr_tc_edge_to_string(tc_edge));
    return;
  }

  OLSR_DEBUG(LOG_TC, "TC: del edge entry %s\n", olsr_tc_edge_to_string(tc_edge));

  /* mark topology as changed */
  changes_topology = true;

  /* split the two edges */
  tc_edge_inv->edge_inv = NULL;
  tc_edge->edge_inv = NULL;

  /* remove both edges */
  internal_delete_tc_edge_entry(tc_edge);
  internal_delete_tc_edge_entry(tc_edge_inv);

  if (was_real && tc_inv != tc_myself && tc_inv->virtual) {
    /* mark tc_entry to be gone in one ms */
    olsr_timer_set(&tc_inv->validity_timer, 1, 0, tc, tc_validity_timer_info);
  }
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
  struct tc_edge_entry *tc_edge, *iterator;
  bool retval = false;

  OLSR_DEBUG(LOG_TC, "TC: deleting outdated TC-edge entries\n");

  OLSR_FOR_ALL_TC_EDGE_ENTRIES(tc, tc_edge, iterator) {
    if (SEQNO_GREATER_THAN(tc->ansn, tc_edge->ansn)) {
      olsr_delete_tc_edge_entry(tc_edge);
      retval = true;
    }
  }

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
  struct tc_edge_entry *tc_edge, *iterator;
  int retval = 0;
  bool passedLowerBorder = false;

  OLSR_DEBUG(LOG_TC, "TC: deleting revoked TCs\n");

  OLSR_FOR_ALL_TC_EDGE_ENTRIES(tc, tc_edge, iterator) {
    if (!passedLowerBorder) {
      if (avl_comp_default(lower_border, &tc_edge->T_dest_addr, NULL) <= 0) {
        passedLowerBorder = true;
      } else {
        continue;
      }
    }

    if (passedLowerBorder) {
      if (avl_comp_default(upper_border, &tc_edge->T_dest_addr, NULL) <= 0) {
        break;
      }
    }

    if (SEQNO_GREATER_THAN(ansn, tc_edge->ansn)) {
      olsr_delete_tc_edge_entry(tc_edge);
      retval = 1;
    }
  }

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
    tc_edge->cost = olsr_calc_tc_cost(tc_edge);
    edge_change = 1;
    OLSR_DEBUG(LOG_TC, "TC:   chg edge entry %s\n", olsr_tc_edge_to_string(tc_edge));
  }

  /* set edge and tc as non-virtual */
  tc_edge->virtual = false;
  tc->virtual = false;

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
olsr_lookup_tc_edge(struct tc_entry *tc, const union olsr_ip_addr *edge_addr)
{
  struct tc_edge_entry *edge;

  edge = avl_find_element(&tc->edge_tree, edge_addr, edge, edge_node);
  return edge;
}

/**
 * Print the topology table to stdout
 */
void
olsr_print_tc_table(void)
{
#if !defined REMOVE_LOG_INFO
  /* The whole function makes no sense without it. */
  struct tc_entry *tc, *tc_iterator;
  const int ipwidth = olsr_cnf->ip_version == AF_INET ? 15 : 30;
  static char NONE[] = "-";
  struct timeval_buf timebuf;

  OLSR_INFO(LOG_TC, "\n--- %s ------------------------------------------------- TOPOLOGY\n\n",
      olsr_clock_getWallclockString(&timebuf));
  OLSR_INFO_NH(LOG_TC, "%-*s %-*s %-7s      %8s %12s %5s\n", ipwidth,
               "Source IP addr", ipwidth, "Dest IP addr", "", olsr_get_linklabel(0), "vtime", "ansn");

  OLSR_FOR_ALL_TC_ENTRIES(tc, tc_iterator) {
    struct tc_edge_entry *tc_edge, *edge_iterator;
    struct millitxt_buf tbuf;
    char *vtime = NONE;

    if (tc->validity_timer) {
      olsr_clock_to_string(&tbuf, olsr_clock_getRelative(tc->validity_timer->timer_clock));
      vtime = tbuf.buf;
    }

    OLSR_FOR_ALL_TC_EDGE_ENTRIES(tc, tc_edge, edge_iterator) {
      struct ipaddr_str addrbuf, dstaddrbuf;
      char lqbuffer1[LQTEXT_MAXLENGTH];

      OLSR_INFO_NH(LOG_TC, "%-*s %-*s %-7s      %8s %12s %5u\n",
                   ipwidth, olsr_ip_to_string(&addrbuf, &tc->addr),
                   ipwidth, olsr_ip_to_string(&dstaddrbuf,
                                              &tc_edge->T_dest_addr),
                   tc_edge->virtual ? "virtual" : "",
                   olsr_get_linkcost_text(tc_edge->cost, false, lqbuffer1, sizeof(lqbuffer1)),
                   vtime, tc_edge->ansn);

    }
  }
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
olsr_input_tc(struct olsr_message * msg,
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

  curr = msg->payload;
  pkt_get_u16(&curr, &ansn);

  /* Get borders */
  pkt_get_u8(&curr, &lower_border);
  pkt_get_u8(&curr, &upper_border);

  tc = olsr_lookup_tc_entry(&msg->originator);

  /* TCs can be splitted, so we are looking for ANSNs equal or higher */
  if (tc && status != RESET_SEQNO_OLSR_MESSAGE && tc->tc_seq != -1
      && !tc->virtual && olsr_seqno_diff(ansn, tc->ansn) < 0) {
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

  OLSR_DEBUG(LOG_TC, "Processing TC from %s, seq 0x%04x\n", olsr_ip_to_string(&buf, &msg->originator), tc->tc_seq);

  /*
   * Now walk the edge advertisements contained in the packet.
   */

  borderSet = 0;
  relevantTc = false;
  while (curr + olsr_cnf->ipsize + olsr_sizeof_TCLQ() <= msg->end) {
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
    borderSet = olsr_calculate_tc_border(lower_border, &lower_border_ip, upper_border, &upper_border_ip);
  }

  /*
   * Set or change the expiration timer accordingly.
   */
  assert(msg);
  olsr_timer_set(&tc->validity_timer, msg->vtime,
                 OLSR_TC_VTIME_JITTER, tc, tc_validity_timer_info);

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
    olsr_timer_set(&tc->edge_gc_timer, OLSR_TC_EDGE_GC_TIME,
                   OLSR_TC_EDGE_GC_JITTER, tc, tc_edge_gc_timer_info);
  }
}

uint32_t
getRelevantTcCount(void)
{
  return relevantTcCount;
}

void
olsr_delete_all_tc_entries(void) {
  struct tc_entry *tc, *tc_iterator;
  struct tc_edge_entry *edge, *edge_iterator;

  /* delete tc_edges */
  OLSR_FOR_ALL_TC_ENTRIES(tc, tc_iterator) {
    OLSR_FOR_ALL_TC_EDGE_ENTRIES(tc, edge, edge_iterator) {
      if (edge->neighbor) {
        /* break connector with neighbor */
        edge->neighbor->tc_edge = NULL;
        edge->neighbor = NULL;
      }
      edge->edge_inv = NULL;
      internal_delete_tc_edge_entry(edge);
    }
  }

  /* delete tc_entries */
  OLSR_FOR_ALL_TC_ENTRIES(tc, tc_iterator) {
    olsr_delete_tc_entry(tc);
  }

  /* kill tc_myself */
  tc_myself = NULL;
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
  struct interface *ifp, *ifp_iterator;
  struct nbr_entry *nbr, *nbr_iterator;
  struct link_entry *link;
  struct nbr_entry *prevNbr;
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

  OLSR_FOR_ALL_NBR_ENTRIES(nbr, nbr_iterator) {
    /* allow fragmentation */
    if (skip) {
      if (olsr_ipcmp(&nbr->nbr_addr, nextIp) != 0) {
        continue;
      }
      skip = false;

      /* rewrite lower border flag */
      prevNbr = avl_prev_element(nbr, nbr_node);
      *border_flags = calculate_border_flag(&prevNbr->nbr_addr, &nbr->nbr_addr);
    }

    /* too long ? */
    if (curr > last) {
      /* rewrite upper border flag */
      prevNbr = avl_prev_element(nbr, nbr_node);

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
    link = get_best_link_to_neighbor_ip(&nbr->nbr_addr);
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
  }

  if (!sendTC && skip) {
    OLSR_DEBUG(LOG_TC, "Nothing to send for this TC...\n");
    return false;
  }

  /* late initialization of length and sequence number */
  pkt_put_u16(&seqno, get_msg_seqno());
  pkt_put_u16(&length_field, curr - msg_buffer);

  /* send to all interfaces */
  OLSR_FOR_ALL_INTERFACES(ifp, ifp_iterator) {
    if (net_outbuffer_bytes_left(ifp) < curr - msg_buffer) {
      net_output(ifp);
      set_buffer_timer(ifp);
    }
    net_outbuffer_push(ifp, msg_buffer, curr - msg_buffer);
  }
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

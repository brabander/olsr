
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

#include "tc_set.h"
#include "olsr.h"
#include "lq_packet.h"
#include "net_olsr.h"
#include "link_set.h"
#include "neighbor_table.h"
#include "olsr_logging.h"

static bool delete_outdated_tc_edges(struct tc_entry *);

/* Root of the link state database */
struct avl_tree tc_tree;
struct tc_entry *tc_myself = NULL; /* Shortcut to ourselves */

/* Some cookies for stats keeping */
static struct olsr_cookie_info *tc_edge_gc_timer_cookie = NULL;
static struct olsr_cookie_info *tc_validity_timer_cookie = NULL;
struct olsr_cookie_info *spf_backoff_timer_cookie = NULL;
struct olsr_cookie_info *tc_mem_cookie = NULL;

static uint32_t relevantTcCount = 0;

/*
 * Sven-Ola 2007-Dec: These four constants include an assumption
 * on how long a typical olsrd mesh memorizes (TC) messages in the
 * RAM of all nodes and how many neighbour changes between TC msgs.
 * In Berlin, we encounter hop values up to 70 which means that
 * messages may live up to ~15 minutes cycling between nodes and
 * obviously breaking out of the timeout_dup() jail. It may be more
 * correct to dynamically adapt those constants, e.g. by using the
 * max hop number (denotes size-of-mesh) in some form or maybe
 * a factor indicating how many (old) versions of olsrd are on.
 */

/* Value window for ansn, identifies old messages to be ignored */
#define TC_ANSN_WINDOW 256

/* Value window for seqno, identifies old messages to be ignored */
#define TC_SEQNO_WINDOW 1024

/* Enlarges the value window for upcoming ansn/seqno to be accepted */
#define TC_ANSN_WINDOW_MULT 4

/* Enlarges the value window for upcoming ansn/seqno to be accepted */
#define TC_SEQNO_WINDOW_MULT 8

static bool
olsr_seq_inrange_low(int beg, int end, uint16_t seq)
{
  if (beg < 0) {
    if (seq >= (uint16_t) beg || seq < end) {
      return true;
    }
  } else if (end >= 0x10000) {
    if (seq >= beg || seq < (uint16_t) end) {
      return true;
    }
  } else if (seq >= beg && seq < end) {
    return true;
  }
  return false;
}

static bool
olsr_seq_inrange_high(int beg, int end, uint16_t seq)
{
  if (beg < 0) {
    if (seq > (uint16_t) beg || seq <= end) {
      return true;
    }
  } else if (end >= 0x10000) {
    if (seq > beg || seq <= (uint16_t) end) {
      return true;
    }
  } else if (seq > beg && seq <= end) {
    return true;
  }
  return false;
}

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

  /*
   * Insert into the global tc tree.
   */
  avl_insert(&tc_tree, &tc->vertex_node, AVL_DUP_NO);
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
  tc_edge_gc_timer_cookie =
    olsr_alloc_cookie("TC edge GC", OLSR_COOKIE_TYPE_TIMER);
  tc_validity_timer_cookie =
    olsr_alloc_cookie("TC validity", OLSR_COOKIE_TYPE_TIMER);
  spf_backoff_timer_cookie =
    olsr_alloc_cookie("SPF backoff", OLSR_COOKIE_TYPE_TIMER);

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
  bool refresh = false;
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
    refresh = true;
  }

  /*
   * The old entry for ourselves is gone, generate a new one and trigger SPF.
   */
  tc_myself = olsr_add_tc_entry(&olsr_cnf->router_id);

  OLSR_FOR_ALL_LINK_ENTRIES(entry) {
    /**
     * check if a main ip change destroyed our TC entries
     */
    if (refresh || entry->link_tc_edge == NULL) {
      struct neighbor_entry *ne = entry->neighbor;
      entry->link_tc_edge = olsr_add_tc_edge_entry(tc_myself, &ne->neighbor_main_addr, 0);

      /*
       * Mark the edge local such that it does not get deleted
       * during cleanup functions.
       */
      entry->link_tc_edge->flags |= TC_EDGE_FLAG_LOCAL;
    }
  } OLSR_FOR_ALL_LINK_ENTRIES_END(link);
  changes_topology = true;
}

/**
 * Delete a TC entry.
 *
 * @param entry the TC entry to delete
 *
 */
void
olsr_delete_tc_entry(struct tc_entry *tc)
{
  struct tc_edge_entry *tc_edge;
  struct rt_path *rtp;
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif
  OLSR_DEBUG(LOG_TC, "TC: del entry %s\n", olsr_ip_to_string(&buf, &tc->addr));


  /*
   * Delete the rt_path for ourselves.
   */
  olsr_delete_routing_table(&tc->addr, 8 * olsr_cnf->ipsize, &tc->addr,
                            OLSR_RT_ORIGIN_TC);

  /* The edgetree and prefix tree must be empty before */
  OLSR_FOR_ALL_TC_EDGE_ENTRIES(tc, tc_edge) {
    olsr_delete_tc_edge_entry(tc_edge);
  } OLSR_FOR_ALL_TC_EDGE_ENTRIES_END(tc, tc_edge);

  OLSR_FOR_ALL_PREFIX_ENTRIES(tc, rtp) {
    olsr_delete_rt_path(rtp);
  } OLSR_FOR_ALL_PREFIX_ENTRIES_END(tc, rtp);

  /* Stop running timers */
  olsr_stop_timer(tc->edge_gc_timer);
  tc->edge_gc_timer = NULL;
  olsr_stop_timer(tc->validity_timer);
  tc->validity_timer = NULL;
  olsr_stop_timer(tc->mid_timer);
  tc->mid_timer = NULL;

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
  struct lqtextbuffer lqbuffer1, lqbuffer2;

  snprintf(buf, sizeof(buf),
	   "%s > %s, cost (%6s) %s",
	   olsr_ip_to_string(&addrbuf, &tc_edge->tc->addr),
	   olsr_ip_to_string(&dstbuf, &tc_edge->T_dest_addr),
	   get_tc_edge_entry_text(tc_edge, '/', &lqbuffer1),
	   get_linkcost_text(tc_edge->cost, false, &lqbuffer2));
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
  struct tc_entry *tc = context;
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
  struct tc_entry *tc = context;
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

  return olsr_is_relevant_costchange(old, tc_edge->cost);
}

/**
 * Add a new tc_edge_entry to the tc_edge_tree
 *
 * @param (last)adr address of the entry
 * @return a pointer to the created entry
 */
struct tc_edge_entry *
olsr_add_tc_edge_entry(struct tc_entry *tc, union olsr_ip_addr *addr,
		       uint16_t ansn)
{
  struct tc_entry *tc_neighbor;
  struct tc_edge_entry *tc_edge = olsr_malloc_tc_edge_entry();
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif
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
  avl_insert(&tc->edge_tree, &tc_edge->edge_node, AVL_DUP);
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
  if (tc_neighbor) {
    struct tc_edge_entry *tc_edge_inv;
    OLSR_DEBUG(LOG_TC, "TC:   found neighbor tc_entry %s\n",
		  olsr_ip_to_string(&buf, &tc_neighbor->addr));

    tc_edge_inv = olsr_lookup_tc_edge(tc_neighbor, &tc->addr);
    if (tc_edge_inv) {
      OLSR_DEBUG(LOG_TC, "TC:   found inverse edge for %s\n",
		    olsr_ip_to_string(&buf, &tc_edge_inv->T_dest_addr));

      /*
       * Connect the edges mutually.
       */
      tc_edge_inv->edge_inv = tc_edge;
      tc_edge->edge_inv = tc_edge_inv;
    }
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
 */
void
olsr_delete_tc_edge_entry(struct tc_edge_entry *tc_edge)
{
  struct tc_entry *tc;
  struct link_entry *link;
  struct tc_edge_entry *tc_edge_inv;

  OLSR_DEBUG(LOG_TC, "TC: del edge entry %s\n", olsr_tc_edge_to_string(tc_edge));

  tc = tc_edge->tc;
  avl_delete(&tc->edge_tree, &tc_edge->edge_node);
  olsr_unlock_tc_entry(tc);

  /*
   * Clear the backpointer of our inverse edge.
   */
  tc_edge_inv = tc_edge->edge_inv;
  if (tc_edge_inv) {
    tc_edge_inv->edge_inv = NULL;
  }

  /*
   * If this is a local edge, delete all references to it.
   */
  if (tc_edge->flags & TC_EDGE_FLAG_LOCAL) {
    OLSR_FOR_ALL_LINK_ENTRIES(link) {
      if (link->link_tc_edge == tc_edge) {
        link->link_tc_edge = NULL;
        break;
      }
    } OLSR_FOR_ALL_LINK_ENTRIES_END(link);
  }

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
    if (SEQNO_GREATER_THAN(tc->ansn, tc_edge->ansn) &&
        !(tc_edge->flags & TC_EDGE_FLAG_LOCAL)) {
      olsr_delete_tc_edge_entry(tc_edge);
      retval = true;
    }
  } OLSR_FOR_ALL_TC_EDGE_ENTRIES_END(tc, tc_edge);

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
olsr_delete_revoked_tc_edges(struct tc_entry *tc, uint16_t ansn,
			     union olsr_ip_addr *lower_border,
			     union olsr_ip_addr *upper_border)
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

    if (SEQNO_GREATER_THAN(ansn, tc_edge->ansn) &&
      !(tc_edge->flags & TC_EDGE_FLAG_LOCAL)) {
      olsr_delete_tc_edge_entry(tc_edge);
      retval = 1;
    }
  } OLSR_FOR_ALL_TC_EDGE_ENTRIES_END(tc, tc_edge);

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
olsr_tc_update_edge(struct tc_entry *tc, uint16_t ansn,
		    const unsigned char **curr, union olsr_ip_addr *neighbor)
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
      OLSR_DEBUG(LOG_TC, "TC:   chg edge entry %s\n",
		    olsr_tc_edge_to_string(tc_edge));
    }
  }
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

  OLSR_INFO(LOG_TC,
	      "\n--- %s ------------------------------------------------- TOPOLOGY\n\n",
	      olsr_wallclock_string());
	OLSR_INFO_NH(LOG_TC, "%-*s %-*s           %-14s  %s\n", ipwidth,
	      "Source IP addr", ipwidth, "Dest IP addr", "      LQ      ",
	      "ETX");

  OLSR_FOR_ALL_TC_ENTRIES(tc) {
    struct tc_edge_entry *tc_edge;
    OLSR_FOR_ALL_TC_EDGE_ENTRIES(tc, tc_edge) {
      struct ipaddr_str addrbuf, dstaddrbuf;
      struct lqtextbuffer lqbuffer1, lqbuffer2;

      OLSR_INFO_NH(LOG_TC, "%-*s %-*s %5s      %-14s %s\n",
		    ipwidth, olsr_ip_to_string(&addrbuf, &tc->addr),
		    ipwidth, olsr_ip_to_string(&dstaddrbuf,
					     &tc_edge->T_dest_addr),
			  (tc_edge->flags & TC_EDGE_FLAG_LOCAL) ? "local" : "",
		    get_tc_edge_entry_text(tc_edge, '/', &lqbuffer1),
		    get_linkcost_text(tc_edge->cost, false, &lqbuffer2));

    } OLSR_FOR_ALL_TC_EDGE_ENTRIES_END(tc, tc_edge);
  } OLSR_FOR_ALL_TC_ENTRIES_END(tc);
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
			 union olsr_ip_addr *lower_border_ip,
			 uint8_t upper_border,
			 union olsr_ip_addr *upper_border_ip)
{
  if (lower_border == 0 && upper_border == 0) {
    return 0;
  }
  if (lower_border == 0xff) {
    memset(lower_border_ip, 0, sizeof(lower_border_ip));
  } else {
    int i;

    lower_border--;
    for (i = 0; i < lower_border / 8; i++) {
      lower_border_ip->v6.s6_addr[olsr_cnf->ipsize - i - 1] = 0;
    }
    lower_border_ip->v6.s6_addr[olsr_cnf->ipsize - lower_border / 8 -
				       1] &= (0xff << (lower_border & 7));
    lower_border_ip->v6.s6_addr[olsr_cnf->ipsize - lower_border / 8 -
				       1] |= (1 << (lower_border & 7));
  }

  if (upper_border == 0xff) {
    memset(upper_border_ip, 0xff, sizeof(upper_border_ip));
  } else {
    int i;

    upper_border--;

    for (i = 0; i < upper_border / 8; i++) {
      upper_border_ip->v6.s6_addr[olsr_cnf->ipsize - i - 1] = 0;
    }
    upper_border_ip->v6.s6_addr[olsr_cnf->ipsize - upper_border / 8 -
				       1] &= (0xff << (upper_border & 7));
    upper_border_ip->v6.s6_addr[olsr_cnf->ipsize - upper_border / 8 -
				       1] |= (1 << (upper_border & 7));
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
bool
olsr_input_tc(union olsr_message *msg,
	      struct interface *input_if __attribute__ ((unused)),
	      union olsr_ip_addr *from_addr)
{
  uint16_t size, msg_seq, ansn;
  uint8_t type, ttl, msg_hops, lower_border, upper_border;
  olsr_reltime vtime;
  union olsr_ip_addr originator;
  const unsigned char *limit, *curr;
  struct tc_entry *tc;
  bool relevantTc;
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif
  union olsr_ip_addr lower_border_ip, upper_border_ip;
  int borderSet = 0;

  curr = (void *)msg;
  if (!msg) {
    return false;
  }

  /* We are only interested in TC message types. */
  pkt_get_u8(&curr, &type);
  if ((type != LQ_TC_MESSAGE) && (type != TC_MESSAGE)) {
    return false;
  }

  pkt_get_reltime(&curr, &vtime);
  pkt_get_u16(&curr, &size);

  pkt_get_ipaddress(&curr, &originator);

  /* Copy header values */
  pkt_get_u8(&curr, &ttl);
  pkt_get_u8(&curr, &msg_hops);
  pkt_get_u16(&curr, &msg_seq);
  pkt_get_u16(&curr, &ansn);

  /* Get borders */
  pkt_get_u8(&curr, &lower_border);
  pkt_get_u8(&curr, &upper_border);

  tc = olsr_lookup_tc_entry(&originator);

  if (tc && 0 != tc->edge_tree.count) {
    if (olsr_seq_inrange_high((int)tc->msg_seq - TC_SEQNO_WINDOW,
			      tc->msg_seq,
			      msg_seq) &&
	olsr_seq_inrange_high((int)tc->ansn - TC_ANSN_WINDOW, tc->ansn, ansn)) {

      /*
       * Ignore already seen seq/ansn values (small window for mesh memory)
       */
      if ((tc->msg_seq == msg_seq) || (tc->ignored++ < 32)) {
	return false;
      }

      OLSR_DEBUG(LOG_TC, "Ignored to much LQTC's for %s, restarting\n",
		    olsr_ip_to_string(&buf, &originator));

    } else
      if (!olsr_seq_inrange_high
	  (tc->msg_seq,
	   (int)tc->msg_seq + TC_SEQNO_WINDOW * TC_SEQNO_WINDOW_MULT, msg_seq)
	  || !olsr_seq_inrange_low(tc->ansn,
				   (int)tc->ansn +
				   TC_ANSN_WINDOW * TC_ANSN_WINDOW_MULT,
				   ansn)) {

      /*
       * Only accept future seq/ansn values (large window for node reconnects).
       * Restart in all other cases. Ignore a single stray message.
       */
      if (!tc->err_seq_valid) {
	tc->err_seq = msg_seq;
	tc->err_seq_valid = true;
      }
      if (tc->err_seq == msg_seq) {
	return false;
      }

      OLSR_DEBUG(LOG_TC, "Detected node restart for %s\n",
		    olsr_ip_to_string(&buf, &originator));
    }
  }

  /*
   * Generate a new tc_entry in the lsdb and store the sequence number.
   */
  if (!tc) {
    tc = olsr_add_tc_entry(&originator);
  }

  /*
   * Update the tc entry.
   */
  tc->msg_hops = msg_hops;
  tc->msg_seq = msg_seq;
  tc->ansn = ansn;
  tc->ignored = 0;
  tc->err_seq_valid = false;

  /*
   * If the sender interface (NB: not originator) of this message
   * is not in the symmetric 1-hop neighborhood of this node, the
   * message MUST be discarded.
   */
  if (check_neighbor_link(from_addr) != SYM_LINK) {
    OLSR_DEBUG(LOG_TC, "Received TC from NON SYM neighbor %s\n",
		  olsr_ip_to_string(&buf, from_addr));
    return false;
  }

  OLSR_DEBUG(LOG_TC, "Processing TC from %s, seq 0x%04x\n",
	      olsr_ip_to_string(&buf, &originator), tc->msg_seq);

  /*
   * Now walk the edge advertisements contained in the packet.
   */

  limit = (unsigned char *)msg + size;
  borderSet = 0;
  relevantTc = false;
  while (curr < limit) {
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
  if (borderSet) {
    borderSet = olsr_calculate_tc_border(lower_border, &lower_border_ip,
					 upper_border, &upper_border_ip);
  }

  /*
   * Set or change the expiration timer accordingly.
   */
  olsr_set_timer(&tc->validity_timer, vtime,
		 OLSR_TC_VTIME_JITTER, OLSR_TIMER_ONESHOT,
		 &olsr_expire_tc_entry, tc, tc_validity_timer_cookie->ci_id);

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
		   OLSR_TC_EDGE_GC_JITTER, OLSR_TIMER_ONESHOT,
		   &olsr_expire_tc_edge_gc, tc, tc_edge_gc_timer_cookie->ci_id);
  }

  /* Forward the message */
  return true;
}

uint32_t
getRelevantTcCount(void) {
  return relevantTcCount;
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

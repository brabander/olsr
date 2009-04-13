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

#include "routing_table.h"
#include "ipcalc.h"
#include "defs.h"
#include "two_hop_neighbor_table.h"
#include "tc_set.h"
#include "mid_set.h"
#include "neighbor_table.h"
#include "olsr.h"
#include "link_set.h"
#include "common/avl.h"
#include "olsr_spf.h"
#include "net_olsr.h"
#include "olsr_logging.h"

#include <assert.h>

/* Cookies */
struct olsr_cookie_info *rt_mem_cookie = NULL;
struct olsr_cookie_info *rtp_mem_cookie = NULL; /* Maybe static */

/*
 * Sven-Ola: if the current internet gateway is switched, the
 * NAT connection info is lost for any active TCP/UDP session.
 * For this reason, we do not want to switch if the advantage
 * is only minimal (cost of loosing all NATs is too high).
 * The following rt_path keeps track of the current inet gw.
 */
static struct rt_path *current_inetgw = NULL;

/* Root of our RIB */
struct avl_tree routingtree;

/*
 * Keep a version number for detecting outdated elements
 * in the per rt_entry rt_path subtree.
 */
unsigned int routingtree_version;

/**
 * avl_comp_ipv4_prefix_origin
 *
 * compare two ipv4 prefixes.
 * first compare the prefixes, then
 *  then compare the prefix lengths,
 *  then compare origin codes
 *
 * return 0 if there is an exact match and
 * -1 / +1 depending on being smaller or bigger.
 */
int
avl_comp_ipv4_prefix_origin (const void *prefix1, const void *prefix2)
{
  const struct olsr_ip_prefix *pfx1 = prefix1;
  const struct olsr_ip_prefix *pfx2 = prefix2;
  const uint32_t addr1 = ntohl(pfx1->prefix.v4.s_addr);
  const uint32_t addr2 = ntohl(pfx2->prefix.v4.s_addr);
  int diff;

  /* prefix */
  diff = addr2 - addr1;
  if (diff) {
    return diff;
  }

  /* prefix length */
  diff = pfx2->prefix_len - pfx1->prefix_len;
  if (diff) {
    return diff;
  }

  /* prefix origin */
  return (pfx2->prefix_origin - pfx1->prefix_origin);
}

/**
 * avl_comp_ipv6_prefix_origin
 *
 * compare two ipv6 prefixes.
 * first compare the prefixes, then
 *  then compare the prefix lengths,
 *  then compare origin codes
 *
 * return 0 if there is an exact match and
 * -1 / +1 depending on being smaller or bigger.
 */
int
avl_comp_ipv6_prefix_origin (const void *prefix1, const void *prefix2)
{
  int diff;
  const struct olsr_ip_prefix *pfx1 = prefix1;
  const struct olsr_ip_prefix *pfx2 = prefix2;

  /* prefix */
  diff = ip6cmp(&pfx1->prefix.v6, &pfx2->prefix.v6);
  if (diff) {
    return diff;
  }

  /* prefix length */
  diff = pfx2->prefix_len - pfx1->prefix_len;
  if (diff) {
    return diff;
  }

  /* prefix origin */
  return (pfx2->prefix_origin - pfx1->prefix_origin);
}

/**
 * avl_comp_ipv4_prefix
 *
 * compare two ipv4 prefixes.
 * first compare the prefixes, then
 *  then compare the prefix lengths.
 *
 * return 0 if there is an exact match and
 * -1 / +1 depending on being smaller or bigger.
 */
int
avl_comp_ipv4_prefix (const void *prefix1, const void *prefix2)
{
  const struct olsr_ip_prefix *pfx1 = prefix1;
  const struct olsr_ip_prefix *pfx2 = prefix2;
  const uint32_t addr1 = ntohl(pfx1->prefix.v4.s_addr);
  const uint32_t addr2 = ntohl(pfx2->prefix.v4.s_addr);
  int diff;

  /* prefix */
  diff = addr2 - addr1;
  if (diff) {
    return diff;
  }

  /* prefix length */
  return (pfx2->prefix_len - pfx1->prefix_len);
}

/**
 * avl_comp_ipv6_prefix
 *
 * compare two ipv6 prefixes.
 * first compare the prefixes, then
 *  then compare the prefix lengths.
 *
 * return 0 if there is an exact match and
 * -1 / +1 depending on being smaller or bigger.
 */
int
avl_comp_ipv6_prefix (const void *prefix1, const void *prefix2)
{
  int diff;
  const struct olsr_ip_prefix *pfx1 = prefix1;
  const struct olsr_ip_prefix *pfx2 = prefix2;

  /* prefix */
  diff = ip6cmp(&pfx1->prefix.v6, &pfx2->prefix.v6);
  if (diff) {
    return diff;
  }

  /* prefix length */
  return (pfx2->prefix_len - pfx1->prefix_len);
}

/**
 * avl_comp_ipv4_addr_origin
 *
 * first compare the addresses, then compare the origin code.
 *
 * return 0 if there is an exact match and
 * -1 / +1 depending on being smaller or bigger.
 */
int
avl_comp_ipv4_addr_origin (const void *prefix1, const void *prefix2)
{
  const struct olsr_ip_prefix *pfx1 = prefix1;
  const struct olsr_ip_prefix *pfx2 = prefix2;
  const uint32_t addr1 = ntohl(pfx1->prefix.v4.s_addr);
  const uint32_t addr2 = ntohl(pfx2->prefix.v4.s_addr);
  int diff;

  /* prefix */
  diff = addr2 - addr1;
  if (diff) {
    return diff;
  }

  /* prefix origin */
  return (pfx2->prefix_origin - pfx1->prefix_origin);
}

/**
 * avl_comp_ipv6_addr_origin
 *
 * first compare the addresses, then compare the origin code.
 *
 * return 0 if there is an exact match and
 * -1 / +1 depending on being smaller or bigger.
 */
int
avl_comp_ipv6_addr_origin (const void *prefix1, const void *prefix2)
{
  int diff;
  const struct olsr_ip_prefix *pfx1 = prefix1;
  const struct olsr_ip_prefix *pfx2 = prefix2;

  /* prefix */
  diff = ip6cmp(&pfx1->prefix.v6, &pfx2->prefix.v6);
  if (diff) {
    return diff;
  }

  /* prefix origin */
  return (pfx2->prefix_origin - pfx1->prefix_origin);
}

/**
 * Initialize the routingtree and kernel change queues.
 */
void
olsr_init_routing_table(void)
{
  OLSR_INFO(LOG_ROUTING, "RIB: initialize routing tree...\n");

  /* the routing tree */
  avl_init(&routingtree, avl_comp_prefix_default);
  routingtree_version = 0;

  /*
   * Get some cookies for memory stats and memory recycling.
   */
  rt_mem_cookie = olsr_alloc_cookie("rt_entry", OLSR_COOKIE_TYPE_MEMORY);
  olsr_cookie_set_memory_size(rt_mem_cookie, sizeof(struct rt_entry));

  rtp_mem_cookie = olsr_alloc_cookie("rt_path", OLSR_COOKIE_TYPE_MEMORY);
  olsr_cookie_set_memory_size(rtp_mem_cookie, sizeof(struct rt_path));
}

/**
 * Look up a max prefix entry (= /32 or /128) in the routing table.
 *
 * @param dst the address of the entry
 *
 * @return a pointer to a rt_entry struct
 * representing the route entry.
 */
struct rt_entry *
olsr_lookup_routing_table(const union olsr_ip_addr *dst)
{
  struct avl_node *rt_tree_node;
  struct olsr_ip_prefix prefix;

  prefix.prefix = *dst;
  prefix.prefix_len = 8 * olsr_cnf->ipsize;

  rt_tree_node = avl_find(&routingtree, &prefix);

  return rt_tree_node ? rt_tree2rt(rt_tree_node) : NULL;
}

/**
 * Update gateway/interface/etx/hopcount and the version for a route path.
 */
void
olsr_update_rt_path(struct rt_path *rtp, struct tc_entry *tc,
                    struct link_entry *link)
{

  rtp->rtp_version = routingtree_version;

  /* gateway */
  rtp->rtp_nexthop.gateway = link->neighbor_iface_addr;

  /* interface */
  if (rtp->rtp_nexthop.interface != link->inter) {
    if (rtp->rtp_nexthop.interface) {
      unlock_interface(rtp->rtp_nexthop.interface);
    }
    rtp->rtp_nexthop.interface = link->inter;
    lock_interface(rtp->rtp_nexthop.interface);
  }

  /* metric/etx */
  rtp->rtp_metric.hops = tc->hops;
  rtp->rtp_metric.cost = tc->path_cost;
}

/**
 * Alloc and key a new rt_entry.
 */
static struct rt_entry *
olsr_alloc_rt_entry(struct olsr_ip_prefix *prefix)
{
  struct rt_entry *rt = olsr_cookie_malloc(rt_mem_cookie);
  if (!rt) {
    return NULL;
  }

  /* Mark this entry as fresh - see olsr_update_rib_routes() */
  rt->rt_nexthop.interface = NULL;

  /* set key and backpointer prior to tree insertion */
  rt->rt_dst = *prefix;

  rt->rt_tree_node.key = &rt->rt_dst;
  avl_insert(&routingtree, &rt->rt_tree_node, AVL_DUP_NO);

  /* init the originator subtree */
  avl_init(&rt->rt_path_tree, avl_comp_addr_origin_default);

  return rt;
}

/**
 * Alloc and key a new rt_path.
 */
static struct rt_path *
olsr_alloc_rt_path(struct tc_entry *tc,
                   struct olsr_ip_prefix *prefix, uint8_t origin)
{
  struct rt_path *rtp = olsr_cookie_malloc(rtp_mem_cookie);

  if (!rtp) {
    return NULL;
  }

  memset(rtp, 0, sizeof(*rtp));

  rtp->rtp_dst = *prefix;
  rtp->rtp_dst.prefix_origin = origin;

  /* set key and backpointer prior to tree insertion */
  rtp->rtp_prefix_tree_node.key = &rtp->rtp_dst;

  /* insert to the tc prefix tree */
  avl_insert(&tc->prefix_tree, &rtp->rtp_prefix_tree_node, AVL_DUP_NO);
  olsr_lock_tc_entry(tc);

  /* backlink to the owning tc entry */
  rtp->rtp_tc = tc;

  /*
   * Initialize the key for the per-originator subtree.
   * Note that the path will not yet be inserted into the routing tree.
   * This will happen later using olsr_insert_rt_path()
   * when the node becomes reachable post SPF calculation.
   * In the originator subtree the prefix length will be set to
   * max. we do not really need this in avl_comp_addr_origin, but since
   * the datatype is olsr_ip_prefix lets keep the environment a tidy place.
   */
  rtp->rtp_originator.prefix = tc->addr;
  rtp->rtp_originator.prefix_len = 8 * olsr_cnf->ipsize;
  rtp->rtp_originator.prefix_origin = origin;
  rtp->rtp_tree_node.key = &rtp->rtp_originator;

  return rtp;
}

/**
 * Create a route entry for a given rt_path and
 * insert it into the global RIB tree.
 */
void
olsr_insert_rt_path(struct rt_path *rtp, struct tc_entry *tc,
                    struct link_entry *link)
{
  struct rt_entry *rt;
  struct avl_node *node;

  /*
   * no unreachable routes please.
   */
  if (tc->path_cost == ROUTE_COST_BROKEN) {
    return;
  }

  /*
   * No bogus prefix lengths.
   */
  if (rtp->rtp_dst.prefix_len > 8 * olsr_cnf->ipsize) {
    return;
  }

  /*
   * first check if there is a route_entry for the prefix.
   */
  node = avl_find(&routingtree, &rtp->rtp_dst);

  if (!node) {

    /* no route entry yet */
    rt = olsr_alloc_rt_entry(&rtp->rtp_dst);

    if (!rt) {
      return;
    }

  } else {
    rt = rt_tree2rt(node);
  }

  /*
   * Insert to the route entry originator tree
   * The key has been initialized in olsr_alloc_rt_path().
   */
  avl_insert(&rt->rt_path_tree, &rtp->rtp_tree_node, AVL_DUP_NO);

  /* backlink to the owning route entry */
  rtp->rtp_rt = rt;

  /* update the version field and relevant parameters */
  olsr_update_rt_path(rtp, tc, link);
}

/**
 * Unlink and free a rt_path.
 */
void
olsr_delete_rt_path(struct rt_path *rtp)
{

  /* remove from the originator tree */
  if (rtp->rtp_rt) {
    avl_delete(&rtp->rtp_rt->rt_path_tree, &rtp->rtp_tree_node);
    rtp->rtp_rt = NULL;
  }

  /* remove from the tc prefix tree */
  if (rtp->rtp_tc) {
    avl_delete(&rtp->rtp_tc->prefix_tree, &rtp->rtp_prefix_tree_node);
    olsr_unlock_tc_entry(rtp->rtp_tc);
    rtp->rtp_tc = NULL;
  }

  /* unlock underlying interface */
  if (rtp->rtp_nexthop.interface) {
    unlock_interface(rtp->rtp_nexthop.interface);
  }

  /* no current inet gw if the rt_path is removed */
  if (current_inetgw == rtp) {
    current_inetgw = NULL;
  }

  olsr_cookie_free(rtp_mem_cookie, rtp);
}

/**
 * compare two route paths.
 *
 * returns TRUE if the first path is better
 * than the second one, FALSE otherwise.
 */
static bool
olsr_cmp_rtp(const struct rt_path *rtp1, const struct rt_path *rtp2, const struct rt_path *inetgw)
{
    olsr_linkcost etx1 = rtp1->rtp_metric.cost;
    olsr_linkcost etx2 = rtp2->rtp_metric.cost;
    if (inetgw == rtp1) etx1 *= olsr_cnf->lq_nat_thresh;
    if (inetgw == rtp2) etx2 *= olsr_cnf->lq_nat_thresh;

    /* etx comes first */
    if (etx1 < etx2) {
      return true;
    }
    if (etx1 > etx2) {
      return false;
    }

    /* hopcount is next tie breaker */
    if (rtp1->rtp_metric.hops < rtp2->rtp_metric.hops) {
      return true;
    }
    if (rtp1->rtp_metric.hops > rtp2->rtp_metric.hops) {
      return false;
    }

    /* originator type code is next tie breaker */
    if (rtp1->rtp_originator.prefix_origin <
        rtp2->rtp_originator.prefix_origin) {
      return true;
    }
    if (rtp1->rtp_originator.prefix_origin >
        rtp2->rtp_originator.prefix_origin) {
      return false;
    }

    /* originator address is final breaker */
    return(olsr_ipcmp(&rtp1->rtp_originator.prefix,
                      &rtp2->rtp_originator.prefix));
}

/**
 * compare the best path of two route entries.
 *
 * returns TRUE if the first entry is better
 * than the second one, FALSE otherwise.
 */
bool
olsr_cmp_rt(const struct rt_entry *rt1, const struct rt_entry *rt2)
{
    return olsr_cmp_rtp(rt1->rt_best, rt2->rt_best, NULL);
}

/**
 * run best route selection among a
 * set of identical prefixes.
 */
void
olsr_rt_best(struct rt_entry *rt)
{
  /* grab the first entry */
  struct avl_node *node = avl_walk_first(&rt->rt_path_tree);

  assert(node != NULL); /* should not happen */

  rt->rt_best = rtp_tree2rtp(node);

  /* walk all remaining originator entries */
  while ((node = avl_walk_next(node))) {
    struct rt_path *rtp = rtp_tree2rtp(node);

    if (olsr_cmp_rtp(rtp, rt->rt_best, current_inetgw)) {
      rt->rt_best = rtp;
    }
  }

  if (0 == rt->rt_dst.prefix_len) {
    current_inetgw = rt->rt_best;
  }
}

/**
 * Insert a prefix into the prefix tree hanging off a lsdb (tc) entry.
 *
 * Check if the candidate route (we call this a rt_path) is known,
 * if not create it.
 * Upon post-SPF processing (if the node is reachable) the prefix
 * will be finally inserted into the global RIB.
 *
 *@param dst the destination
 *@param plen the prefix length
 *@param originator the originating router
 *
 *@return the new rt_path struct
 */
struct rt_path *
olsr_insert_routing_table(const union olsr_ip_addr *dst, int plen,
                          const union olsr_ip_addr *originator, int origin)
{
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str dstbuf, origbuf;
#endif
  struct tc_entry *tc;
  struct rt_path *rtp;
  struct avl_node *node;
  struct olsr_ip_prefix prefix;

  /*
   * No bogus prefix lengths.
   */
  if (plen > 8 * (int)olsr_cnf->ipsize) {
    return NULL;
  }

  /*
   * For all routes we use the tc_entry as an hookup point.
   * If the tc_entry is disconnected, i.e. has no edges it will not
   * be explored during SPF run.
   */
  tc = olsr_locate_tc_entry(originator);

  /*
   * first check if there is a rt_path for the prefix.
   */
  prefix.prefix = *dst;
  prefix.prefix_len = plen;
  prefix.prefix_origin = origin;

  node = avl_find(&tc->prefix_tree, &prefix);

  if (!node) {

    /* no rt_path for this prefix yet */
    rtp = olsr_alloc_rt_path(tc, &prefix, origin);

    if (!rtp) {
      return NULL;
    }

    OLSR_DEBUG(LOG_ROUTING, "RIB: add prefix %s/%d from %s\n",
                olsr_ip_to_string(&dstbuf, dst), plen,
                olsr_ip_to_string(&origbuf, originator));

    /* overload the hna change bit for flagging a prefix change */
    changes_hna = true;

  } else {
    rtp = rtp_prefix_tree2rtp(node);
  }

  return rtp;
}

/**
 * Delete a prefix from the prefix tree hanging off a lsdb (tc) entry.
 */
void
olsr_delete_routing_table(union olsr_ip_addr *dst, int plen,
                          union olsr_ip_addr *originator, int origin)
{
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str dstbuf, origbuf;
#endif

  struct tc_entry *tc;
  struct rt_path *rtp;
  struct avl_node *node;
  struct olsr_ip_prefix prefix;

  /*
   * No bogus prefix lengths.
   */
  if (plen > 8 * (int)olsr_cnf->ipsize) {
    return;
  }

  tc = olsr_lookup_tc_entry(originator);
  if (!tc) {
    return;
  }

  /*
   * Grab the rt_path for the prefix.
   */
  prefix.prefix = *dst;
  prefix.prefix_len = plen;
  prefix.prefix_origin = origin;

  node = avl_find(&tc->prefix_tree, &prefix);

  if (node) {
    rtp = rtp_prefix_tree2rtp(node);
    olsr_delete_rt_path(rtp);

    OLSR_DEBUG(LOG_ROUTING, "RIB: del prefix %s/%d from %s\n",
                olsr_ip_to_string(&dstbuf, dst), plen,
                olsr_ip_to_string(&origbuf, originator));

    /* overload the hna change bit for flagging a prefix change */
    changes_hna = true;
  }
}

/**
 * format a route entry into a buffer
 */
char *
olsr_rt_to_string(const struct rt_entry *rt)
{
  static char buff[128];
  struct ipaddr_str gwstr;
  struct ipprefix_str prefixstr;

  snprintf(buff, sizeof(buff),
           "%s via %s dev %s",
           olsr_ip_prefix_to_string(&prefixstr, &rt->rt_dst),
           olsr_ip_to_string(&gwstr, &rt->rt_nexthop.gateway),
           rt->rt_nexthop.interface ? rt->rt_nexthop.interface->int_name : "(null)"
  );

  return buff;
}

/**
 * format a route path into a buffer
 */
char *
olsr_rtp_to_string(const struct rt_path *rtp)
{
  static char buff[128];
  struct ipaddr_str origstr, gwstr;
  struct lqtextbuffer lqbuffer;
  struct ipprefix_str prefixstr;

  snprintf(buff, sizeof(buff),
           "%s from %s via %s dev %s, "
           "cost %s, metric %u, v %u",
           olsr_ip_prefix_to_string(&prefixstr, &rtp->rtp_rt->rt_dst),
           olsr_ip_to_string(&origstr, &rtp->rtp_originator.prefix),
           olsr_ip_to_string(&gwstr, &rtp->rtp_nexthop.gateway),
           rtp->rtp_nexthop.interface ? rtp->rtp_nexthop.interface->int_name : "(null)",
           get_linkcost_text(rtp->rtp_metric.cost, true, &lqbuffer),
           rtp->rtp_metric.hops,
           rtp->rtp_version);

  return buff;
}

/**
 * Print the routingtree to STDOUT
 *
 */
void
olsr_print_routing_table(struct avl_tree *tree __attribute__((unused)))
{
  /* The whole function makes no sense without it. */
#if !defined REMOVE_LOG_INFO
  struct avl_node *rt_tree_node;
  struct lqtextbuffer lqbuffer;

  OLSR_INFO(LOG_ROUTING, "ROUTING TABLE\n");

  for (rt_tree_node = avl_walk_first(tree);
       rt_tree_node != NULL;
       rt_tree_node = avl_walk_next(rt_tree_node)) {
    struct avl_node *rtp_tree_node;
    struct ipaddr_str origstr, gwstr;
    struct ipprefix_str prefixstr;
    struct rt_entry *rt = rt_tree2rt(rt_tree_node);

    /* first the route entry */
    OLSR_INFO_NH(LOG_ROUTING, "%s, via %s dev %s, best-originator %s\n",
           olsr_ip_prefix_to_string(&prefixstr, &rt->rt_dst),
           olsr_ip_to_string(&origstr, &rt->rt_nexthop.gateway),
           rt->rt_nexthop.interface ? rt->rt_nexthop.interface->int_name : "(null)",
           olsr_ip_to_string(&gwstr, &rt->rt_best->rtp_originator.prefix));

    /* walk the per-originator path tree of routes */
    for (rtp_tree_node = avl_walk_first(&rt->rt_path_tree);
         rtp_tree_node != NULL;
         rtp_tree_node = avl_walk_next(rtp_tree_node)) {
      struct rt_path *rtp = rtp_tree2rtp(rtp_tree_node);
      OLSR_INFO_NH(LOG_ROUTING, "\tfrom %s, cost %s, metric %u, via %s, dev %s, v %u\n",
             olsr_ip_to_string(&origstr, &rtp->rtp_originator.prefix),
             get_linkcost_text(rtp->rtp_metric.cost, true, &lqbuffer),
             rtp->rtp_metric.hops,
             olsr_ip_to_string(&gwstr, &rtp->rtp_nexthop.gateway),
             rt->rt_nexthop.interface ? rt->rt_nexthop.interface->int_name : "(null)",
             rtp->rtp_version);
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

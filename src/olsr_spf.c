
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

#include "olsr_spf.h"
#include "tc_set.h"
#include "neighbor_table.h"
#include "routing_table.h"
#include "lq_plugin.h"
#include "process_routes.h"
#include "olsr_logging.h"

struct timer_entry *spf_backoff_timer = NULL;

/*
 * avl_comp_etx
 *
 * compare two etx metrics.
 * return 0 if there is an exact match and
 * -1 / +1 depending on being smaller or bigger.
 * note that this results in the most optimal code
 * after compiler optimization.
 */
static int
avl_comp_etx(const void *etx1, const void *etx2)
{
  if (*(const olsr_linkcost *)etx1 < *(const olsr_linkcost *)etx2) {
    return -1;
  }

  if (*(const olsr_linkcost *)etx1 > *(const olsr_linkcost *)etx2) {
    return +1;
  }

  return 0;
}

/*
 * olsr_spf_add_cand_tree
 *
 * Key an existing vertex to a candidate tree.
 */
static void
olsr_spf_add_cand_tree(struct avl_tree *tree, struct tc_entry *tc)
{
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
  char lqbuffer[LQTEXT_MAXLENGTH];
#endif
  tc->cand_tree_node.key = &tc->path_cost;

  OLSR_DEBUG(LOG_ROUTING, "SPF: insert candidate %s, cost %s\n",
             olsr_ip_to_string(&buf, &tc->addr), olsr_get_linkcost_text(tc->path_cost, false, lqbuffer, sizeof(lqbuffer)));

  avl_insert(tree, &tc->cand_tree_node, true);
}

/*
 * olsr_spf_del_cand_tree
 *
 * Unkey an existing vertex from a candidate tree.
 */
static void
olsr_spf_del_cand_tree(struct avl_tree *tree, struct tc_entry *tc)
{

#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
  char lqbuffer[LQTEXT_MAXLENGTH];
#endif
  OLSR_DEBUG(LOG_ROUTING, "SPF: delete candidate %s, cost %s\n",
             olsr_ip_to_string(&buf, &tc->addr), olsr_get_linkcost_text(tc->path_cost, false, lqbuffer, sizeof(lqbuffer)));

  avl_delete(tree, &tc->cand_tree_node);
}

/*
 * olsr_spf_add_path_list
 *
 * Insert an SPF result at the end of the path list.
 */
static void
olsr_spf_add_path_list(struct list_node *head, int *path_count, struct tc_entry *tc)
{
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str pathbuf, nbuf;
  char lqbuffer[LQTEXT_MAXLENGTH];
#endif

  OLSR_DEBUG(LOG_ROUTING, "SPF: append path %s, cost %s, via %s\n",
             olsr_ip_to_string(&pathbuf, &tc->addr),
             olsr_get_linkcost_text(tc->path_cost, false, lqbuffer, sizeof(lqbuffer)),
             tc->next_hop ? olsr_ip_to_string(&nbuf, &tc->next_hop->neighbor_iface_addr) : "-");

  list_add_before(head, &tc->path_list_node);
  *path_count = *path_count + 1;
}

/*
 * olsr_spf_extract_best
 *
 * return the node with the minimum pathcost.
 */
static struct tc_entry *
olsr_spf_extract_best(struct avl_tree *tree)
{
  struct avl_node *node = avl_walk_first(tree);

  return (node ? cand_tree2tc(node) : NULL);
}


/*
 * olsr_spf_relax
 *
 * Explore all edges of a node and add the node
 * to the candidate tree if the if the aggregate
 * path cost is better.
 */
static void
olsr_spf_relax(struct avl_tree *cand_tree, struct tc_entry *tc)
{
  struct avl_node *edge_node;
  olsr_linkcost new_cost;

#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf, nbuf;
  char lqbuffer[LQTEXT_MAXLENGTH];
#endif
  OLSR_DEBUG(LOG_ROUTING, "SPF: exploring node %s, cost %s\n",
             olsr_ip_to_string(&buf, &tc->addr),
             olsr_get_linkcost_text(tc->path_cost, false, lqbuffer, sizeof(lqbuffer)));

  /*
   * loop through all edges of this vertex.
   */
  for (edge_node = avl_walk_first(&tc->edge_tree); edge_node; edge_node = avl_walk_next(edge_node)) {

    struct tc_entry *new_tc;
    struct tc_edge_entry *tc_edge = edge_tree2tc_edge(edge_node);

    /*
     * We are not interested in dead-end edges.
     */
    if (!tc_edge->edge_inv) {
      OLSR_DEBUG(LOG_ROUTING, "SPF:   ignoring edge %s\n", olsr_ip_to_string(&buf, &tc_edge->T_dest_addr));
      if (!tc_edge->edge_inv) {
        OLSR_DEBUG(LOG_ROUTING, "SPF:     no inverse edge\n");
      }
      continue;
    }

    /*
     * total quality of the path through this vertex
     * to the destination of this edge
     */
    new_cost = tc->path_cost + tc_edge->common_cost;

    OLSR_DEBUG(LOG_ROUTING, "SPF:   exploring edge %s, cost %s\n",
               olsr_ip_to_string(&buf, &tc_edge->T_dest_addr),
               olsr_get_linkcost_text(new_cost, true, lqbuffer, sizeof(lqbuffer)));

    /*
     * if it's better than the current path quality of this edge's
     * destination node, then we've found a better path to this node.
     */
    new_tc = tc_edge->edge_inv->tc;

    if (new_cost < new_tc->path_cost) {

      /* if this node has been on the candidate tree delete it */
      if (new_tc->path_cost < ROUTE_COST_BROKEN) {
        olsr_spf_del_cand_tree(cand_tree, new_tc);
      }

      /* re-insert on candidate tree with the better metric */
      new_tc->path_cost = new_cost;
      olsr_spf_add_cand_tree(cand_tree, new_tc);

      /* pull-up the next-hop and bump the hop count */
      if (tc->next_hop) {
        new_tc->next_hop = tc->next_hop;
      }
      new_tc->hops = tc->hops + 1;

      OLSR_DEBUG(LOG_ROUTING, "SPF:   better path to %s, cost %s, via %s, hops %d\n",
                 olsr_ip_to_string(&buf, &new_tc->addr),
                 olsr_get_linkcost_text(new_cost, true, lqbuffer, sizeof(lqbuffer)),
                 tc->next_hop ? olsr_ip_to_string(&nbuf, &tc->next_hop->neighbor_iface_addr) : "<none>", new_tc->hops);
    }
  }
}

/*
 * olsr_spf_run_full
 *
 * Run the Dijkstra algorithm.
 *
 * A node gets added to the candidate tree when one of its edges has
 * an overall better root path cost than the node itself.
 * The node with the shortest metric gets moved from the candidate to
 * the path list every pass.
 * The SPF computation is completed when there are no more nodes
 * on the candidate tree.
 */
static void
olsr_spf_run_full(struct avl_tree *cand_tree, struct list_node *path_list, int *path_count)
{
  struct tc_entry *tc;

  *path_count = 0;

  while ((tc = olsr_spf_extract_best(cand_tree))) {

    olsr_spf_relax(cand_tree, tc);

    /*
     * move the best path from the candidate tree
     * to the path list.
     */
    olsr_spf_del_cand_tree(cand_tree, tc);
    olsr_spf_add_path_list(path_list, path_count, tc);
  }
}

/**
 * Callback for the SPF backoff timer.
 */
static void
olsr_expire_spf_backoff(void *context __attribute__ ((unused)))
{
  spf_backoff_timer = NULL;
}

void
olsr_calculate_routing_table(void)
{
#ifdef SPF_PROFILING
  struct timeval t1, t2, t3, t4, t5, spf_init, spf_run, route, kernel, total;
#endif
  struct avl_tree cand_tree;
  struct avl_node *rtp_tree_node;
  struct list_node path_list;          /* head of the path_list */
  struct tc_entry *tc;
  struct rt_path *rtp;
  struct tc_edge_entry *tc_edge;
  struct nbr_entry *neigh;
  struct link_entry *link;
  int path_count = 0;

  /* We are done if our backoff timer is running */
  if (spf_backoff_timer) {
    return;
  }
  spf_backoff_timer =
    olsr_start_timer(OLSR_SPF_BACKOFF_TIME, OLSR_SPF_BACKOFF_JITTER,
                     OLSR_TIMER_ONESHOT, &olsr_expire_spf_backoff, NULL, spf_backoff_timer_cookie);

#ifdef SPF_PROFILING
  gettimeofday(&t1, NULL);
#endif

  /*
   * Prepare the candidate tree and result list.
   */
  avl_init(&cand_tree, avl_comp_etx);
  list_head_init(&path_list);
  olsr_bump_routingtree_version();

  /*
   * Initialize vertices in the lsdb.
   */
  OLSR_FOR_ALL_TC_ENTRIES(tc) {
    tc->next_hop = NULL;
    tc->path_cost = ROUTE_COST_BROKEN;
    tc->hops = 0;
  }
  OLSR_FOR_ALL_TC_ENTRIES_END(tc);


  /*
   * Check if there was a change in the main IP address.
   * Bail if there is no main IP address.
   */
  olsr_change_myself_tc();
  if (!tc_myself) {

    /*
     * All gone now. Flush all routes.
     */
    olsr_update_rib_routes();
    olsr_update_kernel_routes();
    return;
  }

  /*
   * zero ourselves and add us to the candidate tree.
   */
  tc_myself->path_cost = ZERO_ROUTE_COST;
  olsr_spf_add_cand_tree(&cand_tree, tc_myself);

  /*
   * Set the next-hops of our neighbor link.
   */
  OLSR_FOR_ALL_LINK_ENTRIES(link) {

    neigh = link->neighbor;
    tc_edge = link->link_tc_edge;

    if (neigh->is_sym) {
      if (tc_edge->edge_inv) {
        tc_edge->edge_inv->tc->next_hop = link;
      }
    }
  }
  OLSR_FOR_ALL_LINK_ENTRIES_END(link);

#ifdef SPF_PROFILING
  gettimeofday(&t2, NULL);
#endif

  /*
   * Run the SPF calculation.
   */
  olsr_spf_run_full(&cand_tree, &path_list, &path_count);

  OLSR_DEBUG(LOG_ROUTING, "\n--- %s ------------------------------------------------- DIJKSTRA\n\n", olsr_wallclock_string());

#ifdef SPF_PROFILING
  gettimeofday(&t3, NULL);
#endif

  /*
   * In the path list we have all the reachable nodes in our topology.
   */
  for (; !list_is_empty(&path_list); list_remove(path_list.next)) {

    tc = pathlist2tc(path_list.next);
    link = tc->next_hop;

    if (!link) {
      /*
       * Supress the error msg when our own tc_entry
       * does not contain a next-hop.
       */
      if (tc != tc_myself) {
#if !defined REMOVE_LOG_DEBUG
        struct ipaddr_str buf;
#endif
        OLSR_DEBUG(LOG_ROUTING, "SPF: %s no next-hop\n", olsr_ip_to_string(&buf, &tc->addr));
      }
      continue;
    }

    /*
     * Now walk all prefixes advertised by that node.
     * Since the node is reachable, insert the prefix into the global RIB.
     * If the prefix is already in the RIB, refresh the entry such
     * that olsr_delete_outdated_routes() does not purge it off.
     */
    for (rtp_tree_node = avl_walk_first(&tc->prefix_tree); rtp_tree_node; rtp_tree_node = avl_walk_next(rtp_tree_node)) {

      rtp = rtp_prefix_tree2rtp(rtp_tree_node);

      if (rtp->rtp_rt) {

        /*
         * If there is a route entry, the prefix is already in the global RIB.
         */
        olsr_update_rt_path(rtp, tc, link);

      } else {

        /*
         * The prefix is reachable and not yet in the global RIB.
         * Build a rt_entry for it.
         */
        olsr_insert_rt_path(rtp, tc, link);
      }
    }
  }

  /* Update the RIB based on the new SPF results */

  olsr_update_rib_routes();

#ifdef SPF_PROFILING
  gettimeofday(&t4, NULL);
#endif

  /* move the route changes into the kernel */

  olsr_update_kernel_routes();

#ifdef SPF_PROFILING
  gettimeofday(&t5, NULL);
#endif

#ifdef SPF_PROFILING
  timersub(&t2, &t1, &spf_init);
  timersub(&t3, &t2, &spf_run);
  timersub(&t4, &t3, &route);
  timersub(&t5, &t4, &kernel);
  timersub(&t5, &t1, &total);
  OLSR_DEBUG(LOG_ROUTING, "\n--- SPF-stats for %d nodes, %d routes (total/init/run/route/kern): "
             "%d, %d, %d, %d, %d\n",
             path_count, routingtree.count,
             (int)total.tv_usec, (int)spf_init.tv_usec, (int)spf_run.tv_usec, (int)route.tv_usec, (int)kernel.tv_usec);
#endif
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

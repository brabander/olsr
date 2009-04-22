
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

#include "process_routes.h"
#include "log.h"
#include "kernel_routes.h"
#include "olsr_logging.h"

#include <errno.h>

static struct list_node add_kernel_list;
static struct list_node chg_kernel_list;
static struct list_node del_kernel_list;

/*
 * Function hooks for plugins to intercept
 * adding / deleting routes from the kernel
 */
export_route_function olsr_add_route_function;
export_route_function olsr_del_route_function;


void
olsr_init_export_route(void)
{
  OLSR_INFO(LOG_ROUTING, "Initialize route processing...\n");

  /* the add/chg/del kernel queues */
  list_head_init(&add_kernel_list);
  list_head_init(&chg_kernel_list);
  list_head_init(&del_kernel_list);

  olsr_add_route_function = olsr_kernel_add_route;
  olsr_del_route_function = olsr_kernel_del_route;
}

/**
 * Delete all OLSR routes.
 *
 * This is extremely simple - Just increment the version of the
 * tree and then olsr_update_rib_routes() will see all routes in the tree
 * as outdated and olsr_update_kernel_routes() will finally flush it.
 *
 */
void
olsr_delete_all_kernel_routes(void)
{
  OLSR_DEBUG(LOG_ROUTING, "Deleting all routes...\n");

  olsr_bump_routingtree_version();
  olsr_update_rib_routes();
  olsr_update_kernel_routes();
}

/**
 * Enqueue a route on a kernel add/chg/del queue.
 */
static void
olsr_enqueue_rt(struct list_node *head_node, struct rt_entry *rt)
{
  const struct rt_nexthop *nh;

  /* if this node is already on some changelist we are done */
  if (list_node_on_list(&rt->rt_change_node)) {
    return;
  }

  /*
   * For easier route dependency tracking we enqueue nexthop routes
   * at the head of the queue and non-nexthop routes at the tail of the queue.
   */
  nh = olsr_get_nh(rt);

  if (olsr_ipcmp(&rt->rt_dst.prefix, &nh->gateway) == 0) {
    list_add_after(head_node, &rt->rt_change_node);
  } else {
    list_add_before(head_node, &rt->rt_change_node);
  }
}

/**
 * Process a route from the kernel deletion list.
 *
 *@return nada
 */
static void
olsr_del_route(struct rt_entry *rt)
{
  int16_t error = olsr_del_route_function(rt, olsr_cnf->ip_version);

  if (error < 0) {
    OLSR_WARN(LOG_ROUTING, "KERN: ERROR deleting %s: %s\n", olsr_rt_to_string(rt), strerror(errno));
  } else {

    /* release the interface. */
    unlock_interface(rt->rt_nexthop.interface);
  }
}

/**
 * Process a route from the kernel addition list.
 *
 *@return nada
 */
static void
olsr_add_route(struct rt_entry *rt)
{
  if (olsr_cnf->del_gws && 0 == rt->rt_dst.prefix_len) {
    struct rt_entry defrt;
    memset(&defrt, 0, sizeof(defrt));
    /*
     * Note: defrt.nexthop.interface == NULL means "remove unspecified default route"
     */
    while (0 <= olsr_del_route_function(&defrt, olsr_cnf->ip_version)) {
    }
    olsr_cnf->del_gws = false;
    exit(9);
  }

  if (0 > olsr_add_route_function(rt, olsr_cnf->ip_version)) {
    OLSR_WARN(LOG_ROUTING, "KERN: ERROR adding %s: %s\n", olsr_rtp_to_string(rt->rt_best), strerror(errno));
  } else {
    /* route addition has suceeded */

    /* save the nexthop and metric in the route entry */
    rt->rt_nexthop = rt->rt_best->rtp_nexthop;
    rt->rt_metric = rt->rt_best->rtp_metric;

    /* lock the interface such that it does not vanish underneath us */
    lock_interface(rt->rt_nexthop.interface);
  }
}

/**
 * process the kernel add list.
 * the routes are already ordered such that nexthop routes
 * are on the head of the queue.
 * nexthop routes need to be added first and therefore
 * the queue needs to be traversed from head to tail.
 */
static void
olsr_add_routes(struct list_node *head_node)
{
  struct rt_entry *rt;

  while (!list_is_empty(head_node)) {
    rt = changelist2rt(head_node->next);
    olsr_add_route(rt);

    list_remove(&rt->rt_change_node);
  }
}

/**
 * process the kernel change list.
 * the routes are already ordered such that nexthop routes
 * are on the head of the queue.
 * non-nexthop routes need to be changed first and therefore
 * the queue needs to be traversed from tail to head.
 */
static void
olsr_chg_kernel_routes(struct list_node *head_node)
{
  struct rt_entry *rt;
  struct list_node *node;

  if (list_is_empty(head_node)) {
    return;
  }

  /*
   * First pass.
   * traverse from the end to the beginning of the list,
   * such that nexthop routes are deleted last.
   */
  for (node = head_node->prev; head_node != node; node = node->prev) {
    rt = changelist2rt(node);
    olsr_del_route(rt);
  }

  /*
   * Second pass.
   * Traverse from the beginning to the end of the list,
   * such that nexthop routes are added first.
   */
  while (!list_is_empty(head_node)) {
    rt = changelist2rt(head_node->next);
    olsr_add_route(rt);

    list_remove(&rt->rt_change_node);
  }
}

/**
 * process the kernel delete list.
 * the routes are already ordered such that nexthop routes
 * are on the head of the queue.
 * non-nexthop routes need to be deleted first and therefore
 * the queue needs to be traversed from tail to head.
 */
static void
olsr_del_kernel_routes(struct list_node *head_node)
{
  struct rt_entry *rt;

  while (!list_is_empty(head_node)) {
    rt = changelist2rt(head_node->prev);

    /*
     * Only attempt to delete the route from kernel if it was
     * installed previously. A reference to the interface gets
     * set only when a route installation suceeds.
     */
    if (rt->rt_nexthop.interface) {
      olsr_del_route(rt);
    }

    list_remove(&rt->rt_change_node);
    olsr_cookie_free(rt_mem_cookie, rt);
  }
}

/**
 * Check the version number of all route paths hanging off a route entry.
 * If a route does not match the current routing tree number, remove it
 * from the global originator tree for that rt_entry.
 * Reset the best route pointer.
 */
static void
olsr_delete_outdated_routes(struct rt_entry *rt)
{
  struct rt_path *rtp;
  struct avl_node *rtp_tree_node, *next_rtp_tree_node;

  for (rtp_tree_node = avl_walk_first(&rt->rt_path_tree); rtp_tree_node != NULL; rtp_tree_node = next_rtp_tree_node) {

    /*
     * pre-fetch the next node before loosing context.
     */
    next_rtp_tree_node = avl_walk_next(rtp_tree_node);

    rtp = rtp_tree2rtp(rtp_tree_node);

    /*
     * check the version number which gets incremented on every SPF run.
     * comparing for unequalness avoids handling version number wraps.
     */
    if (routingtree_version != rtp->rtp_version) {

      /* remove from the originator tree */
      avl_delete(&rt->rt_path_tree, rtp_tree_node);
      rtp->rtp_rt = NULL;
    }
  }

  /* safety measure against dangling pointers */
  rt->rt_best = NULL;
}

/**
 * Walk all the routes, remove outdated routes and run
 * best path selection on the remaining set.
 * Finally compare the nexthop of the route head and the best
 * path and enqueue an add/chg operation.
 */
void
olsr_update_rib_routes(void)
{
  struct rt_entry *rt;

  OLSR_DEBUG(LOG_ROUTING, "Updating kernel routes...\n");

  /* walk all routes in the RIB. */

  OLSR_FOR_ALL_RT_ENTRIES(rt) {

    /* eliminate first unused routes */
    olsr_delete_outdated_routes(rt);

    if (!rt->rt_path_tree.count) {

      /* oops, all routes are gone - flush the route head */
      avl_delete(&routingtree, rt_tree_node);

      olsr_enqueue_rt(&del_kernel_list, rt);
      continue;
    }

    /* run best route election */
    olsr_rt_best(rt);

    /* nexthop or hopcount change ? */
    if (olsr_nh_change(&rt->rt_best->rtp_nexthop, &rt->rt_nexthop) ||
        (FIBM_CORRECT == olsr_cnf->fib_metric && olsr_hopcount_change(&rt->rt_best->rtp_metric, &rt->rt_metric))) {

      if (!rt->rt_nexthop.interface) {

        /* fresh routes do not have an interface pointer */
        olsr_enqueue_rt(&add_kernel_list, rt);
      } else {

        /* this is a route change. */
        olsr_enqueue_rt(&chg_kernel_list, rt);
      }
    }
  }
  OLSR_FOR_ALL_RT_ENTRIES_END(rt);
}

/**
 * Propagate the accumulated changes from the last rib update to the kernel.
 */
void
olsr_update_kernel_routes(void)
{

  /* delete unreachable routes */
  olsr_del_kernel_routes(&del_kernel_list);

  /* route changes */
  olsr_chg_kernel_routes(&chg_kernel_list);

  /* route additions */
  olsr_add_routes(&add_kernel_list);
#ifdef DEBUG
  olsr_print_routing_table(&routingtree);
#endif
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */


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
#include "olsr_logging.h"
#include "os_kernel_routes.h"

#include <errno.h>

static struct list_entity chg_kernel_list;

/*
 * Function hooks for plugins to intercept
 * adding / deleting routes from the kernel
 */
export_route_function olsr_add_route_function;
export_route_function olsr_del_route_function;

#define MAX_FAILURE_COUNT 10000 //should be FAILURE_LESS_NOISE_COUNT * (int)x
#define FAILURE_LESS_NOISE_COUNT 100 //after x errors only every x errors this is written to log

void
olsr_init_export_route(void)
{
  OLSR_INFO(LOG_ROUTING, "Initialize route processing...\n");

  /* the add/chg and del kernel queues */
  list_init_head(&chg_kernel_list);

  olsr_add_route_function = os_route_add_rtentry;
  olsr_del_route_function = os_route_del_rtentry;
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
 * Enqueue a route on a kernel chg/del queue.
 */
static void
olsr_enqueue_rt(struct list_entity *head_node, struct rt_entry *rt)
{
  const struct rt_nexthop *nh;

  /* if this node is already on some changelist we are done */
  if (list_node_added(&rt->rt_change_node)) {
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
 * Process a route deletion
 *
 *@return actual error count
 */
static int
olsr_del_route(struct rt_entry *rt)
{
  int16_t error;
  if (rt->rt_nexthop.interface == NULL) return 0;

  error = olsr_del_route_function(rt, olsr_cnf->ip_version);

  if (error != 0) {
    if (rt->failure_count>0) {
      /*ignore if we failed to delete a route we never successfully created*/
      OLSR_WARN(LOG_ROUTING, "KERN: SUCCESFULLY failed to delete unexisting %s: %s\n", olsr_rt_to_string(rt), strerror(errno));
      rt->failure_count=0;
    } else {
     rt->failure_count--;

     /*rate limit error messages*/
     if ( (rt->failure_count >= -FAILURE_LESS_NOISE_COUNT ) || (rt->failure_count % FAILURE_LESS_NOISE_COUNT == 0) )
       OLSR_ERROR(LOG_ROUTING, "KERN: ERROR on %d attempt to delete %s: %s\n", rt->failure_count*(-1), olsr_rt_to_string(rt), strerror(errno));

     /*stop trying it*/
     if (rt->failure_count <= -MAX_FAILURE_COUNT)  {
       OLSR_ERROR(LOG_ROUTING, " WILL NOT TRY AGAIN!!\n==============\n");
       rt->failure_count=0;
     }
    }

  } else {
    if (rt->failure_count > 1)
      OLSR_WARN(LOG_ROUTING, "KERN: SUCCESS on %d attempt to delete %s: %s\n", rt->failure_count*(-1), olsr_rt_to_string(rt), strerror(errno));

    rt->failure_count=0;
    /* release the interface. */
    unlock_interface(rt->rt_nexthop.interface);
  }

  return rt->failure_count;
}

/**
 * Process a route from the kernel addition list.
 *
 *@return nada
 */
static void
olsr_add_route(struct rt_entry *rt)
{
  rt->failure_count++;

  if (0 != olsr_add_route_function(rt, olsr_cnf->ip_version)) {
    /*rate limit error messages*/
    if ( (rt->failure_count <= FAILURE_LESS_NOISE_COUNT ) || (rt->failure_count % FAILURE_LESS_NOISE_COUNT == 0) )
      OLSR_ERROR(LOG_ROUTING, "KERN: ERROR on %d attempt to add %s: %s\n", rt->failure_count, olsr_rtp_to_string(rt->rt_best), strerror(errno));

    /*stop trying it*/
    if (rt->failure_count >= MAX_FAILURE_COUNT)  {
       OLSR_ERROR(LOG_ROUTING, " WILL NOT TRY AGAIN!!\n==============\n");
       rt->failure_count=0;
     }
  } else {
    /* route addition has suceeded */

    /* save the nexthop and metric in the route entry */
    rt->rt_nexthop = rt->rt_best->rtp_nexthop;
    rt->rt_metric = rt->rt_best->rtp_metric;

    /* lock the interface such that it does not vanish underneath us */
    lock_interface(rt->rt_nexthop.interface);

    /*reset failure_counter and print info if we needed more than once*/
    if (rt->failure_count > 1)
      OLSR_WARN(LOG_ROUTING, "KERN: SUCCESS on %d attmpt to add %s: %s\n", rt->failure_count, olsr_rtp_to_string(rt->rt_best), strerror(errno));
    rt->failure_count=0;
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
olsr_chg_kernel_routes(struct list_entity *head_node)
{
  struct rt_entry *rt, *iterator;

  if (list_is_empty(head_node)) {
    return;
  }

  /*
   * Traverse from the beginning to the end of the list,
   * such that nexthop routes are added first.
   */
  OLSR_FOR_ALL_RTLIST_ENTRIES(head_node, rt, iterator) {

    /*if netlink and NLFM_REPLACE is available (ipv4 only?) and applicable (fib_metric FLAT only) and used in linux/kernel_*.c no delete would be necesarry here */
    if (rt->rt_nexthop.interface) olsr_del_route(rt); // fresh routes do not have an interface pointer

    olsr_add_route(rt);

    list_remove(&rt->rt_change_node);
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
  struct rt_path *rtp, *iterator;

  OLSR_FOR_ALL_RT_PATH_ENTRIES(rt, rtp, iterator) {
    /*
     * check the version number which gets incremented on every SPF run.
     * comparing for unequalness avoids handling version number wraps.
     */
    if (routingtree_version != rtp->rtp_version) {

      /* remove from the originator tree */
      avl_delete(&rt->rt_path_tree, &rtp->rtp_tree_node);
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
  struct rt_entry *rt, *iterator;

  OLSR_DEBUG(LOG_ROUTING, "Updating kernel routes...\n");

  /* walk all routes in the RIB. */

  OLSR_FOR_ALL_RT_ENTRIES(rt, iterator) {

    /* eliminate first unused routes */
    olsr_delete_outdated_routes(rt);

    if (!rt->rt_path_tree.count) {
      /* oops, all routes are gone - flush the route head */
      if (olsr_del_route(rt) == 0) avl_delete(&routingtree, &rt->rt_tree_node);

      continue;
    }

    /* run best route election */
    olsr_rt_best(rt);

    /* nexthop or hopcount change ? */
    if (olsr_nh_change(&rt->rt_best->rtp_nexthop, &rt->rt_nexthop) ||
        (FIBM_CORRECT == olsr_cnf->fib_metric && olsr_hopcount_change(&rt->rt_best->rtp_metric, &rt->rt_metric))) {

      olsr_enqueue_rt(&chg_kernel_list, rt);
     
    }
  }
}

/**
 * Propagate the accumulated changes from the last rib update to the kernel.
 */
void
olsr_update_kernel_routes(void)
{

  /* route changes and additions */
  olsr_chg_kernel_routes(&chg_kernel_list);

#ifdef DEBUG
  olsr_print_routing_table();
#endif
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

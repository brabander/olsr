/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004, Thomas Lopatic (thomas@lopatic.de)
 * IPv4 performance optimization (c) 2006, sven-ola(gmx.de)
 * SPF implementation (c) 2007, Hannes Gredler (hannes@gredler.at)
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
 * $Id: lq_route.c,v 1.50 2007/09/05 16:17:36 bernd67 Exp $
 */

#include "defs.h"
#include "olsr.h"
#include "tc_set.h"
#include "neighbor_table.h"
#include "two_hop_neighbor_table.h"
#include "link_set.h"
#include "routing_table.h"
#include "mid_set.h"
#include "hna_set.h"
#include "lq_list.h"
#include "lq_avl.h"
#include "lq_route.h"

struct olsr_spf_edge
{
  struct avl_node tree_node;
  struct olsr_spf_vertex *dest;
  float etx;
};

struct olsr_spf_vertex
{
  struct avl_node tree_node; /* node keyed by ip address */
  struct avl_node cand_tree_node; /* node keyed by etx */
  struct list_node path_list_node; /* SPF result list */
  union olsr_ip_addr addr;
  struct avl_tree edge_tree;
  struct link_entry *next_hop; /* the link to the 1st hop neighbor */
  float path_etx;
  olsr_u8_t hops;
};

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
avl_comp_etx (void *etx1, void *etx2)
{       
  if (*(float *)etx1 < *(float *)etx2) {
    return -1;
  }

  if (*(float *)etx1 > *(float *)etx2) {
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
olsr_spf_add_cand_tree (struct avl_tree *tree,
                        struct olsr_spf_vertex *vert)
{
  vert->cand_tree_node.key = &vert->path_etx;
  vert->cand_tree_node.data = vert;

#ifdef DEBUG
  OLSR_PRINTF(1, "SPF: insert candidate %s, cost %f\n",
              olsr_ip_to_string(&(vert->addr)),
              vert->path_etx);
#endif

  avl_insert(tree, &vert->cand_tree_node, 1);
}

/*
 * olsr_spf_del_cand_tree
 *
 * Unkey an existing vertex from a candidate tree.
 */
static void
olsr_spf_del_cand_tree (struct avl_tree *tree,
                        struct olsr_spf_vertex *vert)
{

#ifdef DEBUG
  OLSR_PRINTF(1, "SPF: delete candidate %s, cost %f\n",
              olsr_ip_to_string(&(vert->addr)),
              vert->path_etx);
#endif

  avl_delete(tree, &vert->cand_tree_node);
}

/*
 * olsr_spf_add_path_list
 *
 * Insert an SPF result at the end of the path list.
 */
static void
olsr_spf_add_path_list (struct list_node *head,
                        int *path_count,
                        struct olsr_spf_vertex *vert)
{
  vert->path_list_node.data = vert;

#ifdef DEBUG
  OLSR_PRINTF(1, "SPF: append path %s, cost %f, via %s\n",
              olsr_ip_to_string(&(vert->addr)),
              vert->path_etx,
              olsr_ip_to_string(vert->next_hop ? &vert->next_hop->neighbor_iface_addr : NULL));
#endif

  list_add_before(head, &vert->path_list_node);
  *path_count = *path_count + 1;
}

/*
 * olsr_spf_add_vertex
 *
 * Add a node to the tree of all nodes in the network.
 */
static struct olsr_spf_vertex *
olsr_spf_add_vertex (struct avl_tree *vertex_tree,
                     union olsr_ip_addr *addr, float path_etx)
{
  struct avl_node *node;
  struct olsr_spf_vertex *vert;

  // see whether this destination is already in our tree

  node = avl_find(vertex_tree, addr);

  if (node) {
    return node->data;
  }

  // if it's not in our list, add it

  vert = olsr_malloc(sizeof (struct olsr_spf_vertex), "Dijkstra vertex");

  vert->tree_node.data = vert;
  vert->tree_node.key = &vert->addr;

  COPY_IP(&vert->addr, addr);
    
  vert->path_etx = path_etx;
  vert->next_hop = NULL;
  vert->hops = 0;

  avl_init(&vert->edge_tree, avl_comp_default);

  avl_insert(vertex_tree, &vert->tree_node, 0);
  return vert;
}

static struct olsr_spf_vertex *
olsr_spf_add_edge (struct avl_tree *vertex_tree,
                   union olsr_ip_addr *src, union olsr_ip_addr *dst,
                   float etx)
{
  struct avl_node *node;
  struct olsr_spf_vertex *svert;
  struct olsr_spf_vertex *dvert;
  struct olsr_spf_edge *edge;

  // source lookup

  node = avl_find(vertex_tree, src);

  // source vertex does not exist

  if (node == NULL)
    return NULL;

  svert = node->data;

  // destination lookup

  node = avl_find(vertex_tree, dst);

  // destination vertex does not exist

  if (node == NULL)
    return NULL;

  dvert = node->data;

  // check for existing forward edge

  if (avl_find(&svert->edge_tree, dst) == NULL)
  {
    // add forward edge

    edge = olsr_malloc(sizeof (struct olsr_spf_vertex), "Dijkstra forward edge");

    edge->tree_node.data = edge;
    edge->tree_node.key = &dvert->addr;

    edge->dest = dvert;
    edge->etx = etx;

    avl_insert(&svert->edge_tree, &edge->tree_node, 0);
  }

  // check for existing inverse edge

  if (avl_find(&dvert->edge_tree, src) == NULL)
  {
    // add inverse edge

    edge = olsr_malloc(sizeof (struct olsr_spf_vertex), "Dijkstra inverse edge");

    edge->tree_node.data = edge;
    edge->tree_node.key = &svert->addr;

    edge->dest = svert;
    edge->etx = etx;

    avl_insert(&dvert->edge_tree, &edge->tree_node, 0);
  }

  return svert;
}

static void olsr_free_everything(struct avl_tree *vertex_tree)
{
  struct avl_node *vert_node;
  struct avl_node *edge_node;
  struct olsr_spf_vertex *vert;
  struct olsr_spf_edge *edge;

  vert_node = avl_walk_first(vertex_tree);

  // loop through all vertices

  while (vert_node)
  {
    vert = vert_node->data;
    edge_node = avl_walk_first(&vert->edge_tree);

    // loop through all edges of the current vertex

    while (edge_node != NULL)
    {
      edge = edge_node->data;
      edge_node = avl_walk_next(edge_node);
      free(edge);
    }

    vert_node = avl_walk_next(vert_node);
    free(vert);
  }
}

/*
 * olsr_spf_extract_best
 *
 * return the node with the minimum pathcost.
 */
static struct olsr_spf_vertex *
olsr_spf_extract_best (struct avl_tree *tree)
{
  struct avl_node *node;

  node = avl_walk_first(tree);

  return (node ? node->data :  NULL);
}


#ifdef DEBUG
static char *olsr_etx_to_string(float etx)
{
  static char buff[20];

  if (etx == INFINITE_ETX)
    return "INF";

  snprintf(buff, 20, "%.6f", etx);
  return buff;
}
#endif


/*
 * olsr_spf_relax
 *
 * Explore all edges of a node and add the node
 * to the candidate tree if the if the aggregate
 * path cost is better.
 */
static void
olsr_spf_relax (struct avl_tree *cand_tree, struct olsr_spf_vertex *vert)
{
  struct olsr_spf_edge *edge;
  struct avl_node *edge_node;
  float new_etx;

#ifdef DEBUG
  OLSR_PRINTF(1, "SPF: exploring node %s, cost %f\n",
              olsr_ip_to_string(&(vert->addr)),
              vert->path_etx);
#endif

  edge_node = avl_walk_first(&vert->edge_tree);

  // loop through all edges of this vertex

  while (edge_node != NULL)
  {
    edge = edge_node->data;

    // total quality of the path through this vertex to the
    // destination of this edge

    new_etx = vert->path_etx + edge->etx;

    // if it's better than the current path quality of this
    // edge's destination, then we've found a better path to
    // this destination

    if (new_etx < edge->dest->path_etx)
    {
      /* if this node has been on the candidate tree delete it */
      if (edge->dest->path_etx != INFINITE_ETX) {
        olsr_spf_del_cand_tree(cand_tree, edge->dest);
      }

      /* re-insert on candidate tree with the better metric */
      edge->dest->path_etx = new_etx;
      olsr_spf_add_cand_tree(cand_tree, edge->dest);

      /* pull-up the next-hop and bump the hop count */
      if (vert->next_hop) {
        edge->dest->next_hop = vert->next_hop;
      }
      edge->dest->hops = vert->hops + 1;

#ifdef DEBUG
      OLSR_PRINTF(1, "SPF:   better path to %s, cost %s -> %s, via %s, hops %u\n",
                  olsr_ip_to_string(&(edge->dest->addr)),
                  olsr_etx_to_string(edge->dest->path_etx),
                  olsr_etx_to_string(new_etx),
                  olsr_ip_to_string(vert->next_hop ?
                                    &vert->next_hop->neighbor_iface_addr : NULL),
                  edge->dest->hops);
#endif

    }

    edge_node = avl_walk_next(edge_node);
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
olsr_spf_run_full (struct avl_tree *cand_tree, struct list_node *path_list,
                   int *path_count)
{
  struct olsr_spf_vertex *vert;

  *path_count = 0;

  while ((vert = olsr_spf_extract_best(cand_tree))) {

    olsr_spf_relax(cand_tree, vert);

    /*
     * move the best path from the candidate tree
     * to the path list.
     */
    olsr_spf_del_cand_tree(cand_tree, vert);
    olsr_spf_add_path_list(path_list, path_count, vert);
  }
}

void
olsr_calculate_routing_table (void)
{
  struct avl_tree vertex_tree, cand_tree;
  struct list_node path_list;
  int i, plen, max_host_plen, path_count = 0;
  struct tc_entry *tcsrc;
  struct topo_dst *tcdst;
  struct olsr_spf_vertex *vert, *myself;
  struct neighbor_entry *neigh;
  struct mid_address *mid_walker;
  float etx;
  struct hna_entry *hna_gw;
  struct hna_net *hna;
  struct neighbor_2_entry *neigh2;
  struct interface *inter;
  struct link_entry *link;

#ifdef SPF_PROFILING
  struct timeval t1, t2, t3, t4, t5, t6, spf_init, spf_run, route, kernel, cleanup, total;

  gettimeofday(&t1, NULL);
#endif

  max_host_plen = olsr_host_rt_maxplen();

  // initialize the graph

  avl_init(&vertex_tree, avl_comp_default);
  avl_init(&cand_tree, avl_comp_etx);
  list_head_init(&path_list);

  olsr_bump_routingtree_version();

  // add ourselves to the vertex and candidate tree

  myself = olsr_spf_add_vertex(&vertex_tree, &olsr_cnf->main_addr, ZERO_ETX);
  olsr_spf_add_cand_tree(&cand_tree, myself);

  /*
   * Add our neighbours.
   */
  for (i = 0; i < HASHSIZE; i++)
    for (neigh = neighbortable[i].next; neigh != &neighbortable[i];
         neigh = neigh->next) {

      if (neigh->status != SYM)
        continue;

      olsr_spf_add_vertex(&vertex_tree, &neigh->neighbor_main_addr, INFINITE_ETX);
    }

  // add our two-hop neighbours

  for (i = 0; i < HASHSIZE; i++)
    for (neigh2 = two_hop_neighbortable[i].next;
         neigh2 != &two_hop_neighbortable[i];
         neigh2 = neigh2->next) {

      olsr_spf_add_vertex(&vertex_tree, &neigh2->neighbor_2_addr, INFINITE_ETX);
    }
  // add remaining vertices

  for (i = 0; i < HASHSIZE; i++)
    for (tcsrc = tc_table[i].next; tcsrc != &tc_table[i]; tcsrc = tcsrc->next)
    {
      // add source

      olsr_spf_add_vertex(&vertex_tree, &tcsrc->T_last_addr, INFINITE_ETX);

      // add destinations of this source

      for (tcdst = tcsrc->destinations.next; tcdst != &tcsrc->destinations;
           tcdst = tcdst->next)
        olsr_spf_add_vertex(&vertex_tree, &tcdst->T_dest_addr, INFINITE_ETX);
    }

  // add edges to and from our neighbours

  for (i = 0; i < HASHSIZE; i++)
    for (neigh = neighbortable[i].next; neigh != &neighbortable[i];
         neigh = neigh->next) {

      if (neigh->status == SYM) {

        link = get_best_link_to_neighbor(&neigh->neighbor_main_addr);
	if (!link) {
	  continue;
        }

        if (olsr_cnf->lq_level < 2) {
          /* for non-lq the etx is always 1.0 */
          vert = olsr_spf_add_edge(&vertex_tree, &neigh->neighbor_main_addr,
                                   &olsr_cnf->main_addr, 1.0 );
          vert->next_hop = link;

        } else if (link->loss_link_quality2 >= MIN_LINK_QUALITY &&
                   link->neigh_link_quality2 >= MIN_LINK_QUALITY) {
            
          etx = 1.0 / (link->loss_link_quality2 * link->neigh_link_quality2);

          vert = olsr_spf_add_edge(&vertex_tree, &neigh->neighbor_main_addr,
                                     &olsr_cnf->main_addr, etx);
          vert->next_hop = link;
        }
      }
    }

  /* add remaining edges from TC messages */

  for (i = 0; i < HASHSIZE; i++)
    for (tcsrc = tc_table[i].next; tcsrc != &tc_table[i]; tcsrc = tcsrc->next)
      for (tcdst = tcsrc->destinations.next; tcdst != &tcsrc->destinations;
           tcdst = tcdst->next) {

        if (olsr_cnf->lq_level < 2) {

          /* for non-lq the etx is always 1.0 */
          olsr_spf_add_edge(&vertex_tree, &tcdst->T_dest_addr,
                            &tcsrc->T_last_addr, 1.0);

        } else if (tcdst->link_quality >= MIN_LINK_QUALITY &&
                   tcdst->inverse_link_quality >= MIN_LINK_QUALITY) {

          etx = 1.0 / (tcdst->link_quality * tcdst->inverse_link_quality);

          olsr_spf_add_edge(&vertex_tree, &tcdst->T_dest_addr,
                            &tcsrc->T_last_addr, etx);
        }
      }

#ifdef SPF_PROFILING
  gettimeofday(&t2, NULL);
#endif

  /*
   * Run the SPF calculation.
   */
  olsr_spf_run_full(&cand_tree, &path_list, &path_count);

  OLSR_PRINTF(2, "\n--- %02d:%02d:%02d.%02d ------------------------------------------------- DIJKSTRA\n\n",
              nowtm->tm_hour,
              nowtm->tm_min,
              nowtm->tm_sec,
              (int)now.tv_usec/10000);

#ifdef SPF_PROFILING
  gettimeofday(&t3, NULL);
#endif

  olsr_fill_routing_table_with_neighbors();

  /*
   * In the path tree we have all the reachable nodes in our topology.
   */
  for (; !list_is_empty(&path_list); list_remove(path_list.next)) {

    vert = path_list.next->data;
    link = vert->next_hop;

    if (!link) {
      OLSR_PRINTF(2, "%s no next-hop\n", olsr_ip_to_string(&vert->addr));
      continue;
    }

    /* find the interface for the found link */
    inter = link->if_name ? if_ifwithname(link->if_name)
      : if_ifwithaddr(&link->local_iface_addr);

    /* interface is up ? */
    if (inter) {

      /* add a route to the main address of the destination node */
      olsr_insert_routing_table(&vert->addr, max_host_plen, &vert->addr,
                                &link->neighbor_iface_addr, inter->if_index,
                                vert->hops, vert->path_etx);

      /* add routes to the remaining interfaces of the destination node */
      for (mid_walker = mid_lookup_aliases(&vert->addr);
           mid_walker != NULL;
           mid_walker = mid_walker->next_alias) {

        olsr_insert_routing_table(&mid_walker->alias, max_host_plen, &vert->addr,
                                  &link->neighbor_iface_addr, inter->if_index,
                                  vert->hops, vert->path_etx);
      }

      /* find the node's HNAs */
      hna_gw = olsr_lookup_hna_gw(&vert->addr);

      /* node doesn't announce any HNAs */
      if (!hna_gw) {
        continue;
      }

      /* loop through the node's HNAs */
      for (hna = hna_gw->networks.next;
           hna != &hna_gw->networks;
           hna = hna->next) {

        plen = olsr_get_hna_prefix_len(hna);
        if (vert->path_etx != INFINITE_ETX)
        olsr_insert_routing_table(&hna->A_network_addr, plen, &vert->addr,
                                  &link->neighbor_iface_addr, inter->if_index,
                                  vert->hops, vert->path_etx);
      }
    }
  }

#ifdef SPF_PROFILING
  gettimeofday(&t4, NULL);
#endif

  /* move the route changes into the kernel */

  olsr_update_kernel_routes();

#ifdef SPF_PROFILING
  gettimeofday(&t5, NULL);
#endif

  /* free the SPF graph */

  olsr_free_everything(&vertex_tree);

#ifdef SPF_PROFILING
  gettimeofday(&t6, NULL);

  timersub(&t2, &t1, &spf_init);
  timersub(&t3, &t2, &spf_run);
  timersub(&t4, &t3, &route);
  timersub(&t5, &t4, &kernel);
  timersub(&t6, &t5, &cleanup);
  timersub(&t6, &t1, &total);
  olsr_printf(1, "\n--- SPF-stats for %d nodes, %d routes (total/init/run/route/kern/cleanup): %d, %d, %d, %d, %d, %d\n",
              path_count, routingtree.count,
              (int)total.tv_usec, (int)spf_init.tv_usec, (int)spf_run.tv_usec,
              (int)route.tv_usec, (int)kernel.tv_usec,  (int)cleanup.tv_usec);
#endif
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * End:
 */

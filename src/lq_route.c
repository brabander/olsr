/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004, Thomas Lopatic (thomas@lopatic.de)
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
 * $Id: lq_route.c,v 1.17 2004/12/03 19:10:51 tlopatic Exp $
 */

#if defined USE_LINK_QUALITY
#include "defs.h"
#include "tc_set.h"
#include "neighbor_table.h"
#include "link_set.h"
#include "routing_table.h"
#include "mid_set.h"
#include "lq_list.h"
#include "lq_route.h"

struct dijk_edge
{
  struct list_node node;
  struct dijk_vertex *dest;
  float etx;
};

struct dijk_vertex
{
  struct list_node node;
  union olsr_ip_addr addr;
  struct list edge_list;
  float path_etx;
  struct dijk_vertex *prev;
  olsr_bool done;
};

// XXX - bad complexity

static void add_vertex(struct list *vertex_list, union olsr_ip_addr *addr,
                       float path_etx)
{
  struct list_node *node;
  struct dijk_vertex *vert;

  // see whether this destination is already in our list

  node = list_get_head(vertex_list);

  while (node != NULL)
  {
    vert = node->data;

    if (COMP_IP(&vert->addr, addr))
      break;

    node = list_get_next(node);
  }

  // if it's not in our list, add it

  if (node == NULL)
  {
    vert = olsr_malloc(sizeof (struct dijk_vertex), "Dijkstra vertex");

    vert->node.data = vert;

    COPY_IP(&vert->addr, addr);
    
    vert->path_etx = path_etx;
    vert->prev = NULL;
    vert->done = OLSR_FALSE;

    list_init(&vert->edge_list);

    list_add_tail(vertex_list, &vert->node);
  }
}

// XXX - bad complexity

static void add_edge(struct list *vertex_list, union olsr_ip_addr *src,
                     union olsr_ip_addr *dst, float etx)
{
  struct list_node *node;
  struct dijk_vertex *vert;
  struct dijk_vertex *svert;
  struct dijk_vertex *dvert;
  struct dijk_edge *edge;

  // source and destination lookup

  node = list_get_head(vertex_list);

  svert = NULL;
  dvert = NULL;

  while (node != NULL)
  {
    vert = node->data;

    if (COMP_IP(&vert->addr, src))
    {
      svert = vert;

      if (dvert != NULL)
        break;
    }

    else if (COMP_IP(&vert->addr, dst))
    {
      dvert = vert;

      if (svert != NULL)
        break;
    }

    node = list_get_next(node);
  }

  // source or destination vertex does not exist

  if (svert == NULL || dvert == NULL)
    return;

  node = list_get_head(&svert->edge_list);

  while (node != NULL)
  {
    edge = node->data;

    if (edge->dest == dvert)
      break;

    node = list_get_next(node);
  }

  // edge already exists

  if (node != NULL)
    return;

  edge = olsr_malloc(sizeof (struct dijk_vertex), "Dijkstra edge");

  edge->node.data = edge;

  edge->dest = dvert;
  edge->etx = etx;

  list_add_tail(&svert->edge_list, &edge->node);
}

static void free_everything(struct list *vertex_list)
{
  struct list_node *vnode, *enode;
  struct dijk_vertex *vert;
  struct dijk_edge *edge;

  vnode = list_get_head(vertex_list);

  // loop through all vertices

  while (vnode != NULL)
  {
    vert = vnode->data;

    enode = list_get_head(&vert->edge_list);

    // loop through all edges of the current vertex

    while (enode != NULL)
    {
      edge = enode->data;

      enode = list_get_next(enode);
      free(edge);
    }

    vnode = list_get_next(vnode);
    free(vert);
  }
}

// XXX - bad complexity

static struct dijk_vertex *extract_best(struct list *vertex_list)
{
  float best_etx = INFINITE_ETX + 1.0;
  struct list_node *node;
  struct dijk_vertex *vert;
  struct dijk_vertex *res = NULL;

  node = list_get_head(vertex_list);

  // loop through all vertices
  
  while (node != NULL)
  {
    vert = node->data;

    // see whether the current vertex is better than what we have

    if (!vert->done && vert->path_etx < best_etx)
    {
      best_etx = vert->path_etx;
      res = vert;
    }

    node = list_get_next(node);
  }

  // if we've found a vertex, remove it from the set

  if (res != NULL)
    res->done = OLSR_TRUE;

  return res;
}

static void relax(struct dijk_vertex *vert)
{
  struct dijk_edge *edge;
  float new_etx;
  struct list_node *node;

  node = list_get_head(&vert->edge_list);

  // loop through all edges of this vertex

  while (node != NULL)
  {
    edge = node->data;

    // total quality of the path through this vertex to the
    // destination of this edge

    new_etx = vert->path_etx + edge->etx;

    // if it's better than the current path quality of this
    // edge's destination, then we've found a better path to
    // this destination

    if (new_etx < edge->dest->path_etx)
    {
      edge->dest->path_etx = new_etx;
      edge->dest->prev = vert;
    }

    node = list_get_next(node);
  }
}

static char *etx_to_string(float etx)
{
  static char buff[20];

  if (etx == INFINITE_ETX)
    return "INF";

  sprintf(buff, "%.2f", etx);
  return buff;
}

void olsr_calculate_lq_routing_table(void)
{
  struct list vertex_list;
  int i;
  struct tc_entry *tcsrc;
  struct topo_dst *tcdst;
  struct dijk_vertex *vert;
  struct link_entry *link;
  struct neighbor_entry *neigh;
  struct list_node *node;
  struct dijk_vertex *myself;
  struct dijk_vertex *walker;
  int hops;
  struct addresses *mid_walker;
  float etx;

  // initialize the graph

  list_init(&vertex_list);

  // add ourselves to the vertex list

  add_vertex(&vertex_list, &main_addr, 0.0);

  // add our neighbours

  for (i = 0; i < HASHSIZE; i++)
    for (neigh = neighbortable[i].next; neigh != &neighbortable[i];
         neigh = neigh->next)
      if (neigh->status == SYM)
        add_vertex(&vertex_list, &neigh->neighbor_main_addr, INFINITE_ETX);

  // add remaining vertices

  for (i = 0; i < HASHSIZE; i++)
    for (tcsrc = tc_table[i].next; tcsrc != &tc_table[i]; tcsrc = tcsrc->next)
    {
      // add source

      add_vertex(&vertex_list, &tcsrc->T_last_addr, INFINITE_ETX);

      // add destinations of this source

      for (tcdst = tcsrc->destinations.next; tcdst != &tcsrc->destinations;
           tcdst = tcdst->next)
        add_vertex(&vertex_list, &tcdst->T_dest_addr, INFINITE_ETX);
    }

  // add edges to and from our neighbours

  for (i = 0; i < HASHSIZE; i++)
    for (neigh = neighbortable[i].next; neigh != &neighbortable[i];
         neigh = neigh->next)
      if (neigh->status == SYM)
      {
        link = olsr_neighbor_best_link(&neigh->neighbor_main_addr);

        if (link->loss_link_quality >= MIN_LINK_QUALITY &&
            link->neigh_link_quality >= MIN_LINK_QUALITY)
          {
            etx = 1.0 / (link->loss_link_quality * link->neigh_link_quality);

            add_edge(&vertex_list, &main_addr, &neigh->neighbor_main_addr,
                     etx);

            add_edge(&vertex_list, &neigh->neighbor_main_addr, &main_addr,
                     etx);
          }
      }

  // add remaining edges

  for (i = 0; i < HASHSIZE; i++)
    for (tcsrc = tc_table[i].next; tcsrc != &tc_table[i]; tcsrc = tcsrc->next)
      for (tcdst = tcsrc->destinations.next; tcdst != &tcsrc->destinations;
           tcdst = tcdst->next)
      {
        if (tcdst->link_quality >= MIN_LINK_QUALITY &&
            tcdst->inverse_link_quality >= MIN_LINK_QUALITY)
          {
            etx = 1.0 / (tcdst->link_quality * tcdst->inverse_link_quality);

            add_edge(&vertex_list, &tcsrc->T_last_addr, &tcdst->T_dest_addr,
                     etx);

            add_edge(&vertex_list, &tcdst->T_dest_addr, &tcsrc->T_last_addr,
                     etx);
          }
      }

  // run Dijkstra's algorithm

  for (;;)
  {
    vert = extract_best(&vertex_list);

    if (vert == NULL)
      break;

    relax(vert);
  }

  // save the old routing table

  olsr_move_route_table(routingtable, old_routes);

  node = list_get_head(&vertex_list);

  // we're the first vertex in the list
  
  myself = node->data;

  olsr_printf(2, "\n--- %02d:%02d:%02d.%02d ------------------------------------------------- DIJKSTRA\n\n",
              nowtm->tm_hour,
              nowtm->tm_min,
              nowtm->tm_sec,
              now.tv_usec/10000);

  for (node = list_get_next(node); node != NULL; node = list_get_next(node))
  {
    vert = node->data;

    hops = 1;

    // count hops to until the path ends or until we have reached a
    // one-hop neighbour

    for (walker = vert; walker != NULL && walker->prev != myself;
         walker = walker->prev)
    {
      olsr_printf(2, "%s:%s <- ", olsr_ip_to_string(&walker->addr),
                  etx_to_string(walker->path_etx));
      hops++;
    }

    // if no path to a one-hop neighbour was found, ignore this node

    if (walker != NULL)
      olsr_printf(2, "%s:%s (one-hop)\n", olsr_ip_to_string(&walker->addr),
                  etx_to_string(walker->path_etx));

    else
    {
      olsr_printf(2, "FAILED\n", olsr_ip_to_string(&vert->addr));
      continue;
    }

#if defined linux && 0
    /*
     * on Linux we can add a new route for a destination before removing
     * the old route, so frequent route updates are not a problem, as
     * we never have a time window in which there isn't any route; hence
     * we can use the more volatile ETX value instead of the hop count
     */

    hops = (int)vert->path_etx;

    if (hops > 100)
      hops = 100;
#endif

    // add a route to the main address of the destination node

    olsr_insert_routing_table(&vert->addr, &walker->addr, hops);

    // add routes to the remaining interfaces of the destination node

    for (mid_walker = mid_lookup_aliases(&vert->addr); mid_walker != NULL;
         mid_walker = mid_walker->next)
      olsr_insert_routing_table(&mid_walker->address, &walker->addr, hops);
  }

  // free the graph

  free_everything(&vertex_list);

  // move the route changes into the kernel

  olsr_update_kernel_routes();

  // free the saved routing table

  olsr_free_routing_table(old_routes);
}
#endif

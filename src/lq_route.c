/* 
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2004 Thomas Lopatic (thomas@lopatic.de)
 *
 * This file is part of the olsr.org OLSR daemon.
 *
 * olsr.org is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * olsr.org is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with olsr.org; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: lq_route.c,v 1.1 2004/11/07 17:51:20 tlopatic Exp $
 *
 */

#if defined USE_LINK_QUALITY
#include "defs.h"
#include "tc_set.h"
#include "neighbor_table.h"
#include "link_set.h"
#include "routing_table.h"
#include "lq_list.h"
#include "lq_route.h"

#define olsr_malloc(x, y) malloc(x)

struct dijk_edge
{
  struct list_node node;
  struct dijk_vertex *dest;
  double link_quality;
};

struct dijk_vertex
{
  struct list_node node;
  union olsr_ip_addr addr;
  struct list edge_list;
  double path_quality;
  struct dijk_vertex *prev;
  olsr_bool done;
};

// XXX - bad complexity

static void add_vertex(struct list *vertex_list, union olsr_ip_addr *addr,
                       double path_quality)
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
    
    vert->path_quality = path_quality;
    vert->prev = NULL;
    vert->done = OLSR_FALSE;

    list_init(&vert->edge_list);

    list_add_tail(vertex_list, &vert->node);
  }
}

// XXX - bad complexity

static void add_edge(struct list *vertex_list, union olsr_ip_addr *src,
                     union olsr_ip_addr *dst, double link_quality)
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
  edge->link_quality = link_quality;

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
  double best = 0.0;
  struct list_node *node;
  struct dijk_vertex *vert;
  struct dijk_vertex *res = NULL;

  node = list_get_head(vertex_list);

  // loop through all vertices
  
  while (node != NULL)
  {
    vert = node->data;

    // see whether the current vertex is better than what we have

    if (!vert->done && vert->path_quality >= best)
    {
      best = vert->path_quality;
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
  double new_quality;
  struct list_node *node;

  node = list_get_head(&vert->edge_list);

  // loop through all edges of this vertex

  while (node != NULL)
  {
    edge = node->data;

    // total quality of the path through this vertex to the
    // destination of this edge

    new_quality = vert->path_quality * edge->link_quality;

    // if it's better than the current path quality of this
    // edge's destination, then we've found a better path to
    // this destination

    if (new_quality > edge->dest->path_quality)
    {
      edge->dest->path_quality = new_quality;
      edge->dest->prev = vert;
    }

    node = list_get_next(node);
  }
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

  // initialize the graph

  list_init(&vertex_list);

  // add ourselves to the vertex list

  add_vertex(&vertex_list, &main_addr, 1.0);

  // add remaining vertices

  for (i = 0; i < HASHSIZE; i++)
    for (tcsrc = tc_table[i].next; tcsrc != &tc_table[i]; tcsrc = tcsrc->next)
    {
      // add source

      add_vertex(&vertex_list, &tcsrc->T_last_addr, 0.0);

      // add destinations of this source

      for (tcdst = tcsrc->destinations.next; tcdst != &tcsrc->destinations;
           tcdst = tcdst->next)
        add_vertex(&vertex_list, &tcdst->T_dest_addr, 0.0);
    }

  // add edges to and from our neighbours

  for (i = 0; i < HASHSIZE; i++)
    for (neigh = neighbortable[i].next; neigh != &neighbortable[i];
         neigh = neigh->next)
      if (neigh->status == SYM)
      {
        link = olsr_neighbor_best_link(&neigh->neighbor_main_addr);

        add_edge(&vertex_list, &main_addr, &neigh->neighbor_main_addr,
                 link->neigh_link_quality);

        link = olsr_neighbor_best_inverse_link(&neigh->neighbor_main_addr);

        add_edge(&vertex_list, &main_addr, &neigh->neighbor_main_addr,
                 link->loss_link_quality);
      }

  // add remaining edges

  for (i = 0; i < HASHSIZE; i++)
    for (tcsrc = tc_table[i].next; tcsrc != &tc_table[i]; tcsrc = tcsrc->next)
      for (tcdst = tcsrc->destinations.next; tcdst != &tcsrc->destinations;
           tcdst = tcdst->next)
      {
        add_edge(&vertex_list, &tcsrc->T_last_addr, &tcdst->T_dest_addr,
                 tcdst->link_quality);

        add_edge(&vertex_list, &tcdst->T_dest_addr, &tcsrc->T_last_addr,
                 tcdst->inverse_link_quality);
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

  for (i = 0; i < 2; i++)
  {
    node = list_get_head(&vertex_list);

    // we're the first vertex in the list

    myself = node->data;

    node = list_get_next(node);

    // loop through the remaining vertices

    while (node != NULL)
    {
      vert = node->data;

      // one-hop neighbours go first

      if (i == 0 && vert->prev == myself)
        olsr_insert_routing_table(&vert->addr, &vert->addr, 1);

      // add everybody else in the second pass

      if (i == 1)
      {
        hops = 1;
        walker = vert;

        // count hops to until a one-hop neighbour is reached

        while (walker->prev != myself)
        {
          hops++;
          walker = walker->prev;
        }

        // add, if this is not a one-hop neighbour

        if (hops > 1)
          olsr_insert_routing_table(&vert->addr, &walker->addr, hops);
      }
    }
  }

  // free the graph

  free_everything(&vertex_list);

  // move the route changes into the kernel

  olsr_update_kernel_routes();

  // free the saved routing table

  olsr_free_routing_table(old_routes);
}
#endif


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


#ifndef _OLSR_TWO_HOP_TABLE
#define _OLSR_TWO_HOP_TABLE

#include "defs.h"
#include "lq_plugin.h"

#define	NB2S_COVERED 	0x1     /* node has been covered by a MPR */


struct nbr_list_entry {
  struct avl_node nbr_list_node;
  struct nbr_entry *neighbor;          /* backpointer to owning nbr entry */
  olsr_linkcost second_hop_linkcost;
  olsr_linkcost path_linkcost;
  olsr_linkcost saved_path_linkcost;
};

AVLNODE2STRUCT(nbr_list_node_to_nbr_list, struct nbr_list_entry, nbr_list_node);

struct nbr2_entry {
  struct avl_node nbr2_node;
  union olsr_ip_addr nbr2_addr;
  unsigned int mpr_covered_count;      /* Used in mpr calculation */
  unsigned int processed:1;            /* Used in mpr calculation */
  unsigned int nbr2_refcount;          /* Reference counter */
  struct avl_tree nbr2_nbr_list_tree;  /* subtree for nbr pointers */
} __attribute__ ((packed));;

AVLNODE2STRUCT(nbr2_node_to_nbr2, struct nbr2_entry, nbr2_node);

/*
 * macros for traversing two-hop neighbors and neighbor ref lists.
 * it is recommended to use this because it hides all the internal
 * datastructure from the callers.
 *
 * the loop prefetches the next node in order to not loose context if
 * for example the caller wants to delete the current entry.
 */
#define OLSR_FOR_ALL_NBR2_ENTRIES(nbr2) \
{ \
  struct avl_node *nbr2_tree_node, *next_nbr2_tree_node; \
  for (nbr2_tree_node = avl_walk_first(&nbr2_tree); \
    nbr2_tree_node; nbr2_tree_node = next_nbr2_tree_node) { \
    next_nbr2_tree_node = avl_walk_next(nbr2_tree_node); \
    nbr2 = nbr2_node_to_nbr2(nbr2_tree_node);
#define OLSR_FOR_ALL_NBR2_ENTRIES_END(nbr2) }}

#define OLSR_FOR_ALL_NBR_LIST_ENTRIES(nbr2, nbr_list) \
{ \
  struct avl_node *nbr_list_node, *next_nbr_list_node; \
  for (nbr_list_node = avl_walk_first(&nbr2->nbr2_nbr_list_tree); \
    nbr_list_node; nbr_list_node = next_nbr_list_node) { \
    next_nbr_list_node = avl_walk_next(nbr_list_node); \
    nbr_list = nbr_list_node_to_nbr_list(nbr_list_node);
#define OLSR_FOR_ALL_NBR_LIST_ENTRIES_END(nbr2, nbr_list) }}

/*
 * The two hop neighbor tree
 */
extern struct avl_tree EXPORT(nbr2_tree);

void olsr_init_two_hop_table(void);
void olsr_lock_nbr2(struct nbr2_entry *);
void olsr_unlock_nbr2(struct nbr2_entry *);
void olsr_delete_nbr_list_by_addr(struct nbr2_entry *, const union olsr_ip_addr *);
void olsr_delete_nbr2_entry(struct nbr2_entry *);
struct nbr2_entry *olsr_add_nbr2_entry(const union olsr_ip_addr *);
struct nbr2_entry *olsr_lookup_two_hop_neighbor_table(const union olsr_ip_addr *);
struct nbr2_entry *olsr_lookup_nbr2_entry_alias(const union olsr_ip_addr *);
void olsr_link_nbr_nbr2(struct nbr_entry *, struct nbr2_entry *, float);
void olsr_print_two_hop_neighbor_table(void);

#endif

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

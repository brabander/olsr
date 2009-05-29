
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


#ifndef _OLSR_NEIGH_TBL
#define _OLSR_NEIGH_TBL

#include "defs.h"
#include "olsr_types.h"
#include "common/avl.h"

/*
 * This is a neighbor2 list entry.
 * It is used to describe a set of references to two-hop neighbors.
 * This AVL tree node is hanging off an nbr_entry.
 */
struct nbr2_list_entry {
  struct avl_node nbr2_list_node;
  struct nbr_entry *nbr2_nbr;          /* backpointer to owning nbr entry */
  struct nbr2_entry *nbr2;
  struct timer_entry *nbr2_list_timer;
};

AVLNODE2STRUCT(nbr2_list_node_to_nbr2_list, struct nbr2_list_entry, nbr2_list_node);

#define OLSR_NBR2_LIST_JITTER 5 /* percent */

struct nbr_entry {
  struct avl_node nbr_node;            /* nbr keyed by ip address */
  union olsr_ip_addr nbr_addr;
  unsigned int status:3;
  unsigned int willingness:3;
  unsigned int is_mpr:1;
  unsigned int was_mpr:1;              /* Used to detect changes in MPR */
  unsigned int skip:1;
  int nbr2_nocov;
  unsigned int linkcount;
  struct avl_tree nbr2_list_tree;      /* subtree for nbr2 pointers */
} __attribute__ ((packed));

AVLNODE2STRUCT(nbr_node_to_nbr, struct nbr_entry, nbr_node);

/*
 * macros for traversing neighbors and neighbor2 ref lists.
 * it is recommended to use this because it hides all the internal
 * datastructure from the callers.
 *
 * the loop prefetches the next node in order to not loose context if
 * for example the caller wants to delete the current entry.
 */
#define OLSR_FOR_ALL_NBR_ENTRIES(nbr) \
{ \
  struct avl_node *nbr_tree_node, *next_nbr_tree_node; \
  for (nbr_tree_node = avl_walk_first(&nbr_tree); \
    nbr_tree_node; nbr_tree_node = next_nbr_tree_node) { \
    next_nbr_tree_node = avl_walk_next(nbr_tree_node); \
    nbr = nbr_node_to_nbr(nbr_tree_node);
#define OLSR_FOR_ALL_NBR_ENTRIES_END(nbr) }}

#define OLSR_FOR_ALL_NBR2_LIST_ENTRIES(nbr, nbr2_list) \
{ \
  struct avl_node *nbr2_list_node, *next_nbr2_list_node; \
  for (nbr2_list_node = avl_walk_first(&nbr->nbr2_list_tree); \
    nbr2_list_node; nbr2_list_node = next_nbr2_list_node) { \
    next_nbr2_list_node = avl_walk_next(nbr2_list_node); \
    nbr2_list = nbr2_list_node_to_nbr2_list(nbr2_list_node);
#define OLSR_FOR_ALL_NBR2_LIST_ENTRIES_END(nbr, nbr2_list) }}

/*
 * The one hop neighbor tree
 */
extern struct avl_tree EXPORT(nbr_tree);
extern struct olsr_cookie_info *nbr2_list_timer_cookie;

void olsr_init_neighbor_table(void);
struct nbr2_list_entry *olsr_lookup_nbr2_list_entry(struct nbr_entry *, const union olsr_ip_addr *);
struct nbr2_list_entry *olsr_add_nbr2_list_entry(struct nbr_entry *, struct nbr2_entry *, float);
void olsr_delete_nbr2_list_entry(struct nbr2_list_entry *);
bool olsr_delete_nbr_entry(const union olsr_ip_addr *);
void olsr_link_nbr_nbr2(struct nbr_entry *, struct nbr2_entry *, float);
struct nbr_entry *olsr_add_nbr_entry(const union olsr_ip_addr *);
struct nbr_entry *olsr_lookup_nbr_entry(const union olsr_ip_addr *);
struct nbr_entry *olsr_lookup_nbr_entry_alias(const union olsr_ip_addr *);
void olsr_time_out_two_hop_neighbors(struct nbr_entry *);
void olsr_expire_nbr2_list(void *);
void olsr_print_neighbor_table(void);
int olsr_update_nbr_status(struct nbr_entry *, int);

#endif /* OLSR_NEIGH_TBL */

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

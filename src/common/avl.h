
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

#ifndef _AVL_H
#define _AVL_H

#include "../defs.h"
#include <stddef.h>

struct avl_node {
  struct avl_node *parent;
  struct avl_node *left;
  struct avl_node *right;
  struct avl_node *next;
  struct avl_node *prev;
  void *key;
  signed char balance;
  unsigned char leader;
};

typedef int (*avl_tree_comp) (const void *, const void *);

struct avl_tree {
  struct avl_node *root;
  struct avl_node *first;
  struct avl_node *last;
  unsigned int count;
  avl_tree_comp comp;
};

void EXPORT(avl_init)(struct avl_tree *, avl_tree_comp);
struct avl_node *EXPORT(avl_find) (struct avl_tree *, const void *);
int EXPORT(avl_insert)(struct avl_tree *, struct avl_node *, bool);
void EXPORT(avl_delete)(struct avl_tree *, struct avl_node *);

static INLINE struct avl_node *
avl_walk_first(struct avl_tree *tree)
{
  return tree->first;
}
static INLINE struct avl_node *
avl_walk_last(struct avl_tree *tree)
{
  return tree->last;
}
static INLINE struct avl_node *
avl_walk_next(struct avl_node *node)
{
  return node->next;
}
static INLINE struct avl_node *
avl_walk_prev(struct avl_node *node)
{
  return node->prev;
}

/* and const versions*/
static INLINE const struct avl_node *
avl_walk_first_c(const struct avl_tree *tree)
{
  return tree->first;
}
static INLINE const struct avl_node *
avl_walk_last_c(const struct avl_tree *tree)
{
  return tree->last;
}
static INLINE const struct avl_node *
avl_walk_next_c(const struct avl_node *node)
{
  return node->next;
}
static INLINE const struct avl_node *
avl_walk_prev_c(const struct avl_node *node)
{
  return node->prev;
}

extern avl_tree_comp avl_comp_default;
extern avl_tree_comp avl_comp_addr_origin_default;
extern avl_tree_comp avl_comp_prefix_default;
extern avl_tree_comp avl_comp_prefix_origin_default;
extern int avl_comp_ipv4(const void *, const void *);
extern int avl_comp_ipv6(const void *, const void *);
extern int avl_comp_mac(const void *, const void *);
extern int avl_comp_strcasecmp(const void *, const void *);
extern int avl_comp_int(const void *, const void *);
extern int avl_comp_interface_id(const void *, const void *);

/*
 * Macro to define an inline function to map from a list_node offset back to the
 * base of the datastructure. That way you save an extra data pointer.
 */
#define AVLNODE2STRUCT(funcname, structname, avlnodename) \
static INLINE struct structname * funcname (struct avl_node *ptr)\
{\
  if (!ptr) return NULL; \
  return ((struct structname *) ((char *)ptr - offsetof(struct structname,avlnodename))); \
}

#define OLSR_FOR_ALL_AVL_ENTRIES(avltreeptr, convfunc, variable) \
{ \
  struct avl_node *convfunc##node, *convfunc##next_node; \
  for (convfunc##node = avl_walk_first(avltreeptr); convfunc##node; convfunc##node = convfunc##next_node) { \
    convfunc##next_node = avl_walk_next(convfunc##node); \
    variable = convfunc (convfunc##node);
#define OLSR_FOR_ALL_AVL_ENTRIES_END() }}

#endif /* _AVL_H */

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

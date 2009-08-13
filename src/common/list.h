
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

#ifndef _LIST_H
#define _LIST_H

#include "defs.h"

#include <stddef.h>
#include <stdlib.h>

struct list_node {
  struct list_node *next;
  struct list_node *prev;
};

/* init a circular list  */
static INLINE void
list_head_init(struct list_node *node)
{
  node->prev = node->next = node;
}

/* clear a node  */
static INLINE void
list_node_init(struct list_node *node)
{
  node->prev = node->next = NULL;
}

/* test if a node is on a list */
static INLINE int
list_node_on_list(const struct list_node *node)
{
  return node->prev != NULL || node->next != NULL;
}

/* test if a list is empty */
static INLINE int
list_is_empty(const struct list_node *node)
{
  return node->prev == node && node->next == node;
}

/* Insert at the top of the list */
static INLINE void
list_add_after(struct list_node *pos_node, struct list_node *new_node)
{
  new_node->next = pos_node->next;
  new_node->prev = pos_node;

  pos_node->next->prev = new_node;
  pos_node->next = new_node;
}

/* Insert at the tail of the list */
static INLINE void
list_add_before(struct list_node *pos_node, struct list_node *new_node)
{
  new_node->prev = pos_node->prev;
  new_node->next = pos_node;

  pos_node->prev->next = new_node;
  pos_node->prev = new_node;
}

static INLINE void
list_remove(struct list_node *del_node)
{
  del_node->next->prev = del_node->prev;
  del_node->prev->next = del_node->next;

  list_node_init(del_node);
}

/*
 * Merge elements of list_head2 at the end of list_head1.
 * list_head2 will be left empty.
 */
static INLINE void
list_merge(struct list_node *list_head1, struct list_node *list_head2)
{
  if (!list_is_empty(list_head2)) {
    list_head1->next->prev = list_head2->prev;
    list_head2->prev->next = list_head1->next;
    list_head1->next = list_head2->next;
    list_head2->next->prev = list_head1;
    list_head2->next = list_head2->prev = list_head2;
  }
}

/*
 * Macro to define an inline function to map from a list_node offset back to the
 * base of the datastructure. That way you save an extra data pointer.
 */
#define LISTNODE2STRUCT(funcname, structname, listnodename) \
static INLINE structname * funcname (struct list_node *ptr)\
{\
  return( \
    ptr ? \
      (structname *) (((size_t) ptr) - offsetof(structname, listnodename)) : \
      NULL); \
}

#endif /* _LIST_H */

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

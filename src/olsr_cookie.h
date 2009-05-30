
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

#include "olsr_types.h"
#include "common/list.h"
#include "common/avl.h"

#ifndef _OLSR_COOKIE_H
#define _OLSR_COOKIE_H

extern struct avl_tree olsr_cookie_tree;

typedef enum olsr_cookie_type_ {
  OLSR_COOKIE_TYPE_MIN,
  OLSR_COOKIE_TYPE_MEMORY,
  OLSR_COOKIE_TYPE_TIMER,
  OLSR_COOKIE_TYPE_MAX
} olsr_cookie_type;

/*
 * This is a cookie. A cookie is a tool aimed for olsrd developers.
 * It is used for tracking resource usage in the system and also
 * for locating memory corruption.
 */
struct olsr_cookie_info {
  struct avl_node node;
  char *ci_name;                       /* Name */
  olsr_cookie_type ci_type;            /* Type of cookie */
  unsigned int ci_flags;               /* Misc. flags */
  unsigned int ci_usage;               /* Stats, resource usage */
  unsigned int ci_changes;             /* Stats, resource churn */

  /* only for memory cookies */
  size_t ci_size;                      /* Fixed size for block allocations */
  struct list_node ci_free_list;       /* List head for recyclable blocks */
  unsigned int ci_free_list_usage;     /* Length of free list */
  uint16_t ci_membrand;
};

AVLNODE2STRUCT(cookie_node2cookie, struct olsr_cookie_info, node);

#define OLSR_FOR_ALL_COOKIES(ci) \
{ \
  struct avl_node *ci_tree_node, *next_ci_tree_node; \
  for (ci_tree_node = avl_walk_first(&olsr_cookie_tree); \
    ci_tree_node; ci_tree_node = next_ci_tree_node) { \
    next_ci_tree_node = avl_walk_next(ci_tree_node); \
    ci = cookie_node2cookie(ci_tree_node);
#define OLSR_FOR_ALL_COOKIES_END(ci) }}

/* Cookie flags */
#define COOKIE_NO_MEMCLEAR  ( 1 << 0)   /* Do not clear memory */
#define COOKIE_MEMPOISON    ( 2 << 0)   /* Poison memory pattern */

#define COOKIE_MEMPOISON_PATTERN  0xa6  /* Pattern to spoil memory */
#define COOKIE_FREE_LIST_THRESHOLD 10   /* Blocks / Percent  */

/*
 * Small brand which gets appended on the end of every block allocation.
 * Helps to detect memory corruption, like overruns, double frees.
 */
struct olsr_cookie_mem_brand {
  char cmb_sig[6];
  uint16_t id;
};

/* Externals. */
void olsr_cookie_init(void);
struct olsr_cookie_info *EXPORT(olsr_alloc_cookie) (const char *, olsr_cookie_type);
void olsr_delete_all_cookies(void);
void EXPORT(olsr_cookie_set_memory_size) (struct olsr_cookie_info *, size_t);
void EXPORT(olsr_cookie_set_memory_clear) (struct olsr_cookie_info *, bool);
void EXPORT(olsr_cookie_set_memory_poison) (struct olsr_cookie_info *, bool);
void EXPORT(olsr_cookie_usage_incr) (struct olsr_cookie_info *);
void EXPORT(olsr_cookie_usage_decr) (struct olsr_cookie_info *);

void *EXPORT(olsr_cookie_malloc) (struct olsr_cookie_info *);
void EXPORT(olsr_cookie_free) (struct olsr_cookie_info *, void *);

struct olsr_cookie_info *EXPORT(olsr_cookie_get) (int i);

#endif /* _OLSR_COOKIE_H */

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

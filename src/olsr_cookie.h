
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

#include <stdint.h>

#include "olsr_types.h"
#include "common/list.h"
#include "common/avl.h"

#ifndef _OLSR_COOKIE_H
#define _OLSR_COOKIE_H

extern struct avl_tree EXPORT(olsr_cookie_tree);

/*
 * This is a cookie. A cookie is a tool aimed for olsrd developers.
 * It is used for tracking resource usage in the system and also
 * for locating memory corruption.
 */
struct olsr_cookie_info {
  struct avl_node ci_node;
  /* Name */
  char *ci_name;

  /* Size of memory blocks */
  size_t ci_size;

  /* flags */
  bool ci_poison;
  bool ci_no_memclear;

  /*
   * minimum number of chunks the allocator will keep
   * in the free list before starting to deallocate one
   */
  uint32_t ci_min_free_count;

  /* Stats, resource usage */
  uint32_t ci_usage;

  /* Stats, resource churn */
  uint32_t ci_changes;

  /* List head for recyclable blocks */
  struct list_entity ci_free_list;

  /* Length of free list */
  uint32_t ci_free_list_usage;
};

#define OLSR_FOR_ALL_COOKIES(ci, iterator) avl_for_each_element_safe(&olsr_cookie_tree, ci, ci_node, iterator.loop, iterator.safe)

#define COOKIE_MEMPOISON_PATTERN  0xa6  /* Pattern to spoil memory */
#define COOKIE_FREE_LIST_THRESHOLD 10   /* Blocks / Percent  */

/* Externals. */
void olsr_cookie_init(void);
void olsr_cookie_cleanup(void);

struct olsr_cookie_info *EXPORT(olsr_alloc_cookie) (const char *, size_t size);
void EXPORT(olsr_free_cookie)(struct olsr_cookie_info *);

void EXPORT(olsr_cookie_set_memory_clear) (struct olsr_cookie_info *, bool);
void EXPORT(olsr_cookie_set_memory_poison) (struct olsr_cookie_info *, bool);
void EXPORT(olsr_cookie_set_min_free)(struct olsr_cookie_info *, uint32_t);

void *EXPORT(olsr_cookie_malloc) (struct olsr_cookie_info *);
void EXPORT(olsr_cookie_free) (struct olsr_cookie_info *, void *);

#endif /* _OLSR_COOKIE_H */

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

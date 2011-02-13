
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
struct olsr_memcookie_info {
  struct avl_node ci_node;
  /* Name */
  char *ci_name;

  /* Size of memory blocks */
  size_t ci_size;

  /* size including prefix and custom data */
  size_t ci_total_size;

  /* offset from the inline custom data block */
  size_t ci_custom_offset;

  /*
   * minimum number of chunks the allocator will keep
   * in the free list before starting to deallocate one
   */
  uint32_t ci_min_free_count;

  /* Stats, resource usage */
  uint32_t ci_usage;

  /* Stats, resource churn */
  uint32_t ci_changes;

  /* list of custom additions of this memory cookie */
  struct list_entity ci_custom_list;

  /* List head for used memory blocks */
  struct list_entity ci_used_list;

  /* List head for recyclable blocks */
  struct list_entity ci_free_list;

  /* Length of free list */
  uint32_t ci_free_list_usage;
};

/* Custom addition to existing cookie */
struct olsr_memcookie_custom {
  struct list_entity node;

  /* name of the custom extension */
  char *name;

  /* padded (aligned) number of bytes allocated for the custom extension */
  size_t size;

  /* offset in the memory array for the custom extensions */
  size_t offset;

  /**
   * Called every times a new instance of memory is allocated
   * @param ci pointer to memcookie_info
   * @param ptr pointer to allocated memory
   * @param c_ptr pointer to custom memory section
   */
  void (*init)(struct olsr_memcookie_info *ci, void *ptr, void *c_ptr);

  /**
   * Called every times the custom memory section is moved
   *
   * Content of the custom memory section will already have been copied.
   *
   * @param ci pointer to memcookie info
   * @param ptr pointer to allocated memory
   * @param c_ptr pointer to new position of the custom memory section
   */
  void (*move)(struct olsr_memcookie_info *ci, void *ptr, void *c_ptr);
};

/* should have a length of 2*memory_alignment (4 pointers) */
struct olsr_memory_prefix {
  struct list_entity node;
  uint8_t *custom;
  uint8_t is_inline;
  uint8_t padding[sizeof(size_t) - sizeof(uint8_t)];
};

#define OLSR_FOR_ALL_COOKIES(ci, iterator) avl_for_each_element_safe(&olsr_cookie_tree, ci, ci_node, iterator)
#define OLSR_FOR_ALL_USED_MEM(ci, mem, iterator) list_for_each_element_safe(&ci->ci_used_list, mem, node, iterator)
#define OLSR_FOR_ALL_FREE_MEM(ci, mem, iterator) list_for_each_element_safe(&ci->ci_free_list, mem, node, iterator)
#define OLSR_FOR_ALL_CUSTOM_MEM(ci, custom, iterator) list_for_each_element_safe(&ci->ci_custom_list, custom, node, iterator)

#define COOKIE_FREE_LIST_THRESHOLD 10   /* Blocks / Percent  */

/* Externals. */
void olsr_memcookie_init(void);
void olsr_memcookie_cleanup(void);

struct olsr_memcookie_info *EXPORT(olsr_memcookie_add) (const char *, size_t size);
void EXPORT(olsr_memcookie_remove)(struct olsr_memcookie_info *);

void *EXPORT(olsr_memcookie_malloc) (struct olsr_memcookie_info *);
void EXPORT(olsr_memcookie_free) (struct olsr_memcookie_info *, void *);

struct olsr_memcookie_custom *EXPORT(olsr_memcookie_add_custom)(
    const char *memcookie_name, const char *name, size_t size,
    void (*init)(struct olsr_memcookie_info *, void *, void *),
    void (*move)(struct olsr_memcookie_info *, void *, void *));

void EXPORT(olsr_memcookie_remove_custom)(
    const char *memcookie_name, struct olsr_memcookie_custom *custom);

/**
 * Get pointer to custom memory part of a cookie instance
 * @param custom pointer to custom memcookie info
 * @param ptr pointer to memory block
 * @return pointer to custom memory block
 */
static inline void *
olsr_memcookie_get_custom(struct olsr_memcookie_custom *custom, void *ptr) {
  struct olsr_memory_prefix *mem;

  /* get to the prefix memory structure */
  mem = ptr;
  mem--;

  return mem->custom + custom->offset;
}

/**
 * Set the minimum number of free allocated blocks for a memcookie that
 * will be kept back by the cookie_manager
 * @param ci
 * @param min_free
 */
static inline void
olsr_memcookie_set_minfree(struct olsr_memcookie_info *ci, uint32_t min_free)
{
  ci->ci_min_free_count = min_free;
}

#endif /* _OLSR_COOKIE_H */

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

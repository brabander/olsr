
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

#include "olsr.h"
#include "defs.h"
#include "olsr_cookie.h"
#include "olsr_logging.h"
#include "common/list.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

struct avl_tree olsr_cookie_tree;

static inline size_t calc_aligned_size(size_t size) {
  static const size_t add = sizeof(size_t) * 2 - 1;
  static const size_t mask = ~(sizeof(size_t)*2 - 1);

  return (size + add) & mask;
}

void olsr_cookie_init(void) {
  /* check size of memory prefix */
  assert (sizeof(struct olsr_memory_prefix)
      == calc_aligned_size(sizeof(struct olsr_memory_prefix)));

  avl_init(&olsr_cookie_tree, &avl_comp_strcasecmp, false, NULL);
}

/*
 * Allocate a cookie for the next available cookie id.
 */
struct olsr_cookie_info *
olsr_create_memcookie(const char *cookie_name, size_t size)
{
  struct olsr_cookie_info *ci;

  assert (cookie_name);
  assert (size > 9);

  ci = olsr_malloc(sizeof(struct olsr_cookie_info), "memory cookie");

  /* Now populate the cookie info */
  ci->ci_name = olsr_strdup(cookie_name);
  ci->ci_node.key = ci->ci_name;
  ci->ci_size = size;
  ci->ci_custom_offset = sizeof(struct olsr_memory_prefix) + calc_aligned_size(size);
  ci->ci_min_free_count = COOKIE_FREE_LIST_THRESHOLD;

  /* no custom data at this point */
  ci->ci_total_size = ci->ci_custom_offset;

  /* Init the free list */
  list_init_head(&ci->ci_free_list);
  list_init_head(&ci->ci_used_list);
  list_init_head(&ci->ci_custom_list);

  avl_insert(&olsr_cookie_tree, &ci->ci_node);
  return ci;
}

/*
 * Free a cookie that is no longer being used.
 */
void
olsr_cleanup_memcookie(struct olsr_cookie_info *ci)
{
  struct olsr_memory_prefix *memory_entity, *iterator;

  /* remove from tree */
  avl_delete(&olsr_cookie_tree, &ci->ci_node);

  /* Free name */
  free(ci->ci_name);

  /* Flush all the memory on the free list */
  /*
   * First make all items accessible,
   * such that valgrind does not complain at shutdown.
   */

  /* remove all free memory blocks */
  OLSR_FOR_ALL_FREE_MEM(ci, memory_entity, iterator) {
    free(memory_entity);
  }

  /* free all used memory blocks */
  OLSR_FOR_ALL_USED_MEM(ci, memory_entity, iterator) {
    free(memory_entity->custom);
    free(memory_entity);
  }

  free(ci);
}

/*
 * Flush all cookies. This is really only called upon shutdown.
 */
void
olsr_cookie_cleanup(void)
{
  struct olsr_cookie_info *info, *iterator;

  /*
   * Walk the full index range and kill 'em all.
   */
  OLSR_FOR_ALL_COOKIES(info, iterator) {
    olsr_cleanup_memcookie(info);
  }
}

/*
 * Set if a returned memory block shall be cleared after returning to
 * the free pool. This is only allowed for memory cookies.
 */
void
olsr_cookie_set_min_free(struct olsr_cookie_info *ci, uint32_t min_free)
{
  ci->ci_min_free_count = min_free;
}


/*
 * Increment usage state for a given cookie.
 */
static inline void
olsr_cookie_usage_incr(struct olsr_cookie_info *ci)
{
  ci->ci_usage++;
  ci->ci_changes++;
}

/*
 * Decrement usage state for a given cookie.
 */
static inline void
olsr_cookie_usage_decr(struct olsr_cookie_info *ci)
{
  ci->ci_usage--;
  ci->ci_changes++;
}

/*
 * Allocate a fixed amount of memory based on a passed in cookie type.
 */
void *
olsr_cookie_malloc(struct olsr_cookie_info *ci)
{
  struct olsr_memory_prefix *mem;
  struct olsr_cookie_custom *custom, *iterator;

#if !defined REMOVE_LOG_DEBUG
  bool reuse = false;
#endif

  /*
   * Check first if we have reusable memory.
   */
  if (list_is_empty(&ci->ci_free_list)) {
    /*
     * No reusable memory block on the free_list.
     * Allocate a fresh one.
     */
    mem = olsr_malloc(ci->ci_total_size, ci->ci_name);
  } else {
    /*
     * There is a memory block on the free list.
     * Carve it out of the list, and clean.
     */
    mem = list_first_element(&ci->ci_free_list, mem, node);
    list_remove(&mem->node);

    memset(mem, 0, ci->ci_total_size);

    ci->ci_free_list_usage--;
#if !defined REMOVE_LOG_DEBUG
    reuse = true;
#endif
  }

  /* add to used list */
  list_add_tail(&ci->ci_used_list, &mem->node);

  /* handle custom initialization */
  if (!list_is_empty(&ci->ci_custom_list)) {
    mem->custom = ((uint8_t *)mem) + ci->ci_custom_offset;

    /* call up custom init functions */
    OLSR_FOR_ALL_CUSTOM_MEM(ci, custom, iterator) {
      if (custom->init) {
        custom->init(ci, mem + 1, mem->custom + custom->offset);
      }
    }
  }

  /* Stats keeping */
  olsr_cookie_usage_incr(ci);

  OLSR_DEBUG(LOG_COOKIE, "MEMORY: alloc %s, %p, %lu bytes%s\n",
             ci->ci_name, mem + 1, (unsigned long)ci->ci_size, reuse ? ", reuse" : "");
  return mem + 1;
}

/*
 * Free a memory block owned by a given cookie.
 * Run some corruption checks.
 */
void
olsr_cookie_free(struct olsr_cookie_info *ci, void *ptr)
{
  struct olsr_memory_prefix *mem;
  struct olsr_cookie_custom *custom, *iterator;
#if !defined REMOVE_LOG_DEBUG
  bool reuse = false;
#endif

  /* calculate pointer to memory prefix */
  mem = ptr;
  mem--;

  /* call up custom cleanup */
  OLSR_FOR_ALL_CUSTOM_MEM(ci, custom, iterator) {
    if (custom->cleanup) {
      custom->cleanup(ci, ptr, mem->custom + custom->offset);
    }
  }

  /* remove from used_memory list */
  list_remove(&mem->node);

  /*
   * Rather than freeing the memory right away, try to reuse at a later
   * point. Keep at least ten percent of the active used blocks or at least
   * ten blocks on the free list.
   */
  if (mem->is_inline && ((ci->ci_free_list_usage < ci->ci_min_free_count)
      || (ci->ci_free_list_usage < ci->ci_usage / COOKIE_FREE_LIST_THRESHOLD))) {

    list_add_tail(&ci->ci_free_list, &mem->node);

    ci->ci_free_list_usage++;
#if !defined REMOVE_LOG_DEBUG
    reuse = true;
#endif
  } else {

    /* No interest in reusing memory. */
    if (!mem->is_inline) {
      free (mem->custom);
    }
    free(mem);
  }

  /* Stats keeping */
  olsr_cookie_usage_decr(ci);

  OLSR_DEBUG(LOG_COOKIE, "MEMORY: free %s, %p, %lu bytes%s\n",
             ci->ci_name, ptr, (unsigned long)ci->ci_total_size, reuse ? ", reuse" : "");
}

struct olsr_cookie_custom *
olsr_alloc_cookie_custom(struct olsr_cookie_info *ci, size_t size, const char *name,
    void (*init)(struct olsr_cookie_info *, void *, void *),
    void (*cleanup)(struct olsr_cookie_info *, void *, void *)) {
  struct olsr_cookie_custom *custom;
  struct olsr_memory_prefix *mem, *iterator;
  size_t old_total_size, new_total_size;

  custom = olsr_malloc(sizeof(struct olsr_cookie_custom), name);
  custom->name = strdup(name);
  custom->size = calc_aligned_size(size);
  custom->init = init;
  custom->cleanup = cleanup;

  /* recalculate custom data block size */
  old_total_size = ci->ci_total_size - ci->ci_custom_offset;
  new_total_size = old_total_size + custom->size;

  custom->offset = old_total_size;
  ci->ci_total_size += custom->size;

  /* reallocate custom data blocks on used memory blocks*/
  OLSR_FOR_ALL_USED_MEM(ci, mem, iterator) {
    uint8_t *custom_ptr;

    custom_ptr = olsr_malloc(new_total_size, ci->ci_name);

    /* copy old data */
    if (old_total_size > 0) {
      memcpy(custom_ptr, mem->custom, old_total_size);
    }

    mem->is_inline = false;
    mem->custom = custom_ptr;

    /* call up necessary initialization */
    init(ci, mem + 1, custom_ptr + old_total_size);
  }

  /* remove all free data blocks, they have the wrong size */
  OLSR_FOR_ALL_FREE_MEM(ci, mem, iterator) {
    list_remove(&mem->node);
    free(mem);
  }
  ci->ci_free_list_usage = 0;

  /* add the custom data object to the list */
  list_add_tail(&ci->ci_custom_list, &custom->node);
  return custom;
}

void
olsr_free_cookie_custom(struct olsr_cookie_info *ci, struct olsr_cookie_custom *custom) {
  struct olsr_memory_prefix *mem, *mem_iterator;
  struct olsr_cookie_custom *c_ptr, *c_iterator;
  size_t prefix_block, suffix_block;
  bool match;

  prefix_block = 0;
  suffix_block = 0;
  match = false;

  OLSR_FOR_ALL_CUSTOM_MEM(ci, c_ptr, c_iterator) {
    if (c_ptr == custom) {
      match = true;
      continue;
    }

    if (match) {
      suffix_block += c_ptr->size;
      c_ptr->offset -= custom->size;
    }
    else {
      prefix_block += c_ptr->size;
    }
  }

  /* move the custom memory back into a continous block */
  if (suffix_block > 0) {
    OLSR_FOR_ALL_USED_MEM(ci, mem, mem_iterator) {
      memmove(mem->custom + prefix_block, mem->custom + prefix_block + custom->size, suffix_block);
    }
  }
  ci->ci_total_size -= custom->size;

  /* remove all free data blocks, they have the wrong size */
  OLSR_FOR_ALL_FREE_MEM(ci, mem, mem_iterator) {
    list_remove(&mem->node);
    free(mem);
  }
  ci->ci_free_list_usage = 0;

  /* remove the custom data object from the list */
  list_remove(&custom->node);
  free (custom->name);
  free (custom);
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

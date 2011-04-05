
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

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "common/list.h"
#include "common/avl.h"
#include "common/avl_comp.h"
#include "olsr.h"
#include "defs.h"
#include "olsr_memcookie.h"
#include "olsr_logging.h"

struct avl_tree olsr_cookie_tree;

/**
 * Align a byte size correctly to "two size_t" units
 * @param size number of bytes for an unaligned block
 * @return number of bytes including padding for alignment
 */
static inline size_t
calc_aligned_size(size_t size) {
  static const size_t add = sizeof(size_t) * 2 - 1;
  static const size_t mask = ~(sizeof(size_t)*2 - 1);

  return (size + add) & mask;
}

/**
 * Increment usage state for a given cookie.
 * @param ci pointer to memcookie info
 */
static inline void
olsr_cookie_usage_incr(struct olsr_memcookie_info *ci)
{
  ci->ci_usage++;
  ci->ci_changes++;
}

/**
 * Decrement usage state for a given cookie.
 * @param ci pointer to memcookie info
 */
static inline void
olsr_cookie_usage_decr(struct olsr_memcookie_info *ci)
{
  ci->ci_usage--;
  ci->ci_changes++;
}

/**
 * Initialize the memory cookie system
 */
void
olsr_memcookie_init(void) {
  /* check size of memory prefix */
  assert (sizeof(struct olsr_memory_prefix)
      == calc_aligned_size(sizeof(struct olsr_memory_prefix)));

  avl_init(&olsr_cookie_tree, &avl_comp_strcasecmp, false, NULL);
}

/**
 * Cleanup the memory cookie system
 */
void
olsr_memcookie_cleanup(void)
{
  struct olsr_memcookie_info *info, *iterator;

  if (olsr_cookie_tree.comp == NULL) {
    /* nothing to do */
    return;
  }

  /*
   * Walk the full index range and kill 'em all.
   */
  OLSR_FOR_ALL_COOKIES(info, iterator) {
    olsr_memcookie_remove(info);
  }
}

/**
 * Allocate a new memcookie.
 * @param cookie_name id of the cookie
 * @param size number of bytes to allocate for each cookie
 * @return memcookie_info pointer
 */
struct olsr_memcookie_info *
olsr_memcookie_add(const char *cookie_name, size_t size)
{
  struct olsr_memcookie_info *ci;

  assert (cookie_name);
  ci = olsr_malloc(sizeof(struct olsr_memcookie_info), "memory cookie");

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

/**
 * Delete a memcookie and all attached memory
 * @param ci pointer to memcookie
 */
void
olsr_memcookie_remove(struct olsr_memcookie_info *ci)
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

/**
 * Allocate a fixed amount of memory based on a passed in cookie type.
 * @param ci pointer to memcookie info
 * @return allocated memory
 */
void *
olsr_memcookie_malloc(struct olsr_memcookie_info *ci)
{
  struct olsr_memory_prefix *mem;
  struct olsr_memcookie_custom *custom, *iterator;

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

/**
 * Free a memory block owned by a given cookie.
 * @param ci pointer to memcookie info
 * @param ptr pointer to memory block
 */
void
olsr_memcookie_free(struct olsr_memcookie_info *ci, void *ptr)
{
  struct olsr_memory_prefix *mem;
#if !defined REMOVE_LOG_DEBUG
  bool reuse = false;
#endif

  /* calculate pointer to memory prefix */
  mem = ptr;
  mem--;

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

/**
 * Add a custom memory section to an existing memcookie.
 *
 * Calling this function will call the specified init() callback and
 * the move() callback of all existing custom extensions of this memcookie
 * for every existing piece of allocated memory of the memcookie.
 *
 * @param memcookie_name name of memory cookie to be extended
 * @param name name of custom addition for the memory cookie
 * @param size number of bytes needed for custom addition
 * @param init callback of the custom memcookie manager which is called every
 *   times a new memcookie instance is allocated. Parameters are the memcookie_info
 *   object, a pointer to the allocated memory and a pointer to the custom
 *   part of the memory object.
 * @param move callback of the custom memcookie manager which is called every
 *   times the memory of the custom extension changes (because of adding/removal of
 *   other custom extensions). Parameters are the memcookie_info object, a pointer to
 *   the allocated memory, a pointer to the old position of the custom extension and
 *   a pointer to the new custom extension.
 * @return custom memory cookie
 */
struct olsr_memcookie_custom *
olsr_memcookie_add_custom(const char *memcookie_name, const char *name, size_t size,
    void (*init)(struct olsr_memcookie_info *, void *, void *),
    void (*move)(struct olsr_memcookie_info *, void *, void *)) {
  struct olsr_memcookie_info *ci;
  struct olsr_memcookie_custom *custom_cookie;
  struct olsr_memcookie_custom *custom = NULL, *custom_iterator;
  struct olsr_memory_prefix *mem, *mem_iterator;
  size_t old_total_size, new_total_size;

  ci = avl_find_element(&olsr_cookie_tree, memcookie_name, ci, ci_node);
  if (ci == NULL) {
    OLSR_WARN(LOG_COOKIE, "Memory cookie '%s' does not exist, cannot add custom block '%s'\n",
        memcookie_name, name);
    return NULL;
  }

  custom_cookie = olsr_malloc(sizeof(struct olsr_memcookie_custom), name);
  custom_cookie->name = strdup(name);
  custom_cookie->size = calc_aligned_size(size);
  custom_cookie->init = init;
  custom_cookie->move = move;

  /* recalculate custom data block size */
  old_total_size = ci->ci_total_size - ci->ci_custom_offset;
  new_total_size = old_total_size + custom_cookie->size;

  custom_cookie->offset = old_total_size;
  ci->ci_total_size += custom_cookie->size;

  /* reallocate custom data blocks on used memory blocks*/
  OLSR_FOR_ALL_USED_MEM(ci, mem, mem_iterator) {
    uint8_t *new_custom;

    new_custom = olsr_malloc(new_total_size, ci->ci_name);

    /* copy old data */
    if (old_total_size > 0) {
      memmove(new_custom, mem->custom, old_total_size);
    }

    mem->is_inline = false;
    mem->custom = new_custom;

    /* call up necessary initialization */
    if (custom->init) {
      custom->init(ci, mem + 1, new_custom + old_total_size);
    }

    /* inform the custom cookie managers that their memory has moved */
    OLSR_FOR_ALL_CUSTOM_MEM(ci, custom, custom_iterator) {
      if (custom->move) {
        custom->move(ci, mem+1, new_custom + custom->offset);
      }
    }
  }

  /* remove all free data blocks, they have the wrong size */
  OLSR_FOR_ALL_FREE_MEM(ci, mem, mem_iterator) {
    list_remove(&mem->node);
    free(mem);
  }
  ci->ci_free_list_usage = 0;

  /* add the custom data object to the list */
  list_add_tail(&ci->ci_custom_list, &custom_cookie->node);
  return custom_cookie;
}

/**
 * Remove a custom addition to a memcookie
 * @param memcookie_name name of memcookie
 * @param custom pointer to custom memcookie
 */
void
olsr_memcookie_remove_custom(const char*memcookie_name, struct olsr_memcookie_custom *custom) {
  struct olsr_memcookie_info *ci;
  struct olsr_memory_prefix *mem, *mem_iterator;
  struct olsr_memcookie_custom *c_ptr, *c_iterator;
  size_t prefix_block, suffix_block;
  bool match;

  ci = avl_find_element(&olsr_cookie_tree, memcookie_name, ci, ci_node);
  if (ci == NULL) {
    OLSR_WARN(LOG_COOKIE, "Memory cookie '%s' does not exist, cannot remove custom block '%s'\n",
        memcookie_name, custom->name);
    return;
  }

  prefix_block = 0;
  suffix_block = 0;
  match = false;

  /* calculate size of (not) modified custom data block */
  OLSR_FOR_ALL_CUSTOM_MEM(ci, c_ptr, c_iterator) {
    if (c_ptr == custom) {
      match = true;
      continue;
    }

    if (match) {
      suffix_block += c_ptr->size;
    }
    else {
      prefix_block += c_ptr->size;
    }
  }

  /* move the custom memory back into a continous block */
  if (suffix_block > 0) {
    OLSR_FOR_ALL_USED_MEM(ci, mem, mem_iterator) {
      memmove(mem->custom + prefix_block, mem->custom + prefix_block + custom->size, suffix_block);

      /* recalculate offsets of moved blocks and inform callbacks */
      OLSR_FOR_ALL_CUSTOM_MEM(ci, c_ptr, c_iterator) {
        if (c_ptr == custom) {
          match = true;
          continue;
        }

        if (match) {
          c_ptr->offset -= custom->size;

          if (c_ptr->move) {
            c_ptr->move(ci, mem+1, mem->custom + c_ptr->offset);
          }
        }
      }

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

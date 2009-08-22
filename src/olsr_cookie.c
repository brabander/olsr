
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
#include "log.h"
#include "valgrind/valgrind.h"
#include "valgrind/memcheck.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

struct avl_tree olsr_cookie_tree;

void olsr_cookie_init(void) {
  avl_init(&olsr_cookie_tree, &avl_comp_strcasecmp);
}

/*
 * Allocate a cookie for the next available cookie id.
 */
struct olsr_cookie_info *
olsr_alloc_cookie(const char *cookie_name, olsr_cookie_type cookie_type)
{
  static uint16_t next_brand_id = 1;

  struct olsr_cookie_info *ci;

  assert (cookie_name);

  ci = olsr_malloc(sizeof(struct olsr_cookie_info), "new cookie");

  /* Now populate the cookie info */
  ci->ci_type = cookie_type;
  ci->ci_name = olsr_strdup(cookie_name);

  ci->node.key = ci->ci_name;

  /* Init the free list */
  if (cookie_type == OLSR_COOKIE_TYPE_MEMORY) {
    list_head_init(&ci->ci_free_list);
    VALGRIND_CREATE_MEMPOOL(ci, 0, 1);

    ci->ci_membrand = next_brand_id++;
  }
  else {
    ci->ci_membrand = 0;
  }

  avl_insert(&olsr_cookie_tree, &ci->node, AVL_DUP);
  return ci;
}

/*
 * Free a cookie that is no longer being used.
 */
static void
olsr_free_cookie(struct olsr_cookie_info *ci)
{
  struct list_node *memory_list;

  /* remove from tree */
  avl_delete(&olsr_cookie_tree, &ci->node);

  /* Free name */
  free(ci->ci_name);

  /* Flush all the memory on the free list */
  if (ci->ci_type == OLSR_COOKIE_TYPE_MEMORY) {

    /*
     * First make all items accessible,
     * such that valgrind does not complain at shutdown.
     */
    if (!list_is_empty(&ci->ci_free_list)) {
      for (memory_list = ci->ci_free_list.next; memory_list != &ci->ci_free_list; memory_list = memory_list->next) {
        VALGRIND_MAKE_MEM_DEFINED(memory_list, ci->ci_size);
      }
    }

    while (!list_is_empty(&ci->ci_free_list)) {
      memory_list = ci->ci_free_list.next;
      list_remove(memory_list);
      free(memory_list);
    }
    VALGRIND_DESTROY_MEMPOOL(ci);
  }

  free(ci);
}

/*
 * Flush all cookies. This is really only called upon shutdown.
 */
void
olsr_delete_all_cookies(void)
{
  struct olsr_cookie_info *info;

  /*
   * Walk the full index range and kill 'em all.
   */
  OLSR_FOR_ALL_COOKIES(info) {
    olsr_free_cookie(info);
  } OLSR_FOR_ALL_COOKIES_END(info)
}

/*
 * Set the size for fixed block allocations.
 * This is only allowed for memory cookies.
 */
void
olsr_cookie_set_memory_size(struct olsr_cookie_info *ci, size_t size)
{
  if (!ci) {
    return;
  }

  assert(ci->ci_type == OLSR_COOKIE_TYPE_MEMORY);
  ci->ci_size = size;
}

/*
 * Set if a returned memory block shall be cleared after returning to
 * the free pool. This is only allowed for memory cookies.
 */
void
olsr_cookie_set_memory_clear(struct olsr_cookie_info *ci, bool clear)
{
  if (!ci) {
    return;
  }

  assert(ci->ci_type == OLSR_COOKIE_TYPE_MEMORY);

  if (!clear) {
    ci->ci_flags |= COOKIE_NO_MEMCLEAR;
  } else {
    ci->ci_flags &= ~COOKIE_NO_MEMCLEAR;
  }
}

/*
 * Set if a returned memory block shall be initialized to an all zero or
 * to a poison memory pattern after returning to the free pool.
 * This is only allowed for memory cookies.
 */
void
olsr_cookie_set_memory_poison(struct olsr_cookie_info *ci, bool poison)
{
  if (!ci) {
    return;
  }

  assert(ci->ci_type == OLSR_COOKIE_TYPE_MEMORY);

  if (poison) {
    ci->ci_flags |= COOKIE_MEMPOISON;
  } else {
    ci->ci_flags &= ~COOKIE_MEMPOISON;
  }
}

/*
 * Increment usage state for a given cookie.
 */
void
olsr_cookie_usage_incr(struct olsr_cookie_info *ci)
{
  ci->ci_usage++;
  ci->ci_changes++;
}

/*
 * Decrement usage state for a given cookie.
 */
void
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
  void *ptr;
  struct olsr_cookie_mem_brand *branding;
  struct list_node *free_list_node;
#if !defined REMOVE_LOG_DEBUG
  bool reuse = false;
#endif
  size_t size;

  /*
   * Check first if we have reusable memory.
   */
  if (!ci->ci_free_list_usage) {

    /*
     * No reusable memory block on the free_list.
     * Allocate a fresh one.
     */
    size = ci->ci_size + sizeof(struct olsr_cookie_mem_brand);
    ptr = olsr_malloc(size, ci->ci_name);

    /*
     * Poison the memory for debug purposes ?
     */
    if (ci->ci_flags & COOKIE_MEMPOISON) {
      memset(ptr, COOKIE_MEMPOISON_PATTERN, size);
    }

  } else {

    /*
     * There is a memory block on the free list.
     * Carve it out of the list, and clean.
     */
    free_list_node = ci->ci_free_list.next;
    ptr = (void *)free_list_node;
    VALGRIND_MAKE_MEM_DEFINED(ptr, ci->ci_size);

    /*
     * Before dequeuing the node from the free list,
     * make the list pointers of the node ahead of
     * us accessible, such that valgrind does not
     * log a false positive.
     */
    if (free_list_node->next == &ci->ci_free_list) {
      list_remove(free_list_node);
    } else {

      /*
       * Make next item accessible, remove it and make next item inaccessible.
       */
      VALGRIND_MAKE_MEM_DEFINED(free_list_node->next, ci->ci_size);
      list_remove(free_list_node);
      VALGRIND_MAKE_MEM_NOACCESS(free_list_node->next, ci->ci_size);
    }

    /*
     * Reset the memory unless the caller has told us so.
     */
    if (!(ci->ci_flags & COOKIE_NO_MEMCLEAR)) {

      /*
       * Poison the memory for debug purposes ?
       */
      if (ci->ci_flags & COOKIE_MEMPOISON) {
        memset(ptr, COOKIE_MEMPOISON_PATTERN, ci->ci_size);
      } else {
        memset(ptr, 0, ci->ci_size);
      }
    }

    ci->ci_free_list_usage--;
#if !defined REMOVE_LOG_DEBUG
    reuse = true;
#endif
  }

  /*
   * Now brand mark the end of the memory block with a short signature
   * indicating presence of a cookie. This will be checked against
   * When the block is freed to detect corruption.
   */
  branding = (struct olsr_cookie_mem_brand *)
    (ARM_NOWARN_ALIGN)((unsigned char *)ptr + ci->ci_size);
  memcpy(&branding->cmb_sig, "cookie", 6);
  branding->id = ci->ci_membrand;

  /* Stats keeping */
  olsr_cookie_usage_incr(ci);

  OLSR_DEBUG(LOG_COOKIE, "MEMORY: alloc %s, %p, %lu bytes%s\n",
             ci->ci_name, ptr, (unsigned long)ci->ci_size, reuse ? ", reuse" : "");

  VALGRIND_MEMPOOL_ALLOC(ci, ptr, ci->ci_size);
  return ptr;
}

/*
 * Free a memory block owned by a given cookie.
 * Run some corruption checks.
 */
void
olsr_cookie_free(struct olsr_cookie_info *ci, void *ptr)
{
  struct list_node *free_list_node;
#if !defined REMOVE_LOG_DEBUG
  bool reuse = false;
#endif
  struct olsr_cookie_mem_brand *branding = (struct olsr_cookie_mem_brand *)
    (ARM_NOWARN_ALIGN)((unsigned char *)ptr + ci->ci_size);

  /*
   * Verify if there has been a memory overrun, or
   * the wrong owner is trying to free this.
   */

  if (!(memcmp(&branding->cmb_sig, "cookie", 6) == 0 && branding->id == ci->ci_membrand)) {
    OLSR_ERROR(LOG_COOKIE, "Memory corruption at end of '%s' cookie\n", ci->ci_name);
    olsr_exit(1);
  }

  /* Kill the brand */
  memset(branding, 0, sizeof(*branding));

  /*
   * Rather than freeing the memory right away, try to reuse at a later
   * point. Keep at least ten percent of the active used blocks or at least
   * ten blocks on the free list.
   */
  if ((ci->ci_free_list_usage < COOKIE_FREE_LIST_THRESHOLD) || (ci->ci_free_list_usage < ci->ci_usage / COOKIE_FREE_LIST_THRESHOLD)) {

    free_list_node = (struct list_node *)ptr;
    list_node_init(free_list_node);

    /*
     * Before enqueuing the node to the free list,
     * make the list pointers of the node ahead of
     * us accessible, such that valgrind does not
     * log a false positive.
     */
    if (list_is_empty(&ci->ci_free_list)) {
      list_add_before(&ci->ci_free_list, free_list_node);
    } else {

      /*
       * Make next item accessible, add it and make next item inaccessible.
       */
      VALGRIND_MAKE_MEM_DEFINED(ci->ci_free_list.prev, ci->ci_size);
      list_add_before(&ci->ci_free_list, free_list_node);
      VALGRIND_MAKE_MEM_NOACCESS(ci->ci_free_list.prev, ci->ci_size);
    }

    ci->ci_free_list_usage++;
#if !defined REMOVE_LOG_DEBUG
    reuse = true;
#endif

  } else {

    /*
     * No interest in reusing memory.
     */
    free(ptr);
  }

  /* Stats keeping */
  olsr_cookie_usage_decr(ci);

  OLSR_DEBUG(LOG_COOKIE, "MEMORY: free %s, %p, %lu bytes%s\n",
             ci->ci_name, ptr, (unsigned long)ci->ci_size, reuse ? ", reuse" : "");

  VALGRIND_MEMPOOL_FREE(ci, ptr);
  VALGRIND_MAKE_MEM_NOACCESS(ptr, ci->ci_size);
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

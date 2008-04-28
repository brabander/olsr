
/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2008, Hannes Gredler (hannes@gredler.at)
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
 */

#include "olsr.h"
#include "defs.h"
#include "olsr_cookie.h"
#include "log.h"

#include <assert.h>

/* Root directory of the cookies we have in the system */
struct olsr_cookie_info *cookies[COOKIE_ID_MAX];

/*
 * Allocate a cookie for the next available cookie id.
 */
struct olsr_cookie_info *
olsr_alloc_cookie(const char *cookie_name, olsr_cookie_type cookie_type)
{
  static olsr_bool first = OLSR_TRUE;
  struct olsr_cookie_info *ci;
  int ci_index;

  /* Clear the cookie root array on the first call */
  if (first) {
    for (ci_index = 0; ci_index < COOKIE_ID_MAX; ci_index++) {
      cookies[ci_index] = NULL;
    }
    first = OLSR_FALSE;
  }

  /*
   * Look for an unused index.
   * For ease of troubleshooting (non-zero patterns) we start at index 1.
   */
  for (ci_index = 1; ci_index < COOKIE_ID_MAX; ci_index++) {
    if (!cookies[ci_index]) {
      break;
    }
  }

  assert(ci_index < COOKIE_ID_MAX);	/* increase COOKIE_ID_MAX */

  ci = calloc(1, sizeof(struct olsr_cookie_info));
  cookies[ci_index] = ci;

  /* Now populate the cookie info */
  ci->ci_id = ci_index;
  ci->ci_type = cookie_type;
  if (cookie_name) {
    ci->ci_name = strdup(cookie_name);
  }

  return ci;
}

/*
 * Free a cookie that is no longer being used.
 */
void
olsr_free_cookie(struct olsr_cookie_info *ci)
{

  /* Mark the cookie as unused */
  cookies[ci->ci_id] = NULL;

  /* Free name if set */
  if (ci->ci_name) {
    free(ci->ci_name);
  }
  free(ci);
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
 * Basic sanity checking for a passed-in cookie-id.
 */
static olsr_bool
olsr_cookie_valid(olsr_cookie_t cookie_id)
{
  if ((cookie_id < COOKIE_ID_MAX) && cookies[cookie_id]) {
    return OLSR_TRUE;
  }
  return OLSR_FALSE;
}

/*
 * Increment usage state for a given cookie.
 */
void
olsr_cookie_usage_incr(olsr_cookie_t cookie_id)
{
  if (olsr_cookie_valid(cookie_id)) {
    cookies[cookie_id]->ci_usage++;
    cookies[cookie_id]->ci_changes++;
  }
}

/*
 * Decrement usage state for a given cookie.
 */
void
olsr_cookie_usage_decr(olsr_cookie_t cookie_id)
{
  if (olsr_cookie_valid(cookie_id)) {
    cookies[cookie_id]->ci_usage--;
    cookies[cookie_id]->ci_changes++;
  }
}

/*
 * Return a cookie name.
 * Mostly used for logging purposes.
 */
char *
olsr_cookie_name(olsr_cookie_t cookie_id)
{
  static char unknown[] = "unknown";

  if (olsr_cookie_valid(cookie_id)) {
    return (cookies[cookie_id])->ci_name;
  }

  return unknown;
}

/*
 * Allocate a fixed amount of memory based on a passed in cookie type.
 */
void *
olsr_cookie_malloc(struct olsr_cookie_info *ci)
{
  void *ptr;
  struct olsr_cookie_mem_brand *branding;

  /*
   * Not all the callers do a proper cleaning of memory.
   * Clean it on behalf of those.
   */
  ptr = calloc(1, ci->ci_size + sizeof(struct olsr_cookie_mem_brand));

  if (!ptr) {
    const char *const err_msg = strerror(errno);
    OLSR_PRINTF(1, "OUT OF MEMORY: %s\n", err_msg);
    olsr_syslog(OLSR_LOG_ERR, "olsrd: out of memory!: %s\n", err_msg);
    olsr_exit(ci->ci_name, EXIT_FAILURE);
  }

  /*
   * Now brand mark the end of the memory block with a short signature
   * indicating presence of a cookie. This will be checked against
   * When the block is freed to detect corruption.
   */
  branding = (struct olsr_cookie_mem_brand *)
    ((unsigned char *)ptr + ci->ci_size);
  memcpy(&branding->cmb_sig, "cookie", 6);
  branding->cmb_id = ci->ci_id;

  /* Stats keeping */
  olsr_cookie_usage_incr(ci->ci_id);

#if 1
  olsr_printf(1, "MEMORY: alloc %s, %p, %u bytes\n",
	      ci->ci_name, ptr, ci->ci_size);
#endif

  return ptr;
}

/*
 * Free a memory block owned by a given cookie.
 * Run some corruption checks.
 */
void
olsr_cookie_free(struct olsr_cookie_info *ci, void *ptr)
{
  struct olsr_cookie_mem_brand *branding;

  branding = (struct olsr_cookie_mem_brand *)
    ((unsigned char *)ptr + ci->ci_size);

  /*
   * Verify if there has been a memory overrun, or
   * the wrong owner is trying to free this.
   */
  assert(!memcmp(&branding->cmb_sig, "cookie", 6) &&
	 branding->cmb_id == ci->ci_id);

  /* Kill the brand */
  memset(branding, 0, sizeof(*branding));

  /* Stats keeping */
  olsr_cookie_usage_decr(ci->ci_id);

#if 1
  olsr_printf(1, "MEMORY: free %s, %p, %u bytes\n",
	      ci->ci_name, ptr, ci->ci_size);
#endif

  free(ptr);
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * End:
 */


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
#include "mpr_selector_set.h"
#include "link_set.h"
#include "olsr.h"
#include "olsr_logging.h"

uint16_t ansn = 0;

static struct olsr_cookie_info *mpr_sel_timer_cookie;
static struct olsr_cookie_info *mpr_sel_mem_cookie;

/* Root of MPR selector tree */
static struct avl_tree mprs_tree;


void
olsr_init_mprs(void)
{
  OLSR_INFO(LOG_MPRS, "Initialize MPR set...\n");

  avl_init(&mprs_tree, avl_comp_default);

  /*
   * Get some cookies for getting stats to ease troubleshooting.
   */
  mpr_sel_timer_cookie = olsr_alloc_cookie("MPR Selector", OLSR_COOKIE_TYPE_TIMER);

  mpr_sel_mem_cookie = olsr_alloc_cookie("MPR Selector", OLSR_COOKIE_TYPE_MEMORY);
  olsr_cookie_set_memory_size(mpr_sel_mem_cookie, sizeof(struct mpr_selector));
}

/**
 * Wrapper for the timer callback.
 */
static void
olsr_expire_mpr_sel_entry(void *context)
{
  struct mpr_selector *mpr_sel = context;
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif
  OLSR_DEBUG(LOG_MPRS, "MPRS: Timing out %st\n", olsr_ip_to_string(&buf, &mpr_sel->MS_main_addr));

  mpr_sel->MS_timer = NULL;

  avl_delete(&mprs_tree, &mpr_sel->mprs_node);

  /* Delete entry */
  olsr_cookie_free(mpr_sel_mem_cookie, mpr_sel);
  signal_link_changes(true);
}

/**
 * Lookup an entry in the MPR selector table
 * based on address
 *
 * @param addr the addres to check for
 *
 * @return a pointer to the entry or NULL
 */
struct mpr_selector *
olsr_lookup_mprs_set(const union olsr_ip_addr *addr)
{
  struct avl_node *node;

  node = avl_find(&mprs_tree, addr);
  if (node) {
    return mprs_sel_node_to_mpr_sel(node);
  }
  return NULL;
}


/**
 * Update a MPR selector entry or create an new
 * one if it does not exist
 *
 * @param addr the address of the MPR selector
 * @param vtime tha validity time of the entry
 *
 * @return 1 if a new entry was added 0 if not
 */
int
olsr_update_mprs_set(const union olsr_ip_addr *addr, olsr_reltime vtime)
{
  int rv;
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif
  struct mpr_selector *mprs;

  mprs = olsr_lookup_mprs_set(addr);
  if (mprs == NULL) {
    mprs = olsr_cookie_malloc(mpr_sel_mem_cookie);

    OLSR_DEBUG(LOG_MPRS, "MPRS: adding %s\n", olsr_ip_to_string(&buf, addr));

    /* Fill struct */
    mprs->MS_main_addr = *addr;

    /* Queue */
    mprs->mprs_node.key = &mprs->MS_main_addr;
    avl_insert(&mprs_tree, &mprs->mprs_node, AVL_DUP_NO);

    signal_link_changes(true);
    rv = 1;
  } else {
    OLSR_DEBUG(LOG_MPRS, "MPRS: Update %s\n", olsr_ip_to_string(&buf, addr));
    rv = 0;
  }
  olsr_set_timer(&mprs->MS_timer,
                 vtime, OLSR_MPR_SEL_JITTER, OLSR_TIMER_ONESHOT, &olsr_expire_mpr_sel_entry, mprs, mpr_sel_timer_cookie->ci_id);
  return rv;
}


/**
 *Print the current MPR selector set to STDOUT
 */
void
olsr_print_mprs_set(void)
{
#if !defined REMOVE_LOG_INFO
  struct ipaddr_str buf;
  struct mpr_selector *mprs;

  OLSR_INFO(LOG_MPRS, "MPR SELECTORS:\n");

  OLSR_FOR_ALL_MPRS_ENTRIES(mprs) {
    OLSR_INFO_NH(LOG_MPRS, "\t%s\n", olsr_ip_to_string(&buf, &mprs->MS_main_addr));
  } OLSR_FOR_ALL_MPRS_ENTRIES_END(mprs);
#endif
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

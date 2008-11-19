/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004, Andreas Tonnesen(andreto@olsr.org)
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

#define OLSR_MPR_SEL_JITTER 5 /* percent */

olsr_u16_t ansn = 0;

static struct olsr_cookie_info *mpr_sel_timer_cookie;

/* MPR selector list */
static struct list_node mprs_list_head;

/* inline to recast from link_list back to link_entry */
LISTNODE2STRUCT(list2mpr, struct mpr_selector, mprs_list);

#define FOR_ALL_MPRS_ENTRIES(elem)	\
{ \
  struct list_node *elem_node, *next_elem_node; \
  for (elem_node = mprs_list_head.next;			 \
       elem_node != &mprs_list_head; /* circular list */ \
       elem_node = next_elem_node) { \
    next_elem_node = elem_node->next; \
    elem = list2mpr(elem_node);

#define FOR_ALL_MPRS_ENTRIES_END(elem) }}


void
olsr_init_mprs(void)
{
  list_head_init(&mprs_list_head);

  /*
   * Get some cookies for getting stats to ease troubleshooting.
   */
  mpr_sel_timer_cookie =
    olsr_alloc_cookie("MPR Selector", OLSR_COOKIE_TYPE_TIMER);
}

#if 0
/**
 * Check if we(this node) is selected as a MPR by any
 * neighbors. If the list is empty we are not MPR.
 */
olsr_bool
olsr_is_mpr(void)
{
    return mprs_list.next == &mprs_list ? OLSR_FALSE : OLSR_TRUE;
}
#endif


/**
 * Wrapper for the timer callback.
 */
static void
olsr_expire_mpr_sel_entry(void *context)
{
  struct mpr_selector *mpr_sel = context;
#ifdef DEBUG
  struct ipaddr_str buf;
  OLSR_PRINTF(1, "MPRS: Timing out %st\n",
              olsr_ip_to_string(&buf, &mpr_sel->MS_main_addr));
#endif
  mpr_sel->MS_timer = NULL;

  list_remove(&mpr_sel->mprs_list);

  /* Delete entry */
  free(mpr_sel);
  signal_link_changes(OLSR_TRUE);
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
  struct mpr_selector *mprs;

  if (addr == NULL) {
    return NULL;
  }
  //OLSR_PRINTF(1, "MPRS: Lookup....");
  FOR_ALL_MPRS_ENTRIES(mprs) {
    if (ipequal(&mprs->MS_main_addr, addr)) {
      //OLSR_PRINTF(1, "MATCH\n");
      return mprs;
    }
  } FOR_ALL_MPRS_ENTRIES_END(mprs);
  //OLSR_PRINTF(1, "NO MACH\n");
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
  struct ipaddr_str buf;
  struct mpr_selector *mprs = olsr_lookup_mprs_set(addr);

  if (mprs == NULL) {
    mprs = olsr_malloc(sizeof(*mprs), "Add MPR selector");

    OLSR_PRINTF(1, "MPRS: adding %s\n", olsr_ip_to_string(&buf, addr));

    /* Fill struct */
    mprs->MS_main_addr = *addr;

    /* Queue */
    list_add_before(&mprs_list_head, &mprs->mprs_list);

    signal_link_changes(OLSR_TRUE);
    rv = 1;
  } else {
    OLSR_PRINTF(5, "MPRS: Update %s\n", olsr_ip_to_string(&buf, addr));
    rv = 0;
  }
  olsr_set_timer(&mprs->MS_timer,
		 vtime,
		 OLSR_MPR_SEL_JITTER,
                 OLSR_TIMER_ONESHOT,
		 &olsr_expire_mpr_sel_entry,
		 mprs,
		 mpr_sel_timer_cookie->ci_id);
  return rv;
}


#if 0
/**
 *Print the current MPR selector set to STDOUT
 */
void
olsr_print_mprs_set(void)
{
  struct mpr_selector *mprs;
  OLSR_PRINTF(1, "MPR SELECTORS: ");
  FOR_ALL_MPRS_ENTRIES(mprs) {
    struct ipaddr_str buf;
    OLSR_PRINTF(1, "%s ", olsr_ip_to_string(&buf, &mprs->MS_main_addr));
  } FOR_ALL_MPRS_ENTRIES_END(mprs);
  OLSR_PRINTF(1, "\n");
}
#endif

/*
 * Local Variables:
 * c-basic-offset: 2
 * End:
 */

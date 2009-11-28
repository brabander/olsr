
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

#include "duplicate_set.h"
#include "ipcalc.h"
#include "common/avl.h"
#include "olsr.h"
#include "mid_set.h"
#include "scheduler.h"
#include "olsr_time.h"
#include "olsr_cookie.h"
#include "olsr_logging.h"

#include <stdlib.h>

struct avl_tree forward_set, processing_set;

/* Some cookies for stats keeping */
static struct olsr_cookie_info *duplicate_timer_cookie = NULL;
static struct olsr_cookie_info *duplicate_mem_cookie = NULL;

int
olsr_seqno_diff(uint16_t reference, uint16_t other)
{
  int diff;

  diff = (int)reference - (int)other;

  // overflow ?
  if (diff >= (1 << 15)) {
    diff -= (1 << 16);
  } else if (diff < -(1 << 15)) {
    diff += (1 << 16);
  }
  return diff;
}

void
olsr_init_duplicate_set(void)
{
  OLSR_INFO(LOG_DUPLICATE_SET, "Initialize duplicate set...\n");

  avl_init(&forward_set, avl_comp_default);
  avl_init(&processing_set, avl_comp_default);

  /*
   * Get some cookies for getting stats to ease troubleshooting.
   */
  duplicate_timer_cookie = olsr_alloc_cookie("Duplicate Set", OLSR_COOKIE_TYPE_TIMER);

  duplicate_mem_cookie = olsr_alloc_cookie("dup_entry", OLSR_COOKIE_TYPE_MEMORY);
  olsr_cookie_set_memory_size(duplicate_mem_cookie, sizeof(struct dup_entry));
}

static struct dup_entry *
olsr_create_duplicate_entry(union olsr_ip_addr *ip, uint16_t seqnr)
{
  struct dup_entry *entry;

  entry = olsr_cookie_malloc(duplicate_mem_cookie);
  if (entry != NULL) {
    memcpy(&entry->ip, ip, olsr_cnf->ip_version == AF_INET ? sizeof(entry->ip.v4) : sizeof(entry->ip.v6));
    entry->seqnr = seqnr;
    entry->too_low_counter = 0;
    entry->avl.key = &entry->ip;
    entry->array = 0;
  }
  return entry;
}

static void
olsr_delete_duplicate_entry(struct dup_entry *entry)
{
  avl_delete(entry->tree, &entry->avl);
  entry->tree = NULL;
  olsr_stop_timer(entry->validity_timer);
  entry->validity_timer = NULL;
  olsr_cookie_free(duplicate_mem_cookie, entry);
}

static void
olsr_expire_duplicate_entry(void *context)
{
  struct dup_entry *entry = context;

  olsr_delete_duplicate_entry(entry);
}

/**
 * Clean up the house. Called during shutdown.
 */
void
olsr_flush_duplicate_entries(void)
{
  struct dup_entry *entry;

  OLSR_FOR_ALL_FORWARD_DUP_ENTRIES(entry) {
    olsr_delete_duplicate_entry(entry);
  } OLSR_FOR_ALL_FORWARD_DUP_ENTRIES_END()
    OLSR_FOR_ALL_PROCESS_DUP_ENTRIES(entry) {
    olsr_delete_duplicate_entry(entry);
  } OLSR_FOR_ALL_PROCESS_DUP_ENTRIES_END()
}

bool
olsr_is_duplicate_message(struct olsr_message *m, bool forwarding, enum duplicate_status *status)
{
  struct avl_tree *tree;
  struct dup_entry *entry;
  int diff;
  uint32_t valid_until;
  enum duplicate_status dummy = 0;

#if !defined(REMOVE_LOG_DEBUG)
  struct ipaddr_str buf;
#endif

  if (status == NULL) {
    status = &dummy;
  }

  tree = forwarding ? &forward_set : &processing_set;

  valid_until = GET_TIMESTAMP(DUPLICATE_VTIME);

  /* Check if entry exists */
  entry = (struct dup_entry *)avl_find(tree, &m->originator);
  if (entry == NULL) {
    entry = olsr_create_duplicate_entry(&m->originator, m->seqno);
    if (entry != NULL) {
      avl_insert(tree, &entry->avl, 0);
      entry->tree = tree;
      entry->validity_timer = olsr_start_timer(DUPLICATE_CLEANUP_INTERVAL, DUPLICATE_CLEANUP_JITTER,
                                               OLSR_TIMER_ONESHOT, &olsr_expire_duplicate_entry, entry, duplicate_timer_cookie);
    }

    *status = NEW_OLSR_MESSAGE;
    return false;               // okay, we process this package
  } else {

    /*
     * Refresh timer.
     */
    olsr_change_timer(entry->validity_timer, DUPLICATE_CLEANUP_INTERVAL, DUPLICATE_CLEANUP_JITTER, OLSR_TIMER_ONESHOT);
  }

  diff = olsr_seqno_diff(m->seqno, entry->seqnr);

  if (diff < -31) {
    entry->too_low_counter++;

    // client did restart with a lower number ?
    if (entry->too_low_counter > 16) {
      entry->too_low_counter = 0;
      entry->seqnr = m->seqno;
      entry->array = 1;

      /* start with a new sequence number, so NO duplicate */
      *status = RESET_SEQNO_OLSR_MESSAGE;
      return false;
    }
    OLSR_DEBUG(LOG_DUPLICATE_SET, "blocked %x from %s\n", m->seqno, olsr_ip_to_string(&buf, &m->originator));

    /* much too old */
    *status = TOO_OLD_OLSR_MESSAGE;
    return true;
  }

  entry->too_low_counter = 0;
  if (diff <= 0) {
    uint32_t bitmask = 1 << ((uint32_t) (-diff));

    if ((entry->array & bitmask) != 0) {
      OLSR_DEBUG(LOG_DUPLICATE_SET, "blocked %x (diff=%d,mask=%08x) from %s\n", m->seqno, diff,
                 entry->array, olsr_ip_to_string(&buf, &m->originator));

      /* duplicate ! */
      *status = DUPLICATE_OLSR_MESSAGE;
      return true;
    }
    entry->array |= bitmask;
    OLSR_DEBUG(LOG_DUPLICATE_SET, "processed %x from %s\n", m->seqno, olsr_ip_to_string(&buf, &m->originator));

    /* no duplicate */
    *status = OLD_OLSR_MESSAGE;
    return false;
  } else if (diff < 32) {
    entry->array <<= (uint32_t) diff;
  } else {
    entry->array = 0;
  }
  entry->array |= 1;
  entry->seqnr = m->seqno;
  OLSR_DEBUG(LOG_DUPLICATE_SET, "processed %x from %s\n", m->seqno, olsr_ip_to_string(&buf, &m->originator));

  /* no duplicate */
  *status = NEW_OLSR_MESSAGE;
  return false;
}

void
olsr_print_duplicate_table(void)
{
#if !defined REMOVE_LOG_INFO
  /* The whole function makes no sense without it. */
  struct dup_entry *entry;
  const int ipwidth = olsr_cnf->ip_version == AF_INET ? 15 : 30;

  OLSR_INFO(LOG_DUPLICATE_SET, "\n--- %s ------------------------------------------------- DUPLICATE SET (forwarding)\n\n",
            olsr_wallclock_string());
  OLSR_INFO_NH(LOG_DUPLICATE_SET, "%-*s %8s %s\n", ipwidth, "Node IP", "DupArray", "VTime");

  OLSR_FOR_ALL_FORWARD_DUP_ENTRIES(entry) {
    struct ipaddr_str addrbuf;
    OLSR_INFO_NH(LOG_DUPLICATE_SET, "%-*s %08x %s\n",
                 ipwidth, olsr_ip_to_string(&addrbuf, entry->avl.key), entry->array,
                 olsr_clock_string(entry->validity_timer->timer_clock));
  } OLSR_FOR_ALL_FORWARD_DUP_ENTRIES_END()

    OLSR_INFO(LOG_DUPLICATE_SET, "\n--- %s ------------------------------------------------- DUPLICATE SET (processing)\n\n",
              olsr_wallclock_string());
  OLSR_INFO_NH(LOG_DUPLICATE_SET, "%-*s %8s %s\n", ipwidth, "Node IP", "DupArray", "VTime");

  OLSR_FOR_ALL_PROCESS_DUP_ENTRIES(entry) {
    struct ipaddr_str addrbuf;
    OLSR_INFO_NH(LOG_DUPLICATE_SET, "%-*s %08x %s\n",
                 ipwidth, olsr_ip_to_string(&addrbuf, entry->avl.key), entry->array,
                 olsr_clock_string(entry->validity_timer->timer_clock));
  } OLSR_FOR_ALL_PROCESS_DUP_ENTRIES_END()
#endif
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

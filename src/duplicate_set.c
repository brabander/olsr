
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
#include "mantissa.h"
#include "olsr_cookie.h"
#include "olsr_logging.h"

#include <stdlib.h>

static void olsr_cleanup_duplicate_entry(void *unused);

static struct avl_tree duplicate_set;
static struct timer_entry *duplicate_cleanup_timer;

/* Some cookies for stats keeping */
static struct olsr_cookie_info *duplicate_timer_cookie = NULL;
static struct olsr_cookie_info *duplicate_mem_cookie = NULL;

void
olsr_init_duplicate_set(void)
{
  OLSR_INFO(LOG_DUPLICATE_SET, "Initialize duplicate set...\n");

  avl_init(&duplicate_set, olsr_cnf->ip_version == AF_INET ? &avl_comp_ipv4 : &avl_comp_ipv6);

  /*
   * Get some cookies for getting stats to ease troubleshooting.
   */
  duplicate_timer_cookie = olsr_alloc_cookie("Duplicate Set", OLSR_COOKIE_TYPE_TIMER);

  duplicate_mem_cookie = olsr_alloc_cookie("dup_entry", OLSR_COOKIE_TYPE_MEMORY);
  olsr_cookie_set_memory_size(duplicate_mem_cookie, sizeof(struct dup_entry));


  olsr_set_timer(&duplicate_cleanup_timer, DUPLICATE_CLEANUP_INTERVAL,
                 DUPLICATE_CLEANUP_JITTER, OLSR_TIMER_PERIODIC, &olsr_cleanup_duplicate_entry, NULL, duplicate_timer_cookie->ci_id);
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
  avl_delete(&duplicate_set, &entry->avl);
  olsr_cookie_free(duplicate_mem_cookie, entry);
}

static void
olsr_cleanup_duplicate_entry(void __attribute__ ((unused)) * unused)
{
  struct dup_entry *entry;

  OLSR_FOR_ALL_DUP_ENTRIES(entry) {
    if (TIMED_OUT(entry->valid_until)) {
      olsr_delete_duplicate_entry(entry);
    }
  }
  OLSR_FOR_ALL_DUP_ENTRIES_END(entry);
}

/**
 * Clean up the house. Called during shutdown.
 */
void
olsr_flush_duplicate_entries(void)
{
  struct dup_entry *entry;

  OLSR_FOR_ALL_DUP_ENTRIES(entry) {
    olsr_delete_duplicate_entry(entry);
  } OLSR_FOR_ALL_DUP_ENTRIES_END(entry);

  olsr_stop_timer(duplicate_cleanup_timer);
  duplicate_cleanup_timer = NULL;
}

int
olsr_message_is_duplicate(union olsr_message *m)
{
  struct dup_entry *entry;
  int diff;
  union olsr_ip_addr *mainIp;
  uint32_t valid_until;
  uint16_t seqnr;
  union olsr_ip_addr *ip;
#if !defined(REMOVE_LOG_DEBUG)
  struct ipaddr_str buf;
#endif

  if (olsr_cnf->ip_version == AF_INET) {
    seqnr = ntohs(m->v4.seqno);
    ip = (union olsr_ip_addr *)&m->v4.originator;
  } else {
    seqnr = ntohs(m->v6.seqno);
    ip = (union olsr_ip_addr *)&m->v6.originator;
  }

  // get main address
  mainIp = olsr_lookup_main_addr_by_alias(ip);
  if (mainIp == NULL) {
    mainIp = ip;
  }

  valid_until = GET_TIMESTAMP(DUPLICATE_VTIME);

  entry = (struct dup_entry *)avl_find(&duplicate_set, ip);
  if (entry == NULL) {
    entry = olsr_create_duplicate_entry(ip, seqnr);
    if (entry != NULL) {
      avl_insert(&duplicate_set, &entry->avl, 0);
      entry->valid_until = valid_until;
    }
    return false;               // okay, we process this package
  }

  diff = (int)seqnr - (int)(entry->seqnr);

  // update timestamp
  if (valid_until > entry->valid_until) {
    entry->valid_until = valid_until;
  }
  // overflow ?
  if (diff > (1 << 15)) {
    diff -= (1 << 16);
  }

  if (diff < -31) {
    entry->too_low_counter++;

    // client did restart with a lower number ?
    if (entry->too_low_counter > 16) {
      entry->too_low_counter = 0;
      entry->seqnr = seqnr;
      entry->array = 1;
      return false;             /* start with a new sequence number, so NO duplicate */
    }
    OLSR_DEBUG(LOG_DUPLICATE_SET, "blocked %x from %s\n", seqnr, olsr_ip_to_string(&buf, mainIp));
    return true;                /* duplicate ! */
  }

  entry->too_low_counter = 0;
  if (diff <= 0) {
    uint32_t bitmask = 1 << ((uint32_t) (-diff));

    if ((entry->array & bitmask) != 0) {
      OLSR_DEBUG(LOG_DUPLICATE_SET, "blocked %x (diff=%d,mask=%08x) from %s\n", seqnr, diff,
                 entry->array, olsr_ip_to_string(&buf, mainIp));
      return true;              /* duplicate ! */
    }
    entry->array |= bitmask;
    OLSR_DEBUG(LOG_DUPLICATE_SET, "processed %x from %s\n", seqnr, olsr_ip_to_string(&buf, mainIp));
    return false;               /* no duplicate */
  } else if (diff < 32) {
    entry->array <<= (uint32_t) diff;
  } else {
    entry->array = 0;
  }
  entry->array |= 1;
  entry->seqnr = seqnr;
  OLSR_DEBUG(LOG_DUPLICATE_SET, "processed %x from %s\n", seqnr, olsr_ip_to_string(&buf, mainIp));
  return false;                 /* no duplicate */
}

void
olsr_print_duplicate_table(void)
{
#if !defined REMOVE_LOG_INFO
  /* The whole function makes no sense without it. */
  struct dup_entry *entry;
  const int ipwidth = olsr_cnf->ip_version == AF_INET ? 15 : 30;

  OLSR_INFO(LOG_DUPLICATE_SET, "\n--- %s ------------------------------------------------- DUPLICATE SET\n\n",
            olsr_wallclock_string());
  OLSR_INFO_NH(LOG_DUPLICATE_SET, "%-*s %8s %s\n", ipwidth, "Node IP", "DupArray", "VTime");

  OLSR_FOR_ALL_DUP_ENTRIES(entry) {
    struct ipaddr_str addrbuf;
    OLSR_INFO_NH(LOG_DUPLICATE_SET, "%-*s %08x %s\n",
                 ipwidth, olsr_ip_to_string(&addrbuf, entry->avl.key), entry->array, olsr_clock_string(entry->valid_until));
  } OLSR_FOR_ALL_DUP_ENTRIES_END(entry);
#endif
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

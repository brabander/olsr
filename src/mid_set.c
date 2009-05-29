
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

#include "ipcalc.h"
#include "defs.h"
#include "two_hop_neighbor_table.h"
#include "mid_set.h"
#include "olsr.h"
#include "scheduler.h"
#include "neighbor_table.h"
#include "link_set.h"
#include "tc_set.h"
#include "net_olsr.h"
#include "olsr_cookie.h"
#include "olsr_logging.h"

#include <stdlib.h>

static struct mid_entry *olsr_lookup_mid_entry(const union olsr_ip_addr *);
static void olsr_prune_mid_entries(const union olsr_ip_addr *main_addr, uint16_t mid_seqno);

/* Root of the MID tree */
struct avl_tree mid_tree;

/* Some cookies for stats keeping */
static struct olsr_cookie_info *mid_validity_timer_cookie = NULL;
static struct olsr_cookie_info *mid_address_mem_cookie = NULL;

/**
 * Initialize the MID set
 */
void
olsr_init_mid_set(void)
{
  OLSR_INFO(LOG_MID, "Initialize MID set...\n");

  avl_init(&mid_tree, avl_comp_default);

  /*
   * Get some cookies for getting stats to ease troubleshooting.
   */
  mid_validity_timer_cookie = olsr_alloc_cookie("MID validity", OLSR_COOKIE_TYPE_TIMER);

  mid_address_mem_cookie = olsr_alloc_cookie("MID address", OLSR_COOKIE_TYPE_MEMORY);
  olsr_cookie_set_memory_size(mid_address_mem_cookie, sizeof(struct mid_entry));
}

/**
 * Wrapper for the timer callback.
 */
static void
olsr_expire_mid_entries(void *context)
{
  struct tc_entry *tc = context;
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif

  OLSR_DEBUG(LOG_MID, "MID aliases for %s timed out\n", olsr_ip_to_string(&buf, &tc->addr));

  olsr_flush_mid_entries(tc);
}

/**
 * Set the mid set expiration timer.
 *
 * all timer setting shall be done using this function.
 * The timer param is a relative timer expressed in milliseconds.
 */
static void
olsr_set_mid_timer(struct tc_entry *tc, olsr_reltime rel_timer)
{
  olsr_set_timer(&tc->mid_timer, rel_timer, OLSR_MID_JITTER,
                 OLSR_TIMER_ONESHOT, &olsr_expire_mid_entries, tc, mid_validity_timer_cookie->ci_id);
}

/**
 * Delete possible duplicate entries in 2 hop set
 * and delete duplicate neighbor entries. Redirect
 * link entries to the correct neighbor entry.
 * This optimization is not specified in rfc3626.
 */
static void
olsr_flush_nbr2_duplicates(struct mid_entry *alias)
{
  struct tc_entry *tc = alias->mid_tc;
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf1, buf2;
#endif

  OLSR_FOR_ALL_TC_MID_ENTRIES(tc, alias) {
    struct nbr_entry *nbr;
    struct nbr2_entry *nbr2 = olsr_lookup_nbr2_entry_alias(&alias->mid_alias_addr);

    /* Delete possible 2 hop neighbor */
    if (nbr2) {
      OLSR_DEBUG(LOG_MID, "Delete 2hop neighbor: %s to %s\n",
                 olsr_ip_to_string(&buf1, &alias->mid_alias_addr), olsr_ip_to_string(&buf2, &tc->addr));

      olsr_delete_nbr2_entry(nbr2);
      changes_neighborhood = true;
    }

    /* Delete a possible neighbor entry */
    nbr = olsr_lookup_nbr_entry_alias(&alias->mid_alias_addr);
    if (nbr) {
      struct nbr_entry *real_nbr = olsr_lookup_nbr_entry_alias(&tc->addr);
      if (real_nbr) {

        OLSR_DEBUG(LOG_MID, "Delete bogus neighbor entry %s (real %s)\n",
                   olsr_ip_to_string(&buf1, &alias->mid_alias_addr), olsr_ip_to_string(&buf2, &tc->addr));

        replace_neighbor_link_set(nbr, real_nbr);

        olsr_delete_nbr_entry(&alias->mid_alias_addr);

        changes_neighborhood = true;
      }
    }
  }
  OLSR_FOR_ALL_TC_MID_ENTRIES_END(tc, mid_alias);
}

/**
 * If we have an entry for this alias in neighbortable, we better adjust it's
 * main address, because otherwise a fatal inconsistency between
 * neighbortable and link_set will be created by way of this mid entry.
 */
static void
olsr_fixup_mid_main_addr(const union olsr_ip_addr *main_addr, const union olsr_ip_addr *alias_addr)
{
  struct nbr_entry *nbr_new, *nbr_old = olsr_lookup_nbr_entry_alias(alias_addr);
  struct mid_entry *mid_old;
  int ne_ref_rp_count;
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf1, buf2;
#endif

  if (!nbr_old) {
    return;
  }

  OLSR_DEBUG(LOG_MID, "Main address change %s -> %s detected.\n",
             olsr_ip_to_string(&buf1, alias_addr), olsr_ip_to_string(&buf2, main_addr));

  olsr_delete_nbr_entry(alias_addr);
  nbr_new = olsr_add_nbr_entry(main_addr);

  /* Adjust pointers to neighbortable-entry in link_set */
  ne_ref_rp_count = replace_neighbor_link_set(nbr_old, nbr_new);

  if (ne_ref_rp_count > 0) {
    OLSR_DEBUG(LOG_MID, "Performed %d neighbortable-pointer replacements "
               "(%p -> %p) in link_set.\n", ne_ref_rp_count, nbr_old, nbr_new);
  }

  mid_old = olsr_lookup_mid_entry(alias_addr);
  if (mid_old) {
    /*
     * We knew aliases to the previous main address.
     * Better forget about them now.
     */
    OLSR_DEBUG(LOG_MID, "Flush aliases for old main address.\n");
    olsr_flush_mid_entries(mid_old->mid_tc);
  }
}

/**
 * Insert a fresh alias address for a node.
 *
 * @param main_add the main address of the node
 * @param alias the alias address to insert
 * @param vtime the validity time
 * @param seq the sequence number to register a new node with
 */
static struct mid_entry *
olsr_insert_mid_entry(const union olsr_ip_addr *main_addr,
                      const union olsr_ip_addr *alias_addr, olsr_reltime vtime, uint16_t mid_seqno)
{
  struct tc_entry *tc;
  struct mid_entry *alias;
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf1, buf2;
#endif

  OLSR_DEBUG(LOG_MID, "Inserting alias %s for %s\n", olsr_ip_to_string(&buf1, alias_addr), olsr_ip_to_string(&buf2, main_addr));

  /*
   * Locate first the hookup point
   */
  tc = olsr_locate_tc_entry(main_addr);

  alias = olsr_cookie_malloc(mid_address_mem_cookie);
  alias->mid_alias_addr = *alias_addr;
  alias->mid_tc = tc;
  olsr_lock_tc_entry(tc);

  /*
   * Insert into the per-tc mid subtree.
   */
  alias->mid_tc_node.key = &alias->mid_alias_addr;
  avl_insert(&tc->mid_tree, &alias->mid_tc_node, AVL_DUP_NO);

  /*
   * Insert into the global mid tree.
   */
  alias->mid_node.key = &alias->mid_alias_addr;
  avl_insert(&mid_tree, &alias->mid_node, AVL_DUP_NO);

  /*
   * Add a rt_path for the alias.
   */
  olsr_insert_routing_table(&alias->mid_alias_addr, 8 * olsr_cnf->ipsize, main_addr, OLSR_RT_ORIGIN_MID);

  /*
   * Start the timer. Because we provide the TC reference
   * as callback data for the timer we need to lock
   * the underlying TC entry again.
   */
  olsr_set_mid_timer(alias->mid_tc, vtime);
  olsr_lock_tc_entry(tc);

  /* Set sequence number for alias purging */
  alias->mid_seqno = mid_seqno;

  return alias;
}

/**
 * Update an alias address for a node.
 * If the main address is not registered
 * then a new entry is created.
 *
 * @param main_add the main address of the node
 * @param alias the alias address to insert
 * @param vtime the validity time
 * @param seq the sequence number to register a new node with
 */
static void
olsr_update_mid_entry(const union olsr_ip_addr *main_addr,
                      const union olsr_ip_addr *alias_addr, olsr_reltime vtime, uint16_t mid_seqno)
{
  struct mid_entry *alias;

  if (!olsr_validate_address(alias_addr)) {
    return;
  }

  /*
   * Check first if the alias already exists.
   */
  alias = olsr_lookup_mid_entry(alias_addr);
  if (alias) {
    /* Update sequence number for alias purging */
    alias->mid_seqno = mid_seqno;

    /* XXX handle main IP address changes */

    /* Refresh the timer. */
    olsr_set_mid_timer(alias->mid_tc, vtime);
    return;
  }

  /*
   * This is a fresh alias.
   */
  alias = olsr_insert_mid_entry(main_addr, alias_addr, vtime, mid_seqno);

  /*
   * Do the needful if one of our neighbors has changed its main address.
   */
  olsr_fixup_mid_main_addr(main_addr, alias_addr);
  olsr_flush_nbr2_duplicates(alias);

  /*
   * Recalculate topology.
   */
  changes_neighborhood = true;
  changes_topology = true;
}

/**
 * Lookup a MID alias hanging off a tc_entry by address
 *
 * @param adr the alias address to check
 * @return the MID address entry or NULL if not found
 */
struct mid_entry *
olsr_lookup_tc_mid_entry(struct tc_entry *tc, const union olsr_ip_addr *adr)
{
  return (alias_tree2mid(avl_find(&tc->mid_tree, adr)));
}

/**
 * Lookup an MID alias by address in the global tree.
 *
 * @param adr the alias address to check
 * @return the MID address entry or NULL if not found
 */
static struct mid_entry *
olsr_lookup_mid_entry(const union olsr_ip_addr *adr)
{
  return (global_tree2mid(avl_find(&mid_tree, adr)));
}

/**
 * Lookup the main address for an MID alias address
 *
 * @param adr the alias address to check
 * @return the main address registered on the alias
 * or NULL if not found
 */
union olsr_ip_addr *
olsr_lookup_main_addr_by_alias(const union olsr_ip_addr *adr)
{
  struct mid_entry *alias;

  alias = olsr_lookup_mid_entry(adr);

  return (alias ? &alias->mid_tc->addr : NULL);
}

/**
 * Delete a single MID alias.
 *
 * @param alias the alias to delete.
 */
void
olsr_delete_mid_entry(struct mid_entry *alias)
{
  struct tc_entry *tc;

  tc = alias->mid_tc;

  /*
   * Delete the rt_path for the alias.
   */
  olsr_delete_routing_table(&alias->mid_alias_addr, 8 * olsr_cnf->ipsize, &tc->addr, OLSR_RT_ORIGIN_MID);

  /*
   * Remove from the per-tc tree.
   */
  avl_delete(&tc->mid_tree, &alias->mid_tc_node);

  /*
   * Remove from the global tree.
   */
  avl_delete(&mid_tree, &alias->mid_node);

  olsr_unlock_tc_entry(tc);

  olsr_cookie_free(mid_address_mem_cookie, alias);
}

/**
 * Delete all the MID aliases hanging off a tc entry.
 *
 * @param entry the tc entry holding the aliases.
 */
void
olsr_flush_mid_entries(struct tc_entry *tc)
{
  struct mid_entry *alias;

  OLSR_FOR_ALL_TC_MID_ENTRIES(tc, alias) {
    olsr_delete_mid_entry(alias);
  } OLSR_FOR_ALL_TC_MID_ENTRIES_END(tc, alias);

  /*
   * Kill any pending timers.
   */
  olsr_set_mid_timer(tc, 0);
  olsr_unlock_tc_entry(tc);
}

/**
 * Remove aliases from 'entry' which are not matching
 * the most recent message sequence number. This gets
 * called after receiving a MID message for garbage
 * collection of the old entries.
 *
 * @param main_addr the root of MID entries.
 * @param mid_seqno the most recent message sequence number
 */
static void
olsr_prune_mid_entries(const union olsr_ip_addr *main_addr, uint16_t mid_seqno)
{
  struct tc_entry *tc = olsr_locate_tc_entry(main_addr);
  struct mid_entry *alias;
  OLSR_FOR_ALL_TC_MID_ENTRIES(tc, alias) {
    if (alias->mid_seqno != mid_seqno) {
      olsr_delete_mid_entry(alias);
    }
  }
  OLSR_FOR_ALL_TC_MID_ENTRIES_END(tc, alias);
}

/**
 * Print all MID entries
 * For debuging purposes
 */
void
olsr_print_mid_set(void)
{
#if !defined REMOVE_LOG_INFO
  struct tc_entry *tc;
  struct mid_entry *alias;
  struct ipaddr_str buf1, buf2;

  OLSR_INFO(LOG_MID, "\n--- %s ------------------------------------------------- MID\n\n", olsr_wallclock_string());

  OLSR_FOR_ALL_TC_ENTRIES(tc) {
    bool first = true;
    OLSR_FOR_ALL_TC_MID_ENTRIES(tc, alias) {
      OLSR_INFO_NH(LOG_MID, "%-15s: %s\n",
                   first ? olsr_ip_to_string(&buf1, &tc->addr) : "", olsr_ip_to_string(&buf2, &alias->mid_alias_addr));
      first = false;
    } OLSR_FOR_ALL_TC_MID_ENTRIES_END(tc, alias);
  } OLSR_FOR_ALL_TC_ENTRIES_END(tc);
#endif
}

/**
 * Process an incoming MID message.
 */
bool
olsr_input_mid(union olsr_message *msg, struct interface *input_if __attribute__ ((unused)), union olsr_ip_addr *from_addr)
{
  uint16_t msg_size, msg_seq;
  uint8_t type, ttl, msg_hops;
  const unsigned char *curr;
  olsr_reltime vtime;
  union olsr_ip_addr originator, alias;
  int alias_count;
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif

  curr = (void *)msg;
  if (!msg) {
    return false;
  }

  /* We are only interested in MID message types. */
  pkt_get_u8(&curr, &type);
  if (type != MID_MESSAGE) {
    return false;
  }

  pkt_get_reltime(&curr, &vtime);
  pkt_get_u16(&curr, &msg_size);

  pkt_get_ipaddress(&curr, &originator);

  /* Copy header values */
  pkt_get_u8(&curr, &ttl);
  pkt_get_u8(&curr, &msg_hops);
  pkt_get_u16(&curr, &msg_seq);

  if (!olsr_validate_address(&originator)) {
    return false;
  }

  /*
   * If the sender interface (NB: not originator) of this message
   * is not in the symmetric 1-hop neighborhood of this node, the
   * message MUST be discarded.
   */
  if (check_neighbor_link(from_addr) != SYM_LINK) {
    OLSR_DEBUG(LOG_MID, "Received MID from NON SYM neighbor %s\n", olsr_ip_to_string(&buf, from_addr));
    return false;
  }


  /*
   * How many aliases ?
   */
  alias_count = (msg_size - 12) / olsr_cnf->ipsize;

  OLSR_DEBUG(LOG_MID, "Processing MID from %s with %d aliases, seq 0x%04x\n",
             olsr_ip_to_string(&buf, &originator), alias_count, msg_seq);
  /*
   * Now walk the list of alias advertisements one by one.
   */
  while (alias_count) {
    pkt_get_ipaddress(&curr, &alias);
    olsr_update_mid_entry(&originator, &alias, vtime, msg_seq);
    alias_count--;
  }

  /*
   * Prune the aliases that did not get refreshed by this advertisment.
   */
  olsr_prune_mid_entries(&originator, msg_seq);

  /* Forward the message */
  return true;
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

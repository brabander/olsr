
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
#include "mid_set.h"
#include "olsr.h"
#include "olsr_timer.h"
#include "olsr_socket.h"
#include "neighbor_table.h"
#include "link_set.h"
#include "tc_set.h"
#include "net_olsr.h"
#include "olsr_memcookie.h"
#include "olsr_logging.h"
#include "olsr_protocol.h"

#include <assert.h>
#include <stdlib.h>

static struct mid_entry *olsr_lookup_mid_entry(const union olsr_ip_addr *);
static void olsr_expire_mid_entries(void *context);

/* Root of the MID tree */
struct avl_tree mid_tree;

/* validity timer of MID entries */
static struct olsr_memcookie_info *mid_address_mem_cookie = NULL;
static struct olsr_timer_info *mid_validity_timer_info = NULL;

/**
 * Initialize the MID set
 */
void
olsr_init_mid_set(void)
{
  OLSR_INFO(LOG_MID, "Initialize MID set...\n");

  avl_init(&mid_tree, avl_comp_default, false, NULL);

  /*
   * Get some cookies for getting stats to ease troubleshooting.
   */
  mid_validity_timer_info = olsr_timer_add("MID validity", &olsr_expire_mid_entries, false);

  mid_address_mem_cookie = olsr_memcookie_add("MID address", sizeof(struct mid_entry));
}

/**
 * Wrapper for the timer callback.
 */
static void
olsr_expire_mid_entries(void *context)
{
  struct mid_entry *mid = context;
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif

  OLSR_DEBUG(LOG_MID, "MID aliases for %s timed out\n", olsr_ip_to_string(&buf, &mid->mid_alias_addr));

  olsr_delete_mid_entry(mid);
}

/**
 * Set the mid set expiration timer.
 *
 * all timer setting shall be done using this function.
 * The timer param is a relative timer expressed in milliseconds.
 */
static void
olsr_set_mid_timer(struct mid_entry *mid, uint32_t rel_timer)
{
  olsr_timer_set(&mid->mid_timer, rel_timer, OLSR_MID_JITTER, mid, mid_validity_timer_info);
}

/**
 * Delete possible duplicate entries in tc set.
 * This optimization is not specified in rfc3626.
 */
static void
olsr_flush_tc_duplicates(struct mid_entry *alias) {
  struct tc_entry *tc;
  tc = olsr_lookup_tc_entry(&alias->mid_alias_addr);
  if (tc) {
    olsr_delete_tc_entry(tc);
  }
}

/**
 * Delete possible duplicate entries in 2 hop set
 * and delete duplicate neighbor entries. Redirect
 * link entries to the correct neighbor entry.
 * This optimization is not specified in rfc3626.
 */
static void
olsr_flush_nbr2_duplicates(struct tc_entry *tc)
{
  struct mid_entry *alias, *iterator;
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf1, buf2;
#endif

  OLSR_FOR_ALL_TC_MID_ENTRIES(tc, alias, iterator) {
    struct nbr_entry *nbr;
    struct nbr2_entry *nbr2 = olsr_lookup_nbr2_entry(&alias->mid_alias_addr, false);

    /* Delete possible 2 hop neighbor */
    if (nbr2) {
      OLSR_DEBUG(LOG_MID, "Delete 2hop neighbor: %s to %s\n",
                 olsr_ip_to_string(&buf1, &alias->mid_alias_addr), olsr_ip_to_string(&buf2, &tc->addr));

      olsr_delete_nbr2_entry(nbr2);
      changes_neighborhood = true;
    }

    /* Delete a possible neighbor entry */
    nbr = olsr_lookup_nbr_entry(&alias->mid_alias_addr, false);
    if (nbr) {
      struct nbr_entry *real_nbr = olsr_lookup_nbr_entry(&tc->addr, false);
      if (real_nbr) {

        OLSR_DEBUG(LOG_MID, "Delete bogus neighbor entry %s (real %s)\n",
                   olsr_ip_to_string(&buf1, &alias->mid_alias_addr), olsr_ip_to_string(&buf2, &tc->addr));

        replace_neighbor_link_set(nbr, real_nbr);

        olsr_delete_nbr_entry(nbr);

        changes_neighborhood = true;
      }
    }
  }
}

/**
 * If we have an entry for this alias in neighbortable, we better adjust it's
 * main address, because otherwise a fatal inconsistency between
 * neighbortable and link_set will be created by way of this mid entry.
 */
static void
olsr_fixup_mid_main_addr(const union olsr_ip_addr *main_addr, const union olsr_ip_addr *alias_addr)
{
  struct nbr_entry *nbr_new, *nbr_old;
  struct mid_entry *mid_old;
  int ne_ref_rp_count;
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf1, buf2;
#endif

  nbr_old = olsr_lookup_nbr_entry(alias_addr, false);
  if (!nbr_old) {
    return;
  }

  OLSR_DEBUG(LOG_MID, "Main address change %s -> %s detected.\n",
             olsr_ip_to_string(&buf1, alias_addr), olsr_ip_to_string(&buf2, main_addr));

  olsr_delete_nbr_entry(nbr_old);
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
                      const union olsr_ip_addr *alias_addr, uint32_t vtime)
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
  assert(tc);

  alias = olsr_memcookie_malloc(mid_address_mem_cookie);
  alias->mid_alias_addr = *alias_addr;
  alias->mid_tc = tc;

  /*
   * Insert into the per-tc mid subtree.
   */
  alias->mid_tc_node.key = &alias->mid_alias_addr;
  avl_insert(&tc->mid_tree, &alias->mid_tc_node);

  /*
   * Insert into the global mid tree.
   */
  alias->mid_node.key = &alias->mid_alias_addr;
  avl_insert(&mid_tree, &alias->mid_node);

  /*
   * Add a rt_path for the alias.
   */
  olsr_insert_routing_table(&alias->mid_alias_addr, 8 * olsr_cnf->ipsize, main_addr, OLSR_RT_ORIGIN_MID);

  olsr_set_mid_timer(alias, vtime);

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
                      const union olsr_ip_addr *alias_addr, uint32_t vtime)
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
    /* Refresh the timer. */
    olsr_set_mid_timer(alias, vtime);
    return;
  }

  /*
   * This is a fresh alias.
   */
  alias = olsr_insert_mid_entry(main_addr, alias_addr, vtime);

  /*
   * Do the needful if one of our neighbors has changed its main address.
   */
  olsr_fixup_mid_main_addr(main_addr, alias_addr);
  olsr_flush_nbr2_duplicates(alias->mid_tc);
  olsr_flush_tc_duplicates(alias);

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
  struct mid_entry *mid;
  mid = avl_find_element(&tc->mid_tree, adr, mid, mid_tc_node);
  return mid;
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
  struct mid_entry *mid;
  mid = avl_find_element(&mid_tree, adr, mid, mid_node);
  return mid;
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

  /* kill timer */
  olsr_timer_stop(alias->mid_timer);
  alias->mid_timer = NULL;

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

  olsr_memcookie_free(mid_address_mem_cookie, alias);
}

/**
 * Delete all the MID aliases hanging off a tc entry.
 *
 * @param entry the tc entry holding the aliases.
 */
void
olsr_flush_mid_entries(struct tc_entry *tc)
{
  struct mid_entry *alias, *iterator;
  OLSR_FOR_ALL_TC_MID_ENTRIES(tc, alias, iterator) {
    olsr_delete_mid_entry(alias);
  }
}

/**
 * Print all MID entries
 * For debuging purposes
 */
void
olsr_print_mid_set(void)
{
#if !defined REMOVE_LOG_INFO
  struct tc_entry *tc, *tc_iterator;
  struct mid_entry *alias, *mid_iterator;
  struct ipaddr_str buf1, buf2;

  OLSR_INFO(LOG_MID, "\n--- %s ------------------------------------------------- MID\n\n", olsr_timer_getWallclockString());

  OLSR_FOR_ALL_TC_ENTRIES(tc, tc_iterator) {
    OLSR_FOR_ALL_TC_MID_ENTRIES(tc, alias, mid_iterator) {
      OLSR_INFO_NH(LOG_MID, "%-15s: %s\n", olsr_ip_to_string(&buf1, &tc->addr), olsr_ip_to_string(&buf2, &alias->mid_alias_addr));
    }
  }
#endif
}

/**
 * Process an incoming MID message.
 */
void
olsr_input_mid(struct olsr_message *msg,
    struct interface *input_if __attribute__ ((unused)),
    union olsr_ip_addr *from_addr, enum duplicate_status status)
{
  const uint8_t *curr;
  union olsr_ip_addr alias;
  struct tc_entry *tc;
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif

  /* We are only interested in MID message types. */
  if (msg->type != MID_MESSAGE) {
    return;
  }

  /*
   * If the sender interface (NB: not originator) of this message
   * is not in the symmetric 1-hop neighborhood of this node, the
   * message MUST be discarded.
   */
  if (check_neighbor_link(from_addr) != SYM_LINK) {
    OLSR_DEBUG(LOG_MID, "Received MID from NON SYM neighbor %s\n", olsr_ip_to_string(&buf, from_addr));
    return;
  }

  tc = olsr_locate_tc_entry(&msg->originator);

  if (status != RESET_SEQNO_OLSR_MESSAGE && tc->mid_seq != -1 && olsr_seqno_diff(msg->seqno, tc->mid_seq) <= 0) {
    /* this MID is too old, discard it */
    OLSR_DEBUG(LOG_MID, "Received too old mid from %s: %d < %d\n",
        olsr_ip_to_string(&buf, from_addr), msg->seqno, tc->mid_seq);
    return;
  }
  tc->mid_seq = msg->seqno;

  OLSR_DEBUG(LOG_MID, "Processing MID from %s with %d aliases, seq 0x%04x\n",
             olsr_ip_to_string(&buf, &msg->originator), (int)((msg->end - msg->payload)/olsr_cnf->ipsize), msg->seqno);


  curr = msg->payload;


  /*
   * Now walk the list of alias advertisements one by one.
   */
  while (curr + olsr_cnf->ipsize <= msg->end) {
    pkt_get_ipaddress(&curr, &alias);
    olsr_update_mid_entry(&msg->originator, &alias, msg->vtime);
  }
}

void
generate_mid(void *p  __attribute__ ((unused))) {
  struct interface *ifp, *allif, *iterator;
  struct olsr_message msg;
  uint8_t msg_buffer[MAXMESSAGESIZE - OLSR_HEADERSIZE] __attribute__ ((aligned));
  uint8_t *curr = msg_buffer;
  uint8_t *length_field, *last;
  bool sendMID = false;

  OLSR_INFO(LOG_PACKET_CREATION, "Building MID\n-------------------\n");

  msg.type = MID_MESSAGE;
  msg.vtime = olsr_cnf->mid_params.validity_time;
  msg.size = 0; // fill in later
  msg.originator = olsr_cnf->router_id;
  msg.ttl = MAX_TTL;
  msg.hopcnt = 0;
  msg.seqno = get_msg_seqno();

  curr = msg_buffer;

  length_field = olsr_put_msg_hdr(&curr, &msg);

  last = msg_buffer + sizeof(msg_buffer) - olsr_cnf->ipsize;
  OLSR_FOR_ALL_INTERFACES(allif, iterator) {
    if (olsr_ipcmp(&olsr_cnf->router_id, &allif->ip_addr) != 0) {
      if (curr > last) {
        OLSR_WARN(LOG_MID, "Warning, too many interfaces for MID packet\n");
        return;
      }
      pkt_put_ipaddress(&curr, &allif->ip_addr);
      sendMID = true;
    }
  }

  if (!sendMID) {
    return;
  }

  pkt_put_u16(&length_field, curr - msg_buffer);

  OLSR_FOR_ALL_INTERFACES(ifp, iterator) {
    if (net_outbuffer_bytes_left(ifp) < curr - msg_buffer) {
      net_output(ifp);
      set_buffer_timer(ifp);
    }
    net_outbuffer_push(ifp, msg_buffer, curr - msg_buffer);
  }
}
/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */


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

#include "process_package.h"
#include "link_set.h"
#include "hna_set.h"
#include "neighbor_table.h"
#include "mid_set.h"
#include "olsr.h"
#include "parser.h"
#include "olsr_logging.h"

static void olsr_input_hello(struct olsr_message *msg,
    struct interface *, union olsr_ip_addr *, enum duplicate_status);

static void process_message_neighbors(struct nbr_entry *, const struct lq_hello_message *);

static bool lookup_mpr_status(const struct lq_hello_message *, const struct interface *);

static void hello_tap(struct lq_hello_message *, struct interface *, const union olsr_ip_addr *);


/**
 * Processes an list of neighbors from an incoming HELLO message.
 * @param neighbor the neighbor who sent the message.
 * @param message the HELLO message
 * @return nada
 */
static void
process_message_neighbors(struct nbr_entry *neighbor, const struct lq_hello_message *message)
{
  struct lq_hello_neighbor *message_neighbors;
  struct link_entry *lnk;
  union olsr_ip_addr *neigh_addr;
  struct nbr_con *connector;

  lnk = get_best_link_to_neighbor(neighbor);

  /*
   * Walk our 2-hop neighbors.
   */
  for (message_neighbors = message->neigh; message_neighbors; message_neighbors = message_neighbors->next) {
    /*
     * We are only interested in symmetrical or MPR neighbors.
     */
    if (message_neighbors->neigh_type != SYM_NEIGH && message_neighbors->neigh_type != MPR_NEIGH) {
      continue;
    }

    /*
     * Check all interfaces such that we don't add ourselves to the 2 hop list.
     * IMPORTANT!
     */
    if (if_ifwithaddr(&message_neighbors->addr) != NULL) {
      continue;
    }

    /* Get the main address */
    neigh_addr = olsr_lookup_main_addr_by_alias(&message_neighbors->addr);
    if (neigh_addr == NULL) {
      neigh_addr = &message_neighbors->addr;
    }

    olsr_link_nbr_nbr2(neighbor, neigh_addr, message->comm->vtime);

    if (lnk) {
      connector = olsr_lookup_nbr_con_entry(neighbor, neigh_addr);

      connector->second_hop_linkcost = message_neighbors->cost;
      connector->path_linkcost = lnk->linkcost + message_neighbors->cost;

      changes_neighborhood = true;
    }
  }
}


/**
 * Check if a hello message states this node as a MPR.
 *
 * @param message the message to check
 * @param n_link the buffer to put the link status in
 *
 * @return 1 if we are selected as MPR 0 if not
 */
static bool
lookup_mpr_status(const struct lq_hello_message *message, const struct interface *in_if)
{
  struct lq_hello_neighbor *neighbors;

  for (neighbors = message->neigh; neighbors; neighbors = neighbors->next) {
    if ( neighbors->link_type != UNSPEC_LINK
        && olsr_ipcmp(&neighbors->addr, &in_if->ip_addr) == 0) {
      return neighbors->link_type == SYM_LINK && neighbors->neigh_type == MPR_NEIGH ? true : false;
    }
  }
  /* Not found */
  return false;
}

/**
 * Initializing the parser functions we are using
 * For downwards compatibility reasons we also understand the non-LQ messages.
 */
void
olsr_init_package_process(void)
{
  olsr_parser_add_function(&olsr_input_hello, olsr_get_Hello_MessageId());
  olsr_parser_add_function(&olsr_input_tc, olsr_get_TC_MessageId());
  olsr_parser_add_function(&olsr_input_mid, MID_MESSAGE);
  olsr_parser_add_function(&olsr_input_hna, HNA_MESSAGE);
}

void
olsr_deinit_package_process(void)
{
  olsr_parser_remove_function(&olsr_input_hello);
  olsr_parser_remove_function(&olsr_input_tc);
  olsr_parser_remove_function(&olsr_input_mid);
  olsr_parser_remove_function(&olsr_input_hna);
}

static bool
deserialize_hello(struct lq_hello_message *hello, struct olsr_message *msg)
{
  const uint8_t *curr = msg->payload;
  hello->comm = msg;

  /* parse HELLO specific header */
  pkt_ignore_u16(&curr);
  pkt_get_reltime(&curr, &hello->htime);
  pkt_get_u8(&curr, &hello->will);

  hello->neigh = NULL;
  while (curr < msg->end) {
    const uint8_t *ptr, *limit2;
    uint8_t link_code;
    uint16_t size;

    ptr = curr;
    pkt_get_u8(&curr, &link_code);
    pkt_ignore_u8(&curr);
    pkt_get_u16(&curr, &size);

    limit2 = ptr + size;
    while (curr + olsr_cnf->ipsize + olsr_sizeof_HelloLQ() <= limit2) {
      struct lq_hello_neighbor *neigh = olsr_malloc_lq_hello_neighbor();
      pkt_get_ipaddress(&curr, &neigh->addr);

      olsr_deserialize_hello_lq_pair(&curr, neigh);
      if (is_lost_interface_ip(&neigh->addr)) {
        /* this is a lost interface IP of this node... ignore it */
        olsr_free_lq_hello_neighbor(neigh);
        continue;
      }

      neigh->link_type = EXTRACT_LINK(link_code);
      neigh->neigh_type = EXTRACT_STATUS(link_code);

      neigh->next = hello->neigh;
      hello->neigh = neigh;
    }
  }
  return false;
}


static void olsr_update_mprs_set(struct lq_hello_message *message, struct link_entry *link) {
  bool new_mprs_status;

  new_mprs_status = lookup_mpr_status(message, link->inter);

  if (new_mprs_status && !link->is_mprs) {
    link->neighbor->mprs_count++;
  }
  if (!new_mprs_status && link->is_mprs) {
    link->neighbor->mprs_count--;
  }

  link->is_mprs = new_mprs_status;
}

static void
hello_tap(struct lq_hello_message *message, struct interface *in_if, const union olsr_ip_addr *from_addr)
{
  /*
   * Update link status
   */
  struct link_entry *lnk = update_link_entry(&in_if->ip_addr, from_addr, message, in_if);
  struct lq_hello_neighbor *walker;
  /* just in case our neighbor has changed its HELLO interval */
  olsr_update_packet_loss_hello_int(lnk, message->htime);

  /* find the input interface in the list of neighbor interfaces */
  for (walker = message->neigh; walker != NULL; walker = walker->next) {
    if (walker->link_type != UNSPEC_LINK && olsr_ipcmp(&walker->addr, &in_if->ip_addr) == 0) {
      break;
    }
  }

  olsr_update_mprs_set(message, lnk);

  /*
   * memorize our neighbour's idea of the link quality, so that we
   * know the link quality in both directions
   *
   * walker is NULL if there the current interface was not included in
   * the message (or was included as an UNSPEC_LINK)
   */
  olsr_memorize_foreign_hello_lq(lnk, walker);

  /* update packet loss for link quality calculation */
  olsr_update_packet_loss(lnk);

  /* Check willingness */
  if (lnk->neighbor->willingness != message->will) {
#if !defined REMOVE_LOG_DEBUG
    struct ipaddr_str buf;
#endif
    OLSR_DEBUG(LOG_LINKS, "Willingness for %s changed from %d to %d - UPDATING\n",
               olsr_ip_to_string(&buf, &lnk->neighbor->nbr_addr), lnk->neighbor->willingness, message->will);
    /*
     *If willingness changed - recalculate
     */
    lnk->neighbor->willingness = message->will;
    changes_neighborhood = true;
    changes_topology = true;
  }

  /* Don't register neighbors of neighbors that announces WILL_NEVER */
  if (lnk->neighbor->willingness != WILL_NEVER) {
    process_message_neighbors(lnk->neighbor, message);
  }

  /* Process changes immedeatly in case of MPR updates */
  olsr_process_changes();

  destroy_lq_hello(message);
}

static void
olsr_input_hello(struct olsr_message *msg, struct interface *inif, union olsr_ip_addr *from,
    enum duplicate_status status __attribute__ ((unused)))
{
  struct lq_hello_message hello;
  if (!deserialize_hello(&hello, msg)) {
    hello_tap(&hello, inif, from);
  }
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

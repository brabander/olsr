
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

#include "build_msg.h"
#include "ipcalc.h"
#include "olsr.h"
#include "log.h"
#include "olsr_time.h"
#include "net_olsr.h"
#include "olsr_ip_prefix_list.h"

#include <stdlib.h>

#define OLSR_IPV4_HDRSIZE          12
#define OLSR_IPV6_HDRSIZE          24

#define OLSR_HELLO_IPV4_HDRSIZE    (OLSR_IPV4_HDRSIZE + 4)
#define OLSR_HELLO_IPV6_HDRSIZE    (OLSR_IPV6_HDRSIZE + 4)
#define OLSR_TC_IPV4_HDRSIZE       (OLSR_IPV4_HDRSIZE + 4)
#define OLSR_TC_IPV6_HDRSIZE       (OLSR_IPV6_HDRSIZE + 4)
#define OLSR_MID_IPV4_HDRSIZE      OLSR_IPV4_HDRSIZE
#define OLSR_MID_IPV6_HDRSIZE      OLSR_IPV6_HDRSIZE
#define OLSR_HNA_IPV4_HDRSIZE      OLSR_IPV4_HDRSIZE
#define OLSR_HNA_IPV6_HDRSIZE      OLSR_IPV6_HDRSIZE

static void
  check_buffspace(int msgsize, int buffsize, const char *type);

/* All these functions share this buffer */

static uint8_t msg_buffer[MAXMESSAGESIZE - OLSR_HEADERSIZE];

/* Prototypes for internal functions */

/* IPv4 */

static bool serialize_hna4(struct interface *);

/* IPv6 */

static bool serialize_hna6(struct interface *);

/**
 *Builds a HNA message in the outputbuffer
 *<b>NB! Not internal packetformat!</b>
 *
 *@param ifp the interface to send on
 *@return nada
 */
bool
queue_hna(struct interface * ifp)
{
  OLSR_INFO(LOG_PACKET_CREATION, "Building HNA on %s\n-------------------\n", ifp->int_name);

  switch (olsr_cnf->ip_version) {
  case (AF_INET):
    return serialize_hna4(ifp);
  case (AF_INET6):
    return serialize_hna6(ifp);
  }
  return false;
}

/*
 * Protocol specific versions
 */


static void
check_buffspace(int msgsize, int buffsize, const char *type __attribute__ ((unused)))
{
  if (msgsize > buffsize) {
    OLSR_ERROR(LOG_PACKET_CREATION, "%s build, outputbuffer to small(%d/%d)!\n", type, msgsize, buffsize);
    olsr_exit(EXIT_FAILURE);
  }
}

/**
 *IP version 4
 *
 *@param ifp the interface to send on
 *@return nada
 */
static bool
serialize_hna4(struct interface *ifp)
{
  uint16_t remainsize, curr_size;
  /* preserve existing data in output buffer */
  union olsr_message *m;
  struct hnapair *pair;
  struct ip_prefix_entry *h;
#if !defined REMOVE_LOG_DEBUG
  struct ipprefix_str prefixstr;
#endif
  /* No hna nets */
  if (ifp == NULL) {
    return false;
  }
  if (olsr_cnf->ip_version != AF_INET) {
    return false;
  }
  if (list_is_empty(&olsr_cnf->hna_entries)) {
    return false;
  }

  remainsize = net_outbuffer_bytes_left(ifp);

  curr_size = OLSR_HNA_IPV4_HDRSIZE;

  /* Send pending packet if not room in buffer */
  if (curr_size > remainsize) {
    net_output(ifp);
    remainsize = net_outbuffer_bytes_left(ifp);
  }
  check_buffspace(curr_size, remainsize, "HNA");

  m = (union olsr_message *)msg_buffer;


  /* Fill header */
  m->v4.originator = olsr_cnf->router_id.v4.s_addr;
  m->v4.hopcnt = 0;
  m->v4.ttl = MAX_TTL;
  m->v4.olsr_msgtype = HNA_MESSAGE;
  m->v4.olsr_vtime = reltime_to_me(olsr_cnf->hna_params.validity_time);


  pair = m->v4.message.hna.hna_net;

  OLSR_FOR_ALL_IPPREFIX_ENTRIES(&olsr_cnf->hna_entries, h) {
    union olsr_ip_addr ip_addr;
    if ((curr_size + (2 * olsr_cnf->ipsize)) > remainsize) {
      /* Only add HNA message if it contains data */
      if (curr_size > OLSR_HNA_IPV4_HDRSIZE) {
        OLSR_DEBUG(LOG_PACKET_CREATION, "Sending partial(size: %d, buff left:%d)\n", curr_size, remainsize);
        m->v4.seqno = htons(get_msg_seqno());
        m->v4.olsr_msgsize = htons(curr_size);
        net_outbuffer_push(ifp, msg_buffer, curr_size);
        curr_size = OLSR_HNA_IPV4_HDRSIZE;
        pair = m->v4.message.hna.hna_net;
      }
      net_output(ifp);
      remainsize = net_outbuffer_bytes_left(ifp);
      check_buffspace(curr_size + (2 * olsr_cnf->ipsize), remainsize, "HNA2");
    }
    OLSR_DEBUG(LOG_PACKET_CREATION, "\tNet: %s\n", olsr_ip_prefix_to_string(&prefixstr, &h->net));
    pair->addr = h->net.prefix.v4.s_addr;
    olsr_prefix_to_netmask(&ip_addr, h->net.prefix_len);
    pair->netmask = ip_addr.v4.s_addr;
    pair++;
    curr_size += (2 * olsr_cnf->ipsize);
  }
  OLSR_FOR_ALL_IPPREFIX_ENTRIES_END()

    m->v4.seqno = htons(get_msg_seqno());
  m->v4.olsr_msgsize = htons(curr_size);

  net_outbuffer_push(ifp, msg_buffer, curr_size);

  return false;
}


/**
 *IP version 6
 *
 *@param ifp the interface to send on
 *@return nada
 */
static bool
serialize_hna6(struct interface *ifp)
{
  uint16_t remainsize, curr_size;
  /* preserve existing data in output buffer */
  union olsr_message *m;
  struct hnapair6 *pair6;
  union olsr_ip_addr tmp_netmask;
  struct ip_prefix_entry *h;
#if !defined REMOVE_LOG_DEBUG
  struct ipprefix_str prefixstr;
#endif

  /* No hna nets */
  if ((olsr_cnf->ip_version != AF_INET6) || (!ifp) || list_is_empty(&olsr_cnf->hna_entries))
    return false;


  remainsize = net_outbuffer_bytes_left(ifp);

  curr_size = OLSR_HNA_IPV6_HDRSIZE;

  /* Send pending packet if not room in buffer */
  if (curr_size > remainsize) {
    net_output(ifp);
    remainsize = net_outbuffer_bytes_left(ifp);
  }
  check_buffspace(curr_size, remainsize, "HNA");

  m = (union olsr_message *)msg_buffer;

  /* Fill header */
  m->v6.originator = olsr_cnf->router_id.v6;
  m->v6.hopcnt = 0;
  m->v6.ttl = MAX_TTL;
  m->v6.olsr_msgtype = HNA_MESSAGE;
  m->v6.olsr_vtime = reltime_to_me(olsr_cnf->hna_params.validity_time);

  pair6 = m->v6.message.hna.hna_net;


  OLSR_FOR_ALL_IPPREFIX_ENTRIES(&olsr_cnf->hna_entries, h) {
    if ((curr_size + (2 * olsr_cnf->ipsize)) > remainsize) {
      /* Only add HNA message if it contains data */
      if (curr_size > OLSR_HNA_IPV6_HDRSIZE) {
        OLSR_DEBUG(LOG_PACKET_CREATION, "Sending partial(size: %d, buff left:%d)\n", curr_size, remainsize);
        m->v6.seqno = htons(get_msg_seqno());
        m->v6.olsr_msgsize = htons(curr_size);
        net_outbuffer_push(ifp, msg_buffer, curr_size);
        curr_size = OLSR_HNA_IPV6_HDRSIZE;
        pair6 = m->v6.message.hna.hna_net;
      }
      net_output(ifp);
      remainsize = net_outbuffer_bytes_left(ifp);
      check_buffspace(curr_size + (2 * olsr_cnf->ipsize), remainsize, "HNA2");
    }
    OLSR_DEBUG(LOG_PACKET_CREATION, "\tNet: %s\n", olsr_ip_prefix_to_string(&prefixstr, &h->net));
    pair6->addr = h->net.prefix.v6;
    olsr_prefix_to_netmask(&tmp_netmask, h->net.prefix_len);
    pair6->netmask = tmp_netmask.v6;
    pair6++;
    curr_size += (2 * olsr_cnf->ipsize);
  }
  OLSR_FOR_ALL_IPPREFIX_ENTRIES_END()

    m->v6.olsr_msgsize = htons(curr_size);
  m->v6.seqno = htons(get_msg_seqno());

  net_outbuffer_push(ifp, msg_buffer, curr_size);
  return false;
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2003, Andreas Tønnesen (andreto@olsr.org)
 *               2004, Thomas Lopatic (thomas@lopatic.de)
 *               2006, for some fixups, sven-ola(gmx.de)
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
#include "olsr_protocol.h"
#include "defs.h"
#include "lq_packet.h"
#include "interfaces.h"
#include "link_set.h"
#include "neighbor_table.h"
#include "mpr_selector_set.h"
#include "mid_set.h"
#include "mantissa.h"
#include "process_package.h" // XXX - remove
#include "two_hop_neighbor_table.h"
#include "hysteresis.h"
#include "olsr.h"
#include "build_msg.h"
#include "net_olsr.h"


olsr_bool lq_tc_pending = OLSR_FALSE;

static unsigned char msg_buffer[MAXMESSAGESIZE - OLSR_HEADERSIZE];

static void
create_lq_hello(struct lq_hello_message *lq_hello, struct interface *outif)
{
  struct link_entry *walker;

  // initialize the static fields

  lq_hello->comm.type = LQ_HELLO_MESSAGE;
  lq_hello->comm.vtime = me_to_double(outif->valtimes.hello);
  lq_hello->comm.size = 0;

  lq_hello->comm.orig = olsr_cnf->main_addr;

  lq_hello->comm.ttl = 1;
  lq_hello->comm.hops = 0;
  lq_hello->comm.seqno = get_msg_seqno();

  lq_hello->htime = outif->hello_etime;
  lq_hello->will = olsr_cnf->willingness;

  lq_hello->neigh = NULL;
  
  // loop through the link set

  for (walker = get_link_set(); walker != NULL; walker = walker->next)
    {
      // allocate a neighbour entry
      struct lq_hello_neighbor *neigh = olsr_malloc(sizeof (struct lq_hello_neighbor), "Build LQ_HELLO");

      // a) this neighbor interface IS NOT visible via the output interface
      if(!ipequal(&walker->local_iface_addr, &outif->ip_addr))
        neigh->link_type = UNSPEC_LINK;
      
      // b) this neighbor interface IS visible via the output interface

      else
        neigh->link_type = lookup_link_status(walker);

      // set the entry's link quality

      neigh->link_quality = walker->loss_link_quality;
      neigh->neigh_link_quality = walker->neigh_link_quality;

      // set the entry's neighbour type

      if(walker->neighbor->is_mpr)
        neigh->neigh_type = MPR_NEIGH;

      else if (walker->neighbor->status == SYM)
        neigh->neigh_type = SYM_NEIGH;

      else if (walker->neighbor->status == NOT_SYM)
        neigh->neigh_type = NOT_NEIGH;
  
      // set the entry's neighbour interface address

      neigh->addr = walker->neighbor_iface_addr;
      
      // queue the neighbour entry

      neigh->next = lq_hello->neigh;
      lq_hello->neigh = neigh;
    }
}

static void
destroy_lq_hello(struct lq_hello_message *lq_hello)
{
  struct lq_hello_neighbor *walker, *aux;

  // loop through the queued neighbour entries and free them

  for (walker = lq_hello->neigh; walker != NULL; walker = aux)
    {
      aux = walker->next;
      free(walker);
    }

  lq_hello->neigh = NULL;
}

static void
create_lq_tc(struct lq_tc_message *lq_tc, struct interface *outif)
{
  int i;
  static int ttl_list[] = { 2, 8, 2, 16, 2, 8, 2, MAX_TTL};

  // remember that we have generated an LQ TC message; this is
  // checked in net_output()

  lq_tc_pending = OLSR_TRUE;

  // initialize the static fields

  lq_tc->comm.type = LQ_TC_MESSAGE;
  lq_tc->comm.vtime = me_to_double(outif->valtimes.tc);
  lq_tc->comm.size = 0;

  lq_tc->comm.orig = olsr_cnf->main_addr;

  if (olsr_cnf->lq_fish > 0)
  {
    // Sven-Ola: Too lazy to find the different iface inits. This will do it too.
    if (outif->ttl_index >= (int)(sizeof(ttl_list) / sizeof(ttl_list[0])))
      outif->ttl_index = 0;
    
    lq_tc->comm.ttl = ttl_list[outif->ttl_index++];

    OLSR_PRINTF(3, "Creating LQ TC with TTL %d.\n", lq_tc->comm.ttl);
  }

  else
    lq_tc->comm.ttl = MAX_TTL;

  lq_tc->comm.hops = 0;
  lq_tc->comm.seqno = get_msg_seqno();

  lq_tc->from = olsr_cnf->main_addr;

  lq_tc->ansn = get_local_ansn();

  lq_tc->neigh = NULL;
 
  // loop through all neighbours
  
  for(i = 0; i < HASHSIZE; i++)
    {
      struct neighbor_entry *walker;
      struct tc_mpr_addr    *neigh;
      for(walker = neighbortable[i].next; walker != &neighbortable[i];
          walker = walker->next)
        {
          struct link_entry *lnk;
          // only consider symmetric neighbours

          if(walker->status != SYM)
            continue;

          // TC redundancy == 1: only consider MPRs and MPR selectors

          if (olsr_cnf->tc_redundancy == 1 && !walker->is_mpr &&
              olsr_lookup_mprs_set(&walker->neighbor_main_addr) == NULL)
            continue;

          // TC redundancy == 0: only consider MPR selectors
          if (olsr_cnf->tc_redundancy == 0 &&
              olsr_lookup_mprs_set(&walker->neighbor_main_addr) == NULL)
            continue;

          // allocate a neighbour entry          
          neigh = olsr_malloc(sizeof (struct tc_mpr_addr), "Build LQ_TC");

          // set the entry's main address

          neigh->address = walker->neighbor_main_addr;

          // set the entry's link quality
          lnk = get_best_link_to_neighbor(&neigh->address);

          if (lnk) {
            neigh->link_quality = lnk->loss_link_quality;
            neigh->neigh_link_quality = lnk->neigh_link_quality;
          }

          // queue the neighbour entry

          neigh->next = lq_tc->neigh;
          lq_tc->neigh = neigh;
        }
    }
}

static void
destroy_lq_tc(struct lq_tc_message *lq_tc)
{
  struct tc_mpr_addr *walker, *aux;

  // loop through the queued neighbour entries and free them

  for (walker = lq_tc->neigh; walker != NULL; walker = aux)
    {
      aux = walker->next;
      free(walker);
    }
}

static int common_size(void)
{
  // return the size of the header shared by all OLSR messages

  return (olsr_cnf->ip_version == AF_INET) ?
    sizeof (struct olsr_header_v4) : sizeof (struct olsr_header_v6);
}

static void serialize_common(struct olsr_common *comm)
{
  if (olsr_cnf->ip_version == AF_INET)
    {
      // serialize an IPv4 OLSR message header
      struct olsr_header_v4 *olsr_head_v4 = (struct olsr_header_v4 *)msg_buffer;

      olsr_head_v4->type = comm->type;
      olsr_head_v4->vtime = double_to_me(comm->vtime);
      olsr_head_v4->size = htons(comm->size);

      olsr_head_v4->orig = comm->orig.v4.s_addr;

      olsr_head_v4->ttl = comm->ttl;
      olsr_head_v4->hops = comm->hops;
      olsr_head_v4->seqno = htons(comm->seqno);
    }
  else
    {
      // serialize an IPv6 OLSR message header
      struct olsr_header_v6 *olsr_head_v6 = (struct olsr_header_v6 *)msg_buffer;

      olsr_head_v6->type = comm->type;
      olsr_head_v6->vtime = double_to_me(comm->vtime);
      olsr_head_v6->size = htons(comm->size);

      memcpy(&olsr_head_v6->orig, &comm->orig.v6.s6_addr, sizeof(olsr_head_v6->orig));

      olsr_head_v6->ttl = comm->ttl;
      olsr_head_v6->hops = comm->hops;
      olsr_head_v6->seqno = htons(comm->seqno);
    }
}

static void
serialize_lq_hello(struct lq_hello_message *lq_hello, struct interface *outif)
{
  int rem, size, req;
  struct lq_hello_info_header *info_head;
  struct lq_hello_neighbor *neigh;
  unsigned char *buff;
  int is_first;
  int i;

  // leave space for the OLSR header
  int off = common_size();

  // initialize the LQ_HELLO header

  struct lq_hello_header *head = (struct lq_hello_header *)(msg_buffer + off);

  head->reserved = 0;
  head->htime = double_to_me(lq_hello->htime);
  head->will = lq_hello->will; 

  // 'off' is the offset of the byte following the LQ_HELLO header

  off += sizeof (struct lq_hello_header);

  // our work buffer starts at 'off'...

  buff = msg_buffer + off;

  // ... that's why we start with a 'size' of 0 and subtract 'off' from
  // the remaining bytes in the output buffer

  size = 0;
  rem = net_outbuffer_bytes_left(outif) - off;

  // initially, we want to put at least an info header, an IP address,
  // and the corresponding link quality into the message

  // force signed comparison

  if (rem < (int)(sizeof (struct lq_hello_info_header) + olsr_cnf->ipsize + 4))
  {
    net_output(outif);

    rem = net_outbuffer_bytes_left(outif) - off;
  }

  info_head = NULL;

  // iterate through all neighbor types ('i') and all link types ('j')

  for (i = 0; i <= MAX_NEIGH; i++) 
    {
      static const int LINK_ORDER[] = {SYM_LINK, UNSPEC_LINK, ASYM_LINK, LOST_LINK};
      unsigned int j;
      for(j = 0; j < sizeof(LINK_ORDER) / sizeof(LINK_ORDER[0]); j++)
        {
          is_first = 1;

          // loop through neighbors

          for (neigh = lq_hello->neigh; neigh != NULL; neigh = neigh->next)
            {  
              if (neigh->neigh_type != i || neigh->link_type != LINK_ORDER[j])
                continue;

              // we need space for an IP address plus link quality
              // information

              req = olsr_cnf->ipsize + 4;

              // no, we also need space for an info header, as this is the
              // first neighbor with the current neighor type and link type

              if (is_first != 0)
                req += sizeof (struct lq_hello_info_header);

              // we do not have enough space left

              // force signed comparison

              if ((int)(size + req) > rem)
                {
                  // finalize the OLSR header

                  lq_hello->comm.size = size + off;

                  serialize_common(&lq_hello->comm);

                  // finalize the info header

                  info_head->size =
                    ntohs(buff + size - (unsigned char *)info_head);
			      
                  // output packet

                  net_outbuffer_push(outif, msg_buffer, size + off);

                  net_output(outif);

                  // move to the beginning of the buffer

                  size = 0;
                  rem = net_outbuffer_bytes_left(outif) - off;

                  // we need a new info header

                  is_first = 1;
                }

              // create a new info header

              if (is_first != 0)
                {
                  info_head = (struct lq_hello_info_header *)(buff + size);
                  size += sizeof (struct lq_hello_info_header);

                  info_head->reserved = 0;
                  info_head->link_code = CREATE_LINK_CODE(i, LINK_ORDER[j]);
                }

              // add the current neighbor's IP address

              genipcopy(buff + size, &neigh->addr);
              size += olsr_cnf->ipsize;

              // add the corresponding link quality

              buff[size++] = (unsigned char)(neigh->link_quality * 255);
              buff[size++] = (unsigned char)(neigh->neigh_link_quality * 255);

              // pad

              buff[size++] = 0;
              buff[size++] = 0;

              is_first = 0;
            }

          // finalize the info header, if there are any neighbors with the
          // current neighbor type and link type

          if (is_first == 0)
            info_head->size = ntohs(buff + size - (unsigned char *)info_head);
        }
    }

  // finalize the OLSR header

  lq_hello->comm.size = size + off;

  serialize_common((struct olsr_common *)lq_hello);

  // move the message to the output buffer

  net_outbuffer_push(outif, msg_buffer, size + off);
}

static void
serialize_lq_tc(struct lq_tc_message *lq_tc, struct interface *outif)
{
  int off, rem, size;
  struct lq_tc_header *head;
  struct tc_mpr_addr *neigh;
  unsigned char *buff;

  // leave space for the OLSR header

  off = common_size();

  // initialize the LQ_TC header

  head = (struct lq_tc_header *)(msg_buffer + off);

  head->ansn = htons(lq_tc->ansn);
  head->reserved = 0;

  // 'off' is the offset of the byte following the LQ_TC header

  off += sizeof (struct lq_tc_header);

  // our work buffer starts at 'off'...

  buff = msg_buffer + off;

  // ... that's why we start with a 'size' of 0 and subtract 'off' from
  // the remaining bytes in the output buffer

  size = 0;
  rem = net_outbuffer_bytes_left(outif) - off;

  // initially, we want to put at least an IP address and the corresponding
  // link quality into the message

  // force signed comparison

  if (rem < (int)(olsr_cnf->ipsize + 4))
  {
    net_output(outif);

    rem = net_outbuffer_bytes_left(outif) - off;
  }

  // loop through neighbors

  for (neigh = lq_tc->neigh; neigh != NULL; neigh = neigh->next)
    {  
      // we need space for an IP address plus link quality
      // information

      // force signed comparison

      if ((int)(size + olsr_cnf->ipsize + 4) > rem)
        {
          // finalize the OLSR header

          lq_tc->comm.size = size + off;

          serialize_common((struct olsr_common *)lq_tc);

          // output packet

          net_outbuffer_push(outif, msg_buffer, size + off);

          net_output(outif);

          // move to the beginning of the buffer

          size = 0;
          rem = net_outbuffer_bytes_left(outif) - off;
        }

      // add the current neighbor's IP address
      genipcopy(buff + size, &neigh->address);
      size += olsr_cnf->ipsize;

      // add the corresponding link quality
      buff[size++] = (unsigned char)(neigh->link_quality * 255);
      buff[size++] = (unsigned char)(neigh->neigh_link_quality * 255);

      // pad
      buff[size++] = 0;
      buff[size++] = 0;
    }

  // finalize the OLSR header

  lq_tc->comm.size = size + off;

  serialize_common((struct olsr_common *)lq_tc);

  net_outbuffer_push(outif, msg_buffer, size + off);
}


static int
deserialize_lq_hello(struct hello_message *hello,
                     const void *ser)
{
    const unsigned char *limit;
    olsr_u8_t type;
    olsr_u16_t size;
  
    const unsigned char *curr = ser;
    pkt_get_u8(&curr, &type);
    if (type != LQ_HELLO_MESSAGE) {
        /* No need to do anything more */
        return 1;
    }
    pkt_get_double(&curr, &hello->vtime);
    pkt_get_u16(&curr, &size);

    // Sven-Ola: Check the message source addr
    if (!olsr_validate_address((const union olsr_ip_addr *)curr)) {
        /* No need to do anything more */
        return 1;
    }
    pkt_get_ipaddress(&curr, &hello->source_addr);

    pkt_get_u8(&curr, &hello->ttl);
    pkt_get_u8(&curr, &hello->hop_count);
    pkt_get_u16(&curr, &hello->packet_seq_number);
    pkt_ignore_u16(&curr);

    pkt_get_double(&curr, &hello->htime);
    pkt_get_u8(&curr, &hello->willingness);

    hello->neighbors = NULL;
    limit = ((const unsigned char *)ser) + size;
    while (curr < limit) {
        const struct lq_hello_info_header *info_head = (const struct lq_hello_info_header *)curr;
        const unsigned char *limit2 = curr + ntohs(info_head->size);

        curr = (const unsigned char *)(info_head + 1);      
        while (curr < limit2) {
            struct hello_neighbor *neigh = olsr_malloc(sizeof (struct hello_neighbor),
                                                       "LQ_HELLO deserialization");
            pkt_get_ipaddress(&curr, &neigh->address);

            pkt_get_lq(&curr, &neigh->link_quality);
            pkt_get_lq(&curr, &neigh->neigh_link_quality);
            pkt_ignore_u16(&curr);

            neigh->link   = EXTRACT_LINK(info_head->link_code);
            neigh->status = EXTRACT_STATUS(info_head->link_code);

            neigh->next = hello->neighbors;
            hello->neighbors = neigh;
        }
    }
    return 0;
}

void
olsr_output_lq_hello(void *para)
{
  struct lq_hello_message lq_hello;
  struct interface *outif = para;

  if (outif == NULL) {
    return;
  }

  // create LQ_HELLO in internal format
  create_lq_hello(&lq_hello, outif);

  // convert internal format into transmission format, send it
  serialize_lq_hello(&lq_hello, outif);

  // destroy internal format
  destroy_lq_hello(&lq_hello);

  if(net_output_pending(outif)) {
    net_output(outif);
  }
}

void
olsr_output_lq_tc(void *para)
{
  static int prev_empty = 1;
  struct lq_tc_message lq_tc;
  struct interface *outif = para;

  if (outif == NULL) {
    return;
  }
  // create LQ_TC in internal format

  create_lq_tc(&lq_tc, outif);

  // a) the message is not empty

  if (lq_tc.neigh != NULL) {
      prev_empty = 0;
      
      // convert internal format into transmission format, send it
      serialize_lq_tc(&lq_tc, outif);

  // b) this is the first empty message
  } else if (prev_empty == 0) {
      // initialize timer

      set_empty_tc_timer(GET_TIMESTAMP(olsr_cnf->max_tc_vtime * 3 * 1000));

      prev_empty = 1;

      // convert internal format into transmission format, send it

      serialize_lq_tc(&lq_tc, outif);

  // c) this is not the first empty message, send if timer hasn't fired
  } else if (!TIMED_OUT(get_empty_tc_timer())) {
      serialize_lq_tc(&lq_tc, outif);
  }
  // destroy internal format

  destroy_lq_tc(&lq_tc);

  if(net_output_pending(outif) && TIMED_OUT(outif->fwdtimer)) {
    set_buffer_timer(outif);
  }
}

void
olsr_input_lq_hello(union olsr_message *ser,
                    struct interface *inif,
                    union olsr_ip_addr *from)
{
  struct hello_message hello;

  if (ser == NULL) {
    return;
  }
  if (deserialize_lq_hello(&hello, ser) != 0) {
    return;
  }
  olsr_hello_tap(&hello, inif, from);
}

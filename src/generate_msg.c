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

#include "generate_msg.h"
#include "lq_plugin.h"
#include "olsr.h"
#include "build_msg.h"

/*
 * Infomation repositiries
 */
#include "mid_set.h"
#include "tc_set.h"
#include "mpr_selector_set.h"
#include "neighbor_table.h"
#include "net_olsr.h"

static char pulsedata[] = "\\|/-";
static uint8_t pulse_state = 0;

static void free_tc_packet(struct tc_message *);
static void build_tc_packet(struct tc_message *);

/**
 *Free the memory allocated for a TC packet.
 *
 *@param message the pointer to the packet to erase
 *
 *@return nada
 */
static void 
free_tc_packet(struct tc_message *message)
{
  struct tc_mpr_addr *mprs = message->multipoint_relay_selector_address;
  while (mprs != NULL) {
    struct tc_mpr_addr *prev_mprs = mprs;
    mprs = mprs->next;
    free(prev_mprs);
  }
}

/**
 *Build an internal TC package for this
 *node.
 *
 *@param message the tc_message struct to fill with info
 *@return 0
 */
static void
build_tc_packet(struct tc_message *message)
{
  struct neighbor_entry *entry;

  message->multipoint_relay_selector_address = NULL;
  message->packet_seq_number = 0;
 
  message->hop_count = 0;
  message->ttl = MAX_TTL;
  message->ansn = get_local_ansn();

  message->originator = olsr_cnf->main_addr;
  message->source_addr = olsr_cnf->main_addr;
  
  /* Loop trough all neighbors */  
  OLSR_FOR_ALL_NBR_ENTRIES(entry) {
    struct tc_mpr_addr *message_mpr;
    if (entry->status != SYM) {
      continue;
    }

    switch (olsr_cnf->tc_redundancy) {
    case 2:
      break;
    case 1:
      if (!entry->is_mpr &&
          olsr_lookup_mprs_set(&entry->neighbor_main_addr) == NULL) {
	continue;
      }
      break;
    default:
      if (olsr_lookup_mprs_set(&entry->neighbor_main_addr) == NULL) {
	continue;
      }
      break;
    } /* Switch */

    //printf("\t%s\n", olsr_ip_to_string(&mprs->mpr_selector_addr));
    message_mpr = olsr_malloc_tc_mpr_addr("Build TC");
		    
    message_mpr->address = entry->neighbor_main_addr;
    message_mpr->next = message->multipoint_relay_selector_address;
    message->multipoint_relay_selector_address = message_mpr;

  } OLSR_FOR_ALL_NBR_ENTRIES_END(entry);
}

void
generate_hello(void *p)
{
  struct interface *ifn = p;
  struct hello_message hellopacket;

  olsr_build_hello_packet(&hellopacket, ifn);
      
  if (queue_hello(&hellopacket, ifn)) {
    net_output(ifn);
  }
  olsr_free_hello_packet(&hellopacket);

}

/**
 * Callback for TC generation timer.
 */
void
generate_tc(void *p)
{
  struct interface *ifn = p;
  struct tc_message tcpacket;

  build_tc_packet(&tcpacket);

  /* empty message ? */
  if (!tcpacket.multipoint_relay_selector_address) {
    return;
  }

  if (queue_tc(&tcpacket, ifn)) {
    set_buffer_timer(ifn);
  }

  free_tc_packet(&tcpacket);
}

void
generate_mid(void *p)
{
  struct interface *ifn = p;
  
  if (queue_mid(ifn)) {
    set_buffer_timer(ifn);
  }
}

void
generate_hna(void *p)
{
  struct interface *ifn = p;
  
  if (queue_hna(ifn)) {
    set_buffer_timer(ifn);
  }
}


void
generate_stdout_pulse(void *foo __attribute__((unused)))
{
  if (olsr_cnf->debug_level > 0) {
    if (pulsedata[++pulse_state] == '\0') {
      pulse_state = 0;
    }
    printf("%c\r", pulsedata[pulse_state]);
  }
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

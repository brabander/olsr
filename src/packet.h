/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2004 Andreas Tønnesen (andreto@ifi.uio.no)
 *
 * This file is part of the olsr.org OLSR daemon.
 *
 * olsr.org is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * olsr.org is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with olsr.org; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */


#ifndef _OLSR_PACKET
#define _OLSR_PACKET

#include "olsr_protocol.h"
#include "interfaces.h"




union hna_netmask
{
  olsr_u32_t v4;
  olsr_u16_t v6;
};

struct hello_neighbor
{
  olsr_u8_t             status;
  olsr_u8_t             link;
  union olsr_ip_addr    main_address;
  union olsr_ip_addr    address;
  struct hello_neighbor *next;
};



struct hello_message
{
  double                 vtime;
  double                 htime;
  union olsr_ip_addr     source_addr;
  olsr_u16_t             packet_seq_number;
  olsr_u8_t              hop_count;
  olsr_u8_t              ttl;
  olsr_u8_t              willingness;
  struct hello_neighbor  *neighbors;
  
};


struct tc_mpr_addr
{

  union olsr_ip_addr address;
  struct tc_mpr_addr *next;
};


struct tc_message
{
  double              vtime;
  union olsr_ip_addr  source_addr;
  union olsr_ip_addr  originator;
  olsr_u16_t          packet_seq_number;
  olsr_u8_t           hop_count;
  olsr_u8_t           ttl;
  olsr_u16_t          ansn;
  struct tc_mpr_addr  *multipoint_relay_selector_address;
};



/*
 *HNA message format:
 *NET
 *NETMASK
 *NET
 *NETMASK
 *......
 */

struct hna_net_addr
{
  union olsr_ip_addr  net;
  union hna_netmask   netmask; /* IPv4 netmask */
  struct hna_net_addr *next;
};


struct hna_message
{
  double               vtime;
  union olsr_ip_addr   originator;
  olsr_u16_t           packet_seq_number;
  olsr_u8_t            hop_count;
  olsr_u8_t            hna_ttl;
  struct hna_net_addr  *hna_net;
};


/*
 *MID messages - format:
 *
 *ADDR
 *ADDR
 *ADDR
 *.....
 */

struct mid_alias
{
  union olsr_ip_addr alias_addr;
  struct mid_alias   *next;
};

struct mid_message
{
  double             vtime;
  union olsr_ip_addr mid_origaddr;  /* originator's address */
  olsr_u8_t          mid_hopcnt;    /* number of hops to destination */
  olsr_u8_t          mid_ttl;       /* ttl */
  olsr_u16_t         mid_seqno;     /* sequence number */
  union olsr_ip_addr addr;          /* main address */
  struct mid_alias   *mid_addr;     /* variable length */
};


struct unknown_message
{
  olsr_u16_t          seqno;
  union olsr_ip_addr originator;
  olsr_u8_t          type;
};


int
olsr_build_hello_packet(struct hello_message *, struct interface *);

int
olsr_build_tc_packet(struct tc_message *);

void
olsr_destroy_hello_message(struct hello_message *);

void
olsr_destroy_mid_message(struct mid_message *);

void
olsr_destroy_hna_message(struct hna_message *);

void
olsr_destroy_tc_message(struct tc_message *);



#endif

/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2003 Andreas Tønnesen (andreto@ifi.uio.no)
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
 * 
 * $Id: lq_packet.h,v 1.1 2004/11/03 18:19:54 tlopatic Exp $
 *
 */

#define LQ_HELLO_MESSAGE      5
#define LQ_TC_MESSAGE         6

// deserialized OLSR header

struct olsr_common
{
  olsr_u8_t          type;
  double             vtime;
  olsr_u16_t         size;
  union olsr_ip_addr orig;
  olsr_u8_t          ttl;
  olsr_u8_t          hops;
  olsr_u16_t         seqno;
};

// serialized IPv4 OLSR header

struct olsr_header_v4
{
  olsr_u8_t  type;
  olsr_u8_t  vtime;
  olsr_u16_t size;
  olsr_u32_t orig;
  olsr_u8_t  ttl;
  olsr_u8_t  hops;
  olsr_u16_t seqno;
};

// serialized IPv6 OLSR header

struct olsr_header_v6
{
  olsr_u8_t     type;
  olsr_u8_t     vtime;
  olsr_u16_t    size;
  unsigned char orig[16];
  olsr_u8_t     ttl;
  olsr_u8_t     hops;
  olsr_u16_t    seqno;
};

// deserialized LQ_HELLO

struct lq_hello_neighbor
{
  olsr_u8_t                link_type;
  olsr_u8_t                neigh_type;
  double                   link_quality;
  union olsr_ip_addr       addr;
  struct lq_hello_neighbor *next;
};

struct lq_hello_message
{
  struct olsr_common       comm;
  double                   htime;
  olsr_u8_t                will;
  struct lq_hello_neighbor *neigh;
};

// serialized LQ_HELLO

struct lq_hello_info_header
{
  olsr_u8_t  link_code;
  olsr_u8_t  reserved;
  olsr_u16_t size;
};

struct lq_hello_header
{
  olsr_u16_t reserved;
  olsr_u8_t  htime;
  olsr_u8_t  will;
};

// deserialized LQ_TC

struct lq_tc_neighbor
{
  double                link_quality;
  union olsr_ip_addr    main;
  struct lq_tc_neighbor *next;
};

struct lq_tc_message
{
  struct olsr_common    comm;
  union olsr_ip_addr    from;
  olsr_u16_t            ansn;
  struct lq_tc_neighbor *neigh;
};

// serialized LQ_TC

struct lq_tc_header
{
  olsr_u16_t ansn;
  olsr_u16_t reserved;
};

void olsr_output_lq_hello(void *para);

void olsr_output_lq_tc(void *para);

void olsr_input_lq_hello(union olsr_message *ser, struct interface *inif,
                         union olsr_ip_addr *from);

void olsr_input_lq_tc(union olsr_message *ser, struct interface *inif,
                      union olsr_ip_addr *from);

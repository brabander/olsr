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
 * $Id: link_set.h,v 1.9 2004/10/21 20:57:19 tlopatic Exp $
 *
 */


/*
 * Link sensing database for the OLSR routing daemon
 */


#include "defs.h"

#ifndef _LINK_SET_H
#define _LINK_SET_H

struct link_entry
{
  union olsr_ip_addr local_iface_addr;
  union olsr_ip_addr neighbor_iface_addr;
  struct timeval SYM_time;
  struct timeval ASYM_time;
  struct timeval time;
  struct neighbor_entry *neighbor;

  /*
   *Hysteresis
   */
  float L_link_quality;
  int L_link_pending;
  struct timeval L_LOST_LINK_time;
  struct timeval hello_timeout; /* When we should receive a new HELLO */
  double last_htime;
  olsr_u16_t olsr_seqno;

#if defined USE_LINK_QUALITY
  /*
   * packet loss
   */

  double loss_hello_int;
  struct timeval loss_timeout;

  olsr_u16_t loss_seqno;
  int loss_seqno_valid;
  int loss_missed_hellos;

  int lost_packets;
  int total_packets;

  float loss_link_quality;

  int loss_window_size;
  int loss_index;

  unsigned char loss_bitmap[16];
#endif

  /*
   * Spy
   */
  olsr_u8_t                    spy_activated;

  struct link_entry *next;
};


/* The link sets - one pr. interface */

struct link_entry *link_set;

/* Timers */
struct timeval  hold_time_neighbor;
struct timeval  hold_time_neighbor_nw;

/* Function prototypes */

void
olsr_init_link_set(void);

struct interface *
get_interface_link_set(union olsr_ip_addr *);

union olsr_ip_addr *
get_neighbor_nexthop(union olsr_ip_addr *);

struct link_entry *
lookup_link_entry(union olsr_ip_addr *, union olsr_ip_addr *);

struct link_entry *
update_link_entry(union olsr_ip_addr *, union olsr_ip_addr *, struct hello_message *, struct interface *);

int
check_neighbor_link(union olsr_ip_addr *);

int
replace_neighbor_link_set(struct neighbor_entry *,
			  struct neighbor_entry *);

int
lookup_link_status(struct link_entry *);

#if defined USE_LINK_QUALITY
void olsr_update_packet_loss_hello_int(struct link_entry *entry, double htime);
void olsr_update_packet_loss(union olsr_ip_addr *rem, union olsr_ip_addr *loc,
                        olsr_u16_t seqno);
void olsr_print_link_set(void);
#endif

#endif

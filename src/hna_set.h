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
 */



#ifndef _OLSR_HNA
#define _OLSR_HNA

#include "hashing.h"
#include "packet.h"

/* hna_netmask declared in packet.h */

struct hna_net
{
  union olsr_ip_addr A_network_addr;
  union hna_netmask  A_netmask;
  struct timeval     A_time;
  struct hna_net     *next;
  struct hna_net     *prev;
};

struct hna_entry
{
  union olsr_ip_addr A_gateway_addr;
  struct hna_net     networks;
  struct hna_entry   *next;
  struct hna_entry   *prev;
};

struct hna_entry hna_set[HASHSIZE];
size_t netmask_size;

int
olsr_init_hna_set();


struct hna_net *
olsr_lookup_hna_net(struct hna_net *, union olsr_ip_addr *, union hna_netmask *);


struct hna_entry *
olsr_lookup_hna_gw(union olsr_ip_addr *);



struct hna_entry *
olsr_add_hna_entry(union olsr_ip_addr *);


struct hna_net *
olsr_add_hna_net(struct hna_entry *, union olsr_ip_addr *, union hna_netmask *);


void
olsr_update_hna_entry(union olsr_ip_addr *, union olsr_ip_addr *, union hna_netmask *, float);


void
olsr_time_out_hna_set();


void
delete_hna_net(struct hna_net *);


void
delete_hna_entry(struct hna_entry *);


#endif

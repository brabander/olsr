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
 * $ Id $
 *
 */



#ifndef _OLSR_HNA_LOCAL
#define _OLSR_HNA_LOCAL

#include "defs.h"
#include "hna_set.h"


struct local_hna_entry
{
  union olsr_ip_addr     A_network_addr;
  union hna_netmask      A_netmask;
  struct local_hna_entry *next;
  struct local_hna_entry *prev;
};


struct local_hna_entry local_hna4_set;
struct local_hna_entry local_hna6_set;

extern size_t netmask_size;
int inet_gw;


int
olsr_init_local_hna_set();

void
add_local_hna4_entry(union olsr_ip_addr *, union hna_netmask *);

void
add_local_hna6_entry(union olsr_ip_addr *, union hna_netmask *);

int
remove_local_hna4_entry(union olsr_ip_addr *, union hna_netmask *);

int
remove_local_hna6_entry(union olsr_ip_addr *, union hna_netmask *);

#endif

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
 * 
 * $Id: routing_table.h,v 1.9 2004/11/05 02:06:14 tlopatic Exp $
 *
 */

#ifndef _OLSR_ROUTING_TABLE
#define _OLSR_ROUTING_TABLE

#include <net/route.h>

#include "hna_set.h"


#define NETMASK_HOST 0xffffffff
#define NETMASK_DEFAULT 0x0

struct rt_entry
{
  union olsr_ip_addr    rt_dst;
  union olsr_ip_addr    rt_router;
  union hna_netmask     rt_mask;
  olsr_u8_t  	        rt_flags; 
  olsr_u16_t 	        rt_metric;
  struct interface      *rt_if;
  struct rt_entry       *prev;
  struct rt_entry       *next;
};


struct destination_n
{
  struct rt_entry      *destination;
  struct destination_n *next;
};


/**
 * IPv4 <-> IPv6 wrapper
 */
union olsr_kernel_route
{
  struct
  {
    struct sockaddr rt_dst;
    struct sockaddr rt_gateway;
    olsr_u32_t rt_metric;
  } v4;

  struct
  {
    struct in6_addr rtmsg_dst;
    struct in6_addr rtmsg_gateway;
    olsr_u32_t rtmsg_metric;
  } v6;
};


struct rt_entry routingtable[HASHSIZE];
struct rt_entry hna_routes[HASHSIZE];


int
olsr_init_routing_table(void);

void 
olsr_calculate_routing_table(void);

void
olsr_calculate_hna_routes(void);

void
olsr_print_routing_table(struct rt_entry *);

#endif

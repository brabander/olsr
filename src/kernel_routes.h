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
 * $Id: kernel_routes.h,v 1.5 2004/09/21 19:08:57 kattemat Exp $
 *
 */


 

#ifndef _OLSR_KERNEL_RT
#define _OLSR_KERNEL_RT

#include "defs.h"
#include "routing_table.h"


int
olsr_ioctl_add_route(struct rt_entry *destination);

int
olsr_ioctl_add_route6(struct rt_entry *destination);

int
olsr_ioctl_del_route(struct rt_entry *destination);

int
olsr_ioctl_del_route6(struct rt_entry *destination);


int
add_tunnel_route(union olsr_ip_addr *);

int
delete_tunnel_route();

int
delete_all_inet_gws();


#endif

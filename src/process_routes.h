/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2004 Andreas Tønnesen (andreto@ifi.uio.no)
 *
 * This file is part of olsr.org.
 *
 * UniK olsrd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * UniK olsrd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with olsr.org; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "routing_table.h"
 

#ifndef _OLSR_PROCESS_RT
#define _OLSR_PROCESS_RT

#include <sys/ioctl.h>


olsr_u32_t tunl_netmask, tunl_gw;

struct rt_entry old_routes[HASHSIZE];
struct rt_entry old_hna[HASHSIZE];

int
olsr_init_old_table();

int
olsr_find_up_route(struct rt_entry *dst,struct rt_entry *table);

struct destination_n *
olsr_build_update_list(struct rt_entry *from_table, struct rt_entry *in_table);

void
olsr_update_kernel_routes();

void
olsr_update_kernel_hna_routes();

void
olsr_move_route_table(struct rt_entry *, struct rt_entry *);

void
olsr_delete_routes_from_kernel(struct destination_n *delete_kernel_list);

void
olsr_add_routes_in_kernel(struct destination_n *add_kernel_list);

int
olsr_delete_all_kernel_routes();

#endif

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
 * $Id: tc_set.h,v 1.6 2004/10/09 22:32:47 kattemat Exp $
 *
 */

#include "defs.h"

#ifndef _OLSR_TOP_SET
#define _OLSR_TOP_SET

#include "hashing.h"


struct topo_dst
{
  union olsr_ip_addr T_dest_addr;
  struct timeval T_time;
  olsr_u16_t T_seq;
  struct topo_dst *next;
  struct topo_dst *prev;
};


struct tc_entry
{
  union olsr_ip_addr T_last_addr;
  struct topo_dst destinations;
  struct tc_entry *next;
  struct tc_entry *prev;
};



/* Queue */
struct tc_entry tc_table[HASHSIZE];

int
olsr_init_tc(void);


int
olsr_tc_delete_mprs(struct tc_entry *, struct tc_message *);


struct tc_entry *
olsr_lookup_tc_entry(union olsr_ip_addr *);


struct topo_dst *
olsr_tc_lookup_dst(struct tc_entry *, union olsr_ip_addr *);


int
olsr_tc_delete_entry_if_empty(struct tc_entry *);


struct tc_entry *
olsr_add_tc_entry(union olsr_ip_addr *);


int
olsr_tc_update_mprs(struct tc_entry *, struct tc_message *);

int
olsr_print_tc_table(void);

void
olsr_time_out_tc_set(void);

#endif

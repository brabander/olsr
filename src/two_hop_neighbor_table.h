/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2003 Andreas Tønnesen (andreto@ifi.uio.no)
 *
 * This file is part of olsr.org.
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

#ifndef _OLSR_TWO_HOP_TABLE
#define _OLSR_TWO_HOP_TABLE

#include "defs.h"
#include "hashing.h"

#define	NB2S_COVERED 	0x1		/* node has been covered by a MPR */


struct neighbor_list_entry 
{
  struct	neighbor_entry *neighbor;
  struct	neighbor_list_entry *next;
  struct	neighbor_list_entry *prev;
};


struct neighbor_2_entry
{
  union olsr_ip_addr         neighbor_2_addr;
  olsr_u8_t      	     mpr_covered_count;    /*used in mpr calculation*/
  olsr_u8_t      	     processed;            /*used in mpr calculation*/
  olsr_16_t                  neighbor_2_pointer;   /* Neighbor count */
  struct neighbor_list_entry neighbor_2_nblist; 
  struct neighbor_2_entry    *prev;
  struct neighbor_2_entry    *next;
};

struct neighbor_2_entry two_hop_neighbortable[HASHSIZE];


int
olsr_init_two_hop_table();

void
olsr_delete_neighbor_pointer(struct neighbor_2_entry *, union olsr_ip_addr *);

void
olsr_delete_two_hop_neighbor_table(struct neighbor_2_entry *);

void
olsr_insert_two_hop_neighbor_table(struct neighbor_2_entry *);

struct neighbor_2_entry *
olsr_lookup_two_hop_neighbor_table(union olsr_ip_addr *);

struct neighbor_2_entry *
olsr_lookup_two_hop_neighbor_table_mid(union olsr_ip_addr *);

void
olsr_print_two_hop_neighbor_table();


#endif

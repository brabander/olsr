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
 * $Id: neighbor_table.h,v 1.6 2004/10/09 22:32:47 kattemat Exp $
 *
 */


#ifndef _OLSR_NEIGH_TBL
#define _OLSR_NEIGH_TBL

#include "olsr_protocol.h"
#include "hashing.h"


struct neighbor_2_list_entry 
{
  struct neighbor_2_entry      *neighbor_2;
  struct timeval	       neighbor_2_timer;
  struct neighbor_2_list_entry *next;
  struct neighbor_2_list_entry *prev;
};



struct neighbor_entry
{
  union olsr_ip_addr           neighbor_main_addr;
  olsr_u8_t                    status;
  olsr_u8_t                    willingness;
  olsr_u8_t                    is_mpr;
  olsr_u8_t                    was_mpr; /* Used to detect changes in MPR */
  int                          neighbor_2_nocov;
  int                          linkcount;
  struct neighbor_2_list_entry neighbor_2_list; 
  struct neighbor_entry        *next;
  struct neighbor_entry        *prev;
};


/*
 * The neighbor table
 */

struct neighbor_entry neighbortable[HASHSIZE];

void
olsr_init_neighbor_table(void);

int
olsr_delete_neighbor_2_pointer(struct neighbor_entry *, union olsr_ip_addr *);

struct neighbor_2_list_entry *
olsr_lookup_my_neighbors(struct neighbor_entry *, union olsr_ip_addr *);

int
olsr_delete_neighbor_table(union olsr_ip_addr *);

struct neighbor_entry *
olsr_insert_neighbor_table(union olsr_ip_addr *);

struct neighbor_entry *
olsr_lookup_neighbor_table(union olsr_ip_addr *);

struct neighbor_entry *
olsr_lookup_neighbor_table_alias(union olsr_ip_addr *);

void
olsr_time_out_two_hop_neighbors(struct neighbor_entry  *);

void
olsr_time_out_neighborhood_tables(void);

void
olsr_print_neighbor_table(void);


int
update_neighbor_status(struct neighbor_entry *, int);


#endif

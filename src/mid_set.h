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
 * $Id: mid_set.h,v 1.5 2004/09/21 19:08:57 kattemat Exp $
 *
 */

/*
 *Andreas Tønnesen
 */


#ifndef _OLSR_MID
#define _OLSR_MID

#include "hashing.h"

/*
 *Contains the main addr of a node and a list of aliases
 */
struct mid_entry
{
  union olsr_ip_addr main_addr;
  struct addresses *aliases;
  struct mid_entry *prev;
  struct mid_entry *next;
  struct timeval ass_timer;  
};


struct mid_entry mid_set[HASHSIZE];

int
olsr_init_mid_set();

void 
insert_mid_tuple(union olsr_ip_addr *, struct addresses *, float);

void
insert_mid_alias(union olsr_ip_addr *, union olsr_ip_addr *, float);

union olsr_ip_addr *
mid_lookup_main_addr(union olsr_ip_addr *);

struct addresses *
mid_lookup_aliases(union olsr_ip_addr *);

void
olsr_print_mid_set();

void
olsr_time_out_mid_set();

int
olsr_update_mid_table(union olsr_ip_addr *, float);

int
mid_delete_node(struct mid_entry *);

#endif

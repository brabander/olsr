/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2003 Andreas Tønnesen (andreto@ifi.uio.no)
 *
 * This file is part of the UniK OLSR daemon.
 *
 * The UniK OLSR daemon is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The UniK OLSR daemon is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the UniK OLSR daemon; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */


#ifndef _OLSR_MPR
#define _OLSR_MPR

#include "neighbor_table.h"


struct neighbor_2_list_entry *
olsr_find_2_hop_neighbors_with_1_link(int);

int
olsr_chosen_mpr(struct neighbor_entry *, olsr_u16_t *);

int
olsr_check_mpr_changes();

olsr_u16_t
olsr_calculate_two_hop_neighbors();

struct neighbor_entry *
olsr_find_maximum_covered(int);

void
olsr_clear_two_hop_processed();

void
olsr_calculate_mpr();

void
olsr_clear_mprs();

void
olsr_print_mpr_set();

void
olsr_optimize_mpr_set();


#endif

/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2004 Andreas Tønnesen (andreto@ifi.uio.no)
 *
 * This file is part of olsrd-unik.
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
 * along with olsrd-unik; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */



#ifndef _OLSR_MPRS_SET
#define _OLSR_MPRS_SET



struct mpr_selector
{
  union olsr_ip_addr MS_main_addr;
  struct timeval MS_time;
  struct mpr_selector *next;
  struct mpr_selector *prev;
};


/* MPR selector list */
struct mpr_selector mprs_list;

/* This nodes ansn */
olsr_u16_t ansn;

/* MPR selector counter */
int mprs_count;

/* Timer to send empty TCs */
struct timeval send_empty_tc;

int
olsr_init_mprs_set();


struct mpr_selector *
olsr_add_mpr_selector(union olsr_ip_addr *, float);


struct mpr_selector *
olsr_lookup_mprs_set(union olsr_ip_addr *);


int
olsr_update_mprs_set(union olsr_ip_addr *, float);


void
olsr_time_out_mprs_set();


void
olsr_print_mprs_set();


#endif

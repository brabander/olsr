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
 * $Id: duplicate_set.h,v 1.7 2004/10/18 13:13:36 kattemat Exp $
 *
 */



#ifndef _OLSR_DUP_TABLE
#define _OLSR_DUP_TABLE

#include "olsr_protocol.h"

#define UNKNOWN_MESSAGE 0

struct dup_entry
{
  union olsr_ip_addr     addr;      /* IP address of originator */
  olsr_u16_t             seqno;     /* Seqno of message */
  struct timeval         timer;	    /* Holding time */
  struct dup_iface       *ifaces;   /* Interfaces this message was recieved on */
  olsr_u8_t              forwarded; /* If this message was forwarded or not */
  struct dup_entry       *next;     /* Next entry */
  struct dup_entry       *prev;     /* Prev entry */
};

struct dup_iface
{
  union olsr_ip_addr     addr;      /* Addess of the interface */
  struct dup_iface       *next;     /* Next in line */
};

/* The duplicate table */
struct dup_entry dup_set[HASHSIZE];

struct timeval  hold_time_duplicate;

float dup_hold_time;


void
olsr_init_duplicate_table(void);

void
olsr_time_out_duplicate_table(void *);

int
olsr_check_dup_table_proc(union olsr_ip_addr *, olsr_u16_t);

int
olsr_check_dup_table_fwd(union olsr_ip_addr *, olsr_u16_t, union olsr_ip_addr *);

void
olsr_del_dup_entry(struct dup_entry *);

void
olsr_print_duplicate_table(void);

struct dup_entry *
olsr_add_dup_entry(union olsr_ip_addr *, olsr_u16_t);

int
olsr_update_dup_entry(union olsr_ip_addr *, olsr_u16_t, union olsr_ip_addr *);

int
olsr_set_dup_forward(union olsr_ip_addr *, olsr_u16_t);

int
olsr_check_dup_forward(union olsr_ip_addr *, olsr_u16_t);

#endif

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
 * $Id: process_package.h,v 1.7 2004/11/02 22:55:43 tlopatic Exp $
 *
 */


#ifndef _OLSR_PROCESS_PACKAGE
#define _OLSR_PROCESS_PACKAGE

#include "olsr_protocol.h"
#include "mpr.h"

void
olsr_init_package_process(void);

void
olsr_process_received_hello(union olsr_message *, struct interface *, union olsr_ip_addr *);

void
olsr_process_received_tc(union olsr_message *, struct interface *, union olsr_ip_addr *);

void
olsr_process_received_mid(union olsr_message *, struct interface *, union olsr_ip_addr *);

void
olsr_process_received_hna(union olsr_message *, struct interface *, union olsr_ip_addr *);




void
olsr_process_message_neighbors(struct neighbor_entry *,struct hello_message *);

void
olsr_linking_this_2_entries(struct neighbor_entry *,struct neighbor_2_entry *, float);

int
olsr_lookup_mpr_status(struct hello_message *, struct interface *);

#if defined USE_LINK_QUALITY
void
olsr_process_received_lq_hello(union olsr_message *ser,
                               struct interface *inif,
                               union olsr_ip_addr *from);

void
olsr_process_received_lq_tc(union olsr_message *ser, struct interface *inif,
                            union olsr_ip_addr *from);
#endif

#endif

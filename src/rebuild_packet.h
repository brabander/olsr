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
 * $Id: rebuild_packet.h,v 1.6 2004/11/02 22:55:43 tlopatic Exp $
 *
 */




#ifndef _OLSR_REBUILD
#define _OLSR_REBUILD

#include "olsr_protocol.h"
#include "packet.h"

void
hna_chgestruct(struct hna_message *, union olsr_message *);

void
mid_chgestruct(struct mid_message *, union olsr_message *);

void
unk_chgestruct(struct unknown_message *, union olsr_message *);

void
hello_chgestruct(struct hello_message *, union olsr_message *);

void
tc_chgestruct(struct tc_message *, union olsr_message *, union olsr_ip_addr *);

#if defined USE_LINK_QUALITY
void
lq_hello_chgestruct(struct lq_hello_message *deser, union olsr_message *ser);

void
lq_tc_chgestruct(struct lq_tc_message *deser, union olsr_message *ser,
                 union olsr_ip_addr *from);
#endif

#endif

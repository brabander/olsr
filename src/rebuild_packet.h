/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2003 Andreas Tønnesen (andreto@ifi.uio.no)
 *
 * This file is part of olsrd-unik.
 *
 * olsrd-unik is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * olsrd-unik is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with olsrd-unik; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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


#endif

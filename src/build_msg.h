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
 * $Id: build_msg.h,v 1.8 2004/11/01 19:27:10 tlopatic Exp $
 *
 */

#ifndef _BUILD_MSG_H
#define _BUILD_MSG_H

#include "packet.h"
#include "olsr_protocol.h"

void
hello_build(struct hello_message *, struct interface *);

void
tc_build(struct tc_message *, struct interface *);

void
mid_build(struct interface *);

void
hna_build(struct interface *);

#if defined USE_LINK_QUALITY
void
lq_hello_build(struct lq_hello_message *msg, struct interface *outif);

void
lq_tc_build(struct lq_tc_message *msg, struct interface *outif);
#endif

#endif


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
 * $Id: ifnet.h,v 1.7 2004/10/18 13:13:36 kattemat Exp $
 *
 */

/* Network interface configuration interface.
 * Platform independent - the implementations
 * reside in OS/ifnet.c(e.g. linux/ifnet.c)
 */

#ifndef _OLSR_IFNET
#define _OLSR_IFNET

/* To get ifreq */
//#include <arpa/inet.h>
#include <net/if.h>

int
set_flag(char *, short);

void
check_interface_updates(void *);

int
chk_if_changed(struct if_name *);

int
chk_if_up(struct if_name *, int);

#ifndef WIN32
int
check_wireless_interface(struct ifreq *);
#endif

#endif

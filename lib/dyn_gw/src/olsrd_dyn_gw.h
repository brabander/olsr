/*
 * OLSR plugin
 * Copyright (C) 2004 Andreas Tønnesen (andreto@olsr.org)
 *
 * This file is part of the olsrd dynamic gateway detection.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This plugin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with olsrd-unik; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 * 
 * $Id: olsrd_dyn_gw.h,v 1.5 2004/11/06 12:04:38 kattemat Exp $
 *
 */

/*
 * Dynamic linked library example for UniK OLSRd
 */

#ifndef _OLSRD_PLUGIN_TEST
#define _OLSRD_PLUGIN_TEST

#include "olsrd_plugin.h"

#define INET_NET       0
#define INET_PREFIX    0

#define PROCENTRY_ROUTE "/proc/net/route"

union olsr_ip_addr gw_net;
union hna_netmask gw_netmask;

/* Timeout function to register with the sceduler */
void
olsr_timeout();


/* Event function to register with the sceduler */
void
olsr_event();

int
check_gw(union olsr_ip_addr *, union hna_netmask *);

#endif

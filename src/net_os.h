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
 * $Id: net_os.h,v 1.2 2004/10/18 13:13:37 kattemat Exp $
 *
 */


/*
 * This file defines the OS dependent network related functions
 * that MUST be available to olsrd.
 * The implementations of the functions should be found in
 * <OS>/net.c (e.g. linux/net.c)
 */


#ifndef _OLSR_NET_OS_H
#define _OLSR_NET_OS_H

/* OS dependent functions */

int
bind_socket_to_device(int, char *);

int
convert_ip_to_mac(union olsr_ip_addr *, struct sockaddr *, char *);

int
disable_redirects(char *, int, int);

int
deactivate_spoof(char *, int, int);

int
restore_settings(int);

int
enable_ip_forwarding(int);

int  
getsocket(struct sockaddr *, int, char *);

int  
getsocket6(struct sockaddr_in6 *, int, char *);

int
get_ipv6_address(char *, struct sockaddr_in6 *, int);

#endif

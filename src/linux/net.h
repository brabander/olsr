/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2004 Andreas Tønnesen (andreto@ifi.uio.no)
 *
 * This file is part of olsr.org.
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
 */



#ifndef _OLSR_NET_LINUX
#define _OLSR_NET_LINUX

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <syslog.h>
#include <netinet/in.h>
#include "../olsr_protocol.h"

#define MAXIFS                  8 /* Maximum number of network interfaces */

/* Redirect proc entry */
#define REDIRECT_PROC "/proc/sys/net/ipv4/conf/%s/send_redirects"

/* IP spoof proc entry */
#define SPOOF_PROC "/proc/sys/net/ipv4/conf/%s/rp_filter"

/* The original state of the IP forwarding proc entry */
char orig_fwd_state;

/* Struct uesd to store original redirect/ingress setting */
struct nic_state
{
  int index; /* The OLSR index of the interface */
  char redirect; /* The original state of icmp redirect */
  char spoof; /* The original state of the IP spoof filter */
  struct nic_state *next;
};

struct nic_state nic_states[MAXIFS];


extern int
olsr_printf(int, char *, ...);

#endif

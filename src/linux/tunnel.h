/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2004 Andreas Tønnesen (andreto@ifi.uio.no)
 *
 * This file is part of olsr.org.
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
 * along with olsr.org; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */




#ifndef _OLSR_LINUX_TUNNEL
#define _OLSR_LINUX_TUNNEL



#include <net/if.h>
#include <net/if_arp.h>
#include <asm/types.h>
/* For tunneling */
#include <netinet/ip.h>
#include <linux/if_tunnel.h>


#define TUNL_PROC_FILE "/proc/sys/net/ipv4/conf/tunl0/forwarding"

#ifndef IP_DF
#define IP_DF		0x4000		/* Flag: "Don't Fragment"	*/
#endif

struct ip_tunnel_parm ipt;

/* Address of the tunnel endpoint */
union olsr_ip_addr tnl_addr;

void
set_up_source_tnl(union olsr_ip_addr *, union olsr_ip_addr *, int);

int
add_ip_tunnel(struct ip_tunnel_parm *);

int
del_ip_tunnel(struct ip_tunnel_parm *);

int
set_up_gw_tunnel();
 
int
enable_tunl_forwarding();

#endif

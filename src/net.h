
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



#ifndef _OLSR_NET
#define _OLSR_NET

#include "defs.h"
#include "process_routes.h"
#include <arpa/inet.h>
#include <net/if.h>

/* Packet transform functions */

struct ptf
{
  int (*function)(char *, int *);
  struct ptf *next;
};

struct ptf *ptf_list;

void
init_net();

int
net_output(struct interface*);

int
net_forward();

int
net_sendroute(struct rt_entry *, struct sockaddr *);

int
olsr_prefix_to_netmask(union olsr_ip_addr *, olsr_u16_t);

olsr_u16_t
olsr_netmask_to_prefix(union olsr_ip_addr *);

char *
sockaddr_to_string(struct sockaddr *);

char *
ip_to_string(olsr_u32_t *);

char *
ip6_to_string(struct in6_addr *);

char *
olsr_ip_to_string(union olsr_ip_addr *);

int
join_mcast(struct interface *, int);

int
add_ptf(int (*)(char *, int *));

int
del_ptf(int (*f)(char *, int *));


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


#endif

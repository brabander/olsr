
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
 * $Id: interfaces.h,v 1.5 2004/09/21 19:08:57 kattemat Exp $
 *
 */


#ifndef _OLSR_INTERFACE
#define _OLSR_INTERFACE

#include "olsr_protocol.h"

#define _PATH_PROCNET_IFINET6           "/proc/net/if_inet6"


#define IPV6_ADDR_ANY		0x0000U

#define IPV6_ADDR_UNICAST      	0x0001U
#define IPV6_ADDR_MULTICAST    	0x0002U
#define IPV6_ADDR_ANYCAST	0x0004U

#define IPV6_ADDR_LOOPBACK	0x0010U
#define IPV6_ADDR_LINKLOCAL	0x0020U
#define IPV6_ADDR_SITELOCAL	0x0040U

#define IPV6_ADDR_COMPATv4	0x0080U

#define IPV6_ADDR_SCOPE_MASK	0x00f0U

#define IPV6_ADDR_MAPPED	0x1000U
#define IPV6_ADDR_RESERVED	0x2000U



/**
 *A struct containing all necessary information about each
 *interface participating in the OLSD routing
 */
struct interface 
{
  /* IP version 4 */
  struct	sockaddr int_addr;		/* address */
  struct	sockaddr int_netmask;		/* netmask */
  struct	sockaddr int_broadaddr;         /* broadcast address */
  /* IP version 6 */
  struct        sockaddr_in6 int6_addr;         /* Address */
  struct        sockaddr_in6 int6_multaddr;     /* Multicast */
  /* IP independent */
  union         olsr_ip_addr ip_addr;
  int           olsr_socket;                    /* The broadcast socket for this interface */
  int	        int_metric;			/* metric of interface */
  int	        int_flags;			/* see below */
  char	        *int_name;			/* from kernel if structure */
  int           if_index;                       /* Kernels index of this interface */
  int           if_nr;                          /* This interfaces index internally*/
  int           is_wireless;                    /* wireless interface or not*/
  olsr_u16_t    olsr_seqnum;                    /* Olsr message seqno */
  struct	interface *int_next;
};





struct if_name
{
  char *name;
  int configured;
  int index;
  struct interface *interf;
  struct if_name *next;
};

struct if_name *if_names;

int queued_ifs;

#define	IFF_PASSIVE	0x200000	/* can't tell if up/down */
#define	IFF_INTERFACE	0x400000	/* hardware interface */



/* Ifchange functions */

struct ifchgf
{
  int (*function)(struct interface *, int);
  struct ifchgf *next;
};

struct ifchgf *ifchgf_list;

/* Ifchange actions */

#define IFCHG_IF_ADD           1
#define IFCHG_IF_REMOVE        2
#define IFCHG_IF_UPDATE        3

/* Variables needed to set up new sockets */
extern int precedence;
extern int tos_bits;
extern int bufspace;


/* The interface linked-list */
struct interface *ifnet;

/* Datastructures to use when creating new sockets */
struct sockaddr_in addrsock;
struct sockaddr_in6 addrsock6;

int
ifinit();

struct interface *
if_ifwithsock(int);

struct	interface *
if_ifwithaddr(union olsr_ip_addr *);

int
get_ipv6_address(char *, struct sockaddr_in6 *, int);

void
queue_if(char *);

int
add_ifchgf(int (*f)(struct interface *, int));

int
del_ifchgf(int (*f)(struct interface *, int));

#endif

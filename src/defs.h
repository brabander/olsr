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
 * $Id: defs.h,v 1.21 2004/11/03 10:00:10 kattemat Exp $
 *
 */

#ifndef _OLSR_DEFS
#define _OLSR_DEFS

/* Common includes */
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "log.h"
#include "olsr_protocol.h"
#include "process_routes.h" /* Needed for rt_entry */
#include "net.h" /* IPaddr -> string conversions is used by everyone */
#include "olsr.h" /* Everybody uses theese */
#include "olsr_cfg.h"

#define VERSION "0.4.8-pre"
#define SOFTWARE_VERSION "olsr.org - " VERSION

#define OLSRD_CONF_FILE_NAME "olsrd.conf"
#define OLSRD_GLOBAL_CONF_FILE "/etc/" OLSRD_CONF_FILE_NAME

#define	HOPCNT_MAX		16	/* maximum hops number */
#define	MAXMESSAGESIZE		1500	/* max broadcast size */
#define UDP_IP_HDRSIZE          28

#define OLSR_SELECT_TIMEOUT     2       /* The timeout for the main select loop */

#define MAX_IFS                 32


/* Debug helper macro */
#ifdef DEBUG
#define debug(format,args...) \
   olsr_printf(1, "%s (%s:%d): ", __func__, __FILE__, __LINE__); \
   olsr_printf(1, format, ##args);
#endif


/*
 * Global olsrd configuragtion
 */

struct olsrd_config *olsr_cnf;

/*
 * Generic address list elem
 */
struct addresses 
{
  union olsr_ip_addr address;
  struct addresses *next;
};


int exit_value; /* Global return value for process termination */


/* Timer data */
struct timeval now;		/* current idea of time */
struct tm *nowtm;		/* current idea of time (in tm) */

char ipv6_buf[100];             /* buffer for IPv6 inet_htop */

olsr_bool disp_pack_in;               /* display incoming packet content? */
olsr_bool disp_pack_out;               /* display outgoing packet content? */


int llinfo;

olsr_bool inet_tnl_added; /* If Internet gateway tunnel is added */
olsr_bool use_tunnel; /* Use Internet gateway tunneling */
olsr_bool gw_tunnel; /* Work as Internet gateway */
olsr_bool del_gws;

/*
 * Timer values
 */

extern float will_int;
extern float dup_hold_time;
extern float max_jitter;

size_t ipsize;

/*
 * Address of this hosts OLSR interfaces
 * and main address of this node
 * and number of OLSR interfaces on this host
 */
union olsr_ip_addr main_addr;
/*
 * OLSR UPD port
 */

int olsr_udp_port;

/* The socket used for all ioctls */
int ioctl_s;

float max_tc_vtime;

struct timeval fwdtimer[MAX_IFS];	/* forwarding timer */

extern struct timeval hold_time_fwd;

struct sockaddr_in6 null_addr6;      /* Address used as Originator Address IPv6 */

int minsize;


extern struct ip_tunnel_parm ipt;
extern union olsr_ip_addr tnl_addr; /* The gateway address if inet_tnl_added==1 */

olsr_bool changes;                /* is set if changes occur in MPRS set */ 

/* TC empty message sending */
extern struct timeval send_empty_tc;


/* Used by everyone */

extern int
olsr_printf(int, char *, ...);

/*
 *IPC functions
 *These are moved to a plugin soon
 * soon... duh!
 */

int
ipc_init(void);

int
ipc_input(int);

int
shutdown_ipc(void);

int
ipc_output(struct olsr *);

int
ipc_send_net_info(void);

int
ipc_route_send_rtentry(union olsr_kernel_route *, int, char *);




#endif

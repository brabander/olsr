/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2004 Andreas Tønnesen (andreto@ifi.uio.no)
 *
 * This file is part of olsrd-unik.
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
 * along with olsrd-unik; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _OLSR_DEFS
#define _OLSR_DEFS


#include <sys/time.h>
#include <net/route.h>

#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#define UP             1
#define DOWN           0

#include "olsr_protocol.h"
#include "process_routes.h" /* Needed for rt_entry */
#include "net.h" /* IPaddr -> string conversions is used by everyone */
#include "olsr.h" /* Everybody uses theese */

#define VERSION "0.4.7-pre"
#define SOFTWARE_VERSION "olsr.org - " VERSION

#define	HOPCNT_MAX		16	/* maximum hops number */
#define	MAXMESSAGESIZE		512	/* max broadcast size */

#define OLSR_SELECT_TIMEOUT     2       /* The timeout for the main select loop */
/* Debug helper macro */

#ifdef DEBUG
#define debug(format,args...) \
   olsr_printf(1, "%s (%s:%d): ", __func__, __FILE__, __LINE__); \
   olsr_printf(1, format, ##args);
#endif

/*
 * Address list
 */
struct addresses 
{
  union olsr_ip_addr address;
  struct addresses *next;
};


int exit_value; /* Global return value for process termination */

/* mantissa/exponent Vtime and Htime variables */
olsr_u8_t hello_vtime;
olsr_u8_t hello_nw_vtime;
olsr_u8_t tc_vtime;
olsr_u8_t mid_vtime;
olsr_u8_t hna_vtime;
olsr_u8_t htime;
olsr_u8_t htime_nw;


/* Timer data */
struct timeval now;		/* current idea of time */
struct tm *nowtm;		/* current idea of time (in tm) */

char ipv6_buf[100];             /* buffer for IPv6 inet_htop */
char ipv6_mult[50];             /* IPv6 multicast group */

int disp_pack_in;               /* display incoming packet content? */
int disp_pack_out;               /* display outgoing packet content? */


int use_ipc; /* Should we use the ipc socket for the front-end */

int use_hysteresis;

int llinfo;

int inet_tnl_added; /* If Internet gateway tunnel is added */
int use_tunnel; /* Use Internet gateway tunneling */
int gw_tunnel; /* Work as Internet gateway */


/*
 * Willingness
 */
int willingness_set;
int my_willingness;


/*
 * Timer values
 */

extern float will_int;
extern float neighbor_hold_time_nw;
extern float mid_hold_time;
extern float hna_hold_time;
extern float dup_hold_time;
extern float hello_int_nw;
extern float mid_int;
extern float max_jitter;


/*
 * Debug value
 */
int debug_level;

/*
 * Ipversion beeing used AF_INET or AF_INET6
 * and size of an IP address
 */
int ipversion;
size_t ipsize;

/*
 * IPv6 addresstype to use
 * global or site-local
 */
int ipv6_addrtype;

/*
 * Address of this hosts OLSR interfaces
 * and main address of this node
 * and number of OLSR interfaces on this host
 */
union olsr_ip_addr main_addr;
int nbinterf;
int allow_no_int; /* Run if no interfaces are present? */

/*
 * Hysteresis data
 */

float hyst_scaling;
float hyst_threshold_low;
float hyst_threshold_high;


/*
 * TC redundancy
 */

int tc_redundancy;
int sending_tc;

/*
 * MPR redundacy
 */

int mpr_coverage;


/*
 * OLSR UPD port
 */

int olsr_udp_port;


/* Max OLSR packet size */

int maxmessagesize;

/* Timeout multipliers */

int neighbor_timeout_mult;
int topology_timeout_mult;
int neighbor_timeout_mult_nw;
int mid_timeout_mult;
int hna_timeout_mult;

/* The socket used for all ioctls */

int ioctl_s;

extern float hello_int, tc_int, polling_int, hna_int;


struct timeval fwdtimer;	/* forwarding timer */

extern struct timeval hold_time_fwd;

extern struct sockaddr_in6 null_addr6;


extern int del_gws;
extern int inet_gw; /* Are we an internet gateway? */

extern int minsize;

extern int option_i;

/* Broadcast 255.255.255.255 */
extern int bcast_set;
extern struct sockaddr_in bcastaddr;


extern struct hna_entry *hna_old;



extern struct ip_tunnel_parm ipt;
extern union olsr_ip_addr tnl_addr; /* The gateway address if inet_tnl_added==1 */

extern char	              packet[MAXMESSAGESIZE+1];
extern int                    outputsize;
extern union olsr_packet      *msg;
extern char	              fwd_packet[MAXMESSAGESIZE+1];
extern int                    fwdsize;
extern union olsr_packet      *fwdmsg;

olsr_u8_t changes;                /* is set if changes occur in MPRS set */ 

extern float                  topology_hold_time, neighbor_hold_time;


/* TC empty message sending */
extern struct timeval send_empty_tc;


/* Used by everyone */

extern int
olsr_printf(int, char *, ...);

/*
 *IPC functions
 *These are moved to a plugin soon
 */

int
ipc_init();

int
ipc_input(int);

int
shutdown_ipc();

int
ipc_output(struct olsr *);

int
ipc_send_net_info();

int
ipc_route_send_rtentry(union olsr_kernel_route *, int, char *);




#endif

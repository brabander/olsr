/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004, Andreas Tønnesen(andreto@olsr.org)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met:
 *
 * * Redistributions of source code must retain the above copyright 
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright 
 *   notice, this list of conditions and the following disclaimer in 
 *   the documentation and/or other materials provided with the 
 *   distribution.
 * * Neither the name of olsr.org, olsrd nor the names of its 
 *   contributors may be used to endorse or promote products derived 
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN 
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Visit http://www.olsr.org for more information.
 *
 * If you find this software useful feel free to make a donation
 * to the project. For more information see the website or contact
 * the copyright holders.
 *
 * $Id: defs.h,v 1.38 2005/02/26 23:01:40 kattemat Exp $
 */

#ifndef OLSR_PLUGIN

#ifndef _OLSR_DEFS
#define _OLSR_DEFS

/* Common includes */
#include <sys/time.h>
#include <sys/times.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include "log.h"
#include "olsr_protocol.h"
#include "process_routes.h" /* Needed for rt_entry */
#include "net.h" /* IPaddr -> string conversions is used by everyone */
#include "olsr.h" /* Everybody uses theese */
#include "olsr_cfg.h"

#define VERSION "0.4.9-pre"
#define SOFTWARE_VERSION "olsr.org - " VERSION
#define OLSRD_VERSION_DATE "       *** " SOFTWARE_VERSION " (" __DATE__ ") ***\n"

#define OLSRD_CONF_FILE_NAME "olsrd.conf"
#define OLSRD_GLOBAL_CONF_FILE "/etc/" OLSRD_CONF_FILE_NAME

#define	HOPCNT_MAX		16	/* maximum hops number */
#define	MAXMESSAGESIZE		1500	/* max broadcast size */
#define UDP_IP_HDRSIZE          28
#define MAX_IFS                 16

/* Debug helper macro */
#ifdef DEBUG
#define olsr_debug(lvl,format,args...) \
   olsr_printf(lvl, "%s (%s:%d): ", __func__, __FILE__, __LINE__); \
   olsr_printf(lvl, format, ##args);
#endif

FILE *debug_handle;

#ifdef NO_DEBUG
#define OLSR_PRINTF(lvl, format, args...)
#else
#define OLSR_PRINTF(lvl, format, args...) \
   { \
     if((olsr_cnf->debug_level >= lvl) && debug_handle) \
        fprintf(debug_handle, format, ##args); \
   }
#endif


/*
 * Global olsrd configuragtion
 */

struct olsrd_config *olsr_cnf;

/* Global tick resolution */
olsr_u16_t system_tick_divider;

int exit_value; /* Global return value for process termination */


/* Timer data */
clock_t now_times;              /* current idea of times(2) reported uptime */
struct timeval now;		/* current idea of time */
struct tm *nowtm;		/* current idea of time (in tm) */

olsr_bool disp_pack_in;         /* display incoming packet content? */
olsr_bool disp_pack_out;        /* display outgoing packet content? */

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

/* routing socket */
#if defined __FreeBSD__ || defined __MacOSX__ || defined __NetBSD__
int rts;
#endif

float max_tc_vtime;

clock_t fwdtimer[MAX_IFS];	/* forwarding timer */

int minsize;

olsr_bool changes;                /* is set if changes occur in MPRS set */ 

/* TC empty message sending */
extern clock_t send_empty_tc;

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
ipc_route_send_rtentry(union olsr_ip_addr*, union olsr_ip_addr *, int, int, char *);



#endif
#endif

/*
 * Copyright (c) 2005, Bruno Randolf <bruno.randolf@4g-systems.biz>
 * Copyright (c) 2004, Andreas Tønnesen(andreto-at-olsr.org)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met:
 *
 * * Redistributions of source code must retain the above copyright notice, 
 *   this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice, 
 *   this list of conditions and the following disclaimer in the documentation 
 *   and/or other materials provided with the distribution.
 * * Neither the name of the UniK olsr daemon nor the names of its contributors 
 *   may be used to endorse or promote products derived from this software 
 *   without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED 
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* $Id: olsrd_plugin.h,v 1.2 2005/01/30 16:33:53 kattemat Exp $ */

/*
 * Dynamic linked library example for UniK OLSRd
 */

#ifndef _OLSRD_PLUGIN_DEFS
#define _OLSRD_PLUGIN_DEFS


#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>

#include "olsr_plugin_io.h"

/* Use this as PARSER_TYPE to receive ALL messages! */
#define PROMISCUOUS 0xffffffff


/****************************************************************************
 *           Various datastructures and definitions from olsrd              *
 ****************************************************************************/

/*
 * TYPES SECTION
 */

/* types */
#include <sys/types.h>

#ifndef WIN32
typedef u_int8_t        olsr_u8_t;
typedef u_int16_t       olsr_u16_t;
typedef u_int32_t       olsr_u32_t;
typedef int8_t          olsr_8_t;
typedef int16_t         olsr_16_t;
typedef int32_t         olsr_32_t;
#else
typedef unsigned char olsr_u8_t;
typedef unsigned short olsr_u16_t;
typedef unsigned int olsr_u32_t;
typedef char olsr_8_t;
typedef short olsr_16_t;
typedef int olsr_32_t;
#endif


/*
 * VARIOUS DEFINITIONS
 */

union olsr_ip_addr
{
  olsr_u32_t v4;
  struct in6_addr v6;
};


#define MAX_TTL               0xff


/*
 *Link Types
 */

#define UNSPEC_LINK           0
#define ASYM_LINK             1
#define SYM_LINK              2
#define LOST_LINK             3
#define HIDE_LINK             4
#define MAX_LINK              4



/*
 * Hashing
 */

#define	HASHSIZE	32
#define	HASHMASK	(HASHSIZE - 1)

#define MAXIFS         8 /* Maximum number of interfaces (from defs.h) in uOLSRd */


/****************************************************************************
 *                          INTERFACE SECTION                               *
 ****************************************************************************/
struct vtimes
{
  olsr_u8_t hello;
  olsr_u8_t tc;
  olsr_u8_t mid;
  olsr_u8_t hna;
};

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
  int           int_mtu;                        /* MTU of interface */
  int	        int_flags;			/* see below */
  char	        *int_name;			/* from kernel if structure */
  int           if_index;                       /* Kernels index of this interface */
  int           if_nr;                          /* This interfaces index internally*/
  int           is_wireless;                    /* wireless interface or not*/
  olsr_u16_t    olsr_seqnum;                    /* Olsr message seqno */

  float         hello_etime;
  struct        vtimes valtimes;

  struct	interface *int_next;
};


/****************************************************************************
 *                            PACKET SECTION                                *
 ****************************************************************************/

 /**********************************
 * DEFINE YOUR CUSTOM PACKET HERE *
 **********************************/
#include "nameservice_msg.h"

/* 
 * ALWAYS USE THESE WRAPPERS TO
 * ENSURE IPv4 <-> IPv6 compability 
 */

union olsr_message
{
  struct olsrmsg v4;
  struct olsrmsg6 v6;
};




/***************************************************************************
 *                 Functions provided by uolsrd_plugin.c                   *
 *                  Similar to their siblings in olsrd                     *
 ***************************************************************************/

char ipv6_buf[100]; /* buffer for IPv6 inet_htop */

/* All these could optionally be fetched from olsrd */

olsr_u32_t
olsr_hashing(union olsr_ip_addr *);

void
olsr_get_timestamp(olsr_u32_t, struct timeval *);

void
olsr_init_timer(olsr_u32_t, struct timeval *);

int
olsr_timed_out(struct timeval *);

char *
olsr_ip_to_string(union olsr_ip_addr *);



/****************************************************************************
 *                Function pointers to functions in olsrd                   *
 *              These allow direct access to olsrd functions                *
 ****************************************************************************/

/* The multi-purpose funtion. All other functions are fetched trough this */
int (*olsr_plugin_io)(int, void *, size_t);

/* add a prser function */
void (*olsr_parser_add_function)(void (*)(union olsr_message *, struct interface *, union olsr_ip_addr *), int, int);

/* Register a timeout function */
int (*olsr_register_timeout_function)(void (*)(void));

/* Register a scheduled event */
int (*olsr_register_scheduler_event)(void (*)(void *), void *, float, float, olsr_u8_t *);

/* Get the next message seqno in line */
olsr_u16_t (*get_msg_seqno)(void);

int (*net_outbuffer_push)(struct interface *, olsr_u8_t *, olsr_u16_t);

/* Transmit package */
int (*net_output)(struct interface*);

/* Check the duplicate table for prior processing */
int (*check_dup_proc)(union olsr_ip_addr *, olsr_u16_t);

/* Default forward algorithm */
int (*default_fwd)(union olsr_message *, 
		   union olsr_ip_addr *, 
		   olsr_u16_t,  
		   struct interface *, 
		   union olsr_ip_addr *);

/* Add a socket to the main olsrd select loop */
void (*add_olsr_socket)(int, void(*)(int));

/* get the link status to a neighbor */
int (*check_neighbor_link)(union olsr_ip_addr *);

/* Mantissa/exponen conversions */
olsr_u8_t (*double_to_me)(double);

double (*me_to_double)(olsr_u8_t);

/* olsrd printf wrapper */
int (*olsr_printf)(int, char *, ...);

/* olsrd malloc wrapper */
void *(*olsr_malloc)(size_t, const char *);


/****************************************************************************
 *                             Data from olsrd                              *
 *           NOTE THAT POINTERS POINT TO THE DATA USED BY OLSRD!            *
 *               NEVER ALTER DATA POINTED TO BY THESE POINTERS              * 
 *                   UNLESS YOU KNOW WHAT YOU ARE DOING!!!                  *
 ****************************************************************************/
/**
 * The interface list from olsrd
 */

struct interface   *ifs;

/* These two are set automatically by olsrd at load time */
int                ipversion;  /* IPversion in use */
union olsr_ip_addr *main_addr; /* Main address */


size_t             ipsize;     /* Size of the ipadresses used */
struct timeval     *now;       /* the olsrds schedulers idea of current time */


/****************************************************************************
 *                Functions that the plugin MUST provide                    *
 ****************************************************************************/


/* Initialization function */
int
olsr_plugin_init(void);

/* Destructor function */
void
olsr_plugin_exit(void);

/* Mulitpurpose funtion */
int
plugin_io(int, void *, size_t);

/* Plugin interface version */
int 
get_plugin_interface_version(void);


/*****************************************************************************
 *                               Plugin data                                 *
 *****************************************************************************/

#define MOD_DESC PLUGIN_NAME " " PLUGIN_VERSION " by " PLUGIN_AUTHOR
#define PLUGIN_INTERFACE_VERSION 2

#endif

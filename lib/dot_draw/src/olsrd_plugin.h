

/*
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


/*****************************************************************************
 *                               Plugin data                                 *
 *                       ALTER THIS TO YOUR OWN NEED                         *
 *****************************************************************************/

#define PLUGIN_NAME    "OLSRD dot draw plugin"
#define PLUGIN_VERSION "0.1"
#define PLUGIN_AUTHOR   "Andreas Tønnesen"
#define MOD_DESC PLUGIN_NAME " " PLUGIN_VERSION " by " PLUGIN_AUTHOR
#define PLUGIN_INTERFACE_VERSION 1

/* The type of message you will use */
#define MESSAGE_TYPE 128

/* The type of messages we will receive - can be set to promiscuous */
#define PARSER_TYPE MESSAGE_TYPE



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

union hna_netmask
{
  olsr_u32_t v4;
  olsr_u16_t v6;
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
 * Mantissa scaling factor
 */

#define VTIME_SCALE_FACTOR    0.0625


/*
 * Hashing
 */

#define	HASHSIZE	32
#define	HASHMASK	(HASHSIZE - 1)

#define MAXIFS         8 /* Maximum number of interfaces (from defs.h) in uOLSRd */



/*
 * Neighbor structures
 */

/* One hop neighbor */

struct neighbor_2_list_entry 
{
  struct neighbor_2_entry      *neighbor_2;
  struct timeval	       neighbor_2_timer;
  struct neighbor_2_list_entry *next;
  struct neighbor_2_list_entry *prev;
};

struct neighbor_entry
{
  union olsr_ip_addr           neighbor_main_addr;
  olsr_u8_t                    status;
  olsr_u8_t                    willingness;
  olsr_u8_t                    is_mpr;
  olsr_u8_t                    was_mpr; /* Used to detect changes in MPR */
  int                          neighbor_2_nocov;
  int                          linkcount;
  struct neighbor_2_list_entry neighbor_2_list; 
  struct neighbor_entry        *next;
  struct neighbor_entry        *prev;
};


/* Two hop neighbor */



struct neighbor_list_entry 
{
  struct	neighbor_entry *neighbor;
  struct	neighbor_list_entry *next;
  struct	neighbor_list_entry *prev;
};


struct neighbor_2_entry
{
  union olsr_ip_addr         neighbor_2_addr;
  olsr_u8_t      	     mpr_covered_count;    /*used in mpr calculation*/
  olsr_u8_t      	     processed;            /*used in mpr calculation*/
  olsr_16_t                  neighbor_2_pointer;   /* Neighbor count */
  struct neighbor_list_entry neighbor_2_nblist; 
  struct neighbor_2_entry    *prev;
  struct neighbor_2_entry    *next;
};

/* Topology entry */

struct topo_dst
{
  union olsr_ip_addr T_dest_addr;
  struct timeval T_time;
  olsr_u16_t T_seq;
  struct topo_dst *next;
  struct topo_dst *prev;
};


struct tc_entry
{
  union olsr_ip_addr T_last_addr;
  struct topo_dst destinations;
  struct tc_entry *next;
  struct tc_entry *prev;
};

/* HNA */

/* hna_netmask declared in packet.h */

struct hna_net
{
  union olsr_ip_addr A_network_addr;
  union hna_netmask  A_netmask;
  struct timeval     A_time;
  struct hna_net     *next;
  struct hna_net     *prev;
};

struct hna_entry
{
  union olsr_ip_addr A_gateway_addr;
  struct hna_net     networks;
  struct hna_entry   *next;
  struct hna_entry   *prev;
};

/* The lists */

struct neighbor_entry *neighbortable;
struct neighbor_2_entry *two_hop_neighbortable;
struct tc_entry *tc_table;
struct hna_entry *hna_set;


/* Buffer for olsr_ip_to_string */

char ipv6_buf[100]; /* buffer for IPv6 inet_htop */


/****************************************************************************
 *                Function pointers to functions in olsrd                   *
 *              These allow direct access to olsrd functions                *
 ****************************************************************************/

/* The multi-purpose funtion. All other functions are fetched trough this */
int (*olsr_plugin_io)(int, void *, size_t);

/* Register a scheduled event */
int (*olsr_register_scheduler_event)(void (*)(), float, float, olsr_u8_t *);

/* Register a "process changes" function */
int (*register_pcf)(int (*)(int, int, int));

/* Get the next message seqno in line */
olsr_u16_t (*get_msg_seqno)();

/* Check the duplicate table for prior processing */
int (*check_dup_proc)(union olsr_ip_addr *, olsr_u16_t);


/* Add a socket to the main olsrd select loop */
void (*add_olsr_socket)(int, void(*)(int));

/* Remove a socket from the main olsrd select loop */
int (*remove_olsr_socket)(int, void(*)(int));

/* get the link status to a neighbor */
int (*check_neighbor_link)(union olsr_ip_addr *);

/* Mantissa/exponen conversions */
olsr_u8_t (*double_to_me)(double);

double (*me_to_double)(olsr_u8_t);

/* olsrd printf wrapper */
int (*olsr_printf)(int, char *, ...);

/* olsrd malloc wrapper */
void *(*olsr_malloc)(size_t, const char *);

/* Add hna net IPv4 */
void (*add_local_hna4_entry)(union olsr_ip_addr *, union hna_netmask *);

/* Remove hna net IPv4 */
int (*remove_local_hna4_entry)(union olsr_ip_addr *, union hna_netmask *);

/* Add hna net IPv6 */
void (*add_local_hna6_entry)(union olsr_ip_addr *, union hna_netmask *);

/* Remove hna net IPv6 */
int (*remove_local_hna6_entry)(union olsr_ip_addr *, union hna_netmask *);


/****************************************************************************
 *                             Data from olsrd                              *
 *           NOTE THAT POINTERS POINT TO THE DATA USED BY OLSRD!            *
 *               NEVER ALTER DATA POINTED TO BY THESE POINTERS              * 
 *                   UNLESS YOU KNOW WHAT YOU ARE DOING!!!                  *
 ****************************************************************************/

/* These two are set automatically by olsrd at load time */
int                ipversion;  /* IPversion in use */
union olsr_ip_addr *main_addr; /* Main address */


size_t             ipsize;     /* Size of the ipadresses used */
struct timeval     *now;       /* the olsrds schedulers idea of current time */

/* Data that can be altered by your plugin */
char               *buffer;    /* The packet buffer - put your packet here */
int                *outputsize;/* Pointer to the outputsize - set the size of your packet here */


/****************************************************************************
 *                Functions that the plugin MUST provide                    *
 ****************************************************************************/


/* Initialization function */
int
olsr_plugin_init();

/* IPC initialization function */
int
plugin_ipc_init();

/* Destructor function */
void
olsr_plugin_exit();

/* Mulitpurpose funtion */
int
plugin_io(int, void *, size_t);

#endif

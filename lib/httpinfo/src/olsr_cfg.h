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
 * $Id: olsr_cfg.h,v 1.1 2004/12/18 00:19:09 kattemat Exp $
 */


#ifndef _OLSRD_CFGPARSER_H
#define _OLSRD_CFGPARSER_H

#include "olsrd_plugin.h"


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



struct olsr_msg_params
{
  float                    emission_interval;
  float                    validity_time;
};

struct if_config_options
{
  union olsr_ip_addr       ipv4_broadcast;
  int                      ipv6_addrtype;
  union olsr_ip_addr       ipv6_multi_site;
  union olsr_ip_addr       ipv6_multi_glbl;
  struct olsr_msg_params   hello_params;
  struct olsr_msg_params   tc_params;
  struct olsr_msg_params   mid_params;
  struct olsr_msg_params   hna_params;
};



struct olsr_if
{
  char                     *name;
  char                     *config;
  int                      index;
  olsr_bool                configured;
  struct interface         *interf;
  struct if_config_options *cnf;
  struct olsr_if           *next;
};

struct hna4_entry
{
  union olsr_ip_addr       net;
  union olsr_ip_addr       netmask;
  struct hna4_entry        *next;
};

struct hna6_entry
{
  union olsr_ip_addr       net;
  olsr_u16_t               prefix_len;
  struct hna6_entry        *next;
};

struct hyst_param
{
  float                    scaling;
  float                    thr_high;
  float                    thr_low;
};

struct plugin_param
{
  char                     *key;
  char                     *value;
  struct plugin_param      *next;
};

struct plugin_entry
{
  char                     *name;
  struct plugin_param      *params;
  struct plugin_entry      *next;
};

struct ipc_host
{
  union olsr_ip_addr       host;
  struct ipc_host          *next;
};

struct ipc_net
{
  union olsr_ip_addr       net;
  union olsr_ip_addr       mask;
  struct ipc_net           *next;
};

/*
 * The config struct
 */

struct olsrd_config
{
  int                      debug_level;
  int                      ip_version;
  olsr_bool                allow_no_interfaces;
  olsr_u16_t               tos;
  olsr_bool                willingness_auto;
  olsr_u8_t                willingness;
  int                      ipc_connections;
  olsr_bool                open_ipc;
  olsr_bool                use_hysteresis;
  struct hyst_param        hysteresis_param;
  float                    pollrate;
  olsr_u8_t                tc_redundancy;
  olsr_u8_t                mpr_coverage;
  olsr_bool                clear_screen;
  olsr_u8_t                lq_level;
  olsr_u32_t               lq_wsize;
  struct plugin_entry      *plugins;
  struct hna4_entry        *hna4_entries;
  struct hna6_entry        *hna6_entries;
  struct ipc_host          *ipc_hosts;
  struct ipc_net           *ipc_nets;
  struct olsr_if           *interfaces;
  olsr_u16_t               ifcnt;
};


struct olsrd_config *cfg;


#endif

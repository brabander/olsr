/*
 * OLSR ad-hoc routing table management protocol config parser
 * Copyright (C) 2004 Andreas Tønnesen (andreto@olsr.org)
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
 * $Id: olsr_cfg.h,v 1.3 2004/11/02 19:27:13 kattemat Exp $
 *
 */


#ifndef _OLSRD_CFGPARSER_H
#define _OLSRD_CFGPARSER_H

/* Default valuse not declared in olsr_protocol.h */
#define DEF_POLLRATE       0.1
#define DEF_WILL_AUTO      1
#define DEF_ALLOW_NO_INTS  1
#define DEF_TOS            16
#define DEF_DEBUGLVL       1
#define DEF_OPEN_IPC       0
#define DEF_USE_HYST       1

#ifndef IPV6_ADDR_SITELOCAL
#define IPV6_ADDR_SITELOCAL    0x0040U
#endif

#include "olsr_protocol.h"

#ifdef MAKELIB

struct interface 
{
  int foo;
};

#else 

#ifdef MAKEBIN

struct interface 
{
  int foo;
};

#else

/* Part of olsrd */

#include "interfaces.h"

#endif

#endif

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
  struct if_config_options *next;
};



struct olsr_if
{
  char                     *name;
  char                     *config;
  int                      index;
  int                      configured;
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

/*
 * The config struct
 */

struct olsrd_config
{
  int                      debug_level;
  int                      ip_version;
  olsr_u8_t                allow_no_interfaces;
  olsr_u16_t               tos;
  olsr_u8_t                willingness_auto;
  olsr_u8_t                willingness;
  olsr_u8_t                open_ipc;
  olsr_u8_t                use_hysteresis;
  struct hyst_param        hysteresis_param;
  float                    pollrate;
  olsr_u8_t                tc_redundancy;
  olsr_u8_t                mpr_coverage;
  struct plugin_entry      *plugins;
  struct hna4_entry        *hna4_entries;
  struct hna6_entry        *hna6_entries;
  struct olsr_if           *interfaces;
  olsr_u16_t               ifcnt;
};


/*
 * Interface to parser
 */

struct olsrd_config *
olsrd_parse_cnf(char *);

void
olsrd_free_cnf(struct olsrd_config *);

void
olsrd_print_cnf(struct olsrd_config *);

int
olsrd_write_cnf(struct olsrd_config *, char *);

struct if_config_options *
get_default_if_config(void);

struct olsrd_config *
olsrd_get_default_cnf(void);

#endif

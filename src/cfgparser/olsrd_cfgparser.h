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
 * $Id: olsrd_cfgparser.h,v 1.2 2004/10/16 23:17:48 kattemat Exp $
 *
 */


#ifndef _OLSRD_CFGPARSER_H
#define _OLSRD_CFGPARSER_H


#include "olsr_protocol.h"

struct olsr_msg_params
{
  float                    emission_interval;
  float                    validity_time;
};

struct if_config_options
{
  char                     *name;
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
  struct if_config_options *if_options;
  struct olsr_if           *next;
};

struct hna4_entry
{
  olsr_u32_t               net;
  olsr_u32_t               netmask;
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

struct plugin_entry
{
  char                     *name;
  struct plugin_entry      *next;
};

/*
 * The config struct
 */

struct olsrd_config
{
  olsr_u8_t                debug_level;
  olsr_u8_t                ip_version;
  olsr_u8_t                allow_no_interfaces;
  olsr_u16_t               tos;
  olsr_u8_t                auto_willingness;
  olsr_u8_t                fixed_willingness;
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
  struct if_config_options *if_options;
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

#endif

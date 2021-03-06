
/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004-2009, the olsr.org team - see HISTORY file
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
 */

#ifndef _OLSRD_CFG_H
#define _OLSRD_CFG_H

/* Default values not declared in olsr_protocol.h */
#define DEF_POLLRATE           50
#define DEF_NICCHGPOLLRT       2500
#define DEF_WILL_AUTO          false
#define DEF_WILL               3
#define DEF_ALLOW_NO_INTS      true
#define DEF_TOS                16
#define DEF_DEBUGLVL           "1"
#define DEF_IPC_CONNECTIONS    0
#define DEF_FIB_METRIC         FIBM_FLAT
#define DEF_LQ_ALWAYS_SEND_TC  true
#define DEF_LQ_FISH            1
#define DEF_LQ_NAT_THRESH      1000
#define DEF_CLEAR_SCREEN       true
#define DEF_HTTPPORT           8080
#define DEF_HTTPLIMIT          3
#define DEF_TXTPORT            2006
#define DEF_TXTLIMIT           3

/* Bounds */

#define MIN_INTERVAL        0.01

#define MAX_POLLRATE        10000
#define MIN_POLLRATE        10
#define MAX_NICCHGPOLLRT    100000
#define MIN_NICCHGPOLLRT    1000
#define MAX_DEBUGLVL        3
#define MIN_DEBUGLVL        -2
#define MAX_TOS             16
#define MAX_WILLINGNESS     7
#define MIN_WILLINGNESS     0
#define MAX_MPR_COVERAGE    20
#define MIN_MPR_COVERAGE    1
#define MAX_TC_REDUNDANCY   2
#define MAX_HYST_PARAM      1.0
#define MIN_HYST_PARAM      0.0
#define MAX_LQ_AGING        1.0
#define MIN_LQ_AGING        0.01

/* Option values */
#define CFG_FIBM_FLAT          "flat"
#define CFG_FIBM_CORRECT       "correct"
#define CFG_FIBM_APPROX        "approx"

#define CFG_IP6T_AUTO          "auto"
#define CFG_IP6T_SITELOCAL     "site-local"
#define CFG_IP6T_UNIQUELOCAL   "unique-local"
#define CFG_IP6T_GLOBAL        "global"

#define OLSR_IP6T_AUTO         0
#define OLSR_IP6T_SITELOCAL    1
#define OLSR_IP6T_UNIQUELOCAL  2
#define OLSR_IP6T_GLOBAL       3

#ifndef IPV6_ADDR_GLOBAL
#define IPV6_ADDR_GLOBAL       0x0000U
#endif

#ifndef IPV6_ADDR_SITELOCAL
#define IPV6_ADDR_SITELOCAL    0x0040U
#endif

/* prototype declaration to break loop with interface.h */
struct olsr_if_config;

#include "interfaces.h"
#include "olsr_ip_acl.h"
#include "olsr_logging.h"

enum smart_gw_uplinktype {
  GW_UPLINK_NONE,
  GW_UPLINK_IPV4,
  GW_UPLINK_IPV6,
  GW_UPLINK_IPV46,
  GW_UPLINK_CNT,
};

struct olsr_msg_params {
  uint32_t emission_interval;
  uint32_t validity_time;
};

struct olsr_lq_mult {
  union olsr_ip_addr addr;
  uint32_t value;
  struct olsr_lq_mult *next;
};

struct olsr_if_weight {
  int value;
  bool fixed;
};

struct olsr_if_options {
  union olsr_ip_addr ipv4_broadcast;
  int ipv6_addrtype;
  union olsr_ip_addr ipv6_multi_site;
  union olsr_ip_addr ipv6_multi_glbl;
  struct olsr_if_weight weight;
  struct olsr_msg_params hello_params;
  struct olsr_lq_mult *lq_mult;
  bool autodetect_chg;
  enum interface_mode mode;
};

struct olsr_if_config {
  char *name;
  char *config;
  struct interface *interf;
  struct olsr_if_options *cnf;
  struct olsr_if_config *next;
};

struct plugin_param {
  char *key;
  char *value;
  struct plugin_param *next;
};

struct plugin_entry {
  char *name;
  struct plugin_param *params;
  struct plugin_entry *next;
};

typedef enum {
  FIBM_FLAT,
  FIBM_CORRECT,
  FIBM_APPROX
} olsr_fib_metric_options;

/*
 * The config struct
 */

struct olsr_config {
  int ip_version;                      /* AF_INET of AF_INET6 */
  size_t ipsize;                       /* Size of address */

  unsigned char no_fork:1;             /* Should olsrd run in foreground? */
  unsigned char allow_no_interfaces:1; /* Should olsrd stop if no ifaces? */
  unsigned char willingness_auto:1;    /* Willingness in auto mode? */
  unsigned char clear_screen:1;        /* Clear screen during debug output? */
  unsigned char source_ip_mode:1;      /* Run OLSR routing in sourceip mode */

  uint16_t tos;                        /* IP Type of Service Byte */
  uint8_t rt_proto;                     /* Policy routing proto, 0 == operating sys default */
  uint8_t rt_table;                     /* Policy routing table, 254(main) is default */
  uint8_t rt_table_default;             /* Polroute table for default route, 0==use rttable */
  olsr_fib_metric_options fib_metric;  /* Determines route metrics update mode */

  /* logging information */
  bool log_event[LOG_SEVERITY_COUNT][LOG_SOURCE_COUNT]; /* New style */
  bool log_target_stderr;              /* Log output to stderr? */
  char *log_target_file;               /* Filename for log output file, NULL if unused */
  bool log_target_syslog;              /* Log output also to syslog? */

  struct plugin_entry *plugins;        /* List of plugins to load with plparams */
  struct list_entity hna_entries;      /* List of manually configured HNA entries */
  struct olsr_if_config *if_configs;   /* List of devices to be used by olsrd */

  uint32_t pollrate;               /* Main loop poll rate, in milliseconds */
  uint32_t nic_chgs_pollrate;      /* Interface poll rate */
  uint32_t lq_nat_thresh;              /* Link quality NAT threshold, 1000 == unused */
  uint8_t tc_redundancy;               /* TC anncoument mode, 0=only MPR, 1=MPR+MPRS, 2=All sym neighs */
  uint8_t mpr_coverage;                /* How many additional MPRs should be selected */
  uint8_t lq_fish;                     /* 0==Fisheye off, 1=Fisheye on */
  uint8_t willingness;                 /* Manual Configured Willingness value */

  uint16_t olsr_port;                  /* port number used for OLSR packages */
  char *dlPath;                        /* absolute path for dynamic libraries */

  uint16_t comport_http;               /* communication port for http connections */
  uint16_t comport_http_limit;         /* maximum number of connections (including interactive ones) */
  uint16_t comport_txt;                /* communication port for txt connections */
  uint16_t comport_txt_limit;          /* maximum number of interactive connections */

  struct olsr_msg_params tc_params;
  struct olsr_msg_params mid_params;
  struct olsr_msg_params hna_params;

  /*
   * Someone has added global variables to the config struct.
   * Because this saves binary link info we keep it that way.
   * ========= Please add globals below this line. =========
   */

  union olsr_ip_addr router_id;        /* Main address of this node */
  uint32_t will_int;                      /* Willingness update interval if willingness_auto */
  int exit_value;                      /* Global return value for process termination */

  int ioctl_s;                         /* Socket used for ioctl calls */

  union olsr_ip_addr main_addr, unicast_src_ip;

#if defined linux
  uint8_t rt_table_tunnel;
  int32_t rt_table_pri, rt_table_tunnel_pri;
  int32_t rt_table_defaultolsr_pri, rt_table_default_pri;

  bool use_niit;
  bool use_src_ip_routes;

  bool smart_gw_active, smart_gw_allow_nat, smart_gw_uplink_nat;
  enum smart_gw_uplinktype smart_gw_type;
  uint32_t smart_gw_uplink, smart_gw_downlink;
  struct olsr_ip_prefix smart_gw_prefix;

  int rtnl_s;                       /* Socket used for rtnetlink messages */
  int rt_monitor_socket;

  int niit4to6_if_index, niit6to4_if_index;

/*many potential parameters or helper variables for smartgateway*/
  bool has_ipv4_gateway, has_ipv6_gateway;
#endif
#if defined __FreeBSD__ || defined __MacOSX__ || defined __NetBSD__ || defined __OpenBSD__
  int rts_bsd;                         /* Socket used for route changes on BSDs */
#endif
};

/*
 * Global olsrd configuragtion
 */
extern struct olsr_config *EXPORT(olsr_cnf);

/*
 * Interface to parser
 */

void olsr_parse_cfg(int argc, char *argv[], const char *file, struct olsr_config **rcfg);
struct olsr_if_options *olsr_get_default_if_options(void);
struct olsr_config *olsr_get_default_cfg(void);
int olsr_sanity_check_cfg(struct olsr_config *cfg);
void olsr_free_cfg(struct olsr_config *cfg);

#endif /* _OLSRD_CFG_H */

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

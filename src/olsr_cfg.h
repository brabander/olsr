
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
#define DEF_POLLRATE           0.05
#define DEF_NICCHGPOLLRT       2.5
#define DEF_WILL_AUTO          true
#define DEF_ALLOW_NO_INTS      true
#define DEF_TOS                16
#define DEF_DEBUGLVL           "0"
#define DEF_IPC_CONNECTIONS    0
#define DEF_USE_HYST           false
#define DEF_FIB_METRIC         FIBM_FLAT
#define DEF_LQ_ALWAYS_SEND_TC  true
#define DEF_LQ_FISH            0
#define DEF_LQ_DIJK_LIMIT      255
#define DEF_LQ_DIJK_INTER      0.0
#define DEF_LQ_NAT_THRESH      1.0
#define DEF_LQ_AGING           0.1
#define DEF_CLEAR_SCREEN       false

/* Bounds */

#define MIN_INTERVAL        0.01

#define MAX_POLLRATE        10.0
#define MIN_POLLRATE        0.01
#define MAX_NICCHGPOLLRT    100.0
#define MIN_NICCHGPOLLRT    1.0
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

#include "interfaces.h"
#include "olsr_ip_acl.h"
#include "olsr_cfg_data.h"

struct olsr_msg_params {
  float emission_interval;
  float validity_time;
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
  struct olsr_msg_params tc_params;
  struct olsr_msg_params mid_params;
  struct olsr_msg_params hna_params;
  struct olsr_lq_mult *lq_mult;
  bool autodetect_chg;
  enum interface_mode mode;
};

struct olsr_if_config {
  char *name;
  char *config;
  union olsr_ip_addr hemu_ip;
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
  unsigned char del_gws:1;             /* Delete InternetGWs at startup? */
  unsigned char fixed_origaddr:1;      /* Use a fixed originator addr == Node ID? */
  unsigned char source_ip_mode:1;      /* Run OLSR routing in sourceip mode */

  uint16_t tos;                        /* IP Type of Service Byte */
  uint8_t rtproto;                     /* Policy routing proto, 0 == operating sys default */
  uint8_t rttable;                     /* Policy routing table, 254(main) is default */
  uint8_t rttable_default;             /* Polroute table for default route, 0==use rttable */
  olsr_fib_metric_options fib_metric;  /* Determines route metrics update mode */
  uint8_t ipc_connections;             /* Number of allowed IPC connections */

  /* logging information */
  bool log_event[LOG_SEVERITY_COUNT][LOG_SOURCE_COUNT]; /* New style */
  bool log_target_stderr;              /* Log output to stderr? */
  char *log_target_file;               /* Filename for log output file, NULL if unused */
  bool log_target_syslog;              /* Log output also to syslog? */

  struct plugin_entry *plugins;        /* List of plugins to load with plparams */
  struct list_node hna_entries;        /* List of manually configured HNA entries */
  struct ip_acl ipc_nets;              /* List of allowed IPC peer IPs */
  struct olsr_if_config *if_configs;   /* List of devices to be used by olsrd */

  uint32_t pollrate;                   /* Main loop poll rate, in microseconds */
  float nic_chgs_pollrate;             /* Interface poll rate */
  float lq_nat_thresh;                 /* Link quality NAT threshold, 1.0 == unused */
  uint8_t tc_redundancy;               /* TC anncoument mode, 0=only MPR, 1=MPR+MPRS, 2=All sym neighs */
  uint8_t mpr_coverage;                /* How many additional MPRs should be selected */
  uint8_t lq_fish;                     /* 0==Fisheye off, 1=Fisheye on */
  float lq_dinter;                     /* Dijkstra Calculation interval */
  uint8_t lq_dlimit;                   /* Dijkstra Calculation limit */
  uint8_t willingness;                 /* Manual Configured Willingness value */

  uint16_t olsr_port;                  /* port number used for OLSR packages */
  char *dlPath;                        /* absolute path for dynamic libraries */

  /*
   * Someone has added global variables to the config struct.
   * Because this saves binary link info we keep it that way.
   * ========= Please add globals below this line. =========
   */

  union olsr_ip_addr router_id;        /* Main address of this node */
  float will_int;                      /* Willingness update interval if willingness_auto */
  int exit_value;                      /* Global return value for process termination */

  int ioctl_s;                         /* Socket used for ioctl calls */
#if defined linux
  int rts_linux;                       /* Socket used for rtnetlink messages */
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

typedef enum {
  CFG_ERROR,                           /* Severe parsing error, e.g. file not found, mixed up args */
  CFG_WARN,                            /* Non-severe error, e.g. use of deprecated option */
  CFG_EXIT,                            /* Given options will exit() e.g. "--version" or "--help" */
  CFG_OK                               /* Config is parsed and does not have any errors */
} olsr_parse_cfg_result;

olsr_parse_cfg_result olsr_parse_cfg(int argc, char *argv[], const char *file, char *rmsg, struct olsr_config **rcfg);
struct olsr_if_options *olsr_get_default_if_options(void);
struct olsr_config *olsr_get_default_cfg(void);
int olsr_sanity_check_cfg(struct olsr_config *cfg);
void olsr_free_cfg(struct olsr_config *cfg);

/*
 * Check pollrate function
 */
static inline float
conv_pollrate_to_secs(uint32_t p)
{
  return p / (float)1000000.0;
}
static inline uint32_t
conv_pollrate_to_microsecs(float p)
{
  return (uint32_t) (p * 1000000);
}

#endif /* _OLSRD_CFG_H */

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

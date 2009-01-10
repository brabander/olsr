
/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004, Andreas Tonnesen(andreto@olsr.org)
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
#define DEF_DEBUGLVL           1
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
#define MAX_DEBUGLVL        9
#define MIN_DEBUGLVL        0
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
#include "olsr_logging.h"

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
  int ip_version;
  size_t ipsize;                       /* Size of address */
  uint8_t maxplen;                     /* maximum prefix len */
  unsigned char no_fork:1;
  unsigned char allow_no_interfaces:1;
  unsigned char willingness_auto:1;
  unsigned char clear_screen:1;
  unsigned char del_gws:1;             /* Delete InternetGWs at startup */
  uint16_t tos;
  uint8_t rtproto;
  uint8_t rttable;
  uint8_t rttable_default;
  uint8_t ipc_connections;
  olsr_fib_metric_options fib_metric;

  /* logging information */
  int8_t debug_level;                  /* old style */
  bool log_event[LOG_SEVERITY_COUNT][LOG_SOURCE_COUNT]; /* new style */
  bool log_target_stderr;
  char *log_target_file;
  bool log_target_syslog;

  struct plugin_entry *plugins;
  struct list_node hna_entries;
  struct ip_acl ipc_nets;
  struct olsr_if_config *if_configs;
  uint32_t pollrate;                   /* in microseconds */
  float nic_chgs_pollrate;
  uint8_t tc_redundancy;
  uint8_t mpr_coverage;
  uint8_t lq_fish;
  float lq_dinter;
  uint8_t lq_dlimit;
  uint8_t willingness;

  /* Stuff set by olsrd */
  uint16_t system_tick_divider;        /* Tick resolution */
  union olsr_ip_addr main_addr;        /* Main address of this node */
  float will_int;
  int exit_value;                      /* Global return value for process termination */
  float max_tc_vtime;

  int ioctl_s;                         /* Socket used for ioctl calls */
#if defined linux
  int rts_linux;                       /* Socket used for rtnetlink messages */
#endif

#if defined __FreeBSD__ || defined __MacOSX__ || defined __NetBSD__ || defined __OpenBSD__
  int rts_bsd;                         /* Socket used for route changes on BSDs */
#endif
  float lq_nat_thresh;
};

/*
 * Global olsrd configuragtion
 */
extern struct olsr_config *EXPORT(olsr_cnf);

/*
 * Interface to parser
 */
struct olsr_config *olsr_parse_cfg(int, char **, const char *);
int olsr_sanity_check_cfg(struct olsr_config *);
void olsr_free_cfg(struct olsr_config *);
struct olsr_config *olsr_get_default_cfg(void);

/*
 * Check pollrate function
 */
static inline float
conv_pollrate_to_secs(uint32_t p)
{
  return p / 1000000.0;
}
static inline uint32_t
conv_pollrate_to_microsecs(float p)
{
  return p * 1000000;
}

#endif /* _OLSRD_CFG_H */

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

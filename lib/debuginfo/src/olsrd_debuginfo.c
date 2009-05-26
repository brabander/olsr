
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

/*
 * Dynamic linked library for the olsr.org olsr daemon
 */
#include "olsr.h"
#include "ipcalc.h"
#include "neighbor_table.h"
#include "two_hop_neighbor_table.h"
#include "mpr_selector_set.h"
#include "tc_set.h"
#include "hna_set.h"
#include "mid_set.h"
#include "routing_table.h"
#include "log.h"
#include "misc.h"
#include "olsr_ip_prefix_list.h"
#include "parser.h"
#include "olsr_comport_txt.h"
#include "olsrd_debuginfo.h"
#include "olsr_types.h"
#include "defs.h"

#include "common/autobuf.h"

#define PLUGIN_NAME    "OLSRD debuginfo plugin"
#define PLUGIN_VERSION "0.1"
#define PLUGIN_AUTHOR   "Henning Rogge"
#define MOD_DESC PLUGIN_NAME " " PLUGIN_VERSION " by " PLUGIN_AUTHOR
#define PLUGIN_INTERFACE_VERSION 5

struct debuginfo_cmd {
  const char *name;
  olsr_txthandler handler;
  struct olsr_txtcommand *normal, *csv;
};

static void debuginfo_new(void) __attribute__ ((constructor));
static void debuginfo_delete(void) __attribute__ ((destructor));

static enum olsr_txtcommand_result debuginfo_msgstat(struct comport_connection *con,  char *cmd, char *param);
static enum olsr_txtcommand_result debuginfo_cookies(struct comport_connection *con,  char *cmd, char *param);

static void update_statistics_ptr(void *);
static bool olsr_msg_statistics(union olsr_message *msg, struct interface *input_if, union olsr_ip_addr *from_addr);
static char *olsr_packet_statistics(char *packet, struct interface *interface, union olsr_ip_addr *, int *length);
static void update_statistics_ptr(void *data __attribute__ ((unused)));

/* plugin configuration */
static struct ip_acl allowed_nets;
static uint32_t traffic_interval, traffic_slots, current_slot;

/* plugin parameters */
static const struct olsrd_plugin_parameters plugin_parameters[] = {
  {.name = IP_ACL_ACCEPT_PARAP,.set_plugin_parameter = &ip_acl_add_plugin_accept,.data = &allowed_nets},
  {.name = IP_ACL_REJECT_PARAM,.set_plugin_parameter = &ip_acl_add_plugin_reject,.data = &allowed_nets},
  {.name = IP_ACL_CHECKFIRST_PARAM,.set_plugin_parameter = &ip_acl_add_plugin_checkFirst,.data = &allowed_nets},
  {.name = IP_ACL_DEFAULTPOLICY_PARAM,.set_plugin_parameter = &ip_acl_add_plugin_defaultPolicy,.data = &allowed_nets},
  {.name = "stat_interval", .set_plugin_parameter = &set_plugin_int, .data = &traffic_interval},
  {.name = "stat_slots", .set_plugin_parameter = &set_plugin_int, .data = &traffic_slots},
};

/* command callbacks and names */
static struct debuginfo_cmd commands[] = {
    {"msgstat", &debuginfo_msgstat, NULL, NULL},
    {"cookies", &debuginfo_cookies, NULL, NULL}
};

/* variables for statistics */
static struct avl_tree statistics_tree;
static struct debug_traffic_count total_traffic;
static struct olsr_cookie_info *statistics_timer = NULL;
static struct olsr_cookie_info *statistics_mem = NULL;

static union olsr_ip_addr total_ip_addr;

int
olsrd_plugin_interface_version(void)
{
  return PLUGIN_INTERFACE_VERSION;
}

void
olsrd_get_plugin_parameters(const struct olsrd_plugin_parameters **params, int *size)
{
  *params = plugin_parameters;
  *size = ARRAYSIZE(plugin_parameters);
}


/**
 *Constructor
 */
static void
debuginfo_new(void)
{
  /* Print plugin info to stdout */
  OLSR_INFO(LOG_PLUGINS, "%s\n", MOD_DESC);

  ip_acl_init(&allowed_nets);

  traffic_interval = 5; /* seconds */
  traffic_slots = 12;      /* number of 5000 second slots */
  current_slot = 0;

  memset(&total_ip_addr, 255, sizeof(total_ip_addr));

  /* always allow localhost */
  if (olsr_cnf->ip_version == AF_INET) {
    union olsr_ip_addr ip;

    ip.v4.s_addr = ntohl(INADDR_LOOPBACK);
    ip_acl_add(&allowed_nets, &ip, 32, false);
  } else {
    ip_acl_add(&allowed_nets, (const union olsr_ip_addr *)&in6addr_loopback, 128, false);
    ip_acl_add(&allowed_nets, (const union olsr_ip_addr *)&in6addr_v4mapped_loopback, 128, false);
  }
}

/**
 *Destructor
 */
static void
debuginfo_delete(void)
{
  size_t i;

  for (i=0; i<ARRAYSIZE(commands); i++) {
    olsr_com_remove_normal_txtcommand(commands[i].normal);
    olsr_com_remove_csv_txtcommand(commands[i].csv);
  }
  olsr_parser_remove_function(&olsr_msg_statistics, PROMISCUOUS);
  olsr_preprocessor_remove_function(&olsr_packet_statistics);
  ip_acl_flush(&allowed_nets);
}

int
olsrd_plugin_init(void)
{
  size_t i;

  for (i=0; i<ARRAYSIZE(commands); i++) {
    commands[i].normal = olsr_com_add_normal_txtcommand(commands[i].name, commands[i].handler);
    commands[i].csv = olsr_com_add_csv_txtcommand(commands[i].name, commands[i].handler);
    commands[i].normal->acl = &allowed_nets;
    commands[i].csv->acl = &allowed_nets;
  }

  i = sizeof(struct debug_traffic) + sizeof(struct debug_traffic_count) * traffic_slots;

  statistics_timer = olsr_alloc_cookie("debuginfo statistics timer", OLSR_COOKIE_TYPE_TIMER);
  olsr_start_timer(traffic_interval * 1000, 0, true, &update_statistics_ptr, NULL, statistics_timer->ci_id);

  statistics_mem = olsr_alloc_cookie("debuginfo statistics memory", OLSR_COOKIE_TYPE_MEMORY);
  olsr_cookie_set_memory_size(statistics_mem, i);

  memset(&total_traffic, 0, sizeof(total_traffic));
  avl_init(&statistics_tree, avl_comp_default);

  olsr_parser_add_function(&olsr_msg_statistics, PROMISCUOUS);
  olsr_preprocessor_add_function(&olsr_packet_statistics);
  return 1;
}

static struct debug_traffic *get_debugtraffic_entry(union olsr_ip_addr *ip) {
  struct debug_traffic *tr;
  tr = (struct debug_traffic *) avl_find(&statistics_tree, ip);
  if (tr == NULL) {
    tr = olsr_cookie_malloc(statistics_mem);

    memcpy(&tr->ip, ip, sizeof(union olsr_ip_addr));
    tr->node.key = &tr->ip;

    avl_insert(&statistics_tree, &tr->node, AVL_DUP_NO);
  }
  return tr;
}

static void
update_statistics_ptr(void *data __attribute__ ((unused)))
{
  struct debug_traffic *traffic;
  uint32_t last_slot, i;

  last_slot = current_slot;
  current_slot++;
  if (current_slot == traffic_slots) {
    current_slot = 0;
  }

  /* move data from "current" template to slot array */
  OLSR_FOR_ALL_DEBUGTRAFFIC_ENTRIES(traffic) {
    /* subtract old values from node count and total count */
    for (i=0; i<DTR_COUNT; i++) {
      traffic->total.data[i] -= traffic->traffic[current_slot].data[i];
    }

    /* copy new traffic into slot */
    traffic->traffic[current_slot] = traffic->current;

    /* add new values to node count and total count */
    for (i=0; i<DTR_COUNT; i++) {
      traffic->total.data[i] += traffic->current.data[i];
    }

    /* erase new traffic */
    memset(&traffic->current, 0, sizeof(traffic->current));

    if (traffic->total.data[DTR_MESSAGES] == 0) {
      /* no traffic left, cleanup ! */

      avl_delete(&statistics_tree, &traffic->node);
      olsr_cookie_free(statistics_mem, traffic);
    }
  } OLSR_FOR_ALL_DEBUGTRAFFIC_ENTRIES_END(traffic)
}

/* update message statistics */
static bool
olsr_msg_statistics(union olsr_message *msg,
                    struct interface *input_if __attribute__ ((unused)), union olsr_ip_addr *from_addr __attribute__ ((unused)))
{
  int msgtype, msgsize;
  union olsr_ip_addr origaddr;
  enum debug_traffic_type type;
  struct debug_traffic *tr;
#if !defined REMOVE_DEBUG
  struct ipaddr_str buf;
#endif

  memset(&origaddr, 0, sizeof(origaddr));
  if (olsr_cnf->ip_version == AF_INET) {
    msgtype = msg->v4.olsr_msgtype;
    msgsize = ntohs(msg->v4.olsr_msgsize);
    origaddr.v4.s_addr = msg->v4.originator;
  } else {
    msgtype = msg->v6.olsr_msgtype;
    msgsize = ntohs(msg->v6.olsr_msgsize);
    origaddr.v6 = msg->v6.originator;
  }

  switch (msgtype) {
  case HELLO_MESSAGE:
    type = DTR_HELLO;
    break;
  case TC_MESSAGE:
    type = DTR_TC;
    break;
  case MID_MESSAGE:
    type = DTR_MID;
    break;
  case HNA_MESSAGE:
    type = DTR_HNA;
    break;
  case LQ_HELLO_MESSAGE:
    type = DTR_HELLO;
    break;
  case LQ_TC_MESSAGE:
    type = DTR_TC;
    break;
  default:
    type = DTR_OTHER;
    break;
  }

  /* input data for specific node */
  tr = get_debugtraffic_entry(&origaddr);
  tr->current.data[type]++;
  tr->current.data[DTR_MESSAGES]++;
  tr->current.data[DTR_MSG_TRAFFIC] += msgsize;

  OLSR_DEBUG(LOG_PLUGINS, "Added message type %d to statistics of %s: %d\n",
      type, olsr_ip_to_string(&buf, &tr->ip), tr->current.data[type]);

  /* input data for total traffic handling */
  tr = get_debugtraffic_entry(&total_ip_addr);
  tr->current.data[type]++;
  tr->current.data[DTR_MESSAGES]++;
  tr->current.data[DTR_MSG_TRAFFIC] += msgsize;
  return true;
}

/* update traffic statistics */
static char *
olsr_packet_statistics(char *packet __attribute__ ((unused)),
                       struct interface *interface __attribute__ ((unused)),
                       union olsr_ip_addr *ip, int *length)
{
  struct debug_traffic *tr;
  tr = (struct debug_traffic *) avl_find(&statistics_tree, ip);
  if (tr == NULL) {
    tr = olsr_cookie_malloc(statistics_mem);

    memcpy(&tr->ip, ip, sizeof(union olsr_ip_addr));
    tr->node.key = &tr->ip;

    avl_insert(&statistics_tree, &tr->node, AVL_DUP_NO);
  }

  tr->current.data[DTR_PACK_TRAFFIC] += *length;
  tr->current.data[DTR_PACKETS] ++;

  return packet;
}

static const char *debuginfo_print_trafficip(struct ipaddr_str *buf, union olsr_ip_addr *ip) {
  static const char *total = "Total";
  if (olsr_ipcmp(ip, &total_ip_addr) == 0) {
    return total;
  }
  return olsr_ip_to_string(buf, ip);
}


static bool debuginfo_print_nodestat(struct autobuf *buf, union olsr_ip_addr *ip, struct debug_traffic_count *cnt, const char *template) {
  struct ipaddr_str strbuf;

  return abuf_appendf(buf, template,
      olsr_cnf->ip_version == AF_INET ? INET_ADDRSTRLEN : INET6_ADDRSTRLEN, debuginfo_print_trafficip(&strbuf, ip),
      cnt->data[DTR_HELLO],
      cnt->data[DTR_TC],
      cnt->data[DTR_MID],
      cnt->data[DTR_HNA],
      cnt->data[DTR_OTHER],
      cnt->data[DTR_MESSAGES],
      cnt->data[DTR_MSG_TRAFFIC]) < 0;
}

static enum olsr_txtcommand_result
debuginfo_msgstat(struct comport_connection *con,  char *cmd __attribute__ ((unused)), char *param __attribute__ ((unused)))
{
  struct debug_traffic *tr;

  if (!con->is_csv) {
    if (abuf_appendf(&con->out, "Slot size: %d seconds\tSlot count: %d\n", traffic_interval, traffic_slots) < 0) {
      return ABUF_ERROR;
    }
    if (abuf_appendf(&con->out,
        "Table: Statistics (without duplicates)\n%-*s\tHello\tTC\tMID\tHNA\tOther\tTotal\tBytes\n",
        olsr_cnf->ip_version == AF_INET ? INET_ADDRSTRLEN : INET6_ADDRSTRLEN, "IP"
        ) < 0) {
      return ABUF_ERROR;
    }
  }

  if (param == NULL || strcasecmp(param, "node") == 0) {
    OLSR_FOR_ALL_DEBUGTRAFFIC_ENTRIES(tr) {
      if (debuginfo_print_nodestat(&con->out, &tr->ip, &tr->traffic[current_slot],
          "%-*s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t\n")) {
        return ABUF_ERROR;
      }
    } OLSR_FOR_ALL_DEBUGTRAFFIC_ENTRIES_END(tr)
  }
  else {
    uint32_t mult = 1, divisor = 1;
    struct debug_traffic_count cnt;
    int i;

    if (strcasecmp(param, "total") == 0) {
      divisor = 1;
    }
    else if (strcasecmp(param, "average") == 0) {
      divisor = traffic_slots;
    }
    else if (strcasecmp(param, "avgsec") == 0) {
      divisor = traffic_slots * traffic_interval;
      mult = 1;
    }
    else if (strcasecmp(param, "avgmin") == 0) {
      divisor = traffic_slots * traffic_interval;
      mult = 60;
    }
    else if (strcasecmp(param, "avghour") == 0) {
      divisor = traffic_slots * traffic_interval;
      mult = 3600;
    }
    else {
      abuf_appendf(&con->out, "Error, unknown parameter %s for msgstat\n", param);
      return CONTINUE;
    }

    OLSR_FOR_ALL_DEBUGTRAFFIC_ENTRIES(tr) {
      for (i=0; i<DTR_COUNT; i++) {
        cnt.data[i] = (tr->total.data[i] * mult) / divisor;
      }
      if (debuginfo_print_nodestat(&con->out, &tr->ip, &cnt,
          "%-*s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n")) {
        return ABUF_ERROR;
      }
    } OLSR_FOR_ALL_DEBUGTRAFFIC_ENTRIES_END(tr)
  }

  return CONTINUE;
}

static INLINE bool debuginfo_print_cookies_mem(struct autobuf *buf, const char *format) {
  int i;
  for (i = 1; i < COOKIE_ID_MAX; i++) {
    struct olsr_cookie_info *c = olsr_cookie_get(i);
    if (c == NULL || c->ci_type != OLSR_COOKIE_TYPE_MEMORY) {
      continue;
    }
    if (abuf_appendf(buf, format,
        c->ci_name == NULL ? "Unknown" : c->ci_name,
        (unsigned long)c->ci_size, c->ci_usage, c->ci_free_list_usage) < 0) {
      return true;
    }
  }
  return false;
}
static INLINE bool debuginfo_print_cookies_timer(struct autobuf *buf, const char *format) {
  int i;
  for (i = 1; i < COOKIE_ID_MAX; i++) {
    struct olsr_cookie_info *c = olsr_cookie_get(i);
    if (c == NULL || c->ci_type != OLSR_COOKIE_TYPE_TIMER) {
      continue;
    }
    if (abuf_appendf(buf, format, c->ci_name == NULL ? "Unknown" : c->ci_name,
                       c->ci_usage, c->ci_changes) < 0) {
      return true;
    }
  }
  return false;
}

static enum olsr_txtcommand_result
debuginfo_cookies(struct comport_connection *con,  char *cmd __attribute__ ((unused)), char *param __attribute__ ((unused)))
{
  if (!con->is_csv && abuf_puts(&con->out, "Memory cookies:\n") < 0) {
    return ABUF_ERROR;
  }

  if (debuginfo_print_cookies_mem(&con->out, !con->is_csv ?
      "%-25s (MEMORY) size: %lu usage: %u freelist: %u\n" : "mem_cookie,%s,%lu,%u,%u\n")) {
    return ABUF_ERROR;
  }

  if (!con->is_csv && abuf_puts(&con->out, "\nTimer cookies:\n") < 0) {
    return ABUF_ERROR;
  }

  if (debuginfo_print_cookies_timer(&con->out, !con->is_csv ?
      "%-25s (TIMER) usage: %u changes: %u\n" : "tmr_cookie,%s,%u,%u\n")) {
    return ABUF_ERROR;
  }
  return CONTINUE;
}

/*
 * Local Variables:
 * mode: c
 * style: linux
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */

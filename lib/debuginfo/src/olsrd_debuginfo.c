
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

#include <stdlib.h>

#include "olsr.h"
#include "ipcalc.h"
#include "neighbor_table.h"
#include "tc_set.h"
#include "hna_set.h"
#include "mid_set.h"
#include "routing_table.h"
#include "olsr_logging.h"
#include "olsr_ip_prefix_list.h"
#include "parser.h"
#include "olsr_comport_txt.h"
#include "olsrd_debuginfo.h"
#include "olsr_types.h"
#include "defs.h"

#include "common/autobuf.h"

#define PLUGIN_DESCR    "OLSRD debuginfo plugin"
#define PLUGIN_AUTHOR   "Henning Rogge"

struct debuginfo_cmd {
  const char *name;
  const char *help;
  olsr_txthandler handler;
  struct olsr_txtcommand *cmd, *cmdhelp;
};

static int debuginfo_init(void);
static int debuginfo_enable(void);
static int debuginfo_disable(void);

static enum olsr_txtcommand_result debuginfo_msgstat(struct comport_connection *con,
    const char *cmd, const char *param);
static enum olsr_txtcommand_result debuginfo_pktstat(struct comport_connection *con,
    const char *cmd, const char *param);
static enum olsr_txtcommand_result debuginfo_cookies(struct comport_connection *con,
    const char *cmd, const char *param);
static enum olsr_txtcommand_result debuginfo_log(struct comport_connection *con,
    const char *cmd, const char *param);
static enum olsr_txtcommand_result olsr_debuginfo_displayhelp(struct comport_connection *con,
    const char *cmd, const char *param);

static void update_statistics_ptr(void *);
static void olsr_msg_statistics(struct olsr_message *,
    struct interface *, union olsr_ip_addr *, enum duplicate_status);
static uint8_t *olsr_packet_statistics(uint8_t *binary,
    struct interface *interface, union olsr_ip_addr *ip, int *length);

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

OLSR_PLUGIN6(plugin_parameters) {
  .descr = PLUGIN_DESCR,
  .author = PLUGIN_AUTHOR,
  .init = debuginfo_init,
  .enable = debuginfo_enable,
  .disable = debuginfo_disable,
  .deactivate = false
};

/* command callbacks and names */
static struct debuginfo_cmd commands[] = {
    {"msgstat", "Displays statistics about the incoming OLSR messages\n", &debuginfo_msgstat, NULL, NULL},
    {"pktstat", "Displays statistics about the incoming OLSR packets\n", &debuginfo_pktstat, NULL, NULL},
    {"cookies", "Displays statistics about memory and timer cookies\n", &debuginfo_cookies, NULL, NULL},
    {"log",     "\"log\":      continuous output of logging to this console\n"
                "\"log show\": show configured logging option for debuginfo output\n"
                "\"log add <severity> <source1> <source2> ...\": Add one or more sources of a defined severity for logging\n"
                "\"log remove <severity> <source1> <source2> ...\": Remove one or more sources of a defined severity for logging\n",
        &debuginfo_log, NULL, NULL}
};

/* variables for statistics */
static struct avl_tree stat_msg_tree, stat_pkt_tree;

static struct debug_msgtraffic_count total_msg_traffic;
static struct debug_pkttraffic_count total_pkt_traffic;

static struct olsr_memcookie_info *statistics_msg_mem = NULL;
static struct olsr_memcookie_info *statistics_pkt_mem = NULL;

static struct olsr_timer_info *statistics_timer = NULL;

static union olsr_ip_addr total_ip_addr;

/* variables for log access */
static bool log_debuginfo_mask[LOG_SEVERITY_COUNT][LOG_SOURCE_COUNT];
static int log_source_maxlen, log_severity_maxlen;
static struct comport_connection *log_connection;
static struct log_handler_entry *log_handler;
/**
 *Constructor
 */
static int
debuginfo_init(void)
{
  int i;
  ip_acl_init(&allowed_nets);

  traffic_interval = 5; /* seconds */
  traffic_slots = 12;      /* number of 5000 second slots */
  current_slot = 0;

  memset(&total_ip_addr, 255, sizeof(total_ip_addr));

  /* calculate maximum length of log source names */
  log_source_maxlen = 0;
  for (i=1; i<LOG_SOURCE_COUNT; i++) {
    int len = strlen(LOG_SOURCE_NAMES[i]);

    if (len > log_source_maxlen) {
      log_source_maxlen = len;
    }
  }

  /* calculate maximum length of log severity names */
  log_severity_maxlen = 0;
  for (i=1; i<LOG_SEVERITY_COUNT; i++) {
    int len = strlen(LOG_SEVERITY_NAMES[i]);

    if (len > log_severity_maxlen) {
      log_severity_maxlen = len;
    }
  }

  memcpy(log_debuginfo_mask, log_global_mask, sizeof(log_global_mask));
  log_connection = NULL;
  return 0;
}

/**
 *Destructor
 */
static int
debuginfo_disable(void)
{
  size_t i;

  for (i=0; i<ARRAYSIZE(commands); i++) {
    olsr_com_remove_normal_txtcommand(commands[i].cmd);
    olsr_com_remove_help_txtcommand(commands[i].cmdhelp);
  }
  olsr_parser_remove_function(&olsr_msg_statistics);
  olsr_preprocessor_remove_function(&olsr_packet_statistics);
  ip_acl_flush(&allowed_nets);
  return 0;
}

static int
debuginfo_enable(void)
{
  size_t i;

  /* always allow localhost */
  if (olsr_cnf->ip_version == AF_INET) {
    union olsr_ip_addr ip;

    ip.v4.s_addr = ntohl(INADDR_LOOPBACK);
    ip_acl_add(&allowed_nets, &ip, 32, false);
  } else {
    ip_acl_add(&allowed_nets, (const union olsr_ip_addr *)&in6addr_loopback, 128, false);
    ip_acl_add(&allowed_nets, (const union olsr_ip_addr *)&in6addr_v4mapped_loopback, 128, false);
  }

  for (i=0; i<ARRAYSIZE(commands); i++) {
    commands[i].cmd = olsr_com_add_normal_txtcommand(commands[i].name, commands[i].handler);
    commands[i].cmdhelp = olsr_com_add_help_txtcommand(commands[i].name, olsr_debuginfo_displayhelp);
    commands[i].cmd->acl = &allowed_nets;
  }

  statistics_timer = olsr_timer_add("debuginfo timer", &update_statistics_ptr, true);
  olsr_timer_start(traffic_interval * 1000, 0, NULL, statistics_timer);

  statistics_msg_mem = olsr_memcookie_add("debuginfo msgstat",
      sizeof(struct debug_msgtraffic) + sizeof(struct debug_msgtraffic_count) * traffic_slots);

  statistics_pkt_mem = olsr_memcookie_add("debuginfo pktstat",
      sizeof(struct debug_pkttraffic) + sizeof(struct debug_pkttraffic_count) * traffic_slots);

  memset(&total_msg_traffic, 0, sizeof(total_msg_traffic));
  memset(&total_pkt_traffic, 0, sizeof(total_pkt_traffic));
  avl_init(&stat_msg_tree, avl_comp_default, false, NULL);
  avl_init(&stat_pkt_tree, avl_comp_default, false, NULL);

  olsr_parser_add_function(&olsr_msg_statistics, PROMISCUOUS);
  olsr_preprocessor_add_function(&olsr_packet_statistics);
  return 0;
}

static struct debug_msgtraffic *get_msgtraffic_entry(union olsr_ip_addr *ip) {
  struct debug_msgtraffic *tr;
  tr = (struct debug_msgtraffic *) avl_find(&stat_msg_tree, ip);
  if (tr == NULL) {
    tr = olsr_memcookie_malloc(statistics_msg_mem);

    memcpy(&tr->ip, ip, sizeof(union olsr_ip_addr));
    tr->node.key = &tr->ip;

    avl_insert(&stat_msg_tree, &tr->node);
  }
  return tr;
}

static struct debug_pkttraffic *get_pkttraffic_entry(union olsr_ip_addr *ip, struct interface *in) {
  struct debug_pkttraffic *tr;
  tr = (struct debug_pkttraffic *) avl_find(&stat_pkt_tree, ip);
  if (tr == NULL) {
    tr = olsr_memcookie_malloc(statistics_pkt_mem);

    memcpy(&tr->ip, ip, sizeof(union olsr_ip_addr));
    tr->node.key = &tr->ip;

    tr->int_name = strdup(in ? in->int_name : "---");

    avl_insert(&stat_pkt_tree, &tr->node);
  }
  return tr;
}

static void
update_statistics_ptr(void *data __attribute__ ((unused)))
{
  struct debug_msgtraffic *msg, *msg_iterator;
  struct debug_pkttraffic *pkt, *pkt_iterator;
  uint32_t last_slot, i;

  last_slot = current_slot;
  current_slot++;
  if (current_slot == traffic_slots) {
    current_slot = 0;
  }

  /* move data from "current" template to slot array */
  OLSR_FOR_ALL_MSGTRAFFIC_ENTRIES(msg, msg_iterator) {
    /* subtract old values from node count and total count */
    for (i=0; i<DTR_MSG_COUNT; i++) {
      msg->total.data[i] -= msg->traffic[current_slot].data[i];
    }

    /* copy new traffic into slot */
    msg->traffic[current_slot] = msg->current;

    /* add new values to node count and total count */
    for (i=0; i<DTR_MSG_COUNT; i++) {
      msg->total.data[i] += msg->current.data[i];
    }

    /* erase new traffic */
    memset(&msg->current, 0, sizeof(msg->current));

    if (msg->total.data[DTR_MESSAGES] == 0) {
      /* no traffic left, cleanup ! */

      avl_delete(&stat_msg_tree, &msg->node);
      olsr_memcookie_free(statistics_msg_mem, msg);
    }
  }

  OLSR_FOR_ALL_PKTTRAFFIC_ENTRIES(pkt, pkt_iterator) {
    /* subtract old values from node count and total count */
    for (i=0; i<DTR_PKT_COUNT; i++) {
      pkt->total.data[i] -= pkt->traffic[current_slot].data[i];
    }

    /* copy new traffic into slot */
    pkt->traffic[current_slot] = pkt->current;

    /* add new values to node count and total count */
    for (i=0; i<DTR_PKT_COUNT; i++) {
      pkt->total.data[i] += pkt->current.data[i];
    }

    /* erase new traffic */
    memset(&pkt->current, 0, sizeof(pkt->current));

    if (pkt->total.data[DTR_PACKETS] == 0) {
      /* no traffic left, cleanup ! */

      avl_delete(&stat_pkt_tree, &pkt->node);
      free(pkt->int_name);
      olsr_memcookie_free(statistics_pkt_mem, pkt);
    }
  }
}

/* update message statistics */
static void
olsr_msg_statistics(struct olsr_message *msg,
    struct interface *input_if __attribute__ ((unused)),
    union olsr_ip_addr *from_addr __attribute__ ((unused)), enum duplicate_status status  __attribute__ ((unused)))
{
  enum debug_msgtraffic_type type;
  struct debug_msgtraffic *tr;
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif

  switch (msg->type) {
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
  tr = get_msgtraffic_entry(&msg->originator);
  tr->current.data[type]++;
  tr->current.data[DTR_MESSAGES]++;
  tr->current.data[DTR_MSG_TRAFFIC] += msg->size;

  OLSR_DEBUG(LOG_PLUGINS, "Added message type %d to statistics of %s: %d\n",
      type, olsr_ip_to_string(&buf, &tr->ip), tr->current.data[type]);

  /* input data for total traffic handling */
  tr = get_msgtraffic_entry(&total_ip_addr);
  tr->current.data[type]++;
  tr->current.data[DTR_MESSAGES]++;
  tr->current.data[DTR_MSG_TRAFFIC] += msg->size;
}

/* update traffic statistics */
static uint8_t *
olsr_packet_statistics(uint8_t *binary, struct interface *interface, union olsr_ip_addr *ip, int *length)
{
  struct debug_pkttraffic *tr;
  tr = get_pkttraffic_entry(ip, interface);
  tr->current.data[DTR_PACK_TRAFFIC] += *length;
  tr->current.data[DTR_PACKETS] ++;

  tr = get_pkttraffic_entry(&total_ip_addr, NULL);
  tr->current.data[DTR_PACK_TRAFFIC] += *length;
  tr->current.data[DTR_PACKETS] ++;
  return binary;
}

static const char *debuginfo_print_trafficip(struct ipaddr_str *buf, union olsr_ip_addr *ip) {
  static const char *total = "Total";
  if (olsr_ipcmp(ip, &total_ip_addr) == 0) {
    return total;
  }
  return olsr_ip_to_string(buf, ip);
}

static bool debuginfo_print_msgstat(struct autobuf *buf, union olsr_ip_addr *ip, struct debug_msgtraffic_count *cnt) {
  struct ipaddr_str strbuf;
  return abuf_appendf(buf, "%-*s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t\n",
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
debuginfo_msgstat(struct comport_connection *con,
    const char *cmd __attribute__ ((unused)), const char *param __attribute__ ((unused)))
{
  struct debug_msgtraffic *tr, *iterator;

  if (abuf_appendf(&con->out, "Slot size: %d seconds\tSlot count: %d\n", traffic_interval, traffic_slots) < 0) {
    return ABUF_ERROR;
  }
  if (abuf_appendf(&con->out,
      "Table: Statistics (without duplicates)\n%-*s\tHello\tTC\tMID\tHNA\tOther\tTotal\tBytes\n",
      olsr_cnf->ip_version == AF_INET ? INET_ADDRSTRLEN : INET6_ADDRSTRLEN, "IP"
      ) < 0) {
    return ABUF_ERROR;
  }

  if (param == NULL || strcasecmp(param, "node") == 0) {
    OLSR_FOR_ALL_MSGTRAFFIC_ENTRIES(tr, iterator) {
      if (debuginfo_print_msgstat(&con->out, &tr->ip, &tr->traffic[current_slot])) {
        return ABUF_ERROR;
      }
    }
  }
  else {
    uint32_t mult = 1, divisor = 1;
    struct debug_msgtraffic_count cnt;
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

    OLSR_FOR_ALL_MSGTRAFFIC_ENTRIES(tr, iterator) {
      for (i=0; i<DTR_MSG_COUNT; i++) {
        cnt.data[i] = (tr->total.data[i] * mult) / divisor;
      }
      if (debuginfo_print_msgstat(&con->out, &tr->ip, &cnt)) {
        return ABUF_ERROR;
      }
    }
  }

  return CONTINUE;
}

static bool debuginfo_print_pktstat(struct autobuf *buf, union olsr_ip_addr *ip, char *int_name, struct debug_pkttraffic_count *cnt) {
  struct ipaddr_str strbuf;
  return abuf_appendf(buf, "%-*s\t%s\t%d\t%d\n",
      olsr_cnf->ip_version == AF_INET ? INET_ADDRSTRLEN : INET6_ADDRSTRLEN, debuginfo_print_trafficip(&strbuf, ip),
      int_name,
      cnt->data[DTR_PACKETS],
      cnt->data[DTR_PACK_TRAFFIC]) < 0;
}

static enum olsr_txtcommand_result
debuginfo_pktstat(struct comport_connection *con,
    const char *cmd __attribute__ ((unused)), const char *param __attribute__ ((unused)))
{
  struct debug_pkttraffic *tr, *iterator;

  if (abuf_appendf(&con->out, "Slot size: %d seconds\tSlot count: %d\n", traffic_interval, traffic_slots) < 0) {
    return ABUF_ERROR;
  }
  if (abuf_appendf(&con->out,
      "Table: Statistics (without duplicates)\n%-*s\tInterf.\tPackets\tBytes\n",
      olsr_cnf->ip_version == AF_INET ? INET_ADDRSTRLEN : INET6_ADDRSTRLEN, "IP"
      ) < 0) {
    return ABUF_ERROR;
  }

  if (param == NULL || strcasecmp(param, "node") == 0) {
    OLSR_FOR_ALL_PKTTRAFFIC_ENTRIES(tr, iterator) {
      if (debuginfo_print_pktstat(&con->out, &tr->ip, tr->int_name, &tr->traffic[current_slot])) {
        return ABUF_ERROR;
      }
    }
  }
  else {
    uint32_t mult = 1, divisor = 1;
    struct debug_pkttraffic_count cnt;
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

    OLSR_FOR_ALL_PKTTRAFFIC_ENTRIES(tr, iterator) {
      for (i=0; i<DTR_PKT_COUNT; i++) {
        cnt.data[i] = (tr->total.data[i] * mult) / divisor;
      }
      if (debuginfo_print_pktstat(&con->out, &tr->ip, tr->int_name, &cnt)) {
        return ABUF_ERROR;
      }
    }
  }

  return CONTINUE;
}

static INLINE bool debuginfo_print_cookies_mem(struct autobuf *buf) {
  struct olsr_memcookie_info *c, *iterator;

  OLSR_FOR_ALL_COOKIES(c, iterator) {
    if (abuf_appendf(buf, "%-25s (MEMORY) size: %lu usage: %u freelist: %u\n",
        c->ci_name, (unsigned long)c->ci_size, c->ci_usage, c->ci_free_list_usage) < 0) {
      return true;
    }
  }
  return false;
}

static INLINE bool debuginfo_print_cookies_timer(struct autobuf *buf) {
  struct olsr_timer_info *t, *iterator;

  OLSR_FOR_ALL_TIMERS(t, iterator) {
    if (abuf_appendf(buf, "%-25s (TIMER) usage: %u changes: %u\n",
        t->name, t->usage, t->changes) < 0) {
      return true;
    }
  }
  return false;
}

static enum olsr_txtcommand_result
debuginfo_cookies(struct comport_connection *con,
    const char *cmd __attribute__ ((unused)), const char *param __attribute__ ((unused)))
{
  if (abuf_puts(&con->out, "Memory cookies:\n") < 0) {
    return ABUF_ERROR;
  }

  if (debuginfo_print_cookies_mem(&con->out)) {
    return ABUF_ERROR;
  }

  if (abuf_puts(&con->out, "\nTimer cookies:\n") < 0) {
    return ABUF_ERROR;
  }

  if (debuginfo_print_cookies_timer(&con->out)) {
    return ABUF_ERROR;
  }
  return CONTINUE;
}

static enum olsr_txtcommand_result
debuginfo_update_logfilter(struct comport_connection *con,
    const char *cmd, const char *param, const char *current, bool value) {
  const char *next;
  int src, sev;

  for (sev = 0; sev < LOG_SEVERITY_COUNT; sev++) {
    if ((next = str_hasnextword(current, LOG_SEVERITY_NAMES[sev])) != NULL) {
      break;
    }
  }
  if (sev == LOG_SEVERITY_COUNT) {
    abuf_appendf(&con->out, "Error, unknown severity in command: %s %s\n", cmd, param);
    return CONTINUE;
  }

  current = next;
  while (current && *current) {
    for (src = 0; src < LOG_SOURCE_COUNT; src++) {
      if ((next = str_hasnextword(current, LOG_SOURCE_NAMES[src])) != NULL) {
        log_debuginfo_mask[sev][src] = value;
        break;
      }
    }
    if (src == LOG_SOURCE_COUNT) {
      abuf_appendf(&con->out, "Error, unknown source in command: %s %s\n", cmd, param);
      return CONTINUE;
    }
    current = next;
  }
  return CONTINUE;
}

static void
debuginfo_print_log(enum log_severity severity __attribute__ ((unused)),
              enum log_source source __attribute__ ((unused)),
              bool no_header __attribute__ ((unused)),
              const char *file __attribute__ ((unused)),
              int line __attribute__ ((unused)),
              char *buffer,
              int timeLength __attribute__ ((unused)),
              int prefixLength __attribute__ ((unused)))
{
  abuf_puts(&log_connection->out, buffer);
  abuf_puts(&log_connection->out, "\n");

  olsr_com_activate_output(log_connection);
}

static void
debuginfo_stop_logging(struct comport_connection *con) {
  con->stop_handler = NULL;
  log_connection = NULL;
  olsr_log_removehandler(log_handler);
}

static enum olsr_txtcommand_result
debuginfo_log(struct comport_connection *con, const char *cmd, const char *param) {
  const char *next;
  int src;

  if (param == NULL) {
    if (con->stop_handler) {
      abuf_puts(&con->out, "Error, you cannot stack continous output commands\n");
      return CONTINUE;
    }
    if (log_connection != NULL) {
      abuf_puts(&con->out, "Error, debuginfo cannot handle concurrent logging\n");
      return CONTINUE;
    }

    log_connection = con;
    con->stop_handler = debuginfo_stop_logging;

    log_handler = olsr_log_addhandler(debuginfo_print_log, &log_debuginfo_mask);
    return CONTINOUS;
  }

  if (strcasecmp(param, "show") == 0) {
    abuf_appendf(&con->out, "%*s %6s %6s %6s %6s\n",
        log_source_maxlen, "",
        LOG_SEVERITY_NAMES[SEVERITY_DEBUG],
        LOG_SEVERITY_NAMES[SEVERITY_INFO],
        LOG_SEVERITY_NAMES[SEVERITY_WARN],
        LOG_SEVERITY_NAMES[SEVERITY_ERR]);

    for (src=0; src<LOG_SOURCE_COUNT; src++) {
      abuf_appendf(&con->out, "%*s %*s %*s %*s %*s\n",
        log_source_maxlen, LOG_SOURCE_NAMES[src],
        log_severity_maxlen, log_debuginfo_mask[SEVERITY_DEBUG][src] ? "*" : "",
        log_severity_maxlen, log_debuginfo_mask[SEVERITY_INFO][src] ? "*" : "",
        log_severity_maxlen, log_debuginfo_mask[SEVERITY_WARN][src] ? "*" : "",
        log_severity_maxlen, log_debuginfo_mask[SEVERITY_ERR][src] ? "*" : "");
    }
    return CONTINUE;
  }

  if ((next = str_hasnextword(param, "add")) != NULL) {
    return debuginfo_update_logfilter(con, cmd, param, next, true);
  }

  if ((next = str_hasnextword(param, "remove")) != NULL) {
    return debuginfo_update_logfilter(con, cmd, param, next, false);
  }

  return UNKNOWN;
}

static enum olsr_txtcommand_result
olsr_debuginfo_displayhelp(struct comport_connection *con,
    const char *cmd __attribute__ ((unused)), const char *param __attribute__ ((unused))) {
  size_t i;

  for (i=0; i<ARRAYSIZE(commands); i++) {
    if (strcasecmp(commands[i].name, cmd) == 0) {
      abuf_puts(&con->out, commands[i].help);
      return CONTINUE;
    }
  }
  return UNKNOWN;
}

/*
 * Local Variables:
 * mode: c
 * style: linux
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */

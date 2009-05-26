
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

static enum olsr_txtcommand_result debuginfo_stat(struct comport_connection *con,  char *cmd, char *param);
static enum olsr_txtcommand_result debuginfo_cookies(struct comport_connection *con,  char *cmd, char *param);

static void update_statistics_ptr(void *);
static bool olsr_msg_statistics(union olsr_message *msg, struct interface *input_if, union olsr_ip_addr *from_addr);
static char *olsr_packet_statistics(char *packet, struct interface *interface, union olsr_ip_addr *, int *length);
static void update_statistics_ptr(void *data __attribute__ ((unused)));

/* plugin configuration */
static struct ip_acl allowed_nets;

/* plugin parameters */
static const struct olsrd_plugin_parameters plugin_parameters[] = {
  {.name = IP_ACL_ACCEPT_PARAP,.set_plugin_parameter = &ip_acl_add_plugin_accept,.data = &allowed_nets},
  {.name = IP_ACL_REJECT_PARAM,.set_plugin_parameter = &ip_acl_add_plugin_reject,.data = &allowed_nets},
  {.name = IP_ACL_CHECKFIRST_PARAM,.set_plugin_parameter = &ip_acl_add_plugin_checkFirst,.data = &allowed_nets},
  {.name = IP_ACL_DEFAULTPOLICY_PARAM,.set_plugin_parameter = &ip_acl_add_plugin_defaultPolicy,.data = &allowed_nets}
};

/* command callbacks and names */
static struct debuginfo_cmd commands[] = {
    {"stat", &debuginfo_stat, NULL, NULL},
    {"cookies", &debuginfo_cookies, NULL, NULL}
};

/* variables for statistics */
static uint32_t recv_packets[60], recv_messages[60][6];
static uint32_t recv_last_relevantTCs;
static struct olsr_cookie_info *statistics_timer = NULL;

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

  statistics_timer = olsr_alloc_cookie("debuginfo statistics timer", OLSR_COOKIE_TYPE_TIMER);
  olsr_start_timer(1000, 0, true, &update_statistics_ptr, NULL, statistics_timer->ci_id);

  memset(recv_packets, 0, sizeof(recv_packets));
  memset(recv_messages, 0, sizeof(recv_messages));

  recv_last_relevantTCs = 0;
  olsr_parser_add_function(&olsr_msg_statistics, PROMISCUOUS);
  olsr_preprocessor_add_function(&olsr_packet_statistics);
  return 1;
}


static void
update_statistics_ptr(void *data __attribute__ ((unused)))
{
  uint32_t now = now_times / 1000;
  int i;

  recv_packets[now % 60] = 0;
  for (i = 0; i < 6; i++) {
    recv_messages[now % 60][i] = 0;
  }
}

/* update message statistics */
static bool
olsr_msg_statistics(union olsr_message *msg,
                    struct interface *input_if __attribute__ ((unused)), union olsr_ip_addr *from_addr __attribute__ ((unused)))
{
  uint32_t now = now_times / 1000;
  int idx, msgtype;

  if (olsr_cnf->ip_version == AF_INET) {
    msgtype = msg->v4.olsr_msgtype;
  } else {
    msgtype = msg->v6.olsr_msgtype;
  }

  switch (msgtype) {
  case HELLO_MESSAGE:
  case TC_MESSAGE:
  case MID_MESSAGE:
  case HNA_MESSAGE:
    idx = msgtype - 1;
    break;

  case LQ_HELLO_MESSAGE:
    idx = 0;
    break;
  case LQ_TC_MESSAGE:
    idx = 1;
    break;
  default:
    idx = 4;
    break;
  }

  recv_messages[now % 60][idx]++;
  if (recv_last_relevantTCs != getRelevantTcCount()) {
    recv_messages[now % 60][5]++;
    recv_last_relevantTCs++;
  }
  return true;
}

/* update traffic statistics */
static char *
olsr_packet_statistics(char *packet __attribute__ ((unused)),
                       struct interface *interface __attribute__ ((unused)),
                       union olsr_ip_addr *ip __attribute__ ((unused)), int *length __attribute__ ((unused)))
{
  uint32_t now = now_times / 1000;
  recv_packets[now % 60] += *length;

  return packet;
}

static enum olsr_txtcommand_result
debuginfo_stat(struct comport_connection *con,  char *cmd __attribute__ ((unused)), char *param __attribute__ ((unused)))
{
  static const char *names[] = { "HELLO", "TC", "MID", "HNA", "Other", "Rel.TCs" };

  uint32_t msgs[6], traffic, i, j;
  uint32_t slot = (now_times / 1000 + 59) % 60;

  if (!con->is_csv && abuf_puts(&con->out, "Table: Statistics (without duplicates)\nType\tlast seconds\t\t\t\tlast min.\taverage\n") < 0) {
    return ABUF_ERROR;
  }

  for (j = 0; j < 6; j++) {
    msgs[j] = 0;
    for (i = 0; i < 60; i++) {
      msgs[j] += recv_messages[i][j];
    }
  }

  traffic = 0;
  for (i = 0; i < 60; i++) {
    traffic += recv_packets[i];
  }

  for (i = 0; i < 6; i++) {
    if (abuf_appendf(&con->out, !con->is_csv ? "%s\t%u\t%u\t%u\t%u\t%u\t%u\t\t%u\n" : "stat,%s,%u,%u,%u,%u,%u,%u,%u\n",
                     names[i],
                     recv_messages[(slot) % 60][i],
                     recv_messages[(slot + 59) % 60][i],
                     recv_messages[(slot + 58) % 60][i],
                     recv_messages[(slot + 57) % 60][i],
                     recv_messages[(slot + 56) % 60][i],
                     msgs[i],
                     msgs[i] / 60) < 0) {
        return ABUF_ERROR;
    }
  }
  if (abuf_appendf(&con->out, !con->is_csv ? "\nTraffic: %8u bytes/s\t%u bytes/minute\taverage %u bytes/s\n" : "stat,Traffic,%u,%u,%u\n",
      recv_packets[(slot) % 60], traffic, traffic / 60) < 0) {
    return ABUF_ERROR;
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

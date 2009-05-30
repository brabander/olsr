
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
#include "common/autobuf.h"

#include "olsrd_txtinfo.h"

#define PLUGIN_NAME    "OLSRD txtinfo plugin"
#define PLUGIN_VERSION "0.2"
#define PLUGIN_AUTHOR   "Henning Rogge"
#define MOD_DESC PLUGIN_NAME " " PLUGIN_VERSION " by " PLUGIN_AUTHOR
#define PLUGIN_INTERFACE_VERSION 5

struct debuginfo_cmd {
  const char *name;
  olsr_txthandler handler;
  struct olsr_txtcommand *normal, *csv;
};

static void txtinfo_new(void) __attribute__ ((constructor));
static void txtinfo_delete(void) __attribute__ ((destructor));

static enum olsr_txtcommand_result txtinfo_neigh(struct comport_connection *con, char *cmd, char *param);
static enum olsr_txtcommand_result txtinfo_link(struct comport_connection *con,  char *cmd, char *param);
static enum olsr_txtcommand_result txtinfo_routes(struct comport_connection *con,  char *cmd, char *param);
static enum olsr_txtcommand_result txtinfo_topology(struct comport_connection *con,  char *cmd, char *param);
static enum olsr_txtcommand_result txtinfo_hna(struct comport_connection *con,  char *cmd, char *param);
static enum olsr_txtcommand_result txtinfo_mid(struct comport_connection *con,  char *cmd, char *param);


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
    {"neigh", &txtinfo_neigh, NULL, NULL},
    {"link", &txtinfo_link, NULL, NULL},
    {"routes", &txtinfo_routes, NULL, NULL},
    {"topology", &txtinfo_topology, NULL, NULL},
    {"hna", &txtinfo_hna, NULL, NULL},
    {"mid", &txtinfo_mid, NULL, NULL},
};

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
 * Constructor of plugin, called before parameters are initialized
 */
static void
txtinfo_new(void)
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
 * Destructor of plugin
 */
static void
txtinfo_delete(void)
{
  size_t i;

  for (i=0; i<ARRAYSIZE(commands); i++) {
    olsr_com_remove_normal_txtcommand(commands[i].normal);
    olsr_com_remove_csv_txtcommand(commands[i].csv);
  }
  ip_acl_flush(&allowed_nets);
}

/*
 * Initialization of plugin AFTER parameters have been read
 */
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
  return 1;
}

/**
 * Callback for neigh command
 */
static enum olsr_txtcommand_result
txtinfo_neigh(struct comport_connection *con,  char *cmd __attribute__ ((unused)), char *param __attribute__ ((unused)))
{
  struct nbr_entry *neigh;
  if (!con->is_csv && abuf_puts(&con->out, "Table: Neighbors\nIP address\tSYM\tMPR\tMPRS\tWill.\t2 Hop Neighbors\n") < 0) {
    return ABUF_ERROR;
  }

  /* Neighbors */
  OLSR_FOR_ALL_NBR_ENTRIES(neigh) {
    struct ipaddr_str buf1;
    if (abuf_appendf(&con->out, !con->is_csv ? "%s\t%s\t%s\t%s\t%d\t%d\n" : "neigh,%s,%s,%s,%s,%d,%d\n",
                       olsr_ip_to_string(&buf1, &neigh->nbr_addr),
                       neigh->is_sym ? "YES" : "NO",
                       neigh->is_mpr ? "YES" : "NO",
                       olsr_lookup_mprs_set(&neigh->nbr_addr) ? "YES" : "NO", neigh->willingness, neigh->con_tree.count) < 0) {
        return ABUF_ERROR;
    }
  } OLSR_FOR_ALL_NBR_ENTRIES_END(neigh);

  return CONTINUE;
}

/**
 * Callback for link command
 */
static enum olsr_txtcommand_result
txtinfo_link(struct comport_connection *con,  char *cmd __attribute__ ((unused)), char *param __attribute__ ((unused)))
{
  struct link_entry *lnk;

  OLSR_DEBUG(LOG_NETWORKING, "Starting 'link' command...\n");
  if (!con->is_csv && abuf_puts(&con->out, "Table: Links\nLocal IP\tRemote IP\tLQ\tNLQ\tCost\n") < 0) {
    return ABUF_ERROR;
  }

  /* Link set */
  OLSR_FOR_ALL_LINK_ENTRIES(lnk) {
    struct ipaddr_str buf1, buf2;
    struct lqtextbuffer lqbuffer1, lqbuffer2;
    if (abuf_appendf(&con->out,
                     !con->is_csv ? "%s\t%s\t%s\t%s\t\n" : "link,%s,%s,%s,%s\n",
                     olsr_ip_to_string(&buf1, &lnk->local_iface_addr),
                     olsr_ip_to_string(&buf2, &lnk->neighbor_iface_addr),
                     get_link_entry_text(lnk, '\t', &lqbuffer1), get_linkcost_text(lnk->linkcost, false, &lqbuffer2)) < 0) {
        return ABUF_ERROR;
    }
  } OLSR_FOR_ALL_LINK_ENTRIES_END(lnk);

  return CONTINUE;
}

/**
 * Callback for routes command
 */
static enum olsr_txtcommand_result
txtinfo_routes(struct comport_connection *con,  char *cmd __attribute__ ((unused)), char *param __attribute__ ((unused)))
{
  struct rt_entry *rt;

  if (!con->is_csv && abuf_puts(&con->out, "Table: Routes\nDestination\tGateway IP\tMetric\tETX\tInterface\n") < 0) {
    return ABUF_ERROR;
  }

  /* Walk the route table */
  OLSR_FOR_ALL_RT_ENTRIES(rt) {
    struct ipaddr_str ipbuf;
    struct ipprefix_str prefixstr;
    struct lqtextbuffer lqbuffer;
    if (abuf_appendf(&con->out, !con->is_csv ? "%s\t%s\t%u\t%s\t%s\t\n" : "route,%s,%s,%u,%s,%s\n",
                     olsr_ip_prefix_to_string(&prefixstr, &rt->rt_dst),
                     olsr_ip_to_string(&ipbuf, &rt->rt_best->rtp_nexthop.gateway),
                     rt->rt_best->rtp_metric.hops,
                     get_linkcost_text(rt->rt_best->rtp_metric.cost, true, &lqbuffer),
                     rt->rt_best->rtp_nexthop.interface ? rt->rt_best->rtp_nexthop.interface->int_name : "[null]") < 0) {
      return ABUF_ERROR;
    }
  } OLSR_FOR_ALL_RT_ENTRIES_END(rt);
  return CONTINUE;
}

/**
 * Callback for topology command
 */
static enum olsr_txtcommand_result
txtinfo_topology(struct comport_connection *con,  char *cmd __attribute__ ((unused)), char *param __attribute__ ((unused)))
{
  struct tc_entry *tc;
  if (!con->is_csv && abuf_puts(&con->out, "Table: Topology\nDest. IP\tLast hop IP\tLQ\tNLQ\tCost\n") < 0) {
    return ABUF_ERROR;
  }

  /* Topology */
  OLSR_FOR_ALL_TC_ENTRIES(tc) {
    struct tc_edge_entry *tc_edge;
    OLSR_FOR_ALL_TC_EDGE_ENTRIES(tc, tc_edge) {
      if (tc_edge->edge_inv) {
        struct ipaddr_str dstbuf, addrbuf;
        struct lqtextbuffer lqbuffer1, lqbuffer2;
        if (abuf_appendf(&con->out, !con->is_csv ? "%s\t%s\t%s\t%s\n" : "topo,%s,%s,%s,%s\n",
                         olsr_ip_to_string(&dstbuf, &tc_edge->T_dest_addr),
                         olsr_ip_to_string(&addrbuf, &tc->addr),
                         get_tc_edge_entry_text(tc_edge, '\t', &lqbuffer1),
                         get_linkcost_text(tc_edge->cost, false, &lqbuffer2)) < 0) {
          return ABUF_ERROR;
        }
      }
    } OLSR_FOR_ALL_TC_EDGE_ENTRIES_END(tc, tc_edge);
  } OLSR_FOR_ALL_TC_ENTRIES_END(tc)

  return CONTINUE;
}

/**
 * helper which prints an HNA entry
 */
static INLINE bool
txtinfo_print_hna_entry(struct autobuf *buf, const char *format, const struct olsr_ip_prefix *hna_prefix, const union olsr_ip_addr *ipaddr)
{
  struct ipaddr_str mainaddrbuf;
  struct ipprefix_str addrbuf;
  return abuf_appendf(buf, format, olsr_ip_prefix_to_string(&addrbuf, hna_prefix), olsr_ip_to_string(&mainaddrbuf, ipaddr)) < 0;
}

/**
 * Callback for hna command
 */
static enum olsr_txtcommand_result
txtinfo_hna(struct comport_connection *con,  char *cmd __attribute__ ((unused)), char *param __attribute__ ((unused)))
{
  const struct ip_prefix_entry *hna;
  struct tc_entry *tc;

  const char *format = !con->is_csv ? "%s\t%s\n" : "hna,%s,%s\n";
  if (!con->is_csv && abuf_puts(&con->out, "Table: HNA\nDestination\tGateway\n") < 0) {
    return ABUF_ERROR;
  }

  /* Announced HNA entries */
  OLSR_FOR_ALL_IPPREFIX_ENTRIES(&olsr_cnf->hna_entries, hna) {
    if (txtinfo_print_hna_entry(&con->out, format, &hna->net, &olsr_cnf->router_id)) {
      return ABUF_ERROR;
    }
  }
  OLSR_FOR_ALL_IPPREFIX_ENTRIES_END()

    /* HNA entries */
    OLSR_FOR_ALL_TC_ENTRIES(tc) {
    struct hna_net *tmp_net;
    /* Check all networks */
    OLSR_FOR_ALL_TC_HNA_ENTRIES(tc, tmp_net) {
      if (txtinfo_print_hna_entry(&con->out, format, &tmp_net->hna_prefix, &tc->addr)) {
        return ABUF_ERROR;
      }
    } OLSR_FOR_ALL_TC_HNA_ENTRIES_END(tc, tmp_net);
  } OLSR_FOR_ALL_TC_ENTRIES_END(tc);

  return CONTINUE;
}

/**
 * Callback for mid command
 */
static enum olsr_txtcommand_result
txtinfo_mid(struct comport_connection *con,  char *cmd __attribute__ ((unused)), char *param __attribute__ ((unused)))
{
  struct tc_entry *tc;
  const char *prefix = !con->is_csv ? "" : "mid,";
  const char sep = !con->is_csv ? '\t' : ',';

  if (!con->is_csv && abuf_puts(&con->out, "Table: MID\nIP address\tAliases\n") < 0) {
    return ABUF_ERROR;
  }

  /* MID root is the TC entry */
  OLSR_FOR_ALL_TC_ENTRIES(tc) {
    struct ipaddr_str ipbuf;
    struct mid_entry *alias;

    if (abuf_appendf(&con->out, "%s%s", prefix, olsr_ip_to_string(&ipbuf, &tc->addr)) < 0) {
      return ABUF_ERROR;
    }

    OLSR_FOR_ALL_TC_MID_ENTRIES(tc, alias) {
      if (abuf_appendf(&con->out, "%c%s", sep, olsr_ip_to_string(&ipbuf, &alias->mid_alias_addr)) < 0) {
        return ABUF_ERROR;
      }
    }
    OLSR_FOR_ALL_TC_MID_ENTRIES_END(tc, alias);
    if (abuf_appendf(&con->out, "\n") < 0) {
      return ABUF_ERROR;
    }
  } OLSR_FOR_ALL_TC_ENTRIES_END(tc)
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

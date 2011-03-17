
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

#include <stdio.h>

#include "olsr.h"
#include "ipcalc.h"
#include "neighbor_table.h"
#include "tc_set.h"
#include "hna_set.h"
#include "mid_set.h"
#include "routing_table.h"
#include "olsr_ip_prefix_list.h"
#include "parser.h"
#include "olsr_comport_txt.h"
#include "olsr_comport_http.h"
#include "common/string.h"
#include "common/autobuf.h"
#include "plugin_loader.h"
#include "plugin_util.h"

#define PLUGIN_DESCR    "OLSRD txtinfo plugin"
#define PLUGIN_AUTHOR   "Henning Rogge"


struct txtinfo_cmd {
  const char *name;
  olsr_txthandler handler;
  struct olsr_txtcommand *cmd;
};

static int txtinfo_init(void);
static int txtinfo_enable(void);
static int txtinfo_disable(void);
static int txtinfo_exit(void);

static enum olsr_txtcommand_result txtinfo_neigh(struct comport_connection *con,
    const char *cmd, const char *param);
static enum olsr_txtcommand_result txtinfo_link(struct comport_connection *con,
    const char *cmd, const char *param);
static enum olsr_txtcommand_result txtinfo_routes(struct comport_connection *con,
    const char *cmd, const char *param);
static enum olsr_txtcommand_result txtinfo_topology(struct comport_connection *con,
    const char *cmd, const char *param);
static enum olsr_txtcommand_result txtinfo_hna(struct comport_connection *con,
    const char *cmd, const char *param);
static enum olsr_txtcommand_result txtinfo_mid(struct comport_connection *con,
    const char *cmd, const char *param);
static enum olsr_txtcommand_result txtinfo_interfaces(struct comport_connection *con,
    const char *cmd, const char *param);

/* plugin configuration */
static struct ip_acl allowed_nets;

/* plugin parameters */
static const struct olsrd_plugin_parameters plugin_parameters[] = {
  {.name = IP_ACL_ACCEPT_PARAP,.set_plugin_parameter = &ip_acl_add_plugin_accept,.data = &allowed_nets},
  {.name = IP_ACL_REJECT_PARAM,.set_plugin_parameter = &ip_acl_add_plugin_reject,.data = &allowed_nets},
  {.name = IP_ACL_CHECKFIRST_PARAM,.set_plugin_parameter = &ip_acl_add_plugin_checkFirst,.data = &allowed_nets},
  {.name = IP_ACL_DEFAULTPOLICY_PARAM,.set_plugin_parameter = &ip_acl_add_plugin_defaultPolicy,.data = &allowed_nets}
};

OLSR_PLUGIN6(plugin_parameters) {
  .descr = PLUGIN_DESCR,
  .author = PLUGIN_AUTHOR,
  .init = txtinfo_init,
  .enable = txtinfo_enable,
  .disable = txtinfo_disable,
  .exit = txtinfo_exit,
  .deactivate = true
};

/* command callbacks and names */
static struct txtinfo_cmd commands[] = {
    {"link", &txtinfo_link, NULL},
    {"neigh", &txtinfo_neigh, NULL},
    {"topology", &txtinfo_topology, NULL},
    {"hna", &txtinfo_hna, NULL},
    {"mid", &txtinfo_mid, NULL},
    {"routes", &txtinfo_routes, NULL},
    {"interfaces", &txtinfo_interfaces, NULL},
};

/* base path for http access (should end with a '/') */
static const char TXTINFO_HTTP_PATH[] = "/txtinfo/";

/* constants and static storage for template engine */
static const char KEY_LOCALIP[] = "localip";
static const char KEY_NEIGHIP[] = "neighip";
static const char KEY_ALIASIP[] = "aliasip";
static const char KEY_DESTPREFIX[] = "destprefix";
static const char KEY_SYM[] = "issym";
static const char KEY_MPR[] = "ismpr";
static const char KEY_MPRS[] = "ismprs";
static const char KEY_VIRTUAL[] = "isvirtual";
static const char KEY_WILLINGNESS[] = "will";
static const char KEY_2HOP[] = "2hop";
static const char KEY_LINKCOST[] = "linkcost";
static const char KEY_RAWLINKCOST[] = "rawlinkcost";
static const char KEY_HOPCOUNT[] = "hopcount";
static const char KEY_FAILCOUNT[] = "failcount";
static const char KEY_INTERFACE[] = "interface";
static const char KEY_VTIME[] = "vtime";
static const char KEY_STATE[] = "state";
static const char KEY_MTU[] = "mtu";
static const char KEY_SRCIP[] = "srcip";
static const char KEY_DSTIP[] = "dstip";

static struct ipaddr_str buf_localip, buf_neighip, buf_aliasip, buf_srcip, buf_dstip;
struct ipprefix_str buf_destprefix;
static char buf_sym[6], buf_mrp[4], buf_mprs[4], buf_virtual[4];
static char buf_willingness[7];
static char buf_2hop[6];
static char buf_rawlinkcost[11];
static char buf_linkcost[LQTEXT_MAXLENGTH];
static char buf_hopcount[4];
static char buf_failcount[8];
static char buf_state[5];
static char buf_mtu[5];
static char buf_interface[IF_NAMESIZE];
struct millitxt_buf buf_vtime;

static size_t tmpl_indices[32];

/* a link metric may contain up to 8 values */
static const char *keys_link[] = {
  KEY_LOCALIP, KEY_NEIGHIP, KEY_SYM, KEY_MPR, KEY_VTIME,
  KEY_RAWLINKCOST, KEY_LINKCOST,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
};
static char *values_link[] = {
  buf_localip.buf, buf_neighip.buf, buf_sym, buf_mrp, buf_vtime.buf,
  buf_rawlinkcost, buf_linkcost,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
};
static char tmpl_link[256], headline_link[256];
static size_t link_keys_static = 0, link_keys_count = 0, link_value_size = 0;

static const char *tmpl_neigh = "%neighip%\t%issym%\t%ismpr%\t%ismprs%\t%will%\t%2hop%\n";
static const char *keys_neigh[] = {
  KEY_NEIGHIP, KEY_SYM, KEY_MPR, KEY_MPRS, KEY_WILLINGNESS, KEY_2HOP
};
static char *values_neigh[] = {
  buf_neighip.buf, buf_sym, buf_mprs, buf_mprs, buf_willingness, buf_2hop
};

static const char *tmpl_routes = "%destprefix%\t%neighip%\t%hopcount%\t%linkcost%\t%interface%\t%failcount%\n";
static const char *keys_routes[] = {
  KEY_DESTPREFIX, KEY_NEIGHIP, KEY_HOPCOUNT, KEY_VTIME,
  KEY_INTERFACE, KEY_RAWLINKCOST, KEY_LINKCOST, KEY_FAILCOUNT
};
static char *values_routes[] = {
  buf_destprefix.buf, buf_neighip.buf, buf_hopcount, buf_vtime.buf,
  buf_interface, buf_rawlinkcost, buf_linkcost, buf_failcount
};

static const char *tmpl_topology = "%neighip%\t%localip%\t%isvirtual%\t%linkcost%\n";
static const char *keys_topology[] = {
  KEY_LOCALIP, KEY_NEIGHIP, KEY_VIRTUAL, KEY_VTIME,
  KEY_RAWLINKCOST, KEY_LINKCOST
};
static char *values_topology[] = {
  buf_localip.buf, buf_neighip.buf, buf_virtual, buf_vtime.buf,
  buf_rawlinkcost, buf_linkcost
};

static const char *tmpl_hna = "%destprefix%\t%localip%\t%vtime%\n";
static const char *keys_hna[] = {
  KEY_LOCALIP, KEY_DESTPREFIX, KEY_VTIME
};
static char *values_hna[] = {
  buf_localip.buf, buf_destprefix.buf, buf_vtime.buf
};

static const char *tmpl_mid = "%localip%\t%aliasip%\t%vtime%\n";
static const char *keys_mid[] = {
  KEY_LOCALIP, KEY_ALIASIP, KEY_VTIME
};
static char *values_mid[] = {
  buf_localip.buf, buf_aliasip.buf, buf_vtime.buf
};

static const char *tmpl_interface = "%interface%\t%state%\t%mtu%\t%srcip%\t%dstip%\n";
static const char *keys_interface[] = {
  KEY_INTERFACE, KEY_STATE, KEY_MTU, 
  KEY_SRCIP, KEY_DSTIP
};
static char *values_interface[] = {
  buf_interface, buf_state, buf_mtu, 
  buf_srcip.buf, buf_dstip.buf
};

/* String constants for later use */
static const char *OLSR_YES = "yes";
static const char *OLSR_NO  = "no";

/**
 * Constructor of plugin, called before parameters are initialized
 */
static int
txtinfo_init(void)
{
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
  return 0;
}

/**
 * Destructor of plugin
 */
static int
txtinfo_disable(void)
{
  size_t i;

  for (i=0; i<ARRAYSIZE(commands); i++) {
    olsr_com_remove_normal_txtcommand(commands[i].cmd);
  }
  for (i=link_keys_static; i<link_keys_count; i++) {
    free(values_link[i]);
  }
  return 0;
}

/*
 * Initialization of plugin AFTER parameters have been read
 */
static int
txtinfo_enable(void)
{
  size_t i;

  /* count static link keys */
  while (keys_link[link_keys_static]) {
    link_keys_static++;
    link_keys_count++;
  }

  /* generate dynamic link keys */
  for (i=1; i<olsr_get_linklabel_count(); i++) {
    keys_link[link_keys_static + i - 1] = olsr_get_linklabel(i);
    values_link[link_keys_static + i - 1] = olsr_malloc(LQTEXT_MAXLENGTH, "txtinfo linktemplate values");
    link_keys_count++;
  }
  link_value_size = LQTEXT_MAXLENGTH;

  /* generate link template */
  strscpy(tmpl_link, "%localip%\t%neighip", sizeof(tmpl_link));
  for (i=1; i<olsr_get_linklabel_count(); i++) {
    strscat(tmpl_link, "%\t%", sizeof(tmpl_link));
    strscat(tmpl_link, olsr_get_linklabel(i), sizeof(tmpl_link));
  }
  strscat(tmpl_link, "%\t%linkcost%\n", sizeof(tmpl_link));

  /* generate link summary */
  strscpy(headline_link, "Table: Links\nLocal IP\tRemote IP", sizeof(headline_link));
  for (i=1; i<olsr_get_linklabel_count(); i++) {
    strscat(headline_link, "\t", sizeof(headline_link));
    strscat(headline_link, olsr_get_linklabel(i), sizeof(headline_link));
  }
  strscat(headline_link, "\t", sizeof(headline_link));
  strscat(headline_link, olsr_get_linklabel(0), sizeof(headline_link));
  strscat(headline_link, "\n", sizeof(headline_link));

  for (i=0; i<ARRAYSIZE(commands); i++) {
    commands[i].cmd = olsr_com_add_normal_txtcommand(commands[i].name, commands[i].handler);
    commands[i].cmd->acl = &allowed_nets;
  }
  return 0;
}

static int
txtinfo_exit(void) {
  ip_acl_flush(&allowed_nets);
  return 0;
}

/**
 * Parse user templates for \%, \n and \t
 *
 * @param template
 */
static char *
parse_user_template(const char *template) {
  // TODO: dynamic buffer ?
  static char buffer[1024];
  char *dst = buffer;

  while (*template && (size_t)(dst - buffer) < sizeof(buffer)-1) {
    if (*template == '\\') {
      template++;
      switch (*template) {
        case 0:
          *dst = 0;
          break;
        case 'n':
          *dst = '\n';
          break;
        case 't':
          *dst = '\t';
          break;
        case '\\':
          *dst = '\\';
          break;
        default:
          *dst++ = '\\';
          *dst = *template;
          break;
      }
    }
    else {
      *dst = *template;
    }
    template++;
    dst++;
  }
  *dst = 0;
  return buffer;
}

/**
 * Callback for neigh command
 */
static enum olsr_txtcommand_result
txtinfo_neigh(struct comport_connection *con,
    const char *cmd __attribute__ ((unused)), const char *param)
{
  struct nbr_entry *neigh, *iterator;
  const char *template;
  int indexLength;

  template = param != NULL ? parse_user_template(param) : tmpl_neigh;
  if (param == NULL &&
      abuf_puts(&con->out, "Table: Neighbors\nIP address\tSYM\tMPR\tMPRS\tWill.\t2 Hop Neighbors\n") < 0) {
    return ABUF_ERROR;
  }

  if ((indexLength = abuf_template_init(keys_neigh, ARRAYSIZE(keys_neigh), template, tmpl_indices, ARRAYSIZE(tmpl_indices))) < 0) {
    return ABUF_ERROR;
  }

  /* Neighbors */
  OLSR_FOR_ALL_NBR_ENTRIES(neigh, iterator) {
    olsr_ip_to_string(&buf_neighip, &neigh->nbr_addr);
    strscpy(buf_sym, neigh->is_sym ? OLSR_YES : OLSR_NO, sizeof(buf_sym));
    strscpy(buf_mrp, neigh->is_mpr ? OLSR_YES : OLSR_NO, sizeof(buf_mrp));
    strscpy(buf_mprs, neigh->mprs_count>0 ? OLSR_YES : OLSR_NO, sizeof(buf_mprs));

    snprintf(buf_willingness, sizeof(buf_willingness), "%d", neigh->willingness);
    snprintf(buf_2hop, sizeof(buf_2hop), "%d", neigh->con_tree.count);

    if (abuf_templatef(&con->out, template, values_neigh, tmpl_indices, indexLength) < 0) {
        return ABUF_ERROR;
    }
  }

  return CONTINUE;
}

/**
 * Callback for link command
 */
static enum olsr_txtcommand_result
txtinfo_link(struct comport_connection *con,
    const char *cmd __attribute__ ((unused)), const char *param)
{
  struct link_entry *lnk, *iterator;
  size_t i;
  const char *template;
  int indexLength;

  template = param != NULL ? parse_user_template(param) : tmpl_link;
  if (param == NULL) {
    if (abuf_puts(&con->out, headline_link) < 0) {
      return ABUF_ERROR;
    }
  }

  if ((indexLength = abuf_template_init(keys_link, link_keys_count,
      template, tmpl_indices, ARRAYSIZE(tmpl_indices))) < 0) {
    return ABUF_ERROR;
  }

  /* Link set */
  OLSR_FOR_ALL_LINK_ENTRIES(lnk, iterator) {
    olsr_ip_to_string(&buf_localip, &lnk->local_iface_addr);
    olsr_ip_to_string(&buf_neighip, &lnk->neighbor_iface_addr);
    strscpy(buf_sym, lnk->status == SYM_LINK ? OLSR_YES : OLSR_NO, sizeof(buf_sym));
    strscpy(buf_mrp, lnk->is_mpr ? OLSR_YES : OLSR_NO, sizeof(buf_mrp));
    olsr_clock_to_string(&buf_vtime, lnk->link_sym_timer == NULL ? 0 : lnk->link_sym_timer->timer_clock - olsr_clock_getNow());
    snprintf(buf_rawlinkcost, sizeof(buf_rawlinkcost), "%ud", lnk->linkcost);

    olsr_get_linkcost_text(lnk->linkcost, false, buf_linkcost, sizeof(buf_linkcost));
    for (i=1; i< olsr_get_linklabel_count(); i++) {
      olsr_get_linkdata_text(lnk, i, values_link[link_keys_static + i - 1], link_value_size);
    }

    if (abuf_templatef(&con->out, template, values_link, tmpl_indices, indexLength) < 0) {
        return ABUF_ERROR;
    }
  }

  return CONTINUE;
}

/**
 * Callback for routes command
 */
static enum olsr_txtcommand_result
txtinfo_routes(struct comport_connection *con,
    const char *cmd __attribute__ ((unused)), const char *param __attribute__ ((unused)))
{
  struct rt_entry *rt, *iterator;
  const char *template;
  int indexLength;

  template = param != NULL ? parse_user_template(param) : tmpl_routes;
  if (param == NULL) {
    if (abuf_appendf(&con->out, "Table: Routes\nDestination\tGateway IP\tMetric\t%s\tInterface\n",
        olsr_get_linklabel(0)) < 0) {
      return ABUF_ERROR;
    }
  }

  if ((indexLength = abuf_template_init(keys_routes, ARRAYSIZE(keys_routes),
      template, tmpl_indices, ARRAYSIZE(tmpl_indices))) < 0) {
    return ABUF_ERROR;
  }

  /* Walk the route table */
  OLSR_FOR_ALL_RT_ENTRIES(rt, iterator) {
    if (!rt->rt_best) {
      /* ignore entries without paths, they will be erased soon */
      continue;
    }
    olsr_ip_prefix_to_string(&buf_destprefix, &rt->rt_dst);
    olsr_ip_to_string(&buf_neighip, &rt->rt_best->rtp_nexthop.gateway);

    if (rt->failure_count < 0)  snprintf(buf_failcount, sizeof(buf_failcount), "%d del", rt->failure_count*(-1));
    else if (rt->failure_count == 0) snprintf(buf_failcount, sizeof(buf_failcount), "0");
    else snprintf(buf_failcount, sizeof(buf_failcount), "%d add", rt->failure_count);

    snprintf(buf_hopcount, sizeof(buf_hopcount), "%d", rt->rt_best->rtp_metric.hops);
    snprintf(buf_rawlinkcost, sizeof(buf_rawlinkcost), "%ud", rt->rt_best->rtp_metric.cost);
    olsr_get_linkcost_text(rt->rt_best->rtp_metric.cost, true, buf_linkcost, sizeof(buf_linkcost));
    strscpy(buf_interface,
        rt->rt_best->rtp_nexthop.interface ? rt->rt_best->rtp_nexthop.interface->int_name : "[null]",
        sizeof(buf_interface));

    if (abuf_templatef(&con->out, template, values_routes, tmpl_indices, indexLength) < 0) {
        return ABUF_ERROR;
    }
  }
  return CONTINUE;
}

/**
 * Callback for topology command
 */
static enum olsr_txtcommand_result
txtinfo_topology(struct comport_connection *con,
    const char *cmd __attribute__ ((unused)), const char *param __attribute__ ((unused)))
{
  struct tc_entry *tc, *iterator;
  const char *template;
  int indexLength;

  template = param != NULL ? parse_user_template(param) : tmpl_topology;
  if (param == NULL) {
    if (abuf_appendf(&con->out, "Table: Topology\nDest. IP\tLast hop IP\tVirtual\t%s\n",
        olsr_get_linklabel(0)) < 0) {
      return ABUF_ERROR;
    }
  }

  if ((indexLength = abuf_template_init(keys_topology, ARRAYSIZE(keys_topology),
      template, tmpl_indices, ARRAYSIZE(tmpl_indices))) < 0) {
    return ABUF_ERROR;
  }

  /* Topology */
  OLSR_FOR_ALL_TC_ENTRIES(tc, iterator) {
    struct tc_edge_entry *tc_edge, *edge_iterator;
    olsr_ip_to_string(&buf_localip, &tc->addr);
    if (tc->validity_timer) {
      olsr_clock_to_string(&buf_vtime, tc->validity_timer->timer_clock - olsr_clock_getNow());
    }
    else {
      strscpy(buf_vtime.buf, "0.0", sizeof(buf_vtime));
    }

    OLSR_FOR_ALL_TC_EDGE_ENTRIES(tc, tc_edge, edge_iterator) {
      olsr_ip_to_string(&buf_neighip, &tc_edge->T_dest_addr);
      strscpy(buf_virtual, tc_edge->virtual ? OLSR_YES : OLSR_NO, sizeof(buf_virtual));
      if (tc_edge->virtual) {
        buf_linkcost[0] = 0;
        buf_rawlinkcost[0] = '0';
        buf_rawlinkcost[1] = 0;
      }
      else {
        snprintf(buf_rawlinkcost, sizeof(buf_rawlinkcost), "%ud", tc_edge->cost);
        olsr_get_linkcost_text(tc_edge->cost, false, buf_linkcost, sizeof(buf_linkcost));
      }

      if (abuf_templatef(&con->out, template, values_topology, tmpl_indices, indexLength) < 0) {
          return ABUF_ERROR;
      }
    }
  }

  return CONTINUE;
}

/**
 * Callback for interfaces command
 */
static enum olsr_txtcommand_result
txtinfo_interfaces(struct comport_connection *con,
    const char *cmd __attribute__ ((unused)), const char *param __attribute__ ((unused)))
{
  const struct olsr_if_config *ifs;
  const char *template;
  int indexLength;

  template = param != NULL ? parse_user_template(param) : tmpl_interface;
  if (param == NULL) {
    if (abuf_puts(&con->out, "Table: Interfaces\nName\tState\tMTU\tSrc-Adress\tDst-Adress\n") < 0) {
      return ABUF_ERROR;
    }
  }

  if ((indexLength = abuf_template_init(keys_interface, ARRAYSIZE(keys_interface),
      template, tmpl_indices, ARRAYSIZE(tmpl_indices))) < 0) {
    return ABUF_ERROR;
  }


  for (ifs = olsr_cnf->if_configs; ifs != NULL; ifs = ifs->next) {
    const struct interface *const rifs = ifs->interf;

    //prepare values
    strscpy(buf_interface, ifs->name, sizeof(buf_interface));

    if (!rifs) {
      snprintf(buf_state, sizeof(buf_state), "DOWN");
      snprintf(buf_mtu, sizeof(buf_mtu), "-");
      snprintf(buf_srcip.buf, sizeof(buf_srcip.buf), "-");
      snprintf(buf_dstip.buf, sizeof(buf_srcip.buf), "-");
    } else {
      snprintf(buf_mtu, sizeof(buf_mtu), "%d", rifs->int_mtu);
      snprintf(buf_state, sizeof(buf_state), "UP");

      if (olsr_cnf->ip_version == AF_INET){
        ip4_to_string(&buf_srcip, rifs->int_src.v4.sin_addr);
        ip4_to_string(&buf_dstip, rifs->int_multicast.v4.sin_addr);
      } else {
        ip6_to_string(&buf_srcip, &rifs->int_src.v6.sin6_addr);
        ip6_to_string(&buf_dstip, &rifs->int_multicast.v6.sin6_addr); 
      }
    }

    if (abuf_templatef(&con->out, template, 
        values_interface, tmpl_indices, indexLength) < 0) {
        return ABUF_ERROR;
    }
  }

  return CONTINUE;
}

/**
 * Callback for hna command
 */
static enum olsr_txtcommand_result
txtinfo_hna(struct comport_connection *con,
    const char *cmd __attribute__ ((unused)), const char *param __attribute__ ((unused)))
{
  const struct ip_prefix_entry *hna, *prefix_iterator;
  struct tc_entry *tc, *tc_iterator;
  const char *template;
  int indexLength;

  template = param != NULL ? parse_user_template(param) : tmpl_hna;
  if (param == NULL) {
    if (abuf_puts(&con->out, "Table: HNA\nDestination\tGateway\tvtime\n") < 0) {
      return ABUF_ERROR;
    }
  }

  if ((indexLength = abuf_template_init(keys_hna, ARRAYSIZE(keys_hna),
      template, tmpl_indices, ARRAYSIZE(tmpl_indices))) < 0) {
    return ABUF_ERROR;
  }

  /* Announced HNA entries */
  OLSR_FOR_ALL_IPPREFIX_ENTRIES(&olsr_cnf->hna_entries, hna, prefix_iterator) {
    olsr_ip_to_string(&buf_localip, &olsr_cnf->router_id);
    olsr_ip_prefix_to_string(&buf_destprefix, &hna->net);
    strscpy(buf_vtime.buf, "0.0", sizeof(buf_vtime));

    if (abuf_templatef(&con->out, template, values_hna, tmpl_indices, indexLength) < 0) {
        return ABUF_ERROR;
    }
  }

  /* HNA entries */
  OLSR_FOR_ALL_TC_ENTRIES(tc, tc_iterator) {
    struct hna_net *tmp_net, *hna_iterator;

    olsr_ip_to_string(&buf_localip, &tc->addr);
    if (tc->validity_timer) {
      olsr_clock_to_string(&buf_vtime, tc->validity_timer->timer_clock - olsr_clock_getNow());
    }
    else {
      strscpy(buf_vtime.buf, "0.0", sizeof(buf_vtime));
    }

    /* Check all networks */
    OLSR_FOR_ALL_TC_HNA_ENTRIES(tc, tmp_net, hna_iterator) {
      olsr_ip_prefix_to_string(&buf_destprefix, &tmp_net->hna_prefix);

      if (abuf_templatef(&con->out, template, values_hna, tmpl_indices, indexLength) < 0) {
          return ABUF_ERROR;
      }
    }
  }

  return CONTINUE;
}

/**
 * Callback for mid command
 */
static enum olsr_txtcommand_result
txtinfo_mid(struct comport_connection *con,
    const char *cmd __attribute__ ((unused)), const char *param __attribute__ ((unused)))
{
  struct tc_entry *tc, *tc_iterator;
  struct interface *interface, *ifp_iterator;

  const char *template;
  int indexLength;

  template = param != NULL ? parse_user_template(param) : tmpl_mid;
  if (param == NULL) {
    if (abuf_puts(&con->out, "Table: MID\nIP address\tAliases\tvtime\n") < 0) {
      return ABUF_ERROR;
    }
  }

  if ((indexLength = abuf_template_init(keys_mid, ARRAYSIZE(keys_mid),
      template, tmpl_indices, ARRAYSIZE(tmpl_indices))) < 0) {
    return ABUF_ERROR;
  }

  OLSR_FOR_ALL_INTERFACES(interface, ifp_iterator) {
    if (olsr_ipcmp(&olsr_cnf->router_id, &interface->ip_addr) != 0) {
      olsr_ip_to_string(&buf_localip, &olsr_cnf->router_id);
      olsr_ip_to_string(&buf_aliasip, &interface->ip_addr);
      strscpy(buf_vtime.buf, "0.0", sizeof(buf_vtime));

      if (abuf_templatef(&con->out, template, values_mid, tmpl_indices, indexLength) < 0) {
          return ABUF_ERROR;
      }
    }
  }

  /* MID root is the TC entry */
  OLSR_FOR_ALL_TC_ENTRIES(tc, tc_iterator) {
    struct mid_entry *alias, *alias_iterator;

    olsr_ip_to_string(&buf_localip, &tc->addr);
    if (tc->validity_timer) {
      if (tc->validity_timer) {
        olsr_clock_to_string(&buf_vtime, tc->validity_timer->timer_clock - olsr_clock_getNow());
      }
      else {
        strscpy(buf_vtime.buf, "0.0", sizeof(buf_vtime));
      }

      OLSR_FOR_ALL_TC_MID_ENTRIES(tc, alias, alias_iterator) {
        olsr_ip_to_string(&buf_aliasip, &alias->mid_alias_addr);

        if (abuf_templatef(&con->out, template, values_mid, tmpl_indices, indexLength) < 0) {
          return ABUF_ERROR;
        }
      }
    }
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

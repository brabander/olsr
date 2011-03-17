
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


#ifdef _WRS_KERNEL
#include <vxWorks.h>
#include <sockLib.h>
#include <wrn/coreip/netinet/in.h>
#else
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#endif
#include <stdio.h>

#include "olsr.h"
#include "ipcalc.h"
#include "neighbor_table.h"
#include "tc_set.h"
#include "hna_set.h"
#include "link_set.h"
#include "olsr_ip_prefix_list.h"
#include "olsr_logging.h"
#include "os_net.h"
#include "plugin_util.h"

#define PLUGIN_DESCR    "OLSRD dot draw plugin"
#define PLUGIN_AUTHOR   "Andreas Tonnesen"

#ifdef _WRS_KERNEL
static int ipc_open;
static int ipc_socket_up;
#define DOT_DRAW_PORT 2004
#endif

static int dotdraw_init(void);
static int dotdraw_enable(void);
static int dotdraw_exit(void);

static int ipc_socket;

static union olsr_ip_addr ipc_accept_ip;
static int ipc_port;

/* plugin parameters */
static const struct olsrd_plugin_parameters plugin_parameters[] = {
  {.name = "port",.set_plugin_parameter = &set_plugin_port,.data = &ipc_port},
  {.name = "accept",.set_plugin_parameter = &set_plugin_ipaddress,.data = &ipc_accept_ip},
};

OLSR_PLUGIN6(plugin_parameters) {
  .descr = PLUGIN_DESCR,
  .author = PLUGIN_AUTHOR,
  .init = dotdraw_init,
  .enable = dotdraw_enable,
  .exit = dotdraw_exit,
  .deactivate = false
};

/* Event function to register with the sceduler */
static int
  pcf_event(int, int, int, int);

static void
  ipc_action(int, void *, unsigned int);

static void
  ipc_print_neigh_link(int, const struct nbr_entry *neighbor);

static void
  ipc_print_tc_link(int, const struct tc_entry *, const struct tc_edge_entry *);

static void
  ipc_print_net(int, const union olsr_ip_addr *, const struct olsr_ip_prefix *);

static void
  ipc_send(int, const char *, int);

static void
  ipc_send_fmt(int, const char *format, ...) __attribute__ ((format(printf, 2, 3)));

#define ipc_send_str(fd, data) ipc_send((fd), (data), strlen(data))


static int
dotdraw_init(void)
{
  /* defaults for parameters */
  ipc_port = 2004;
  ipc_accept_ip.v4.s_addr = htonl(INADDR_LOOPBACK);

  ipc_socket = -1;
  return 0;
}

/**
 * destructor - called at unload
 */
static int
dotdraw_exit(void)
{
  if (ipc_socket != -1) {
    os_close(ipc_socket);
    ipc_socket = -1;
  }
  return 0;
}


static void
ipc_print_neigh_link(int ipc_connection, const struct nbr_entry *neighbor)
{
  struct ipaddr_str mainaddrstrbuf, strbuf;
  olsr_linkcost etx = 0.0;
  const char *style;
  const char *adr = olsr_ip_to_string(&mainaddrstrbuf, &olsr_cnf->router_id);
  char lqbuffer[LQTEXT_MAXLENGTH];

  if (neighbor->is_sym == 0) {  /* non SYM */
    style = "dashed";
  } else {
    const struct link_entry *lnk = get_best_link_to_neighbor_ip(&neighbor->nbr_addr);
    if (lnk) {
      etx = lnk->linkcost;
    }
    style = "solid";
  }

  ipc_send_fmt(ipc_connection,
               "\"%s\" -> \"%s\"[label=\"%s\", style=%s];\n",
               adr, olsr_ip_to_string(&strbuf, &neighbor->nbr_addr),
               olsr_get_linkcost_text(etx, false, lqbuffer, sizeof(lqbuffer)), style);

  if (neighbor->is_mpr) {
    ipc_send_fmt(ipc_connection, "\"%s\"[shape=box];\n", adr);
  }
}

static int
dotdraw_enable(void) {
  struct sockaddr_in addr;
  uint32_t yes = 1;

  if (ipc_socket != -1) {
    os_close(ipc_socket);
  }

  /* Init ipc socket */
  ipc_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (ipc_socket == -1) {
    OLSR_WARN(LOG_PLUGINS, "(DOT DRAW)IPC socket %s\n", strerror(errno));
    return 1;
  }

  if (setsockopt(ipc_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(yes)) < 0) {
    OLSR_WARN(LOG_PLUGINS, "SO_REUSEADDR failed %s\n", strerror(errno));
    os_close(ipc_socket);
    return 1;
  }
#if defined __FreeBSD__ && defined SO_NOSIGPIPE
  if (setsockopt(ipc_socket, SOL_SOCKET, SO_NOSIGPIPE, (char *)&yes, sizeof(yes)) < 0) {
    OLSR_WARN(LOG_PLUGINS, "SO_REUSEADDR failed %s\n", strerror(errno));
    os_close(ipc_socket);
    return 1;
  }
#endif

  /* Bind the socket */

  /* complete the socket structure */
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(ipc_port);

  /* bind the socket to the port number */
  if (bind(ipc_socket, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    OLSR_WARN(LOG_PLUGINS, "(DOT DRAW)IPC bind %s\n", strerror(errno));
    os_close(ipc_socket);
    return 1;
  }

  /* show that we are willing to listen */
  if (listen(ipc_socket, 1) == -1) {
    OLSR_WARN(LOG_PLUGINS, "(DOT DRAW)IPC listen %s\n", strerror(errno));
    os_close(ipc_socket);
    return 1;
  }

  /* Register socket with olsrd */
  if (NULL == olsr_socket_add(ipc_socket, &ipc_action, NULL, OLSR_SOCKET_READ)) {
    OLSR_WARN(LOG_PLUGINS, "(DOT DRAW)Could not register socket with scheduler\n");
    os_close(ipc_socket);
    return 1;
  }

  return 0;
}


static void
ipc_action(int fd __attribute__ ((unused)), void *data __attribute__ ((unused)), unsigned int flags __attribute__ ((unused)))
{
  struct sockaddr_in pin;
  socklen_t addrlen = sizeof(struct sockaddr_in);
  int ipc_connection = accept(ipc_socket, (struct sockaddr *)&pin, &addrlen);
  if (ipc_connection == -1) {
    OLSR_WARN(LOG_PLUGINS, "(DOT DRAW)IPC accept: %s\n", strerror(errno));
    return;
  }
#ifndef _WRS_KERNEL
  if (ip4cmp(&pin.sin_addr, &ipc_accept_ip.v4) != 0) {
    OLSR_WARN(LOG_PLUGINS, "Front end-connection from foreign host (%s) not allowed!\n", inet_ntoa(pin.sin_addr));
    os_close(ipc_connection);
    return;
  }
#endif
  OLSR_DEBUG(LOG_PLUGINS, "(DOT DRAW)IPC: Connection from %s\n", inet_ntoa(pin.sin_addr));
  pcf_event(ipc_connection, 1, 1, 1);
  os_close(ipc_connection);  /* close connection after one output */
}


/**
 *Scheduled event
 */
static int
pcf_event(int ipc_connection, int chgs_neighborhood, int chgs_topology, int chgs_hna)
{
  int res = 0;

  if (chgs_neighborhood || chgs_topology || chgs_hna) {
    struct nbr_entry *neighbor_table_tmp, *nbr_iterator;
    struct tc_entry *tc, *tc_iterator;
    struct hna_net *hna, *hna_iterator;
    struct ip_prefix_entry *prefix, *prefix_iterator;

    /* Print tables to IPC socket */
    ipc_send_str(ipc_connection, "digraph topology\n{\n");

    /* Neighbors */
    OLSR_FOR_ALL_NBR_ENTRIES(neighbor_table_tmp, nbr_iterator) {
      ipc_print_neigh_link(ipc_connection, neighbor_table_tmp);
    }

    /* Topology */
    OLSR_FOR_ALL_TC_ENTRIES(tc, tc_iterator) {
      struct tc_edge_entry *tc_edge, *edge_iterator;
      OLSR_FOR_ALL_TC_EDGE_ENTRIES(tc, tc_edge, edge_iterator) {
        if (tc_edge->edge_inv) {
          ipc_print_tc_link(ipc_connection, tc, tc_edge);
        }
      }
    }

    /* HNA entries */
    OLSR_FOR_ALL_TC_ENTRIES(tc, tc_iterator) {
      /* Check all networks */
      OLSR_FOR_ALL_TC_HNA_ENTRIES(tc, hna, hna_iterator) {
        ipc_print_net(ipc_connection, &tc->addr, &hna->hna_prefix);
      }
    }

    /* Local HNA entries */
    OLSR_FOR_ALL_IPPREFIX_ENTRIES(&olsr_cnf->hna_entries, prefix, prefix_iterator) {
      ipc_print_net(ipc_connection, &olsr_cnf->router_id, &prefix->net);
    }
      ipc_send_str(ipc_connection, "}\n\n");

    res = 1;
  }

  if (ipc_socket == -1) {
    dotdraw_enable();
  }
  return res;
}

static void
ipc_print_tc_link(int ipc_connection, const struct tc_entry *entry, const struct tc_edge_entry *dst_entry)
{
  struct ipaddr_str strbuf1, strbuf2;
  char lqbuffer[LQTEXT_MAXLENGTH];

  ipc_send_fmt(ipc_connection,
               "\"%s\" -> \"%s\"[label=\"%s\"];\n",
               olsr_ip_to_string(&strbuf1, &entry->addr),
               olsr_ip_to_string(&strbuf2, &dst_entry->T_dest_addr),
               olsr_get_linkcost_text(dst_entry->cost, false, lqbuffer, sizeof(lqbuffer)));
}


static void
ipc_print_net(int ipc_connection, const union olsr_ip_addr *gw, const struct olsr_ip_prefix *net)
{
  struct ipaddr_str gwbuf;
  struct ipprefix_str netbuf;

  ipc_send_fmt(ipc_connection,
               "\"%s\" -> \"%s\"[label=\"HNA\"];\n", olsr_ip_to_string(&gwbuf, gw), olsr_ip_prefix_to_string(&netbuf, net));
  ipc_send_fmt(ipc_connection, "\"%s\"[shape=diamond];\n", netbuf.buf);
}

static void
ipc_send(int ipc_connection, const char *data, int size)
{
  if (ipc_connection != -1) {
#if defined __FreeBSD__ || defined __NetBSD__ || defined __OpenBSD__ || defined __MacOSX__ || \
defined _WRS_KERNEL
#define FLAGS 0
#else
#define FLAGS MSG_NOSIGNAL
#endif
    if (send(ipc_connection, data, size, FLAGS) == -1) {
      OLSR_WARN(LOG_PLUGINS, "(DOT DRAW)IPC connection lost!\n");
      os_close(ipc_connection);
    }
  }
}

static void
ipc_send_fmt(int ipc_connection, const char *format, ...)
{
  if (ipc_connection != -1) {
    char buf[4096];
    int len;
    va_list arg;
    va_start(arg, format);
    len = vsnprintf(buf, sizeof(buf), format, arg);
    va_end(arg);
    ipc_send(ipc_connection, buf, len);
  }
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */


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

#include "olsrd_dot_draw.h"
#include "olsr.h"
#include "ipcalc.h"
#include "neighbor_table.h"
#include "tc_set.h"
#include "hna_set.h"
#include "link_set.h"
#include "olsr_ip_prefix_list.h"
#include "olsr_logging.h"

#ifdef _WRS_KERNEL
#include <vxWorks.h>
#include <sockLib.h>
#include <wrn/coreip/netinet/in.h>
#else
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#endif

#ifdef _WRS_KERNEL
static int ipc_open;
static int ipc_socket_up;
#define DOT_DRAW_PORT 2004
#endif

static int ipc_socket;

/* IPC initialization function */
static int
  plugin_ipc_init(void);

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


/**
 *Do initialization here
 *
 *This function is called by the my_init
 *function in uolsrd_plugin.c
 */
#ifdef _WRS_KERNEL
int
olsrd_dotdraw_init(void)
#else
int
olsrd_plugin_init(void)
#endif
{
  /* Initial IPC value */
  ipc_socket = -1;

  plugin_ipc_init();

  return 1;
}


/**
 * destructor - called at unload
 */
#ifdef _WRS_KERNEL
void
olsrd_dotdraw_exit(void)
#else
void
olsr_plugin_exit(void)
#endif
{
  if (ipc_socket != -1) {
    CLOSESOCKET(ipc_socket);
  }
}


static void
ipc_print_neigh_link(int ipc_connection, const struct nbr_entry *neighbor)
{
  struct ipaddr_str mainaddrstrbuf, strbuf;
  olsr_linkcost etx = 0.0;
  const char *style;
  const char *adr = olsr_ip_to_string(&mainaddrstrbuf, &olsr_cnf->router_id);
  struct lqtextbuffer lqbuffer;

  if (neighbor->status == 0) {  /* non SYM */
    style = "dashed";
  } else {
    const struct link_entry *lnk = get_best_link_to_neighbor(&neighbor->neighbor_main_addr);
    if (lnk) {
      etx = lnk->linkcost;
    }
    style = "solid";
  }

  ipc_send_fmt(ipc_connection,
               "\"%s\" -> \"%s\"[label=\"%s\", style=%s];\n",
               adr, olsr_ip_to_string(&strbuf, &neighbor->neighbor_main_addr), get_linkcost_text(etx, false, &lqbuffer), style);

  if (neighbor->is_mpr) {
    ipc_send_fmt(ipc_connection, "\"%s\"[shape=box];\n", adr);
  }
}


static int
plugin_ipc_init(void)
{
  struct sockaddr_in addr;
  uint32_t yes = 1;

  if (ipc_socket != -1) {
    CLOSESOCKET(ipc_socket);
  }

  /* Init ipc socket */
  ipc_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (ipc_socket == -1) {
    OLSR_WARN(LOG_PLUGINS, "(DOT DRAW)IPC socket %s\n", strerror(errno));
    return 0;
  }

  if (setsockopt(ipc_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(yes)) < 0) {
    OLSR_WARN(LOG_PLUGINS, "SO_REUSEADDR failed %s\n", strerror(errno));
    CLOSESOCKET(ipc_socket);
    return 0;
  }
#if defined __FreeBSD__ && defined SO_NOSIGPIPE
  if (setsockopt(ipc_socket, SOL_SOCKET, SO_NOSIGPIPE, (char *)&yes, sizeof(yes)) < 0) {
    OLSR_WARN(LOG_PLUGINS, "SO_REUSEADDR failed %s\n", strerror(errno));
    CLOSESOCKET(ipc_socket);
    return 0;
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
    CLOSESOCKET(ipc_socket);
    return 0;
  }

  /* show that we are willing to listen */
  if (listen(ipc_socket, 1) == -1) {
    OLSR_WARN(LOG_PLUGINS, "(DOT DRAW)IPC listen %s\n", strerror(errno));
    CLOSESOCKET(ipc_socket);
    return 0;
  }

  /* Register with olsrd */
  add_olsr_socket(ipc_socket, &ipc_action, NULL, NULL, SP_PR_READ);

  return 1;
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
    CLOSESOCKET(ipc_connection);
    return;
  }
#endif
  OLSR_DEBUG(LOG_PLUGINS, "(DOT DRAW)IPC: Connection from %s\n", inet_ntoa(pin.sin_addr));
  pcf_event(ipc_connection, 1, 1, 1);
  CLOSESOCKET(ipc_connection);  /* close connection after one output */
}


/**
 *Scheduled event
 */
static int
pcf_event(int ipc_connection, int chgs_neighborhood, int chgs_topology, int chgs_hna)
{
  int res = 0;

  if (chgs_neighborhood || chgs_topology || chgs_hna) {
    struct nbr_entry *neighbor_table_tmp;
    struct tc_entry *tc;
    struct ip_prefix_entry *hna;

    /* Print tables to IPC socket */
    ipc_send_str(ipc_connection, "digraph topology\n{\n");

    /* Neighbors */
    OLSR_FOR_ALL_NBR_ENTRIES(neighbor_table_tmp) {
      ipc_print_neigh_link(ipc_connection, neighbor_table_tmp);
    } OLSR_FOR_ALL_NBR_ENTRIES_END(neighbor_table_tmp);

    /* Topology */
    OLSR_FOR_ALL_TC_ENTRIES(tc) {
      struct tc_edge_entry *tc_edge;
      OLSR_FOR_ALL_TC_EDGE_ENTRIES(tc, tc_edge) {
        if (tc_edge->edge_inv) {
          ipc_print_tc_link(ipc_connection, tc, tc_edge);
        }
      }
      OLSR_FOR_ALL_TC_EDGE_ENTRIES_END(tc, tc_edge);
    }
    OLSR_FOR_ALL_TC_ENTRIES_END(tc);

    /* HNA entries */
    OLSR_FOR_ALL_TC_ENTRIES(tc) {
      /* Check all networks */
      struct hna_net *tmp_net;
      OLSR_FOR_ALL_TC_HNA_ENTRIES(tc, tmp_net) {
        ipc_print_net(ipc_connection, &tc->addr, &tmp_net->hna_prefix);
      }
    } OLSR_FOR_ALL_TC_HNA_ENTRIES_END(tc, tmp_hna);
    OLSR_FOR_ALL_TC_ENTRIES_END(tc);

    /* Local HNA entries */
    OLSR_FOR_ALL_IPPREFIX_ENTRIES(&olsr_cnf->hna_entries, hna) {
      ipc_print_net(ipc_connection, &olsr_cnf->router_id, &hna->net);
    } OLSR_FOR_ALL_IPPREFIX_ENTRIES_END()
      ipc_send_str(ipc_connection, "}\n\n");

    res = 1;
  }

  if (ipc_socket == -1) {
    plugin_ipc_init();
  }
  return res;
}

static void
ipc_print_tc_link(int ipc_connection, const struct tc_entry *entry, const struct tc_edge_entry *dst_entry)
{
  struct ipaddr_str strbuf1, strbuf2;
  struct lqtextbuffer lqbuffer;

  ipc_send_fmt(ipc_connection,
               "\"%s\" -> \"%s\"[label=\"%s\"];\n",
               olsr_ip_to_string(&strbuf1, &entry->addr),
               olsr_ip_to_string(&strbuf2, &dst_entry->T_dest_addr), get_linkcost_text(dst_entry->cost, false, &lqbuffer));
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
      CLOSESOCKET(ipc_connection);
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

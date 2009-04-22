
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
#include "olsrd_txtinfo.h"
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

#include "common/autobuf.h"

#include <unistd.h>
#include <errno.h>

#ifdef WIN32
#undef EWOULDBLOCK
#undef EAGAIN
#define EWOULDBLOCK WSAEWOULDBLOCK
#define EAGAIN WSAEWOULDBLOCK
#undef errno
#define errno WSAGetLastError()
#undef SHUT_WR
#define SHUT_WR SD_SEND
#undef strerror
#define strerror(x) StrError(x)
#define read(fd,buf,size) recv((fd), (buf), (size), 0)
#define write(fd,buf,size) send((fd), (buf), (size), 0)
#endif


struct ipc_conn {
  struct autobuf resp;
  int respstart;
  int requlen;
  char requ[256];
  char csv;
};

static int listen_socket = -1;


static void conn_destroy(struct ipc_conn *);

static void ipc_action(int, void *, unsigned int);

static void ipc_http(int, void *, unsigned int);

static void ipc_http_read(int, struct ipc_conn *);

static void ipc_http_read_dummy(int, struct ipc_conn *);

static void ipc_http_write(int, struct ipc_conn *);

static int send_info(struct ipc_conn *, int);

static int ipc_print_neigh(struct ipc_conn *);

static int ipc_print_link(struct ipc_conn *);

static int ipc_print_routes(struct ipc_conn *);

static int ipc_print_topology(struct ipc_conn *);

static int ipc_print_hna_entry(struct autobuf *, const struct olsr_ip_prefix *, const union olsr_ip_addr *, char csv);
static int ipc_print_hna(struct ipc_conn *);

static int ipc_print_mid(struct ipc_conn *);

static int ipc_print_stat(struct ipc_conn *);

static void update_statistics_ptr(void *);
static bool olsr_msg_statistics(union olsr_message *msg, struct interface *input_if, union olsr_ip_addr *from_addr);
static char *olsr_packet_statistics(char *packet, struct interface *interface, union olsr_ip_addr *, int *length);
static void update_statistics_ptr(void *data __attribute__ ((unused)));

#define isprefix(str, pre) (strncmp((str), (pre), strlen(pre)) == 0)

#define SIW_NEIGH	  (1 << 0)
#define SIW_LINK	  (1 << 1)
#define SIW_ROUTE	  (1 << 2)
#define SIW_HNA		  (1 << 3)
#define SIW_MID		  (1 << 4)
#define SIW_TOPO	  (1 << 5)
#define SIW_STAT	  (1 << 6)
#define SIW_COOKIES	  (1 << 7)
#define SIW_CSV		  (1 << 8)
#define SIW_ALL		  (SIW_NEIGH|SIW_LINK|SIW_ROUTE|SIW_HNA|SIW_MID|SIW_TOPO)

/* variables for statistics */
static uint32_t recv_packets[60], recv_messages[60][6];
static uint32_t recv_last_relevantTCs;
struct olsr_cookie_info *statistics_timer = NULL;

/**
 * destructor - called at unload
 */
void
olsr_plugin_exit(void)
{
  olsr_parser_remove_function(&olsr_msg_statistics, PROMISCUOUS);
  olsr_preprocessor_remove_function(&olsr_packet_statistics);
  CLOSESOCKET(listen_socket);
}

/**
 *Do initialization here
 *
 *This function is called by the my_init
 *function in uolsrd_plugin.c
 */
int
olsrd_plugin_init(void)
{
  struct sockaddr_storage sst;
  uint32_t yes = 1;
  socklen_t addrlen;

  statistics_timer = olsr_alloc_cookie("Txtinfo statistics timer", OLSR_COOKIE_TYPE_TIMER);
  olsr_start_timer(1000, 0, true, &update_statistics_ptr, NULL, statistics_timer->ci_id);

  /* Init ipc socket */
  listen_socket = socket(olsr_cnf->ip_version, SOCK_STREAM, 0);
  if (listen_socket == -1) {
    OLSR_WARN(LOG_PLUGINS, "(TXTINFO) socket()=%s\n", strerror(errno));
    return 0;
  }
  if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(yes)) < 0) {
    OLSR_WARN(LOG_PLUGINS, "(TXTINFO) setsockopt()=%s\n", strerror(errno));
    CLOSESOCKET(listen_socket);
    return 0;
  }
#if defined __FreeBSD__ && defined SO_NOSIGPIPE
  if (setsockopt(listen_socket, SOL_SOCKET, SO_NOSIGPIPE, (char *)&yes, sizeof(yes)) < 0) {
    OLSR_WARN(LOG_PLUGINS, "(TXTINFO) reusing address failed: %s", strerror(errno));
    CLOSESOCKET(listen_socket);
    return 0;
  }
#endif
  /* Bind the socket */

  /* complete the socket structure */
  memset(&sst, 0, sizeof(sst));
  if (olsr_cnf->ip_version == AF_INET) {
    struct sockaddr_in *addr4 = (struct sockaddr_in *)&sst;
    addr4->sin_family = AF_INET;
    addrlen = sizeof(*addr4);
#ifdef SIN6_LEN
    addr4->sin_len = addrlen;
#endif
    addr4->sin_addr.s_addr = INADDR_ANY;
    addr4->sin_port = htons(ipc_port);
  } else {
    struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&sst;
    addr6->sin6_family = AF_INET6;
    addrlen = sizeof(*addr6);
#ifdef SIN6_LEN
    addr6->sin6_len = addrlen;
#endif
    addr6->sin6_addr = in6addr_any;
    addr6->sin6_port = htons(ipc_port);
  }

  /* bind the socket to the port number */
  if (bind(listen_socket, (struct sockaddr *)&sst, addrlen) == -1) {
    OLSR_WARN(LOG_PLUGINS, "(TXTINFO) bind()=%s\n", strerror(errno));
    CLOSESOCKET(listen_socket);
    return 0;
  }

  /* show that we are willing to listen */
  if (listen(listen_socket, 1) == -1) {
    OLSR_WARN(LOG_PLUGINS, "(TXTINFO) listen()=%s\n", strerror(errno));
    CLOSESOCKET(listen_socket);
    return 0;
  }

  /* Register with olsrd */
  add_olsr_socket(listen_socket, NULL, &ipc_action, NULL, SP_IMM_READ);

  OLSR_INFO(LOG_PLUGINS, "(TXTINFO) listening on port %d\n", ipc_port);

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

/* destroy the connection */
static void
conn_destroy(struct ipc_conn *conn)
{
  abuf_free(&conn->resp);
  free(conn);
}

static void
kill_connection(int fd, struct ipc_conn *conn)
{
  remove_olsr_socket(fd, NULL, &ipc_http);
  CLOSESOCKET(fd);
  conn_destroy(conn);
}

static void
ipc_action(int fd, void *data __attribute__ ((unused)), unsigned int flags __attribute__ ((unused)))
{
  struct ipc_conn *conn;
  struct sockaddr_storage pin;
#if !defined REMOVE_LOG_DEBUG || !defined REMOVE_LOG_WARN
  struct ipaddr_str buf;
#endif
  socklen_t addrlen = sizeof(pin);
  int http_connection;
  union olsr_ip_addr *ipaddr;

  http_connection = accept(fd, (struct sockaddr *)&pin, &addrlen);
  if (http_connection == -1) {
    /* this may well happen if the other side immediately closes the connection. */
    OLSR_WARN(LOG_PLUGINS, "(TXTINFO) accept()=%s\n", strerror(errno));
    return;
  }

  /* check if we want ot speak with it */
  if (olsr_cnf->ip_version == AF_INET) {
    struct sockaddr_in *addr4 = (struct sockaddr_in *)&pin;
    ipaddr = (union olsr_ip_addr *)&addr4->sin_addr;
  } else {
    struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&pin;
    ipaddr = (union olsr_ip_addr *)&addr6->sin6_addr;
  }

  if (!ip_acl_acceptable(&allowed_nets, ipaddr, olsr_cnf->ip_version)) {
    OLSR_WARN(LOG_PLUGINS, "(TXTINFO) From host(%s) not allowed!\n", olsr_ip_to_string(&buf, ipaddr));
    CLOSESOCKET(http_connection);
    return;
  }
  OLSR_DEBUG(LOG_PLUGINS, "(TXTINFO) Connect from %s\n", olsr_ip_to_string(&buf, ipaddr));

  /* make the fd non-blocking */
  if (set_nonblocking(http_connection) < 0) {
    CLOSESOCKET(http_connection);
    return;
  }

  conn = olsr_malloc(sizeof(*conn), "Connection object for ");
  conn->requlen = 0;
  *conn->requ = '\0';
  conn->respstart = 0;
  abuf_init(&conn->resp, 1000);
  add_olsr_socket(http_connection, NULL, &ipc_http, conn, SP_IMM_READ);
}

static void
ipc_http_read_dummy(int fd, struct ipc_conn *conn)
{
  /* just read dummy stuff */
  ssize_t bytes_read;
  char dummybuf[128];
  do {
    bytes_read = read(fd, dummybuf, sizeof(dummybuf));
  } while (bytes_read > 0);
  if (bytes_read == 0) {
    /* EOF */
    if (conn->respstart < conn->resp.len && conn->resp.len > 0) {
      disable_olsr_socket(fd, NULL, &ipc_http, SP_IMM_READ);
      conn->requlen = -1;
      return;
    }
  } else if (errno == EINTR || errno == EAGAIN) {
    /* ignore interrupted sys-calls */
    return;
  }
  /* we had either an error or EOF and we are completely done */
  kill_connection(fd, conn);
}

static void
ipc_http_read(int fd, struct ipc_conn *conn)
{
  int send_what = 0;
  const char *p;
  ssize_t bytes_read = read(fd, conn->requ + conn->requlen, sizeof(conn->requ) - conn->requlen - 1);    /* leave space for a terminating '\0' */
  if (bytes_read < 0) {
    if (errno == EINTR || errno == EAGAIN) {
      return;
    }
    OLSR_WARN(LOG_PLUGINS, "(TXTINFO) read error: %s", strerror(errno));
    kill_connection(fd, conn);
    return;
  }
  conn->requlen += bytes_read;
  conn->requ[conn->requlen] = '\0';
  conn->csv = 0;

  /* look if we have the necessary info. We get here somethign like "GET /path HTTP/1.0" */
  p = strchr(conn->requ, '/');
  if (p == NULL) {
    /* input buffer full ? */
    if ((sizeof(conn->requ) - conn->requlen - 1) == 0) {
      kill_connection(fd, conn);
      return;
    }
    /* we didn't get all. Wait for more data. */
    return;
  }
  while (p != NULL) {
    if (isprefix(p, "/neighbours"))
      send_what = send_what | SIW_LINK | SIW_NEIGH;
    else if (isprefix(p, "/neigh"))
      send_what = send_what | SIW_NEIGH;
    else if (isprefix(p, "/link"))
      send_what = send_what | SIW_LINK;
    else if (isprefix(p, "/route"))
      send_what = send_what | SIW_ROUTE;
    else if (isprefix(p, "/hna"))
      send_what = send_what | SIW_HNA;
    else if (isprefix(p, "/mid"))
      send_what = send_what | SIW_MID;
    else if (isprefix(p, "/topo"))
      send_what = send_what | SIW_TOPO;
    else if (isprefix(p, "/stat"))
      send_what = send_what | SIW_STAT;
    else if (isprefix(p, "/cookies"))
      send_what = send_what | SIW_COOKIES;
    else if (isprefix(p, "/csv"))
      send_what = send_what | SIW_CSV;
    p = strchr(++p, '/');
  }
  if (send_what == 0) {
    send_what = SIW_ALL;
  } else if (send_what == SIW_CSV) {
    send_what = SIW_ALL | SIW_CSV;
  }

  if (send_info(conn, send_what) < 0) {
    kill_connection(fd, conn);
    return;
  }
  enable_olsr_socket(fd, NULL, &ipc_http, SP_IMM_WRITE);
}

static void
ipc_http_write(int fd, struct ipc_conn *conn)
{
  ssize_t bytes_written = write(fd, conn->resp.buf + conn->respstart,
                                conn->resp.len - conn->respstart);
  if (bytes_written < 0) {
    if (errno == EINTR || errno == EAGAIN) {
      return;
    }
    OLSR_WARN(LOG_PLUGINS, "(TXTINFO) write error: %s", strerror(errno));
    kill_connection(fd, conn);
    return;
  }
  conn->respstart += bytes_written;
  if (conn->respstart >= conn->resp.len) {
    /* we are done. */
    if (conn->requlen < 0) {
      /* we are completely done. */
      kill_connection(fd, conn);
    } else if (shutdown(fd, SHUT_WR) < 0) {
      kill_connection(fd, conn);
    } else {
      disable_olsr_socket(fd, NULL, &ipc_http, SP_IMM_WRITE);
    }
  }
}

static void
ipc_http(int fd, void *data, unsigned int flags)
{
  struct ipc_conn *conn = data;
  if ((flags & SP_IMM_READ) != 0) {
    if (conn->resp.len > 0) {
      ipc_http_read_dummy(fd, conn);
    } else {
      ipc_http_read(fd, conn);
    }
  }
  if ((flags & SP_IMM_WRITE) != 0) {
    ipc_http_write(fd, conn);
  }
}


static int
ipc_print_neigh(struct ipc_conn *conn)
{
  struct neighbor_entry *neigh;

  if (!conn->csv) {
    if (abuf_appendf(&conn->resp, "Table: Neighbors\nIP address\tSYM\tMPR\tMPRS\tWill.\t2 Hop Neighbors\n") < 0) {
      return -1;
    }
  }

  /* Neighbors */
  OLSR_FOR_ALL_NBR_ENTRIES(neigh) {
    struct neighbor_2_list_entry *list_2;
    struct ipaddr_str buf1;
    int thop_cnt = 0;
    for (list_2 = neigh->neighbor_2_list.next; list_2 != &neigh->neighbor_2_list; list_2 = list_2->next) {
      thop_cnt++;
    }
    if (!conn->csv) {
      if (abuf_appendf(&conn->resp,
                       "%s\t%s\t%s\t%s\t%d\t%d\n",
                       olsr_ip_to_string(&buf1, &neigh->neighbor_main_addr),
                       neigh->status == SYM ? "YES" : "NO",
                       neigh->is_mpr ? "YES" : "NO",
                       olsr_lookup_mprs_set(&neigh->neighbor_main_addr) ? "YES" : "NO", neigh->willingness, thop_cnt) < 0) {
        return -1;
      }
    } else {
      if (abuf_appendf(&conn->resp,
                       "neigh,%s,%s,%s,%s,%d,%d\n",
                       olsr_ip_to_string(&buf1, &neigh->neighbor_main_addr),
                       neigh->status == SYM ? "YES" : "NO",
                       neigh->is_mpr ? "YES" : "NO",
                       olsr_lookup_mprs_set(&neigh->neighbor_main_addr) ? "YES" : "NO", neigh->willingness, thop_cnt) < 0) {
        return -1;
      }
    }
  }
  OLSR_FOR_ALL_NBR_ENTRIES_END(neigh);

  if (!conn->csv) {
    if (abuf_appendf(&conn->resp, "\n") < 0) {
      return -1;
    }
  }
  return 0;
}

static int
ipc_print_link(struct ipc_conn *conn)
{
  struct link_entry *lnk;

  if (!conn->csv) {
    if (abuf_appendf(&conn->resp, "Table: Links\nLocal IP\tRemote IP\tLQ\tNLQ\tCost\n") < 0) {
      return -1;
    }
  }

  /* Link set */
  OLSR_FOR_ALL_LINK_ENTRIES(lnk) {
    struct ipaddr_str buf1, buf2;
    struct lqtextbuffer lqbuffer1, lqbuffer2;
    if (!conn->csv) {
      if (abuf_appendf(&conn->resp,
                       "%s\t%s\t%s\t%s\t\n",
                       olsr_ip_to_string(&buf1, &lnk->local_iface_addr),
                       olsr_ip_to_string(&buf2, &lnk->neighbor_iface_addr),
                       get_link_entry_text(lnk, '\t', &lqbuffer1), get_linkcost_text(lnk->linkcost, false, &lqbuffer2)) < 0) {
        return -1;
      }
    } else {
      if (abuf_appendf(&conn->resp,
                       "link,%s,%s,%s,%s\n",
                       olsr_ip_to_string(&buf1, &lnk->local_iface_addr),
                       olsr_ip_to_string(&buf2, &lnk->neighbor_iface_addr),
                       get_link_entry_text(lnk, ',', &lqbuffer1), get_linkcost_text(lnk->linkcost, false, &lqbuffer2)) < 0) {
        return -1;
      }
    }
  }
  OLSR_FOR_ALL_LINK_ENTRIES_END(lnk);

  if (!conn->csv) {
    if (abuf_appendf(&conn->resp, "\n") < 0) {
      return -1;
    }
  }
  return 0;
}

static int
ipc_print_routes(struct ipc_conn *conn)
{
  struct rt_entry *rt;

  if (!conn->csv) {
    if (abuf_appendf(&conn->resp, "Table: Routes\nDestination\tGateway IP\tMetric\tETX\tInterface\n") < 0) {
      return -1;
    }
  }

  /* Walk the route table */
  OLSR_FOR_ALL_RT_ENTRIES(rt) {
    struct ipaddr_str buf;
    struct ipprefix_str prefixstr;
    struct lqtextbuffer lqbuffer;
    if (!conn->csv) {
      if (abuf_appendf(&conn->resp,
                       "%s\t%s\t%u\t%s\t%s\t\n",
                       olsr_ip_prefix_to_string(&prefixstr, &rt->rt_dst),
                       olsr_ip_to_string(&buf, &rt->rt_best->rtp_nexthop.gateway),
                       rt->rt_best->rtp_metric.hops,
                       get_linkcost_text(rt->rt_best->rtp_metric.cost, true, &lqbuffer),
                       rt->rt_best->rtp_nexthop.interface ? rt->rt_best->rtp_nexthop.interface->int_name : "[null]") < 0) {
        return -1;
      }
    } else {
      if (abuf_appendf(&conn->resp,
                       "route,%s,%s,%u,%s,%s\n",
                       olsr_ip_prefix_to_string(&prefixstr, &rt->rt_dst),
                       olsr_ip_to_string(&buf, &rt->rt_best->rtp_nexthop.gateway),
                       rt->rt_best->rtp_metric.hops,
                       get_linkcost_text(rt->rt_best->rtp_metric.cost, true, &lqbuffer),
                       rt->rt_best->rtp_nexthop.interface ? rt->rt_best->rtp_nexthop.interface->int_name : "[null]") < 0) {
        return -1;
      }
    }
  }
  OLSR_FOR_ALL_RT_ENTRIES_END(rt);

  if (!conn->csv) {
    if (abuf_appendf(&conn->resp, "\n") < 0) {
      return -1;
    }
  }
  return 0;
}

static int
ipc_print_topology(struct ipc_conn *conn)
{
  struct tc_entry *tc;

  if (!conn->csv) {
    if (abuf_appendf(&conn->resp, "Table: Topology\nDest. IP\tLast hop IP\tLQ\tNLQ\tCost\n") < 0) {
      return -1;
    }
  }

  /* Topology */
  OLSR_FOR_ALL_TC_ENTRIES(tc) {
    struct tc_edge_entry *tc_edge;
    OLSR_FOR_ALL_TC_EDGE_ENTRIES(tc, tc_edge) {
      if (tc_edge->edge_inv) {
        struct ipaddr_str dstbuf, addrbuf;
        struct lqtextbuffer lqbuffer1, lqbuffer2;
        if (!conn->csv) {
          if (abuf_appendf(&conn->resp,
                           "%s\t%s\t%s\t%s\n",
                           olsr_ip_to_string(&dstbuf, &tc_edge->T_dest_addr),
                           olsr_ip_to_string(&addrbuf, &tc->addr),
                           get_tc_edge_entry_text(tc_edge, '\t', &lqbuffer1),
                           get_linkcost_text(tc_edge->cost, false, &lqbuffer2)) < 0) {
            return -1;
          }
        } else {
          if (abuf_appendf(&conn->resp,
                           "topo,%s,%s,%s,%s\n",
                           olsr_ip_to_string(&dstbuf, &tc_edge->T_dest_addr),
                           olsr_ip_to_string(&addrbuf, &tc->addr),
                           get_tc_edge_entry_text(tc_edge, ',', &lqbuffer1),
                           get_linkcost_text(tc_edge->cost, false, &lqbuffer2)) < 0) {
            return -1;
          }
        }
      }
    }
    OLSR_FOR_ALL_TC_EDGE_ENTRIES_END(tc, tc_edge);
  }
  OLSR_FOR_ALL_TC_ENTRIES_END(tc);

  if (!conn->csv) {
    if (abuf_appendf(&conn->resp, "\n") < 0) {
      return -1;
    }
  }
  return 0;
}

static int
ipc_print_hna_entry(struct autobuf *autobuf, const struct olsr_ip_prefix *hna_prefix, const union olsr_ip_addr *ipaddr, char csv)
{
  struct ipaddr_str mainaddrbuf;
  struct ipprefix_str addrbuf;
  if (!csv) {
    return abuf_appendf(autobuf,
                        "%s\t%s\n", olsr_ip_prefix_to_string(&addrbuf, hna_prefix), olsr_ip_to_string(&mainaddrbuf, ipaddr));
  } else {
    return abuf_appendf(autobuf,
                        "hna,%s,%s\n", olsr_ip_prefix_to_string(&addrbuf, hna_prefix), olsr_ip_to_string(&mainaddrbuf, ipaddr));
  }
}

static int
ipc_print_hna(struct ipc_conn *conn)
{
  const struct ip_prefix_entry *hna;
  struct tc_entry *tc;

  if (!conn->csv) {
    if (abuf_appendf(&conn->resp, "Table: HNA\nDestination\tGateway\n") < 0) {
      return -1;
    }
  }

  /* Announced HNA entries */
  OLSR_FOR_ALL_IPPREFIX_ENTRIES(&olsr_cnf->hna_entries, hna) {
    if (ipc_print_hna_entry(&conn->resp, &hna->net, &olsr_cnf->router_id, conn->csv) < 0) {
      return -1;
    }
  }
  OLSR_FOR_ALL_IPPREFIX_ENTRIES_END()

    /* HNA entries */
    OLSR_FOR_ALL_TC_ENTRIES(tc) {
    struct hna_net *tmp_net;
    /* Check all networks */
    OLSR_FOR_ALL_TC_HNA_ENTRIES(tc, tmp_net) {
      if (ipc_print_hna_entry(&conn->resp, &tmp_net->hna_prefix, &tc->addr, conn->csv) < 0) {
        return -1;
      }
    }
    OLSR_FOR_ALL_TC_HNA_ENTRIES_END(tc, tmp_net);
  }
  OLSR_FOR_ALL_TC_ENTRIES_END(tc);

  if (!conn->csv) {
    if (abuf_appendf(&conn->resp, "\n") < 0) {
      return -1;
    }
  }
  return 0;
}

static int
ipc_print_mid(struct ipc_conn *conn)
{
  struct tc_entry *tc;

  if (!conn->csv) {
    if (abuf_appendf(&conn->resp, "Table: MID\nIP address\tAliases\n") < 0) {
      return -1;
    }
  }

  /* MID root is the TC entry */
  OLSR_FOR_ALL_TC_ENTRIES(tc) {
    struct ipaddr_str buf;
    struct mid_entry *alias;
    char sep = '\t';
    if (conn->csv) {
      sep = ',';
    }

    if (!conn->csv) {
      if (abuf_puts(&conn->resp, olsr_ip_to_string(&buf, &tc->addr)) < 0) {
        return -1;
      }
    } else {
      if (abuf_appendf(&conn->resp, "mid,%s", olsr_ip_to_string(&buf, &tc->addr)) < 0) {
        return -1;
      }
    }

    OLSR_FOR_ALL_TC_MID_ENTRIES(tc, alias) {
      if (abuf_appendf(&conn->resp, "%c%s", sep, olsr_ip_to_string(&buf, &alias->mid_alias_addr)) < 0) {
        return -1;
      }
      if (!conn->csv) {
        sep = ';';
      } else {
        sep = ',';
      }
    }
    OLSR_FOR_ALL_TC_MID_ENTRIES_END(tc, alias);
    if (abuf_appendf(&conn->resp, "\n") < 0) {
      return -1;
    }
  }
  OLSR_FOR_ALL_TC_ENTRIES_END(tc);
  if (!conn->csv) {
    if (abuf_appendf(&conn->resp, "\n") < 0) {
      return -1;
    }
  }
  return 0;
}

static int
ipc_print_stat(struct ipc_conn *conn)
{
  static const char *names[] = { "HELLO", "TC", "MID", "HNA", "Other", "Rel.TCs" };

  uint32_t msgs[6], traffic, i, j;
  uint32_t slot = (now_times / 1000 + 59) % 60;

  if (!conn->csv) {
    if (abuf_appendf(&conn->resp, "Table: Statistics (without duplicates)\nType\tlast seconds\t\t\t\tlast min.\taverage\n") < 0) {
      return -1;
    }
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
    if (!conn->csv) {
      if (abuf_appendf(&conn->resp, "%s\t%u\t%u\t%u\t%u\t%u\t%u\t\t%u\n", names[i],
                       recv_messages[(slot) % 60][i],
                       recv_messages[(slot + 59) % 60][i],
                       recv_messages[(slot + 58) % 60][i],
                       recv_messages[(slot + 57) % 60][i], recv_messages[(slot + 56) % 60][i], msgs[i], msgs[i] / 60) < 0) {
        return -1;
      }
    } else {
      if (abuf_appendf(&conn->resp, "stat,%s,%u,%u,%u,%u,%u,%u,%u\n", names[i],
                       recv_messages[(slot) % 60][i],
                       recv_messages[(slot + 59) % 60][i],
                       recv_messages[(slot + 58) % 60][i],
                       recv_messages[(slot + 57) % 60][i], recv_messages[(slot + 56) % 60][i], msgs[i], msgs[i] / 60) < 0) {
        return -1;
      }
    }
  }
  if (!conn->csv) {
    if (abuf_appendf(&conn->resp, "\nTraffic: %8u bytes/s\t%u bytes/minute\taverage %u bytes/s\n",
                     recv_packets[(slot) % 60], traffic, traffic / 60) < 0) {
      return -1;
    }
  } else {
    if (abuf_appendf(&conn->resp, "stat,Traffic,%u,%u,%u\n", recv_packets[(slot) % 60], traffic, traffic / 60) < 0) {
      return -1;
    }
  }

  if (!conn->csv) {
    if (abuf_appendf(&conn->resp, "\n") < 0) {
      return -1;
    }
  }
  return 0;
}

static int
ipc_print_cookies(struct ipc_conn *conn)
{
  int i;

  if (!conn->csv) {
    if (abuf_appendf(&conn->resp, "Memory cookies:\n") < 0) {
      return -1;
    }
  }

  for (i = 1; i < COOKIE_ID_MAX; i++) {
    struct olsr_cookie_info *c = olsr_cookie_get(i);
    if (c == NULL || c->ci_type != OLSR_COOKIE_TYPE_MEMORY) {
      continue;
    }
    if (!conn->csv) {
      if (abuf_appendf(&conn->resp, "%-25s ", c->ci_name == NULL ? "Unknown" : c->ci_name) < 0) {
        return -1;
      }
      if (abuf_appendf(&conn->resp, "(MEMORY) size: %lu usage: %u freelist: %u\n",
                       (unsigned long)c->ci_size, c->ci_usage, c->ci_free_list_usage) < 0) {
        return -1;
      }
    } else {
      if (abuf_appendf(&conn->resp, "mem_cookie,%s,%lu,%u,%u\n", c->ci_name == NULL ? "Unknown" : c->ci_name,
                       (unsigned long)c->ci_size, c->ci_usage, c->ci_free_list_usage) < 0) {
        return -1;
      }
    }
  }

  if (!conn->csv) {
    if (abuf_appendf(&conn->resp, "\nTimer cookies:\n") < 0) {
      return -1;
    }
  }

  for (i = 1; i < COOKIE_ID_MAX; i++) {
    struct olsr_cookie_info *c = olsr_cookie_get(i);
    if (c == NULL || c->ci_type != OLSR_COOKIE_TYPE_TIMER) {
      continue;
    }
    if (!conn->csv) {
      if (abuf_appendf(&conn->resp, "%-25s ", c->ci_name == NULL ? "Unknown" : c->ci_name) < 0) {
        return -1;
      }
      if (abuf_appendf(&conn->resp, "(TIMER) usage: %u changes: %u\n", c->ci_usage, c->ci_changes) < 0) {
        return -1;
      }
    } else {
      if (abuf_appendf(&conn->resp, "tmr_cookie,%s,%u,%u\n", c->ci_name == NULL ? "Unknown" : c->ci_name,
                       c->ci_usage, c->ci_changes) < 0) {
        return -1;
      }
    }
  }

  if (!conn->csv) {
    if (abuf_appendf(&conn->resp, "\n") < 0) {
      return -1;
    }
  }
  return 0;
}

static int
send_info(struct ipc_conn *conn, int send_what)
{
  int rv;

  /* comma separated values output format */
  if ((send_what & SIW_CSV) != 0) {
    conn->csv = 1;
  }

  /* Print minimal http header */
  if (!conn->csv) {
    if (abuf_appendf(&conn->resp, "HTTP/1.0 200 OK\n" "Content-type: text/plain\n\n") < 0) {
      return -1;
    }
  }

  /* Print tables to IPC socket */

  rv = 0;
  /* links + Neighbors */
  if ((send_what & SIW_LINK) != 0 && ipc_print_link(conn) < 0) {
    rv = -1;
  }
  if ((send_what & SIW_NEIGH) != 0 && ipc_print_neigh(conn) < 0) {
    rv = -1;
  }
  /* topology */
  if ((send_what & SIW_TOPO) != 0) {
    rv = ipc_print_topology(conn);
  }
  /* hna */
  if ((send_what & SIW_HNA) != 0) {
    rv = ipc_print_hna(conn);
  }
  /* mid */
  if ((send_what & SIW_MID) != 0) {
    rv = ipc_print_mid(conn);
  }
  /* routes */
  if ((send_what & SIW_ROUTE) != 0) {
    rv = ipc_print_routes(conn);
  }
  /* statistics */
  if ((send_what & SIW_STAT) != 0) {
    rv = ipc_print_stat(conn);
  }
  /* cookies */
  if ((send_what & SIW_COOKIES) != 0) {
    rv = ipc_print_cookies(conn);
  }
  return rv;
}

/*
 * Local Variables:
 * mode: c
 * style: linux
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */

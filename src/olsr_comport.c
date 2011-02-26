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
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#ifdef WIN32
#include <io.h>
#else
#include <netdb.h>
#endif

#include "defs.h"
#include "olsr_logging.h"
#include "olsr_cfg.h"
#include "olsr_timer.h"
#include "olsr_socket.h"
#include "olsr_memcookie.h"
#include "common/autobuf.h"
#include "common/avl.h"
#include "common/list.h"
#include "ipcalc.h"
#include "olsr.h"
#include "olsr_comport_http.h"
#include "olsr_comport_txt.h"
#include "olsr_comport.h"
#include "os_net.h"

#define HTTP_TIMEOUT  30000
#define TXT_TIMEOUT 60000

#define COMPORT_MAX_INPUTBUFFER 65536

#define OLSR_FOR_ALL_COMPORT_ENTRIES(comport, iterator) list_for_each_element_safe(&olsr_comport_head, comport, node, iterator)

struct list_entity olsr_comport_head;

/* server socket */
static struct olsr_socket_entry *comsocket_http;
static struct olsr_socket_entry *comsocket_txt;

static struct olsr_memcookie_info *connection_cookie;
static struct olsr_timer_info *connection_timeout;

/* counter for open connections */
static int connection_http_count;
static int connection_txt_count;

static int olsr_com_openport(int port);

static void olsr_com_parse_request(int fd, void *data, unsigned int flags);
static void olsr_com_parse_connection(int fd, void *data, unsigned int flags);
static void olsr_com_cleanup_session(struct comport_connection *con);

static void olsr_com_timeout_handler(void *);

void
olsr_com_init(void) {
  int sock_http, sock_txt;

  connection_cookie =
      olsr_memcookie_add("comport connections", sizeof(struct comport_connection));

  connection_timeout = olsr_timer_add("comport timout",
      &olsr_com_timeout_handler, false);

  connection_http_count = 0;
  connection_txt_count = 0;

  list_init_head(&olsr_comport_head);

  olsr_com_init_http();
  olsr_com_init_txt();

  if (olsr_cnf->comport_http > 0) {
    sock_http = olsr_com_openport(olsr_cnf->comport_http);

    if (NULL == (comsocket_http =
        olsr_socket_add(sock_http, &olsr_com_parse_request, NULL, OLSR_SOCKET_READ))) {
      OLSR_ERROR(LOG_COMPORT, "Cannot http-register socket with scheduler");
      olsr_exit(1);
    }
  }
  if (olsr_cnf->comport_txt > 0) {
    sock_txt = olsr_com_openport(olsr_cnf->comport_txt);

    if (NULL == (comsocket_txt =
        olsr_socket_add(sock_txt, &olsr_com_parse_request, NULL, OLSR_SOCKET_READ))) {
      OLSR_ERROR(LOG_COMPORT, "Cannot register txt-socket with scheduler");
      olsr_exit(1);
    }
  }
}

void
olsr_com_destroy(void) {
  struct comport_connection *con, *iterator;
  OLSR_FOR_ALL_COMPORT_ENTRIES(con, iterator) {
    olsr_com_cleanup_session(con);
  }

  olsr_com_destroy_http();
  olsr_com_destroy_txt();
}

void
olsr_com_activate_output(struct comport_connection *con) {
  olsr_socket_enable(con->sock, OLSR_SOCKET_WRITE);
}

static int
olsr_com_openport(int port) {
  struct sockaddr_storage sst;
  uint32_t yes = 1;
  socklen_t addrlen;

#if !defined REMOVE_LOG_WARN
  char ipchar = olsr_cnf->ip_version == AF_INET ? '4' : '6';
#endif

  /* Init ipc socket */
  int s = socket(olsr_cnf->ip_version, SOCK_STREAM, 0);
  if (s == -1) {
    OLSR_ERROR(LOG_COMPORT, "Cannot open %d com-socket for IPv%c: %s\n", port, ipchar, strerror(errno));
    olsr_exit(1);
  }

  if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *) &yes, sizeof(yes)) < 0) {
    OLSR_ERROR(LOG_COMPORT, "Com-port %d SO_REUSEADDR for IPv%c failed: %s\n", port, ipchar, strerror(errno));
    os_close(s);
    olsr_exit(1);
  }

  /* Bind the socket */

  /* complete the socket structure */
  memset(&sst, 0, sizeof(sst));
  if (olsr_cnf->ip_version == AF_INET) {
    struct sockaddr_in *addr4 = (struct sockaddr_in *) &sst;
    addr4->sin_family = AF_INET;
    addrlen = sizeof(*addr4);
#ifdef SIN6_LEN
    addr4->sin_len = addrlen;
#endif
    addr4->sin_addr.s_addr = INADDR_ANY;
    addr4->sin_port = htons(port);
  } else {
    struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *) &sst;
    addr6->sin6_family = AF_INET6;
    addrlen = sizeof(*addr6);
#ifdef SIN6_LEN
    addr6->sin6_len = addrlen;
#endif
    addr6->sin6_addr = in6addr_any;
    addr6->sin6_port = htons(port);
  }

  /* bind the socket to the port number */
  if (bind(s, (struct sockaddr *) &sst, addrlen) == -1) {
    OLSR_ERROR(LOG_COMPORT, "Com-port %d bind failed for IPv%c: %s\n", port, ipchar, strerror(errno));
    os_close(s);
    olsr_exit(1);
  }

  /* show that we are willing to listen */
  if (listen(s, 1) == -1) {
    OLSR_ERROR(LOG_COMPORT, "Com-port %d listen for IPv%c failed %s\n", port, ipchar, strerror(errno));
    os_close(s);
    olsr_exit(1);
  }

  return s;
}

static void
olsr_com_parse_request(int fd, void *data __attribute__ ((unused)), unsigned int flags __attribute__ ((unused))) {
  struct comport_connection *con;
  struct sockaddr_storage addr;
  socklen_t addrlen;
  int sock;
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif

  addrlen = sizeof(addr);
  sock = accept(fd, (struct sockaddr *) &addr, &addrlen);
  if (sock < 0) {
    return;
  }

  con = olsr_memcookie_malloc(connection_cookie);
  abuf_init(&con->in, 1024);
  abuf_init(&con->out, 0);

  con->is_http = fd == comsocket_http->fd;

  if (olsr_cnf->ip_version == AF_INET6) {
    struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *) &addr;
    con->addr.v6 = addr6->sin6_addr;
  } else {
    struct sockaddr_in *addr4 = (struct sockaddr_in *) &addr;
    con->addr.v4 = addr4->sin_addr;
  }

  if (con->is_http) {
    if (connection_http_count < olsr_cnf->comport_http_limit) {
      connection_http_count++;
      con->state = HTTP_LOGIN;
      con->send_as = PLAIN;
      con->timeout_value = HTTP_TIMEOUT;
    } else {
      con->state = SEND_AND_QUIT;
      con->send_as = HTTP_503_SERVICE_UNAVAILABLE;
    }
  } else { /* !con->is_http */
    if (connection_txt_count < olsr_cnf->comport_txt_limit) {
      connection_txt_count++;
      con->state = INTERACTIVE;
      con->send_as = PLAIN;
      con->timeout_value = TXT_TIMEOUT;
    } else {
      abuf_puts(&con->out, "Too many txt connections, sorry...\n");
      con->state = SEND_AND_QUIT;
      con->send_as = PLAIN;
    }
  }

  OLSR_DEBUG(LOG_COMPORT, "Got connection through socket %d from %s.\n",
      sock, olsr_ip_to_string(&buf, &con->addr));

  con->timeout = olsr_timer_start(con->timeout_value, 0, con, connection_timeout);

  con->sock = olsr_socket_add(sock, &olsr_com_parse_connection, con,
      OLSR_SOCKET_READ | OLSR_SOCKET_WRITE);

  list_add_after(&olsr_comport_head, &con->node);
}

static void
olsr_com_cleanup_session(struct comport_connection *con) {
  if (con->is_http) {
    connection_http_count--;
  } else {
    connection_txt_count--;
  }

  list_remove(&con->node);

  if (con->stop_handler) {
    con->stop_handler(con);
  }

  os_close(con->sock->fd);
  olsr_socket_remove(con->sock);

  abuf_free(&con->in);
  abuf_free(&con->out);

  olsr_memcookie_free(connection_cookie, con);
}

static void
olsr_com_timeout_handler(void *data) {
  struct comport_connection *con = data;
  olsr_com_cleanup_session(con);
}

static void
olsr_com_parse_connection(int fd, void *data, unsigned int flags) {
  struct comport_connection *con = data;
#if !defined(REMOVE_LOG_WARN)
  struct ipaddr_str buf;
#endif

  OLSR_DEBUG(LOG_COMPORT, "Parsing connection of socket %d\n", fd);
  /* read data if necessary */
  if (flags & OLSR_SOCKET_READ) {
    char buffer[1024];
    int len;

    len = recv(fd, buffer, sizeof(buffer), 0);
    if (len > 0) {
      OLSR_DEBUG(LOG_COMPORT, "  recv returned %d\n", len);
      if (con->state != SEND_AND_QUIT) {
        abuf_memcpy(&con->in, buffer, len);
      }

      if (con->in.len > COMPORT_MAX_INPUTBUFFER) {
        if (con->state == INTERACTIVE) {
          abuf_puts(&con->out, "Sorry, input buffer overflow...\n");
        } else if (con->state == HTTP_LOGIN) {
          con->send_as = HTTP_413_REQUEST_TOO_LARGE;
        }
        con->state = SEND_AND_QUIT;
      }
    } else if (len < 0 && errno != EINTR) {
      OLSR_WARN(LOG_COMPORT, "Error while reading from communication stream with %s: %s\n",
          olsr_ip_to_string(&buf, &con->addr), strerror(errno));
      con->state = CLEANUP;
    }
    else {
      con->state = SEND_AND_QUIT;
    }
  }

  switch (con->state) {
    case HTTP_LOGIN:
      olsr_com_parse_http(con, flags);
      break;
    case INTERACTIVE:
      olsr_com_parse_txt(con, flags);
      break;
    default:
      break;
  }

  /* maybe we have to create an error message */
  if (con->out.len == 0 && con->state == SEND_AND_QUIT && con->send_as != PLAIN
      && con->send_as != HTTP_PLAIN && con->send_as != HTTP_200_OK) {
    olsr_com_create_httperror(con);
  }

  /* send data if necessary */
  if (con->out.len > 0) {
    if (con->state == SEND_AND_QUIT && con->send_as != PLAIN) {
      /* create header */
      olsr_com_build_httpheader(con);
      con->send_as = PLAIN;
    }

    if (flags & OLSR_SOCKET_WRITE) {
      int len;

      len = send(fd, con->out.buf, con->out.len, 0);
      if (len > 0) {
        OLSR_DEBUG(LOG_COMPORT, "  send returned %d\n", len);
        abuf_pull(&con->out, len);
      } else if (len < 0 && errno != EINTR) {
        OLSR_WARN(LOG_COMPORT, "Error while writing to communication stream with %s: %s\n",
            olsr_ip_to_string(&buf, &con->addr), strerror(errno));
        con->state = CLEANUP;
      }
    } else {
      OLSR_DEBUG(LOG_COMPORT, "  activating output in scheduler\n");
      olsr_socket_enable(con->sock, OLSR_SOCKET_WRITE);
    }
  }
  if (con->out.len == 0) {
    OLSR_DEBUG(LOG_COMPORT, "  deactivating output in scheduler\n");
    olsr_socket_disable(con->sock, OLSR_SOCKET_WRITE);
    if (con->state == SEND_AND_QUIT) {
      con->state = CLEANUP;
    }
  }

  /* end of connection ? */
  if (con->state == CLEANUP) {
    OLSR_DEBUG(LOG_COMPORT, "  cleanup\n");
    /* clean up connection by calling timeout directly */
    olsr_timer_stop(con->timeout);
    con->timeout = NULL;
    olsr_com_cleanup_session(con);
  }
  return;
}

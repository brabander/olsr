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
#include "scheduler.h"
#include "olsr_cookie.h"
#include "common/autobuf.h"
#include "common/avl.h"
#include "common/list.h"
#include "ipcalc.h"
#include "olsr.h"
#include "olsr_comport_http.h"
#include "olsr_comport_txt.h"
#include "olsr_comport.h"

#define HTTP_TIMEOUT  30000
#define TXT_TIMEOUT 60000

#define MAX_HTTP_PARA 10
#define COMPORT_MAX_INPUTBUFFER 65536

struct list_node olsr_comport_head;

/* server socket */
static int comsocket_http = 0;
static int comsocket_txt = 0;

static struct olsr_cookie_info *connection_cookie;
static struct olsr_cookie_info *connection_timeout;

/* counter for open connections */
static int connection_http_count;
static int connection_txt_count;

static int olsr_com_openport(int port);

static void olsr_com_parse_request(int fd, void *data, unsigned int flags);
static void olsr_com_parse_connection(int fd, void *data, unsigned int flags);
static void olsr_com_cleanup_session(struct comport_connection *con);
static void olsr_com_parse_http(struct comport_connection *con,
    unsigned int flags);
static void olsr_com_parse_txt(struct comport_connection *con,
    unsigned int flags);

static void olsr_com_timeout_handler(void *);

void olsr_com_init(void) {
  connection_cookie = olsr_alloc_cookie("comport connections",
      OLSR_COOKIE_TYPE_MEMORY);
  olsr_cookie_set_memory_size(connection_cookie,
      sizeof(struct comport_connection));

  connection_timeout = olsr_alloc_cookie("comport timout",
      OLSR_COOKIE_TYPE_TIMER);

  if (olsr_cnf->comport_http > 0) {
    if ((comsocket_http = olsr_com_openport(olsr_cnf->comport_http)) == -1) {
      return;
    }

    add_olsr_socket(comsocket_http, &olsr_com_parse_request, NULL, NULL,
        SP_PR_READ);
  }
  if (olsr_cnf->comport_txt > 0) {
    if ((comsocket_txt = olsr_com_openport(olsr_cnf->comport_txt)) == -1) {
      return;
    }

    add_olsr_socket(comsocket_txt, &olsr_com_parse_request, NULL, NULL,
        SP_PR_READ);
  }

  connection_http_count = 0;
  connection_txt_count = 0;

  list_head_init(&olsr_comport_head);

  olsr_com_init_http();
  olsr_com_init_txt();
}

void olsr_com_destroy(void) {
  while (!list_is_empty(&olsr_comport_head)) {
    struct comport_connection *con;

    if (NULL != (con = comport_node2con(olsr_comport_head.next))) {
      olsr_com_cleanup_session(con);
    }
  }

  olsr_com_destroy_http();
  olsr_com_destroy_txt();
}

void olsr_com_activate_output(struct comport_connection *con) {
  enable_olsr_socket(con->fd, &olsr_com_parse_connection, NULL, SP_PR_WRITE);
}

static int olsr_com_openport(int port) {
  struct sockaddr_storage sst;
  uint32_t yes = 1;
  socklen_t addrlen;

#if !defined REMOVE_LOG_WARN
  char ipchar = olsr_cnf->ip_version == AF_INET ? '4' : '6';
#endif

  /* Init ipc socket */
  int s = socket(olsr_cnf->ip_version, SOCK_STREAM, 0);
  if (s == -1) {
    OLSR_WARN(LOG_COMPORT, "Cannot open %d com-socket for IPv%c: %s\n", port, ipchar, strerror(errno));
    return -1;
  }

  if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *) &yes, sizeof(yes)) < 0) {
    OLSR_WARN(LOG_COMPORT, "Com-port %d SO_REUSEADDR for IPv%c failed: %s\n", port, ipchar, strerror(errno));
    CLOSESOCKET(s);
    return -1;
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
    OLSR_WARN(LOG_COMPORT, "Com-port %d bind failed for IPv%c: %s\n", port, ipchar, strerror(errno));
    CLOSESOCKET(s);
    return -1;
  }

  /* show that we are willing to listen */
  if (listen(s, 1) == -1) {
    OLSR_WARN(LOG_COMPORT, "Com-port %d listen for IPv%c failed %s\n", port, ipchar, strerror(errno));
    CLOSESOCKET(s);
    return -1;
  }

  return s;
}

static void olsr_com_parse_request(int fd, void *data __attribute__ ((unused)), unsigned int flags __attribute__ ((unused))) {
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

  con = olsr_cookie_malloc(connection_cookie);
  abuf_init(&con->in, 1024);
  abuf_init(&con->out, 0);

  con->is_http = fd == comsocket_http;
  con->fd = sock;

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

  con->timeout = olsr_start_timer(con->timeout_value, 0, false,
      &olsr_com_timeout_handler, con, connection_timeout);

  add_olsr_socket(sock, &olsr_com_parse_connection, NULL, con, SP_PR_READ
      | SP_PR_WRITE);

  list_add_after(&olsr_comport_head, &con->node);
}

static void olsr_com_cleanup_session(struct comport_connection *con) {
  if (con->is_http) {
    connection_http_count--;
  } else {
    connection_txt_count--;
  }

  list_remove(&con->node);

  if (con->stop_handler) {
    con->stop_handler(con);
  }
  remove_olsr_socket(con->fd, &olsr_com_parse_connection, NULL);
  CLOSESOCKET(con->fd);

  abuf_free(&con->in);
  abuf_free(&con->out);

  olsr_cookie_free(connection_cookie, con);
}

static void olsr_com_timeout_handler(void *data) {
  struct comport_connection *con = data;
  olsr_com_cleanup_session(con);
}

static void olsr_com_parse_connection(int fd, void *data, unsigned int flags) {
  struct comport_connection *con = data;
#if !defined(REMOVE_LOG_WARN)
  struct ipaddr_str buf;
#endif

  OLSR_DEBUG(LOG_COMPORT, "Parsing connection of socket %d\n", fd);
  /* read data if necessary */
  if (flags & SP_PR_READ) {
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
    } else if (len < 0) {
      OLSR_WARN(LOG_COMPORT, "Error while reading from communication stream with %s: %s\n",
          olsr_ip_to_string(&buf, &con->addr), strerror(errno));
      con->state = CLEANUP;
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

    if (flags & SP_PR_WRITE) {
      int len;

      len = send(fd, con->out.buf, con->out.len, 0);
      if (len > 0) {
        OLSR_DEBUG(LOG_COMPORT, "  send returned %d\n", len);
        abuf_pull(&con->out, len);
      } else if (len < 0) {
        OLSR_WARN(LOG_COMPORT, "Error while writing to communication stream with %s: %s\n",
            olsr_ip_to_string(&buf, &con->addr), strerror(errno));
        con->state = CLEANUP;
      }
    } else {
      OLSR_DEBUG(LOG_COMPORT, "  activating output in scheduler\n");
      enable_olsr_socket(fd, &olsr_com_parse_connection, NULL, SP_PR_WRITE);
    }
  }
  if (con->out.len == 0) {
    OLSR_DEBUG(LOG_COMPORT, "  deactivating output in scheduler\n");
    disable_olsr_socket(fd, &olsr_com_parse_connection, NULL, SP_PR_WRITE);
    if (con->state == SEND_AND_QUIT) {
      con->state = CLEANUP;
    }
  }

  /* end of connection ? */
  if (con->state == CLEANUP) {
    OLSR_DEBUG(LOG_COMPORT, "  cleanup\n");
    /* clean up connection by calling timeout directly */
    olsr_stop_timer(con->timeout);
    olsr_com_cleanup_session(con);
  }
  return;
}

static void olsr_com_parse_http(struct comport_connection *con,
    unsigned int flags  __attribute__ ((unused))) {
  char *para_keyvalue[MAX_HTTP_PARA * 2] = { NULL }, *para = NULL, *str = NULL;
  int para_count = 0;
  char req_type[11] = { 0 };
  char filename[252] = { 0 };
  char filename_copy[252] = { 0 };
  char http_version[11] = { 0 };

  int idx = 0;
  int i = 0;

  /*
   * find end of http header, might be useful for POST to keep the index !
   * (implemented as a finite element automaton)
   */

  while (i < 5) {
    switch (con->in.buf[idx++]) {
      case '\0':
        i = 6;
        break;
      case '\r':
        i = (i == 3) ? 4 : 2;
        break;
      case '\n':
        if (i == 1 || i == 4) {
          i = 5;
        } else if (i == 2) {
          i = 3;
        } else {
          i = 1;
        }
        break;
      default:
        i = 0;
        break;
    }
  }

  if (i != 5) {
    OLSR_DEBUG(LOG_COMPORT, "  need end of http header, still waiting...\n");
    return;
  }

  /* got http header */
  if (sscanf(con->in.buf, "%10s %250s %10s\n", req_type, filename, http_version)
      != 3 || (strcmp(http_version, "HTTP/1.1") != 0 && strcmp(http_version,
      "HTTP/1.0") != 0)) {
    con->send_as = HTTP_400_BAD_REQ;
    con->state = SEND_AND_QUIT;
    return;
  }

  OLSR_DEBUG(LOG_COMPORT, "HTTP Request: %s %s %s\n", req_type, filename, http_version);

  /* store a copy for the http_handlers */
  strcpy(filename_copy, filename);
  if (strcmp(req_type, "POST") == 0) {
    /* load the rest of the header for POST commands */
    char *lengthField = strstr(con->in.buf, "\nContent-Length:");
    int clen = 0;

    if (lengthField == NULL) {
      con->send_as = HTTP_400_BAD_REQ;
      con->state = SEND_AND_QUIT;
      return;
    }

    sscanf(lengthField, "%*s %d\n", &clen);
    if (con->in.len < idx + clen) {
      /* we still need more data */
      return;
    }
    para = &con->in.buf[idx];
  }

  /* strip the URL marker away */
  str = strchr(filename, '#');
  if (str) {
    *str = 0;
  }

  /* we have everything to process the http request */
  con->state = SEND_AND_QUIT;
  if (strcmp(req_type, "GET") == 0) {
    /* HTTP-GET request */
    para = strchr(filename, '?');
    if (para != NULL) {
      *para++ = 0;
    }
  } else if (strcmp(req_type, "POST") != 0) {
    con->send_as = HTTP_501_NOT_IMPLEMENTED;
    return;
  }

  /* handle HTTP GET & POST including parameters */
  while (para && para_count < MAX_HTTP_PARA * 2) {
    /* split the string at the next '=' (the key/value splitter) */
    str = strchr(para, '=');
    if (!str) {
      break;
    }
    *str++ = 0;

    /* we have null terminated key at *para. Now decode the key */
    para_keyvalue[para_count++] = para;
    olsr_com_decode_url(para);

    /* split the string at the next '&' (the splitter of multiple key/value pairs */
    para = strchr(str, '&');
    if (para) {
      *para++ = 0;
    }

    /* we have a null terminated value at *str, Now decode it */
    olsr_com_decode_url(str);
    para_keyvalue[para_count++] = str;
  }

  /* create body */
  i = strlen(filename);

  /*
   * add a '/' at the end if it's not there to detect
   *  paths without terminating '/' from the browser
   */
  if (filename[i - 1] != '/') {
    strcat(filename, "/");
  }

  while (i > 0) {
    if (olsr_com_handle_htmlsite(con, filename, filename_copy, para_count >> 1,
        para_keyvalue)) {
      return;
    }

    /* try to find a handler for a path prefix */
    do {
      filename[i--] = 0;
    } while (i > 0 && filename[i] != '/');
  }
  con->send_as = HTTP_404_NOT_FOUND;
}

static void olsr_com_parse_txt(struct comport_connection *con,
    unsigned int flags  __attribute__ ((unused))) {
  static char defaultCommand[] = "/link/neigh/topology/hna/mid/routes";
  static char tmpbuf[128];

  enum olsr_txtcommand_result res;
  char *eol;
  int len;
  bool processedCommand = false, chainCommands = false;
  uint32_t old_timeout;

  old_timeout = con->timeout_value;

  /* loop over input */
  while (con->in.len > 0 && con->state == INTERACTIVE) {
    char *para = NULL, *cmd = NULL, *next = NULL;

    /* search for end of line */
    eol = memchr(con->in.buf, '\n', con->in.len);

    if (eol == NULL) {
      break;
    }

    /* terminate line with a 0 */
    if (eol != con->in.buf && eol[-1] == '\r') {
      eol[-1] = 0;
    }
    *eol++ = 0;

    /* handle line */
    OLSR_DEBUG(LOG_COMPORT, "Interactive console: %s\n", con->in.buf);
    cmd = &con->in.buf[0];
    processedCommand = true;

    /* apply default command */
    if (strcmp(cmd, "/") == 0) {
      strcpy(tmpbuf, defaultCommand);
      cmd = tmpbuf;
    }

    if (cmd[0] == '/') {
      cmd++;
      chainCommands = true;
    }
    while (cmd) {
      len = con->out.len;

      /* handle difference between multicommand and singlecommand mode */
      if (chainCommands) {
        next = strchr(cmd, '/');
        if (next) {
          *next++ = 0;
        }
      }
      para = strchr(cmd, ' ');
      if (para != NULL) {
        *para++ = 0;
      }

      /* if we are doing continous output, stop it ! */
      if (con->stop_handler) {
        con->stop_handler(con);
        con->stop_handler = NULL;
      }

      if (strlen(cmd) != 0) {
        res = olsr_com_handle_txtcommand(con, cmd, para);
        switch (res) {
          case CONTINUE:
            break;
          case CONTINOUS:
            break;
          case ABUF_ERROR:
            con->out.len = len;
            abuf_appendf(&con->out,
                "Error in autobuffer during command '%s'.\n", cmd);
            break;
          case UNKNOWN:
            con->out.len = len;
            abuf_appendf(&con->out, "Error, unknown command '%s'\n", cmd);
            break;
          case QUIT:
            con->state = SEND_AND_QUIT;
            break;
        }
        /* put an empty line behind each command */
        if (con->show_echo) {
          abuf_puts(&con->out, "\n");
        }
      }
      cmd = next;
    }

    /* remove line from input buffer */
    abuf_pull(&con->in, eol - con->in.buf);

    if (con->in.buf[0] == '/') {
      /* end of multiple command line */
      con->state = SEND_AND_QUIT;
    }
  }

  if (old_timeout != con->timeout_value) {
    olsr_set_timer(&con->timeout, con->timeout_value, 0, false, &olsr_com_timeout_handler, con, connection_timeout);
  }

  /* print prompt */
  if (processedCommand && con->state == INTERACTIVE && con->show_echo) {
    abuf_puts(&con->out, "> ");
  }
}


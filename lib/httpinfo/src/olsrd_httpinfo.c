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

#include "olsrd_httpinfo.h"
#include "admin_interface.h"
#include "gfx.h"

#include "olsr.h"
#include "olsr_cfg.h"
#include "interfaces.h"
#include "olsr_protocol.h"
#include "net_olsr.h"
#include "link_set.h"
#include "ipcalc.h"
#include "lq_plugin.h"
#include "olsr_cfg_gen.h"
#include "common/string.h"
#include "olsr_ip_prefix_list.h"
#include "olsr_logging.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#ifdef WIN32
#include <io.h>
#else
#include <netdb.h>
#endif

#ifdef OS
#undef OS
#endif

#ifdef WIN32
#define OS "Windows"
#endif
#ifdef linux
#define OS "GNU/Linux"
#endif
#ifdef __FreeBSD__
#define OS "FreeBSD"
#endif

#ifndef OS
#define OS "Undefined"
#endif

static char copyright_string[] __attribute__((unused)) = "olsr.org HTTPINFO plugin Copyright (c) 2004, Andreas Tonnesen(andreto@olsr.org) All rights reserved.";

#define MAX_CLIENTS 3

#define MAX_HTTPREQ_SIZE (1024 * 10)

#define DEFAULT_TCP_PORT 1978

#define HTML_BUFSIZE (1024 * 4000)

#define FRAMEWIDTH (resolve_ip_addresses ? 900 : 800)

#define FILENREQ_MATCH(req, filename) \
        !strcmp(req, filename) || \
        (strlen(req) && !strcmp(&req[1], filename))

static const char httpinfo_css[] =
  "#A{text-decoration:none}\n"
  "TH{text-align:left}\n"
  "H1,H3,TD,TH{font-family:Helvetica;font-size:80%}\n"
  "h2{font-family:Helvetica; font-size:14px;text-align:center;line-height:16px;"
  "text-decoration:none;border:1px solid #ccc;margin:5px;background:#ececec;}\n"
  "hr{border:none;padding:1px;background:url(grayline.gif) repeat-x bottom;}\n"
  "#maintable{margin:0px;padding:5px;border-left:1px solid #ccc;"
  "border-right:1px solid #ccc;border-bottom:1px solid #ccc;}\n"
  "#footer{font-size:10px;line-height:14px;text-decoration:none;color:#666;}\n"
  "#hdr{font-size:14px;text-align:center;line-height:16px;text-decoration:none;"
  "border:1px solid #ccc;margin:5px;background:#ececec;}\n"
  "#container{width:1000px;padding:30px;border:1px solid #ccc;background:#fff;}\n"
  "#tabnav{height:20px;margin:0;padding-left:10px;"
  "background:url(grayline.gif) repeat-x bottom;}\n"
  "#tabnav li{margin:0;padding:0;display:inline;list-style-type:none;}\n"
  "#tabnav a:link,#tabnav a:visited{float:left;background:#ececec;font-size:12px;"
  "line-height:14px;font-weight:bold;padding:2px 10px 2px 10px;margin-right:4px;"
  "border:1px solid #ccc;text-decoration:none;color:#777;}\n"
  "#tabnav a:link.active,#tabnav a:visited.active{border-bottom:1px solid #fff;"
  "background:#ffffff;color:#000;}\n"
  "#tabnav a:hover{background:#777777;color:#ffffff;}\n"
  ".input_text{background:#E5E5E5;margin-left:5px; margin-top:0px;text-align:left;"
  "width:100px;padding:0px;color:#000;text-decoration:none;font-family:verdana;"
  "font-size:12px;border:1px solid #ccc;}\n"
  ".input_button{background:#B5D1EE;margin-left:5px;margin-top:0px;text-align:center;"
  "width:120px;padding:0px;color:#000;text-decoration:none;font-family:verdana;"
  "font-size:12px;border:1px solid #000;}\n";

typedef void (*build_body_callback)(struct autobuf *abuf);

struct tab_entry {
  const char *tab_label;
  const char *filename;
  build_body_callback build_body_cb;
  bool display_tab;
};

struct static_bin_file_entry {
  const char *filename;
  unsigned char *data;
  unsigned int data_size;
};

struct static_txt_file_entry {
  const char *filename;
  const char *data;
};

#if ADMIN_INTERFACE
struct dynamic_file_entry {
  const char *filename;
  process_data_func process_data_cb;
};
#endif

static int get_http_socket(int);

static void build_tabs(struct autobuf *abuf, int);

static void parse_http_request(int, void *, unsigned int flags);

static void build_http_header(struct autobuf *abuf, http_header_type, bool, uint32_t);

static void build_frame(struct autobuf *abuf, build_body_callback frame_body_cb);

static void build_routes_body(struct autobuf *abuf);

static void build_config_body(struct autobuf *abuf);

static void build_neigh_body(struct autobuf *abuf);

static void build_topo_body(struct autobuf *abuf);

static void build_mid_body(struct autobuf *abuf);

static void build_nodes_body(struct autobuf *abuf);

static void build_all_body(struct autobuf *abuf);

static void build_about_body(struct autobuf *abuf);

static void build_cfgfile_body(struct autobuf *abuf);

static void build_ip_txt(struct autobuf *abuf, const bool want_link,
			 const char * const ipaddrstr, const int prefix_len);

static void build_ipaddr_link(struct autobuf *abuf, const bool want_link,
			      const union olsr_ip_addr * const ipaddr,
			      const int prefix_len);
static void section_title(struct autobuf *abuf, const char *title);

static ssize_t writen(int fd, const void *buf, size_t count);

static struct timeval start_time;
static struct http_stats stats;
static int client_sockets[MAX_CLIENTS];
static int curr_clients;
static int http_socket;

#if 0
int netsprintf(char *str, const char* format, ...) __attribute__((format(printf, 2, 3)));
static int netsprintf_direct = 0;
static int netsprintf_error = 0;
#define sprintf netsprintf
#define NETDIRECT
#endif

static const struct tab_entry tab_entries[] = {
    {"Configuration",  "config",  build_config_body,  true},
    {"Routes",         "routes",  build_routes_body,  true},
    {"Links/Topology", "nodes",   build_nodes_body,   true},
    {"All",            "all",     build_all_body,     true},
#if ADMIN_INTERFACE
    {"Admin",          "admin",   build_admin_body,   true},
#endif
    {"About",          "about",   build_about_body,   true},
    {"FOO",            "cfgfile", build_cfgfile_body, false},
    {NULL,             NULL,      NULL,               false}
};

static const struct static_bin_file_entry static_bin_files[] = {
    {"favicon.ico",  favicon_ico, sizeof(favicon_ico)},
    {"logo.gif",     logo_gif, sizeof(logo_gif)},
    {"grayline.gif", grayline_gif, sizeof(grayline_gif)},
    {NULL, NULL, 0}
};

static const struct static_txt_file_entry static_txt_files[] = {
    {"httpinfo.css", httpinfo_css},
    {NULL, NULL}
};


#if ADMIN_INTERFACE
static const struct dynamic_file_entry dynamic_files[] = {
    {"set_values", process_set_values},
    {NULL, NULL}
};
#endif


static int
get_http_socket(int port)
{
  struct sockaddr_storage sst;
  uint32_t yes = 1;
  socklen_t addrlen;

  /* Init ipc socket */
  int s = socket(olsr_cnf->ip_version, SOCK_STREAM, 0);
  if (s == -1) {
    OLSR_WARN(LOG_PLUGINS, "(HTTPINFO)socket %s\n", strerror(errno));
    return -1;
  }

  if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(yes)) < 0) {
    OLSR_WARN(LOG_PLUGINS, "(HTTPINFO)SO_REUSEADDR failed %s\n", strerror(errno));
    CLOSESOCKET(s);
    return -1;
  }

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
      addr4->sin_port = htons(port);
  } else {
      struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&sst;
      addr6->sin6_family = AF_INET6;
      addrlen = sizeof(*addr6);
#ifdef SIN6_LEN
      addr6->sin6_len = addrlen;
#endif
      addr6->sin6_addr = in6addr_any;
      addr6->sin6_port = htons(port);
  }

  /* bind the socket to the port number */
  if (bind(s, (struct sockaddr *)&sst, addrlen) == -1) {
    OLSR_WARN(LOG_PLUGINS, "(HTTPINFO) bind failed %s\n", strerror(errno));
    CLOSESOCKET(s);
    return -1;
  }

  /* show that we are willing to listen */
  if (listen(s, 1) == -1) {
    OLSR_WARN(LOG_PLUGINS, "(HTTPINFO) listen failed %s\n", strerror(errno));
    CLOSESOCKET(s);
    return -1;
  }

  return s;
}

/**
 *Do initialization here
 *
 *This function is called by the my_init
 *function in olsrd_plugin.c
 */
int
olsrd_plugin_init(void)
{
  /* Get start time */
  gettimeofday(&start_time, NULL);

  curr_clients = 0;
  /* set up HTTP socket */
  http_socket = get_http_socket(http_port != 0 ? http_port :  DEFAULT_TCP_PORT);

  if (http_socket < 0) {
    OLSR_ERROR(LOG_PLUGINS, "(HTTPINFO) could not initialize HTTP socket\n");
    olsr_exit(0);
  }

  /* always allow localhost */
  if (olsr_cnf->ip_version == AF_INET) {
    union olsr_ip_addr ip;

    ip.v4.s_addr = ntohl(INADDR_LOOPBACK);
    ip_acl_add(&allowed_nets, &ip, 32, false);
  } else {
    ip_acl_add(&allowed_nets, (const union olsr_ip_addr *)&in6addr_loopback, 128, false);
    ip_acl_add(&allowed_nets, (const union olsr_ip_addr *)&in6addr_v4mapped_loopback, 128, false);
  }

  /* Register socket */
  add_olsr_socket(http_socket, &parse_http_request, NULL, NULL, SP_PR_READ);

  return 1;
}

/* Non reentrant - but we are not multithreaded anyway */
static void
parse_http_request(int fd, void *data __attribute__((unused)), unsigned int flags __attribute__((unused)))
{
  struct sockaddr_storage pin;
  socklen_t addrlen;
  union olsr_ip_addr *ipaddr;
  char req[MAX_HTTPREQ_SIZE];
  //static char body[HTML_BUFSIZE];
  struct autobuf body, header;
  char req_type[11];
  char filename[251];
  char http_version[11];
  unsigned int c = 0;
  int r = 1 /*, size = 0 */;

  abuf_init(&body, 0);
  abuf_init(&header, 0);

  if (curr_clients >= MAX_CLIENTS) {
    return;
  }
  curr_clients++;

  addrlen = sizeof(pin);
  client_sockets[curr_clients] = accept(fd, (struct sockaddr *)&pin, &addrlen);
  if (client_sockets[curr_clients] == -1) {
    OLSR_WARN(LOG_PLUGINS, "(HTTPINFO) accept: %s\n", strerror(errno));
    goto close_connection;
  }

  if(((struct sockaddr *)&pin)->sa_family != olsr_cnf->ip_version) {
    OLSR_WARN(LOG_PLUGINS, "(HTTPINFO) Connection with wrong IP version?!\n");
    goto close_connection;
  }

  if(olsr_cnf->ip_version == AF_INET) {
    struct sockaddr_in *addr4 = (struct sockaddr_in *)&pin;
    ipaddr = (union olsr_ip_addr *)&addr4->sin_addr;
  } else {
    struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&pin;
    ipaddr = (union olsr_ip_addr *)&addr6->sin6_addr;
  }

  if (!ip_acl_acceptable(&allowed_nets, ipaddr, olsr_cnf->ip_version)) {
#if !defined REMOVE_LOG_WARN
    struct ipaddr_str strbuf;
#endif
    OLSR_WARN(LOG_PLUGINS, "HTTP request from non-allowed host %s!\n",
                olsr_ip_to_string(&strbuf, ipaddr));
    goto close_connection;
  }

  memset(req, 0, sizeof(req));
  //memset(body, 0, sizeof(body));

  while ((r = recv(client_sockets[curr_clients], &req[c], 1, 0)) > 0 && (c < sizeof(req)-1)) {
      c++;

      if ((c > 3 && !strcmp(&req[c-4], "\r\n\r\n")) ||
	 (c > 1 && !strcmp(&req[c-2], "\n\n")))
          break;
  }

  if (r < 0) {
    OLSR_WARN(LOG_PLUGINS, "(HTTPINFO) Failed to recieve data from client!\n");
    stats.err_hits++;
    goto close_connection;
  }

  /* Get the request */
  if (sscanf(req, "%10s %250s %10s\n", req_type, filename, http_version) != 3) {
    /* Try without HTTP version */
    if (sscanf(req, "%10s %250s\n", req_type, filename) != 2) {
      OLSR_WARN(LOG_PLUGINS, "(HTTPINFO) Error parsing request %s!\n", req);
      stats.err_hits++;
      goto close_connection;
    }
  }

  OLSR_DEBUG(LOG_PLUGINS, "Request: %s\nfile: %s\nVersion: %s\n\n", req_type, filename, http_version);

  if (!strcmp(req_type, "POST")) {
#if ADMIN_INTERFACE
    int i = 0;
    while (dynamic_files[i].filename) {
        OLSR_DEBUG(LOG_PLUGINS, "POST checking %s\n", dynamic_files[i].filename);
        if (FILENREQ_MATCH(filename, dynamic_files[i].filename)) {
            uint32_t param_size;

            stats.ok_hits++;

            param_size = recv(client_sockets[curr_clients], req, sizeof(req)-1, 0);

            req[param_size] = '\0';
            OLSR_DEBUG(LOG_PLUGINS, "Dynamic read %d bytes\n", param_size);

            //memcpy(body, dynamic_files[i].data, static_bin_files[i].data_size);
            //size += dynamic_files[i].process_data_cb(req, param_size, &body[size], sizeof(body)-size);
	    dynamic_files[i].process_data_cb(req, param_size, &body);

            build_http_header(&header, HTTP_OK, true, body.len);
            goto send_http_data;
        }
        i++;
    }
#endif
    /* We only support GET */
    abuf_puts(&body, HTTP_400_MSG);
    stats.ill_hits++;
    build_http_header(&header, HTTP_BAD_REQ, true, body.len);
  } else if (!strcmp(req_type, "GET")) {
    int i = 0;

    for (i = 0; static_bin_files[i].filename; i++) {
        if (FILENREQ_MATCH(filename, static_bin_files[i].filename)) {
	    break;
        }
    }

    if (static_bin_files[i].filename) {
      stats.ok_hits++;
      //memcpy(body, static_bin_files[i].data, static_bin_files[i].data_size);
      abuf_memcpy(&body, static_bin_files[i].data, static_bin_files[i].data_size);
      //size = static_bin_files[i].data_size;
      build_http_header(&header, HTTP_OK, false, body.len);
      goto send_http_data;
    }

    i = 0;
    while (static_txt_files[i].filename)	{
      if (FILENREQ_MATCH(filename, static_txt_files[i].filename)) {
        break;
      }
      i++;
    }

    if (static_txt_files[i].filename) {
      stats.ok_hits++;

      //size += snprintf(&body[size], sizeof(body)-size, "%s", static_txt_files[i].data);
      abuf_puts(&body, static_txt_files[i].data);

      build_http_header(&header, HTTP_OK, false, body.len);
      goto send_http_data;
    }

    i = 0;
    if (strlen(filename) > 1) {
      while (tab_entries[i].filename) {
        if (FILENREQ_MATCH(filename, tab_entries[i].filename)) {
          break;
        }
        i++;
      }
    }

    if (tab_entries[i].filename) {
#ifdef NETDIRECT
      build_http_header(&header, HTTP_OK, true);
      r = send(client_sockets[curr_clients], req, c, 0);
      if (r < 0) {
        OLSR_WARN(LOG_PLUGINS, "(HTTPINFO) Failed sending data to client!\n");
        goto close_connection;
      }
      netsprintf_error = 0;
      netsprintf_direct = 1;
#endif
      abuf_appendf(&body,
                       "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n"
                       "<head>\n"
                       "<meta http-equiv=\"Content-type\" content=\"text/html; charset=ISO-8859-1\">\n"
                       "<title>olsr.org httpinfo plugin</title>\n"
                       "<link rel=\"icon\" href=\"favicon.ico\" type=\"image/x-icon\">\n"
                       "<link rel=\"shortcut icon\" href=\"favicon.ico\" type=\"image/x-icon\">\n"
                       "<link rel=\"stylesheet\" type=\"text/css\" href=\"httpinfo.css\">\n"
                       "</head>\n"
                       "<body bgcolor=\"#ffffff\" text=\"#000000\">\n"
                       "<table border=\"0\" cellpadding=\"0\" cellspacing=\"0\" width=\"%d\">\n"
                       "<tbody><tr bgcolor=\"#ffffff\">\n"
                       "<td height=\"69\" valign=\"middle\" width=\"80%%\">\n"
                       "<font color=\"black\" face=\"timesroman\" size=\"6\">&nbsp;&nbsp;&nbsp;<a href=\"http://www.olsr.org/\">olsr.org OLSR daemon</a></font></td>\n"
                       "<td height=\"69\" valign=\"middle\" width=\"20%%\">\n"
                       "<a href=\"http://www.olsr.org/\"><img border=\"0\" src=\"/logo.gif\" alt=\"olsrd logo\"></a></td>\n"
                       "</tr>\n"
                       "</tbody>\n"
                       "</table>\n",
                       FRAMEWIDTH);

      build_tabs(&body, i);
      build_frame(&body,
		  tab_entries[i].build_body_cb);

      stats.ok_hits++;

      abuf_appendf(&body,
                       "</table>\n"
                       "<div id=\"footer\">\n"
                       "<center>\n"
                       "(C)2005 Andreas T&oslash;nnesen<br/>\n"
                       "<a href=\"http://www.olsr.org/\">http://www.olsr.org</a>\n"
                       "</center>\n"
                       "</div>\n"
                       "</body>\n"
                       "</html>\n");

#ifdef NETDIRECT
      netsprintf_direct = 1;
      goto close_connection;
#else
      build_http_header(&header, HTTP_OK, true, body.len);
      goto send_http_data;
#endif
    }


    stats.ill_hits++;
    abuf_puts(&body, HTTP_404_MSG);
    build_http_header(&header, HTTP_BAD_FILE, true, body.len);
  } else {
    /* We only support GET */
    abuf_puts(&body, HTTP_400_MSG);
    stats.ill_hits++;
    build_http_header(&header, HTTP_BAD_REQ, true, body.len);
  }

 send_http_data:

  r = writen(client_sockets[curr_clients], header.buf, header.len);
  if (r < 0) {
      OLSR_WARN(LOG_PLUGINS, "(HTTPINFO) Failed sending data to client!\n");
      goto close_connection;
  }

  r = writen(client_sockets[curr_clients], body.buf, body.len);
  if (r < 0) {
      OLSR_WARN(LOG_PLUGINS, "(HTTPINFO) Failed sending data to client!\n");
      goto close_connection;
  }

 close_connection:
  abuf_free(&body);
  abuf_free(&header);
  CLOSESOCKET(client_sockets[curr_clients]);
  curr_clients--;
}


void
build_http_header(struct autobuf *abuf,
		  http_header_type type,
		  bool is_html,
		  uint32_t msgsize)
{
  time_t currtime;
  const char *h;

  switch(type) {
  case HTTP_BAD_REQ:
      h = HTTP_400;
      break;
  case HTTP_BAD_FILE:
      h = HTTP_404;
      break;
  default:
      /* Defaults to OK */
      h = HTTP_200;
      break;
  }
  abuf_puts(abuf, h);

  /* Date */
  time(&currtime);
  abuf_strftime(abuf, "Date: %a, %d %b %Y %H:%M:%S GMT\r\n", localtime(&currtime));

  /* Server version */
  abuf_appendf(abuf, "Server: %s %s %s\r\n", PLUGIN_NAME, PLUGIN_VERSION, HTTP_VERSION);

  /* connection-type */
  abuf_puts(abuf, "Connection: closed\r\n");

  /* MIME type */
  abuf_appendf(abuf, "Content-type: text/%s\r\n", is_html ? "html" : "plain");

  /* Content length */
  if (msgsize > 0) {
      abuf_appendf(abuf, "Content-length: %u\r\n", msgsize);
  }

  /* Cache-control
   * No caching dynamic pages
   */
  abuf_puts(abuf, "Cache-Control: no-cache\r\n");

  if (!is_html) {
    abuf_puts(abuf, "Accept-Ranges: bytes\r\n");
  }
  /* End header */
  abuf_puts(abuf, "\r\n");

  OLSR_DEBUG(LOG_PLUGINS, "HEADER:\n%s", abuf->buf);
}


static void build_tabs(struct autobuf *abuf, int active)
{
  int tabs;

  abuf_appendf(abuf,
		  "<table align=\"center\" border=\"0\" cellpadding=\"0\" cellspacing=\"0\" width=\"%d\">\n"
		  "<tr bgcolor=\"#ffffff\"><td>\n"
		  "<ul id=\"tabnav\">\n",
		  FRAMEWIDTH);
  for (tabs = 0; tab_entries[tabs].tab_label; tabs++) {
    if (!tab_entries[tabs].display_tab) {
      continue;
    }
    abuf_appendf(abuf,
		    "<li><a href=\"%s\"%s>%s</a></li>\n",
		    tab_entries[tabs].filename,
		    tabs == active ? " class=\"active\"" : "",
		    tab_entries[tabs].tab_label);
  }
  abuf_appendf(abuf,
		  "</ul>\n"
		  "</td></tr>\n"
		  "<tr><td>\n");
}


/*
 * destructor - called at unload
 */
void
olsr_plugin_exit(void)
{
  ip_acl_flush(&allowed_nets);

  if (http_socket >= 0) {
    CLOSESOCKET(http_socket);
  }
}


static void section_title(struct autobuf *abuf, const char *title)
{
  abuf_appendf(abuf,
                  "<h2>%s</h2>\n"
                  "<table width=\"100%%\" border=\"0\" cellspacing=\"0\" cellpadding=\"0\" align=\"center\">\n", title);
}

static void build_frame(struct autobuf *abuf,
			build_body_callback frame_body_cb)
{
  abuf_puts(abuf, "<div id=\"maintable\">\n");
  frame_body_cb(abuf);
  abuf_puts(abuf, "</div>\n");
}

static void fmt_href(struct autobuf *abuf,
                    const char * const ipaddr)
{
  abuf_appendf(abuf, "<a href=\"http://%s:%d/all\">", ipaddr, http_port);
}

static void build_ip_txt(struct autobuf *abuf,
			 const bool print_link,
			 const char * const ipaddrstr,
			 const int prefix_len)
{
  if (print_link) {
    fmt_href(abuf, ipaddrstr);
  }

  abuf_puts(abuf, ipaddrstr);
  /* print ip address or ip prefix ? */
  if (prefix_len != -1 && prefix_len != 8 * (int)olsr_cnf->ipsize) {
    abuf_appendf(abuf, "/%d", prefix_len);
  }

  if (print_link) { /* Print the link only if there is no prefix_len */
    abuf_puts(abuf, "</a>");
  }
}

static void build_ipaddr_link(struct autobuf *abuf,
			      const bool want_link,
			      const union olsr_ip_addr * const ipaddr,
			      const int prefix_len)
{
  struct ipaddr_str ipaddrstr;
  const struct hostent * const hp =
#ifndef WIN32
      resolve_ip_addresses ? gethostbyaddr(ipaddr, olsr_cnf->ipsize, olsr_cnf->ip_version) :
#endif
      NULL;
  /* Print the link only if there is no prefix_len */
  const int print_link = want_link && (prefix_len == -1 || prefix_len == 8 * (int)olsr_cnf->ipsize);
  olsr_ip_to_string(&ipaddrstr, ipaddr);

  abuf_puts(abuf, "<td>");
  build_ip_txt(abuf, print_link, ipaddrstr.buf, prefix_len);
  abuf_puts(abuf, "</td>");

  if (resolve_ip_addresses) {
    if (hp) {
      abuf_puts(abuf, "<td>(");
      if (print_link) {
        fmt_href(abuf, ipaddrstr.buf);
      }
      abuf_puts(abuf, hp->h_name);
      if (print_link) {
        abuf_puts(abuf, "</a>");
      }
      abuf_puts(abuf, ")</td>");
    } else {
      abuf_puts(abuf, "<td/>");
    }
  }
}

#define build_ipaddr_with_link(abuf, ipaddr, plen) \
          build_ipaddr_link((abuf), true, (ipaddr), (plen))
#define build_ipaddr_no_link(abuf, ipaddr, plen) \
          build_ipaddr_link((abuf), false, (ipaddr), (plen))

static void build_route(struct autobuf *abuf, const struct rt_entry * rt)
{
  struct lqtextbuffer lqbuffer;

  abuf_puts(abuf, "<tr>");
  build_ipaddr_with_link(abuf,
			 &rt->rt_dst.prefix,
			 rt->rt_dst.prefix_len);
  build_ipaddr_with_link(abuf,
			 &rt->rt_best->rtp_nexthop.gateway,
			 -1);

  abuf_appendf(abuf,
		  "<td>%u</td>",
		  rt->rt_best->rtp_metric.hops);
  abuf_appendf(abuf,
		  "<td>%s</td>",
		  get_linkcost_text(rt->rt_best->rtp_metric.cost, true, &lqbuffer));
  abuf_appendf(abuf,
		  "<td>%s</td></tr>\n",
		  rt->rt_best->rtp_nexthop.interface ? rt->rt_best->rtp_nexthop.interface->int_name : "[null]");
}

static void build_routes_body(struct autobuf *abuf)
{
  struct rt_entry *rt;
  const char *colspan = resolve_ip_addresses ? " colspan=\"2\"" : "";
  section_title(abuf, "OLSR Routes in Kernel");
  abuf_appendf(abuf,
		  "<tr><th%s>Destination</th><th%s>Gateway</th><th>Metric</th><th>ETX</th><th>Interface</th></tr>\n",
		  colspan, colspan);

  /* Walk the route table */
  OLSR_FOR_ALL_RT_ENTRIES(rt) {
      build_route(abuf, rt);
  } OLSR_FOR_ALL_RT_ENTRIES_END(rt);

  abuf_puts(abuf, "</table>\n");
}

static void build_config_body(struct autobuf *abuf)
{
  const struct olsr_if_config *ifs;
  const struct plugin_entry *pentry;
  const struct plugin_param *pparam;
  struct ipaddr_str mainaddrbuf;

  abuf_appendf(abuf,
		  "Version: %s (built on %s on %s)\n<br>OS: %s\n<br>",
		  olsrd_version, build_date, build_host, OS);
  {
    const time_t currtime = time(NULL);
    abuf_strftime(abuf, "System time: <em>%a, %d %b %Y %H:%M:%S</em><br>", localtime(&currtime));
  }

  {
    struct timeval now, uptime;
    int hours, mins, days;
    gettimeofday(&now, NULL);
    timersub(&now, &start_time, &uptime);

    days = uptime.tv_sec/86400;
    uptime.tv_sec %= 86400;
    hours = uptime.tv_sec/3600;
    uptime.tv_sec %= 3600;
    mins = uptime.tv_sec/60;
    uptime.tv_sec %= 60;

    abuf_puts(abuf, "Olsrd uptime: <em>");
    if (days) {
      abuf_appendf(abuf, "%d day(s) ", days);
    }
    abuf_appendf(abuf, "%02d hours %02d minutes %02d seconds</em><br/>\n", hours, mins, (int)uptime.tv_sec);
  }

  abuf_appendf(abuf, "HTTP stats(ok/dyn/error/illegal): <em>%u/%u/%u/%u</em><br>\n", stats.ok_hits, stats.dyn_hits, stats.err_hits, stats.ill_hits);

  abuf_appendf(abuf, "Click <a href=\"/cfgfile\">here</a> to <em>generate a configuration file for this node</em>.\n");

  abuf_puts(abuf, "<h2>Variables</h2>\n");

  abuf_puts(abuf, "<table width=\"100%%\" border=\"0\">\n<tr>");

  abuf_appendf(abuf, "<td>Main address: <strong>%s</strong></td>\n", olsr_ip_to_string(&mainaddrbuf, &olsr_cnf->router_id));
  abuf_appendf(abuf, "<td>IP version: %d</td>\n", olsr_cnf->ip_version == AF_INET ? 4 : 6);

  // TODO: add logging information into http info ?
  abuf_appendf(abuf, "<td></td>\n");
  abuf_appendf(abuf, "<td>FIB Metrics: %s</td>\n", FIBM_FLAT == olsr_cnf->fib_metric ? CFG_FIBM_FLAT : FIBM_CORRECT == olsr_cnf->fib_metric ? CFG_FIBM_CORRECT : CFG_FIBM_APPROX);

  abuf_puts(abuf, "</tr>\n<tr>\n");

  abuf_appendf(abuf, "<td>Pollrate: %0.2f</td>\n", conv_pollrate_to_secs(olsr_cnf->pollrate));
  abuf_appendf(abuf, "<td>TC redundancy: %d</td>\n", olsr_cnf->tc_redundancy);
  abuf_appendf(abuf, "<td>MPR coverage: %d</td>\n", olsr_cnf->mpr_coverage);
  abuf_appendf(abuf, "<td>NAT threshold: %f</td>\n", olsr_cnf->lq_nat_thresh);

  abuf_puts(abuf, "</tr>\n<tr>\n");

  abuf_appendf(abuf, "<td>Fisheye: %s</td>\n", olsr_cnf->lq_fish ? "Enabled" : "Disabled");
  abuf_appendf(abuf, "<td>TOS: 0x%04x</td>\n", olsr_cnf->tos);
  abuf_appendf(abuf, "<td>RtTable: 0x%04x/%d</td>\n", olsr_cnf->rttable, olsr_cnf->rttable);
  abuf_appendf(abuf, "<td>RtTableDefault: 0x%04x/%d</td>\n", olsr_cnf->rttable_default, olsr_cnf->rttable_default);
  abuf_appendf(abuf, "<td>Willingness: %d %s</td>\n", olsr_cnf->willingness, olsr_cnf->willingness_auto ? "(auto)" : "");

  abuf_puts(abuf, "</tr></table>\n");

  abuf_puts(abuf, "<h2>Interfaces</h2>\n");
  abuf_puts(abuf, "<table width=\"100%%\" border=\"0\">\n");
  for (ifs = olsr_cnf->if_configs; ifs != NULL; ifs = ifs->next) {
    const struct interface * const rifs = ifs->interf;
    abuf_appendf(abuf, "<tr><th colspan=\"3\">%s</th>\n", ifs->name);
    if (!rifs) {
      abuf_puts(abuf, "<tr><td colspan=\"3\">Status: DOWN</td></tr>\n");
      continue;
    }

    if (olsr_cnf->ip_version == AF_INET) {
      struct ipaddr_str addrbuf, maskbuf, bcastbuf;
      abuf_appendf(abuf,
		      "<tr>\n"
		      "<td>IP: %s</td>\n"
		      "<td>MASK: %s</td>\n"
		      "<td>BCAST: %s</td>\n"
		      "</tr>\n",
		      ip4_to_string(&addrbuf, rifs->int_addr.sin_addr),
		      ip4_to_string(&maskbuf, rifs->int_netmask.sin_addr),
		      ip4_to_string(&bcastbuf, rifs->int_broadaddr.sin_addr));
    } else {
      struct ipaddr_str addrbuf, maskbuf;
      abuf_appendf(abuf,
		      "<tr>\n"
		      "<td>IP: %s</td>\n"
		      "<td>MCAST: %s</td>\n"
		      "<td></td>\n"
		      "</tr>\n",
		      ip6_to_string(&addrbuf, &rifs->int6_addr.sin6_addr),
		      ip6_to_string(&maskbuf, &rifs->int6_multaddr.sin6_addr));
    }
    abuf_appendf(abuf,
		    "<tr>\n"
		    "<td>MTU: %d</td>\n"
		    "<td>WLAN: %s</td>\n"
		    "<td>STATUS: UP</td>\n"
		    "</tr>\n",
		    rifs->int_mtu,
		    rifs->is_wireless ? "Yes" : "No");
  }
  abuf_puts(abuf, "</table>\n");

  abuf_appendf(abuf,
		  "<em>Olsrd is configured to %s if no interfaces are available</em><br>\n",
		  olsr_cnf->allow_no_interfaces ? "run even" : "halt");

  abuf_puts(abuf, "<h2>Plugins</h2>\n");
  abuf_puts(abuf, "<table width=\"100%%\" border=\"0\"><tr><th>Name</th><th>Parameters</th></tr>\n");
  for (pentry = olsr_cnf->plugins; pentry; pentry = pentry->next) {
    abuf_appendf(abuf,
		    "<tr><td>%s</td>\n"
		    "<td><select>\n"
		    "<option>KEY, VALUE</option>\n",
		    pentry->name);

    for (pparam = pentry->params; pparam; pparam = pparam->next) {
      abuf_appendf(abuf, "<option>\"%s\", \"%s\"</option>\n", pparam->key, pparam->value);
    }
    abuf_puts(abuf, "</select></td></tr>\n");

  }
  abuf_puts(abuf, "</table>\n");

  section_title(abuf, "Announced HNA entries");
  if (list_is_empty(&olsr_cnf->hna_entries)) {
    struct ip_prefix_entry *hna;
    abuf_puts(abuf, "<tr><th>Network</th></tr>\n");
    OLSR_FOR_ALL_IPPREFIX_ENTRIES(&olsr_cnf->hna_entries, hna) {
      struct ipprefix_str netbuf;
      abuf_appendf(abuf,
		      "<tr><td>%s</td></tr>\n",
		      olsr_ip_prefix_to_string(&netbuf, &hna->net));
    } OLSR_FOR_ALL_IPPREFIX_ENTRIES_END()
  } else {
    abuf_puts(abuf, "<tr><td></td></tr>\n");
  }
  abuf_puts(abuf, "</table>\n");
}

static void build_neigh_body(struct autobuf *abuf)
{
  struct neighbor_entry *neigh;
  struct link_entry *lnk;
  const char *colspan = resolve_ip_addresses ? " colspan=\"2\"" : "";

  section_title(abuf, "Links");

  abuf_appendf(abuf,
                   "<tr><th%s>Local IP</th><th%s>Remote IP</th>", colspan, colspan);
  abuf_puts(abuf,
                   "<th>LinkCost</th>");
  abuf_puts(abuf, "</tr>\n");

  /* Link set */
  OLSR_FOR_ALL_LINK_ENTRIES(lnk) {
    struct lqtextbuffer lqbuffer1, lqbuffer2;
    abuf_puts(abuf, "<tr>");
    build_ipaddr_with_link(abuf, &lnk->local_iface_addr, -1);
    build_ipaddr_with_link(abuf, &lnk->neighbor_iface_addr, -1);
    abuf_appendf(abuf,
                     "<td>(%s) %s</td>",
                     get_link_entry_text(lnk, '/', &lqbuffer1),
                     get_linkcost_text(lnk->linkcost, false, &lqbuffer2));
    abuf_puts(abuf, "</tr>\n");
  } OLSR_FOR_ALL_LINK_ENTRIES_END(lnk);

  abuf_puts(abuf, "</table>\n");

  section_title(abuf, "Neighbors");
  abuf_appendf(abuf,
                   "<tr><th%s>IP Address</th><th>SYM</th><th>MPR</th><th>MPRS</th><th>Willingness</th><th>2 Hop Neighbors</th></tr>\n", colspan);
  /* Neighbors */
  OLSR_FOR_ALL_NBR_ENTRIES(neigh) {
    struct neighbor_2_list_entry *list_2;
    int thop_cnt;
    abuf_puts(abuf, "<tr>");
    build_ipaddr_with_link(abuf, &neigh->neighbor_main_addr, -1);
    abuf_appendf(abuf,
                     "<td>%s</td>"
                     "<td>%s</td>"
                     "<td>%s</td>"
                     "<td>%d</td>",
                     (neigh->status == SYM) ? "YES" : "NO",
                     neigh->is_mpr ? "YES" : "NO",
                     olsr_lookup_mprs_set(&neigh->neighbor_main_addr) ? "YES" : "NO",
                     neigh->willingness);

    abuf_puts(abuf, "<td><select>\n"
                     "<option>IP ADDRESS</option>\n");


    for (list_2 = neigh->neighbor_2_list.next, thop_cnt = 0;
         list_2 != &neigh->neighbor_2_list;
         list_2 = list_2->next, thop_cnt++) {
      struct ipaddr_str strbuf;
      abuf_appendf(abuf, "<option>%s</option>\n",
                       olsr_ip_to_string(&strbuf, &list_2->neighbor_2->neighbor_2_addr));
    }
    abuf_appendf(abuf, "</select> (%d)</td></tr>\n", thop_cnt);
  } OLSR_FOR_ALL_NBR_ENTRIES_END(neigh);

  abuf_puts(abuf, "</table>\n");
}

static void build_topo_body(struct autobuf *abuf)
{
  struct tc_entry *tc;
  const char *colspan = resolve_ip_addresses ? " colspan=\"2\"" : "";

  section_title(abuf, "Topology Entries");
  abuf_appendf(abuf, "<tr><th%s>Destination IP</th><th%s>Last Hop IP</th>", colspan, colspan);
  abuf_puts(abuf, "<th colspan=\"3\">Linkcost</th>");
  abuf_puts(abuf, "</tr>\n");

  OLSR_FOR_ALL_TC_ENTRIES(tc) {
      struct tc_edge_entry *tc_edge;
      OLSR_FOR_ALL_TC_EDGE_ENTRIES(tc, tc_edge) {
      	if (tc_edge->edge_inv)  {
          struct lqtextbuffer lqbuffer1, lqbuffer2;
          abuf_puts(abuf, "<tr>");
          build_ipaddr_with_link(abuf, &tc_edge->T_dest_addr, -1);
          build_ipaddr_with_link(abuf, &tc->addr, -1);
          abuf_appendf(abuf,
			     "<td>(%s)</td><td>&nbsp;</td><td>%s</td>\n",
			     get_tc_edge_entry_text(tc_edge, '/', &lqbuffer1),
			     get_linkcost_text(tc_edge->cost, false, &lqbuffer2));
          abuf_puts(abuf, "</tr>\n");
      	}
      } OLSR_FOR_ALL_TC_EDGE_ENTRIES_END(tc, tc_edge);
  } OLSR_FOR_ALL_TC_ENTRIES_END(tc);

  abuf_puts(abuf, "</table>\n");
}

static void build_mid_body(struct autobuf *abuf)
{
  struct tc_entry *tc;
  const char *colspan = resolve_ip_addresses ? " colspan=\"2\"" : "";

  section_title(abuf, "MID Entries");
  abuf_appendf(abuf,
		  "<tr><th%s>Main Address</th><th>Aliases</th></tr>\n", colspan);

  /* MID */
  OLSR_FOR_ALL_TC_ENTRIES(tc) {
    struct mid_entry *alias;
    abuf_puts(abuf, "<tr>");
    build_ipaddr_with_link(abuf, &tc->addr, -1);
    abuf_puts(abuf, "<td><select>\n<option>IP ADDRESS</option>\n");

    OLSR_FOR_ALL_TC_MID_ENTRIES(tc, alias) {
      struct ipaddr_str strbuf;
      abuf_appendf(abuf, "<option>%s</option>\n",
		       olsr_ip_to_string(&strbuf, &alias->mid_alias_addr));
    } OLSR_FOR_ALL_TC_MID_ENTRIES_END(tc, alias);
    abuf_appendf(abuf, "</select> (%u)</td></tr>\n", tc->mid_tree.count);
  } OLSR_FOR_ALL_TC_ENTRIES_END(tc);

  abuf_puts(abuf, "</table>\n");
}


static void build_nodes_body(struct autobuf *abuf)
{
  build_neigh_body(abuf);
  build_topo_body(abuf);
  build_mid_body(abuf);
}

static void build_all_body(struct autobuf *abuf)
{
  build_config_body(abuf);
  build_routes_body(abuf);
  build_nodes_body(abuf);
}


static void build_about_body(struct autobuf *abuf)
{
  abuf_appendf(abuf,
                  "<strong>" PLUGIN_NAME " version " PLUGIN_VERSION "</strong><br/>\n"
                  "by Andreas T&oslash;nnesen (C)2005.<br/>\n"
                  "Compiled "
#if ADMIN_INTERFACE
                           "<em>with experimental admin interface</em> "
#endif
                                                                      "%s at %s<hr/>\n"
                  "This plugin implements a HTTP server that supplies\n"
                  "the client with various dynamic web pages representing\n"
                  "the current olsrd status.<br/>The different pages include:\n"
                  "<ul>\n<li><strong>Configuration</strong> - This page displays information\n"
                  "about the current olsrd configuration. This includes various\n"
                  "olsr settings such as IP version, MID/TC redundancy, hysteresis\n"
                  "etc. Information about the current status of the interfaces on\n"
                  "which olsrd is configured to run is also displayed. Loaded olsrd\n"
                  "plugins are shown with their plugin parameters. Finally all local\n"
                  "HNA entries are shown. These are the networks that the local host\n"
                  "will anounce itself as a gateway to.</li>\n"
                  "<li><strong>Routes</strong> - This page displays all routes currently set in\n"
                  "the kernel <em>by olsrd</em>. The type of route is also displayed(host\n"
                  "or HNA).</li>\n"
                  "<li><strong>Links/Topology</strong> - This page displays all information about\n"
                  "links, neighbors, topology, MID and HNA entries.</li>\n"
                  "<li><strong>All</strong> - Here all the previous pages are displayed as one.\n"
                  "This is to make all information available as easy as possible(for example\n"
                  "for a script) and using as few resources as possible.</li>\n"
#if ADMIN_INTERFACE
                  "<li><strong>Admin</strong> - This page is highly experimental(and unsecure)!\n"
                  "As of now it is not working at all but it provides a impression of\n"
                  "the future possibilities of httpinfo. This is to be a interface to\n"
                  "changing olsrd settings in realtime. These settings include various\n"
                  "\"basic\" settings and local HNA settings.</li>\n"
#endif
                  "<li><strong>About</strong> - this help page.</li>\n</ul>"
                  "<hr/>\n"
                  "Send questions or comments to\n"
                  "<a href=\"mailto:olsr-users@olsr.org\">olsr-users@olsr.org</a> or\n"
                  "<a href=\"mailto:andreto-at-olsr.org\">andreto-at-olsr.org</a><br/>\n"
                  "Official olsrd homepage: <a href=\"http://www.olsr.org/\">http://www.olsr.org</a><br/>\n",
                  build_date, build_host);
}

static void build_cfgfile_body(struct autobuf *abuf)
{
  abuf_puts(abuf,
                   "\n\n"
                   "<strong>This is an automatically generated configuration\n"
                   "file based on the current olsrd configuration of this node.</strong><br/>\n"
                   "<hr/>\n"
                   "<pre>\n");

#ifdef NETDIRECT
  {
        /* Hack to make netdirect stuff work with
           olsr_write_cnf_buf
        */
        char tmpBuf[10000];
        size = olsr_write_cnf_buf(olsr_cnf, true, tmpBuf, 10000);
        snprintf(&buf[size], bufsize-size, tmpBuf);
  }
#else
  olsr_write_cnf_buf(abuf, olsr_cnf, true);
#endif

  abuf_puts(abuf, "</pre>\n<hr/>\n");
}
#if 0
/*
 * In a bigger mesh, there are probs with the fixed
 * bufsize. Because the Content-Length header is
 * optional, the sprintf() is changed to a more
 * scalable solution here.
 */

int netsprintf(char *str, const char* format, ...)
{
	va_list arg;
	int rv;
	va_start(arg, format);
	rv = vsprintf(str, format, arg);
	va_end(arg);
	if (0 != netsprintf_direct) {
		if (0 == netsprintf_error) {
			if (0 > send(client_sockets[curr_clients], str, rv, 0)) {
				OLSR_WARN(LOG_PLUGINS, "(HTTPINFO) Failed sending data to client!\n");
				netsprintf_error = 1;
			}
		}
		return 0;
	}
	return rv;
}
#endif

static ssize_t writen(int fd, const void *buf, size_t count)
{
    size_t bytes_left = count;
    const char *p = buf;
    while (bytes_left > 0) {
        const ssize_t written = write(fd, p, bytes_left);
        if (written == -1)  { /* error */
            if (errno == EINTR ) {
                continue;
            }
            return -1;
        }
        /* We wrote something */
        bytes_left -= written;
        p += written;
    }
    return count;
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

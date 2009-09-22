
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
#include <string.h>

#include "common/autobuf.h"
#include "common/avl.h"
#include "olsr_logging.h"
#include "olsr_cookie.h"
#include "olsr_comport.h"
#include "olsr_comport_http.h"
#include "olsr_comport_txt.h"
#include "olsr_cfg.h"
#include "ipcalc.h"

static const char HTTP_VERSION[] = "HTTP/1.1";
static const char TELNET_PATH[] = "/telnet/";

static struct olsr_cookie_info *htmlsite_cookie;
struct avl_tree http_handler_tree;

/**Response types */
static char http_200_response[] = "OK";
static char http_400_response[] = "Bad Request";
static char http_401_response[] = "Unauthorized";
static char http_403_response[] = "Forbidden";
static char http_404_response[] = "Not Found";
static char http_413_response[] = "Request Entity Too Large";
static char http_501_response[] = "Not Implemented";
static char http_503_response[] = "Service Unavailable";

/* sample for a static html page */
#if 0
static void init_test(void) {
  static char content[] = "<html><body>Yes, you got it !</body></html>";
  static char path[] = "/";
  static char acl[] = "d2lraTpwZWRpYQ=="; /* base 64 .. this is "wikipedia" */
  static char *aclPtr[] = { acl };
  struct olsr_html_site *site;

  site = olsr_com_add_htmlsite(path, content, strlen(content));
  olsr_com_set_htmlsite_acl_auth(site, NULL, 1, aclPtr);
}
#endif

static void
olsr_com_html2telnet_gate(struct comport_connection *con, char *path, int pCount, char *p[]) {
  if (strlen(path) > strlen(TELNET_PATH)) {
    char *cmd = &path[strlen(TELNET_PATH)];
    char *next;
    int count = 1;
    enum olsr_txtcommand_result result;

    while (cmd) {
      next = strchr(cmd, '/');
      if (next) {
        *next++ = 0;
      }

      result = olsr_com_handle_txtcommand(con, cmd, pCount > count ? p[count] : NULL);

      /* sorry, no continous output */
      if (result == CONTINOUS) {
        con->stop_handler(con);
      }
      else if (result == UNKNOWN) {
        abuf_appendf(&con->out, "Unknown command %s\n", cmd);
        con->send_as = HTTP_404_NOT_FOUND;
      }
      else if (result != CONTINUE) {
        abuf_appendf(&con->out, "Http-Telnet gate had problems with command %s\n", cmd);
        con->send_as = HTTP_400_BAD_REQ;
      }
      cmd = next;
      count += 2;
    }
  }
}

void
olsr_com_init_http(void) {
  avl_init(&http_handler_tree, &avl_comp_strcasecmp);

  htmlsite_cookie = olsr_alloc_cookie("comport http sites", OLSR_COOKIE_TYPE_MEMORY);
  olsr_cookie_set_memory_size(htmlsite_cookie, sizeof(struct olsr_html_site));

  /* activate telnet gateway */
  olsr_com_add_htmlhandler(olsr_com_html2telnet_gate, TELNET_PATH);
  //init_test();
}

void olsr_com_destroy_http(void) {
  struct olsr_html_site *site;
  OLSR_FOR_ALL_HTML_ENTRIES(site) {
    olsr_com_remove_htmlsite(site);
  } OLSR_FOR_ALL_HTML_ENTRIES_END()
}

struct olsr_html_site *
olsr_com_add_htmlsite(char *path, char *content, size_t length) {
  struct olsr_html_site *site;

  site = olsr_cookie_malloc(htmlsite_cookie);
  site->node.key = strdup(path);

  site->static_site = true;
  site->site_data = content;
  site->site_length = length;

  avl_insert(&http_handler_tree, &site->node, false);
  return site;
}

struct olsr_html_site *
olsr_com_add_htmlhandler(void(*sitehandler)(struct comport_connection *con, char *path, int parameter_count, char *parameters[]),
    const char *path) {
  struct olsr_html_site *site;

  site = olsr_cookie_malloc(htmlsite_cookie);
  site->node.key = strdup(path);

  site->static_site = false;
  site->sitehandler = sitehandler;

  avl_insert(&http_handler_tree, &site->node, false);
  return site;
}

void
olsr_com_remove_htmlsite(struct olsr_html_site *site) {
  avl_delete(&http_handler_tree, &site->node);
  free(site->node.key);
  olsr_cookie_free(htmlsite_cookie, site);
}

void
olsr_com_set_htmlsite_acl_auth(struct olsr_html_site *site, struct ip_acl *ipacl, int auth_count, char **auth_entries) {
  site->acl = ipacl;
  site->auth_count = auth_count;
  site->auth = auth_entries;
}

/* handle the html site. returns true on successful handling (even if it was unauthorized)
 * false if we did not find a site
*/
bool
olsr_com_handle_htmlsite(struct comport_connection *con, char *path,
    char *fullpath, int para_count, char **para) {
  char *str;
  int i;
  struct olsr_html_site *site;

  site = (struct olsr_html_site *)avl_find(&http_handler_tree, path);
  if (site == NULL) {
    OLSR_DEBUG(LOG_COMPORT, "No httphandler found for path %s\n", path);
    return false;
  }

  /* check if username/password is necessary */
  if (site->auth) {
    /* test for correct ACL */
    char key[256] = { 0 };

    con->send_as = HTTP_401_UNAUTHORIZED;

    str = strstr(con->in.buf, "\nAuthorization: Basic ");
    if (str != NULL && sscanf(str + 1, "%*s %*s %s\n", key) == 1) {
      OLSR_DEBUG(LOG_COMPORT, "ACL string received: %s\n", key);
      for (i = 0; i < site->auth_count; i++) {
        if (strcmp(site->auth[i], key) == 0) {
          con->send_as = HTTP_200_OK;
          break;
        }
      }
    }
    if (con->send_as == HTTP_401_UNAUTHORIZED) {
      OLSR_DEBUG(LOG_COMPORT, "Error, invalid authorization\n");
      return true;
    }
  }

  /* check if ip is allowed */
  if (site->acl != NULL && !ip_acl_acceptable(site->acl, &con->addr, olsr_cnf->ip_version)) {
    struct ipaddr_str buf;

    con->send_as = HTTP_403_FORBIDDEN;
    OLSR_DEBUG(LOG_COMPORT, "Error, access by IP %s is not allowed for path %s\n",
        path, olsr_ip_to_string(&buf, &con->addr));
    return true;
  }

  /* call site handler */
  if (site->static_site) {
    abuf_memcpy(&con->out, site->site_data, site->site_length);
  } else {
    site->sitehandler(con, fullpath, para_count, para);
  }
  con->send_as = HTTP_200_OK;
  return true;
}

void
olsr_com_build_httpheader(struct comport_connection *con) {
  struct autobuf buf;
  time_t currtime;

  abuf_init(&buf, 1024);

  abuf_appendf(&buf, "%s %d %s\r\n", HTTP_VERSION, con->send_as, olsr_com_get_http_message(con->send_as));

  /* Date */
  time(&currtime);
  abuf_strftime(&buf, "Date: %a, %d %b %Y %H:%M:%S GMT\r\n", localtime(&currtime));

  /* Server version */
  abuf_appendf(&buf, "Server: %s %s %s %s\r\n", olsrd_version, build_date, build_host, HTTP_VERSION);

  /* connection-type */
  abuf_puts(&buf, "Connection: closed\r\n");

  /* MIME type */
  abuf_appendf(&buf, "Content-type: text/%s\r\n", con->send_as != HTTP_PLAIN ? "html" : "plain");

  /* Content length */
  if (con->out.len > 0) {
    abuf_appendf(&buf, "Content-length: %u\r\n", con->out.len);
  }

  if (con->send_as == HTTP_401_UNAUTHORIZED) {
    abuf_appendf(&buf, "WWW-Authenticate: Basic realm=\"%s\"\r\n", "RealmName");
  }
  /* Cache-control
   * No caching dynamic pages
   */
  abuf_puts(&buf, "Cache-Control: no-cache\r\n");

  if (con->send_as == HTTP_PLAIN) {
    abuf_puts(&buf, "Accept-Ranges: bytes\r\n");
  }
  /* End header */
  abuf_puts(&buf, "\r\n");

  abuf_memcpy_prefix(&con->out, buf.buf, buf.len);
  OLSR_DEBUG(LOG_PLUGINS, "HEADER:\n%s", buf.buf);

  abuf_free(&buf);
}

void
olsr_com_create_httperror(struct comport_connection *con) {
  abuf_appendf(&con->out, "<body><h1>HTTP error %d: %s</h1></body>", con->send_as, olsr_com_get_http_message(con->send_as));
}

char *
olsr_com_get_http_message(enum http_header_type type) {
  static char nothing[] = "";

  switch (type) {
    case HTTP_PLAIN:
    case HTTP_200_OK:
      return http_200_response;
    case HTTP_400_BAD_REQ:
      return http_400_response;
    case HTTP_401_UNAUTHORIZED:
      return http_401_response;
    case HTTP_403_FORBIDDEN:
      return http_403_response;
    case HTTP_404_NOT_FOUND:
      return http_404_response;
    case HTTP_413_REQUEST_TOO_LARGE:
      return http_413_response;
    case HTTP_501_NOT_IMPLEMENTED:
      return http_501_response;
    case HTTP_503_SERVICE_UNAVAILABLE:
      return http_503_response;
    default:
      return nothing;
  }
}

void olsr_com_decode_url(char *str) {
  char *dst = str;

  while (*str) {
    if (*str == '%') {
      int value = 0;

      str++;
      sscanf(str, "%02x", &value);
      *dst++ = (char) value;
      str += 2;
    } else {
      *dst++ = *str++;
    }
  }
  *dst = 0;
}

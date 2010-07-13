
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

#include <assert.h>
#include <string.h>

#include "common/autobuf.h"
#include "common/avl.h"
#include "common/avl_olsr_comp.h"
#include "common/string.h"
#include "olsr_logging.h"
#include "olsr_cookie.h"
#include "olsr_comport.h"
#include "olsr_comport_http.h"
#include "olsr_comport_txt.h"
#include "olsr_cfg.h"
#include "ipcalc.h"

#define HTTP_TESTSITE

static const char HTTP_VERSION[] = "HTTP/1.0";
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

static bool parse_http_header(char *message, size_t message_len, struct http_request *request);

/* sample for a static html page */
#ifdef HTTP_TESTSITE

static void
test_handler(struct comport_connection *con, struct http_request *request) {
  size_t i;

  abuf_puts(&con->out, "<html><body>");
  abuf_appendf(&con->out, "<br>Request: %s</br>\n", request->method);
  abuf_appendf(&con->out, "<br>Filename: %s</br>\n", request->request_uri);
  abuf_appendf(&con->out, "<br>Http-Version: %s</br>\n", request->http_version);

  for (i=0; i<request->query_count; i++) {
    abuf_appendf(&con->out, "<br>URL-Parameter: %s = %s</br>\n", request->query_name[i], request->query_value[i]);
  }
  for (i=0; i<request->header_count; i++) {
    abuf_appendf(&con->out, "<br>Header: %s = %s</br>\n", request->header_name[i], request->header_value[i]);
  }
  abuf_puts(&con->out, "</body></html>");
}

static void init_test(void) {
  static char content[] = "<html><body>Yes, you got it !</body></html>";
  static char acl[] = "d2lraTpwZWRpYQ=="; /* base 64 .. this is "wiki:pedia" */
  static char *aclPtr[] = { acl };
  struct olsr_html_site *site;

  site = olsr_com_add_htmlsite("/", content, strlen(content));
  olsr_com_set_htmlsite_acl_auth(site, NULL, 1, aclPtr);

  olsr_com_add_htmlhandler(test_handler, "/print/");
}
#endif

static void
olsr_com_html2telnet_gate(struct comport_connection *con, struct http_request *request) {
  if (strlen(request->request_uri) > strlen(TELNET_PATH)) {
    char *cmd = &request->request_uri[strlen(TELNET_PATH)];
    char *next;
    size_t count = 1;
    enum olsr_txtcommand_result result;

    while (cmd) {
      next = strchr(cmd, '/');
      if (next) {
        *next++ = 0;
      }

      result = olsr_com_handle_txtcommand(con, cmd, request->query_count > count ? request->query_value[count] : NULL);

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
      count ++;
    }
  }
}

void
olsr_com_init_http(void) {
  avl_init(&http_handler_tree, &avl_comp_strcasecmp, false, NULL);

  htmlsite_cookie = olsr_alloc_cookie("comport http sites", OLSR_COOKIE_TYPE_MEMORY);
  olsr_cookie_set_memory_size(htmlsite_cookie, sizeof(struct olsr_html_site));

  /* activate telnet gateway */
  olsr_com_add_htmlhandler(olsr_com_html2telnet_gate, TELNET_PATH);
#ifdef HTTP_TESTSITE
  init_test();
#endif
}

void olsr_com_destroy_http(void) {
  struct olsr_html_site *site;
  struct list_iterator iterator;

  OLSR_FOR_ALL_HTML_ENTRIES(site, iterator) {
    olsr_com_remove_htmlsite(site);
  }
}

struct olsr_html_site *
olsr_com_add_htmlsite(const char *path, const char *content, size_t length) {
  struct olsr_html_site *site;

  site = olsr_cookie_malloc(htmlsite_cookie);
  site->node.key = strdup(path);

  site->static_site = true;
  site->site_data = content;
  site->site_length = length;

  avl_insert(&http_handler_tree, &site->node);
  return site;
}

struct olsr_html_site *
olsr_com_add_htmlhandler(void(*sitehandler)(struct comport_connection *con, struct http_request *request),
    const char *path) {
  struct olsr_html_site *site;

  site = olsr_cookie_malloc(htmlsite_cookie);
  site->node.key = strdup(path);

  site->static_site = false;
  site->sitehandler = sitehandler;

  avl_insert(&http_handler_tree, &site->node);
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
static bool
olsr_com_handle_htmlsite(struct comport_connection *con, char *path,
    struct http_request *request) {
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
#if !defined(REMOVE_LOG_DEBUG)
    struct ipaddr_str buf;
#endif
    con->send_as = HTTP_403_FORBIDDEN;
    OLSR_DEBUG(LOG_COMPORT, "Error, access by IP %s is not allowed for path %s\n",
        path, olsr_ip_to_string(&buf, &con->addr));
    return true;
  }

  /* call site handler */
  if (site->static_site) {
    abuf_memcpy(&con->out, site->site_data, site->site_length);
  } else {
    site->sitehandler(con, request);
  }
  con->send_as = HTTP_200_OK;
  return true;
}

static bool parse_http_header(char *message, size_t message_len, struct http_request *request) {
  size_t header_index;

  assert(message);
  assert(request);

  memset(request, 0, sizeof(struct http_request));
  request->method = message;

  while(true) {
    if (message_len < 2) {
      goto unexpected_end;
    }

    if (*message == ' ' && request->http_version == NULL) {
      *message = '\0';

      if (request->request_uri == NULL) {
        request->request_uri = &message[1];
      }
      else if (request->http_version == NULL) {
        request->http_version = &message[1];
      }
    }
    else if (*message == '\r') {
      *message = '\0';
    }
    else if (*message == '\n') {
      *message = '\0';

      message++; message_len--;
      break;
    }

    message++; message_len--;
  }

  if (request->http_version == NULL) {
    goto unexpected_end;
  }

  for(header_index = 0; true; header_index++) {
    if (message_len < 1) {
      goto unexpected_end;
    }

    if (*message == '\n') {
      break;
    }
    else if (*message == '\r') {
      if (message_len < 2) return true;

      if (message[1] == '\n') {
        break;
      }
    }

    if (header_index >= MAX_HTTP_HEADERS) {
      goto too_many_fields;
    }

    request->header_name[header_index] = message;

    while(true) {
      if (message_len < 1) {
        goto unexpected_end;
      }

      if (*message == ':') {
        *message = '\0';

        message++; message_len--;
        break;
      }
      else if (*message == ' ' || *message == '\t') {
        *message = '\0';
      }
      else if (*message == '\n' || *message == '\r') {
        goto unexpected_end;
      }

      message++; message_len--;
    }

    while(true) {
      if (message_len < 1) {
        goto unexpected_end;
      }

      if (request->header_value[header_index] == NULL) {
        if (*message != ' ' && *message != '\t') {
          request->header_value[header_index] = message;
        }
      }

      if (*message == '\n') {
        if (message_len < 2) {
          goto unexpected_end;
        }

        if (message[1] == ' ' || message[1] == '\t') {
          *message = ' ';
          message[1] = ' ';

          message += 2; message_len -= 2;
          continue;
        }

        *message = '\0';

        if (request->header_value[header_index] == NULL) {
          request->header_value[header_index] = message;
        }

        message++; message_len--;
        break;
      }
      else if (*message == '\r') {
        if (message_len < 2) {
          goto unexpected_end;
        }

        if (message[1] == '\n') {
          if (message_len < 3) {
            goto unexpected_end;
          }

          if (message[2] == ' ' || message[2] == '\t') {
            *message = ' ';
            message[1] = ' ';
            message[2] = ' ';

            message += 3; message_len -= 3;
            continue;
          }

          *message = '\0';

          if (request->header_value[header_index] == NULL) {
            request->header_value[header_index] = message;
          }

          message += 2; message_len -= 2;
          break;
        }
      }

      message++; message_len--;
    }
  }

  request->header_count = header_index;
  return false;

too_many_fields:
  OLSR_DEBUG(LOG_COMPORT, "Error, too many HTTP header fields\n");
  return true;

unexpected_end:
  OLSR_DEBUG(LOG_COMPORT, "Error, unexpected end of HTTP header\n");
  return true;
}

static size_t parse_query_string(char *s, char **name, char **value, size_t count) {
  char *ptr;
  size_t i = 0;

  assert(s);
  assert(name);
  assert(value);

  while (s != NULL && i < count) {
    name[i] = s;

    s = strchr(s, '&');
    if (s != NULL) {
      *s++ = '\0';
    }

    ptr = strchr(name[i], '=');
    if (ptr != NULL) {
      *ptr++ = '\0';
      value[i] = ptr;
    } else {
      value[i] = &name[i][strlen(name[i])];
    }

    if(name[i][0] != '\0') {
      i++;
    }
  }

  return i;
}

void olsr_com_parse_http(struct comport_connection *con,
    unsigned int flags  __attribute__ ((unused))) {
  struct http_request request;
  char *para = NULL, *str = NULL;
  char processed_filename[256];
  int idx = 0;
  size_t i = 0;

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

  if (parse_http_header(con->in.buf, con->in.len, &request)) {
    OLSR_DEBUG(LOG_COMPORT, "Error, illegal http header.\n");
    con->send_as = HTTP_400_BAD_REQ;
    con->state = SEND_AND_QUIT;
    return;
  }

  if (strcasecmp(request.http_version, "HTTP/1.1") != 0 && strcasecmp(request.http_version, "HTTP/1.0") != 0) {
    OLSR_DEBUG(LOG_COMPORT, "Unknown Http-Version: '%s'\n", request.http_version);
    con->send_as = HTTP_400_BAD_REQ;
    con->state = SEND_AND_QUIT;
    return;
  }

  OLSR_DEBUG(LOG_COMPORT, "HTTP Request: %s %s %s\n", request.method, request.request_uri, request.http_version);

  /* store a copy for the http_handlers */
  strscpy(processed_filename, request.request_uri, sizeof(processed_filename));
  if (strcmp(request.method, "POST") == 0) {
    /* load the rest of the header for POST commands */
    int clen;

    for (i=0, clen=-1; i<request.header_count; i++) {
      if (strcasecmp(request.header_name[i], "Content-Length") == 0) {
        clen = atoi(request.header_value[i]);
        break;
      }
    }

    if (clen == -1) {
      con->send_as = HTTP_400_BAD_REQ;
      con->state = SEND_AND_QUIT;
      return;
    }

    if (con->in.len < idx + clen) {
      /* we still need more data */
      return;
    }

    request.form_count = parse_query_string(&con->in.buf[idx], request.form_name, request.form_value, MAX_HTTP_FORM);
  }

  /* strip the URL marker away */
  str = strchr(processed_filename, '#');
  if (str) {
    *str = 0;
  }

  /* we have everything to process the http request */
  con->state = SEND_AND_QUIT;
  olsr_com_decode_url(processed_filename);

  if (strcmp(request.method, "GET") == 0) {
    /* HTTP-GET request */
    para = strchr(processed_filename, '?');
    if (para != NULL) {
      *para++ = 0;
      request.query_count = parse_query_string(para, request.query_name, request.query_value, MAX_HTTP_QUERY);
    }
  } else if (strcmp(request.method, "POST") != 0) {
    con->send_as = HTTP_501_NOT_IMPLEMENTED;
    return;
  }

  /* create body */
  i = strlen(processed_filename);

  /*
   * add a '/' at the end if it's not there to detect
   *  paths without terminating '/' from the browser
   */
  if (processed_filename[i - 1] != '/' && request.query_count == 0) {
    strcat(processed_filename, "/");
  }

  while (i > 0) {
    if (olsr_com_handle_htmlsite(con, processed_filename, &request)) {
      return;
    }

    /* try to find a handler for a path prefix */
    if (i > 0 && processed_filename[i] == '/') {
      processed_filename[i--] = 0;
    }
    else {
      do {
        processed_filename[i--] = 0;
      } while (i > 0 && processed_filename[i] != '/');
    }
  }
  con->send_as = HTTP_404_NOT_FOUND;
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
  if (con->http_contenttype == NULL) {
    con->http_contenttype = con->send_as != HTTP_PLAIN ? "text/html" : "text/plain";
  }
  abuf_appendf(&buf, "Content-type: %s\r\n", con->http_contenttype);

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
    if (*str == '%' && str[1] && str[2]) {
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

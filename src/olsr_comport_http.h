
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
#ifndef OLSR_COMPORT_HTTP_H_
#define OLSR_COMPORT_HTTP_H_

#include "common/avl.h"
#include "common/autobuf.h"
#include "olsr_ip_acl.h"
#include "olsr_comport.h"

#define MAX_HTTP_FORM 8
#define MAX_HTTP_QUERY 8
#define MAX_HTTP_HEADERS 32

/* this is the data about the http header delivered to the html site handler */
struct http_request {
  char *method; /* get/post/... */
  char *request_uri;
  char *http_version;

  /* fields of the http header */
  char *header_name[MAX_HTTP_HEADERS];
  char *header_value[MAX_HTTP_HEADERS];
  size_t header_count;

  /* parameter of the URI for GET */
  char *query_name[MAX_HTTP_QUERY];
  char *query_value[MAX_HTTP_QUERY];
  size_t query_count;

  /* parameter of the URI for POST */
  char *form_name[MAX_HTTP_FORM];
  char *form_value[MAX_HTTP_FORM];

  size_t form_count;
};


/* this is a html site datastructure for each site */
/* it is stored in an AVL tree */
struct olsr_html_site {
  struct avl_node node;
  char *path;

  bool static_site;   /* is this a static site y/n? */

  char **auth;		  /* ptr to list of char* name=passwd in base64 */
  int auth_count;	  /* number of list entries */

  struct ip_acl *acl; /* allow only certain IPs ? */

	/* for static sites... */
  const char *site_data;
  size_t site_length;

	/* for non static, this is the handler */
  void (*sitehandler)(struct comport_connection *con, struct http_request *request);
};

#define OLSR_FOR_ALL_HTML_ENTRIES(site, iterator) avl_for_each_element_safe(&http_handler_tree, site, node, iterator)

void olsr_com_init_http(void);
void olsr_com_destroy_http(void);

struct olsr_html_site *EXPORT(olsr_com_add_htmlsite) (
    const char *path, const char *content, size_t length);
struct olsr_html_site *EXPORT(olsr_com_add_htmlhandler) (
    void (*sitehandler)(struct comport_connection *con, struct http_request *request),
    const char *path);
void EXPORT(olsr_com_remove_htmlsite) (struct olsr_html_site *site);
void EXPORT(olsr_com_set_htmlsite_acl_auth) (struct olsr_html_site *site,
    struct ip_acl *acl, int auth_count, char **auth_entries);

void olsr_com_parse_http(struct comport_connection *con,
    unsigned int flags  __attribute__ ((unused)));
void EXPORT(olsr_com_build_httpheader) (struct comport_connection *con);
void EXPORT(olsr_com_create_httperror) (struct comport_connection *con);
char *EXPORT(olsr_com_get_http_message) (enum http_header_type type);
void EXPORT(olsr_com_decode_url) (char *str);

extern struct avl_tree EXPORT(http_handler_tree);

#endif /* OLSR_COMPORT_HTTP_H_ */

/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004, Andreas Tønnesen(andreto@olsr.org)
 *                     includes code by Bruno Randolf
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
 * $Id: olsrd_httpinfo.h,v 1.1 2004/12/15 20:11:50 kattemat Exp $
 */

/*
 * Dynamic linked library for the olsr.org olsr daemon
 */

#ifndef _OLSRD_HTTP_INFO
#define _OLSRD_HTTP_INFO

#include "olsrd_plugin.h"


#define HTTP_VERSION "HTTP/1.1"

/**Response types */
#define HTTP_100 HTTP_VERSION " 100 Continue\r\n"
#define HTTP_101 HTTP_VERSION " 101 Switching Protocols\r\n"
#define HTTP_200 HTTP_VERSION " 200 OK\r\n"
#define HTTP_201 HTTP_VERSION " 201 Created\r\n"
#define HTTP_202 HTTP_VERSION " 202 Accepted\r\n"
#define HTTP_203 HTTP_VERSION " 203 Non-Authoritative Information\r\n"
#define HTTP_204 HTTP_VERSION " 204 No Content\r\n"
#define HTTP_205 HTTP_VERSION " 205 Reset Content\r\n"
#define HTTP_206 HTTP_VERSION " 206 Partial Content\r\n"
#define HTTP_300 HTTP_VERSION " 300 Multiple Choices\r\n"
#define HTTP_301 HTTP_VERSION " 301 Moved Permanently\r\n"
#define HTTP_302 HTTP_VERSION " 302 Found\r\n"
#define HTTP_303 HTTP_VERSION " 303 See Other\r\n"
#define HTTP_304 HTTP_VERSION " 304 Not Modified\r\n"
#define HTTP_305 HTTP_VERSION " 305 Use Proxy\r\n"
#define HTTP_307 HTTP_VERSION " 307 Temporary Redirect\r\n"
#define HTTP_400 HTTP_VERSION " 400 Bad Request\r\n"
#define HTTP_401 HTTP_VERSION " 401 Unauthorized\r\n"
#define HTTP_402 HTTP_VERSION " 402 Payment Required\r\n"
#define HTTP_403 HTTP_VERSION " 403 Forbidden\r\n"
#define HTTP_404 HTTP_VERSION " 404 Not Found\r\n"
#define HTTP_405 HTTP_VERSION " 405 Method Not Allowed\r\n"
#define HTTP_406 HTTP_VERSION " 406 Not Acceptable\r\n"
#define HTTP_407 HTTP_VERSION " 407 Proxy Authentication Required\r\n"
#define HTTP_408 HTTP_VERSION " 408 Request Time-out\r\n"


#define HTTP_400_MSG "<html><h1>400 - ERROR</h1><hr><i>" PLUGIN_NAME " version " PLUGIN_VERSION  "</i></html>"
#define HTTP_404_MSG "<html><h1>404 - ERROR, no such file</h1><hr>This server does not support file requests!<br><br><i>" PLUGIN_NAME " version " PLUGIN_VERSION  "</i></html>"
#define HTTP_200_MSG "<html><h1>OLSRD HTTP INFO-PLUGIN</h1><hr><i>" PLUGIN_NAME " version " PLUGIN_VERSION  "</i></html>"

typedef enum
  {
    HTTP_BAD_REQ,
    HTTP_BAD_FILE,
    HTTP_OK
  }http_header_type;

char netmask[5];

char *
olsr_ip_to_string(union olsr_ip_addr *);

char *
olsr_netmask_to_string(union hna_netmask *);


#endif

/*
 * HTTP Info plugin for the olsr.org OLSR daemon
 * Copyright (c) 2004, Andreas Tønnesen(andreto@olsr.org)
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
 * $Id: olsrd_httpinfo.c,v 1.6 2004/12/17 11:44:30 kattemat Exp $
 */

/*
 * Dynamic linked library for the olsr.org olsr daemon
 */

#include "olsrd_httpinfo.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#define MAX_CLIENTS 3

#define MAX_HTTPREQ_SIZE 1024 * 10

#define DEFAULT_TCP_PORT 8080

static int
get_http_socket(int);

void
parse_http_request(int);

int
build_http_header(http_header_type, olsr_u32_t, char *, olsr_u32_t);

static int
build_frame(char *, char *, olsr_u32_t, int(*frame_body_cb)(char *, olsr_u32_t));

int
build_status_body(char *, olsr_u32_t);

int
build_neigh_body(char *, olsr_u32_t);

int
build_topo_body(char *, olsr_u32_t);

int
build_hna_body(char *, olsr_u32_t);

int
build_mid_body(char *, olsr_u32_t);


static int client_sockets[MAX_CLIENTS];
static int curr_clients;
static int http_socket;

/**
 *Do initialization here
 *
 *This function is called by the my_init
 *function in uolsrd_plugin.c
 */
int
olsr_plugin_init()
{

  curr_clients = 0;
  /* set up HTTP socket */
  http_socket = get_http_socket(http_port != 0 ? http_port :  DEFAULT_TCP_PORT);

  if(http_socket < 0)
    {
      fprintf(stderr, "(HTTPINFO) could not initialize HTTP socket\n");
      exit(0);
    }

  /* Register socket */
  add_olsr_socket(http_socket, &parse_http_request);

  return 1;
}

static int
get_http_socket(int port)
{
  struct sockaddr_in sin;
  olsr_u32_t yes = 1;
  int s;

  /* Init ipc socket */
  if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
    {
      olsr_printf(1, "(HTTPINFO)socket %s\n", strerror(errno));
      return -1;
    }

  if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(yes)) < 0) 
    {
      perror("SO_REUSEADDR failed");
      close(s);
      return -1;
    }

  /* Bind the socket */
  
  /* complete the socket structure */
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_port = htons(port);
  
  /* bind the socket to the port number */
  if (bind(s, (struct sockaddr *) &sin, sizeof(sin)) == -1) 
    {
      olsr_printf(1, "(HTTPINFO) bind failed %s\n", strerror(errno));
      return -1;
    }
      
  /* show that we are willing to listen */
  if (listen(s, 1) == -1) 
    {
      olsr_printf(1, "(HTTPINFO) listen failed %s\n", strerror(errno));
      return -1;
    }

  return s;
}


/* Non reentrant - but we are not multithreaded anyway */
void
parse_http_request(int fd)
{
  struct sockaddr_in pin;
  socklen_t addrlen;
  char *addr;  
  char req[MAX_HTTPREQ_SIZE];
  static char body[1024*50];
  char req_type[11];
  char filename[251];
  char http_version[11];
  int c = 0, r = 1;

  addrlen = sizeof(struct sockaddr_in);

  if(curr_clients >= MAX_CLIENTS)
    return;

  curr_clients++;

  if ((client_sockets[curr_clients] = accept(fd, (struct sockaddr *)  &pin, &addrlen)) == -1)
    {
      olsr_printf(1, "(HTTPINFO) accept: %s\n", strerror(errno));
      goto close_connection;
    }

  addr = inet_ntoa(pin.sin_addr);


  memset(req, 0, MAX_HTTPREQ_SIZE);
  memset(body, 0, 1024*10);

  while((r = recv(client_sockets[curr_clients], &req[c], 1, 0)) > 0 && (c < (MAX_HTTPREQ_SIZE-1)))
    {
      c++;

      if((c > 3 && !strcmp(&req[c-4], "\r\n\r\n")) ||
	 (c > 1 && !strcmp(&req[c-2], "\n\n")))
	break;
    }
  
  if(r < 0)
    {
      printf("(HTTPINFO) Failed to recieve data from client!\n");
      goto close_connection;
    }
  
  /* Get the request */
  if(sscanf(req, "%10s %250s %10s\n", req_type, filename, http_version) != 3)
    {
      /* Try without HTTP version */
      if(sscanf(req, "%10s %250s\n", req_type, filename) != 2)
	{
	  printf("(HTTPINFO) Error parsing request %s!\n", req);
	  goto close_connection;
	}
    }
  
  
  printf("Request: %s\nfile: %s\nVersion: %s\n\n", req_type, filename, http_version);

  if(strcmp(req_type, "GET"))
    {
      /* We only support GET */
      strcpy(body, HTTP_400_MSG);
      c = build_http_header(HTTP_BAD_REQ, strlen(body), req, MAX_HTTPREQ_SIZE);
    }
  else if(strlen(filename) > 1)
    {
      /* We only support request for / */
      strcpy(body, HTTP_404_MSG);
      c = build_http_header(HTTP_BAD_FILE, strlen(body), req, MAX_HTTPREQ_SIZE);
    }
  else
    {
      int i = 0;
      while(http_ok_head[i])
          {
              strcat(body, http_ok_head[i]);
              i++;
          }
      printf("\n\n");
      /* All is good */

      build_frame("Status", &body[strlen(body)], MAX_HTTPREQ_SIZE - strlen(body), &build_status_body);
      build_frame("Neighbors", &body[strlen(body)], MAX_HTTPREQ_SIZE - strlen(body), &build_neigh_body);
      build_frame("Topology", &body[strlen(body)], MAX_HTTPREQ_SIZE - strlen(body), &build_topo_body);
      build_frame("HNA", &body[strlen(body)], MAX_HTTPREQ_SIZE - strlen(body), &build_hna_body);
      build_frame("MID", &body[strlen(body)], MAX_HTTPREQ_SIZE - strlen(body), &build_mid_body);

      i = 0;
      while(http_ok_tail[i])
          {
              strcat(body, http_ok_tail[i]);
              i++;
          }


      c = build_http_header(HTTP_OK, strlen(body), req, MAX_HTTPREQ_SIZE);
    }
  
  r = send(client_sockets[curr_clients], req, c, 0);   
  if(r < 0)
    {
      printf("(HTTPINFO) Failed sending data to client!\n");
      goto close_connection;
    }

  r = send(client_sockets[curr_clients], body, strlen(body), 0);
  if(r < 0)
    {
      printf("(HTTPINFO) Failed sending data to client!\n");
      goto close_connection;
    }

 close_connection:
  close(client_sockets[curr_clients]);
  curr_clients--;

}


int
build_http_header(http_header_type type, olsr_u32_t size, char *buf, olsr_u32_t bufsize)
{
  time_t currtime;
  char timestr[45];
  char modtimestr[50];
  char tmp[30];

  memset(buf, 0, bufsize);

  switch(type)
    {
    case(HTTP_BAD_REQ):
      memcpy(buf, HTTP_400, strlen(HTTP_400));
      break;
    case(HTTP_BAD_FILE):
      memcpy(buf, HTTP_404, strlen(HTTP_404));
      break;
    default:
      /* Defaults to OK */
      memcpy(buf, HTTP_200, strlen(HTTP_200));
      break;
    }


  /* Date */
  if(time(&currtime))
    {
      strftime(timestr, 45, "Date: %a, %d %b %Y %H:%M:%S GMT\r\n", gmtime(&currtime));
      strcat(buf, timestr);
    }
  
  /* Server version */
  strcat(buf, "Server: ");
  strcat(buf, PLUGIN_NAME " ");
  strcat(buf, PLUGIN_VERSION " ");
  strcat(buf, HTTP_VERSION);
  strcat(buf, "\r\n");

  /* connection-type */
  strcat(buf,"Connection: closed\r\n");

  /* MIME type */
  strcat(buf, "Content-type: text/html\r\n");

  /* Content length */
  if(size > 0)
    {
      sprintf(tmp, "Content-length: %i\r\n", size);
      strcat(buf, tmp);
    }


  /* Cache-control 
   * No caching dynamic pages
   */
  strcat(buf, "Cache-Control: no-cache\r\n");


  /* End header */
  strcat(buf, "\r\n");
  
  printf("HEADER:\n%s", buf);

  return strlen(buf);

}

/*
 * destructor - called at unload
 */
void
olsr_plugin_exit()
{
  if(http_socket)
    close(http_socket);
}


/* Mulitpurpose funtion */
int
plugin_io(int cmd, void *data, size_t size)
{

  switch(cmd)
    {
    default:
      return 0;
    }
  
  return 1;

}


static int
build_frame(char *title, char *buf, olsr_u32_t size, int(*frame_body_cb)(char *, olsr_u32_t))
{
  int i;
  printf("Building frame \"%s\"\n", title);
  strcat(buf, http_frame[0]);
  sprintf(&buf[strlen(buf)], http_frame[1], title);

  i = 2;

  while(http_frame[i])
    {
      if(!strcmp(http_frame[i], "<!-- BODY -->"))
	frame_body_cb(&buf[strlen(buf)], size - strlen(buf));
      else
	strcat(buf, http_frame[i]);      

      i++;
    }

  return strlen(buf);
}


int
build_status_body(char *buf, olsr_u32_t bufsize)
{
    char systime[100];
    time_t currtime;

    time(&currtime);
    strftime(systime, 100, "System time: <i>%a, %d %b %Y %H:%M:%S</i><br>", gmtime(&currtime));
    
    strcat(buf, systime);
}



int
build_neigh_body(char *buf, olsr_u32_t bufsize)
{
  struct neighbor_entry *neighbor_table_tmp;
  struct neighbor_2_list_entry *list_2;
  int size = 0, index;

  size += sprintf(&buf[size], "Links\n");
  size += sprintf(&buf[size], "<table width=100% BORDER=0 CELLSPACING=0 CELLPADDING=0 ALIGN=center><tr BGCOLOR=\"#AAAAAA\"><td>IP address</td><td>Hysteresis</td><td>LinkQuality</td><td>lost</td><td>total</td><td>NLQ</td><td>ETX</td></tr>\n");

  size += sprintf(&buf[size], "</table><hr>\n");

  size += sprintf(&buf[size], "Neighbors\n");
  size += sprintf(&buf[size], "<table width=100% BORDER=0 CELLSPACING=0 CELLPADDING=0 ALIGN=center><tr BGCOLOR=\"#AAAAAA\"><td>IP address</td><td>SYM</td><td>MPR</td><td>MPRS</td><td>Willingness</td></tr>\n");

  /* Neighbors */
  for(index=0;index<HASHSIZE;index++)
    {
      for(neighbor_table_tmp = neighbortable[index].next;
	  neighbor_table_tmp != &neighbortable[index];
	  neighbor_table_tmp = neighbor_table_tmp->next)
	{
	  if(neighbor_table_tmp->is_mpr)
	    {
	      size += sprintf(&buf[size], "%s -> %s(MPR)\n", olsr_ip_to_string(main_addr), olsr_ip_to_string(&neighbor_table_tmp->neighbor_main_addr));		  
	    }
	  else
	    {
	      size += sprintf(&buf[size], "%s -> %s\n", olsr_ip_to_string(main_addr), olsr_ip_to_string(&neighbor_table_tmp->neighbor_main_addr));		  
	    }
	  
	  for(list_2 = neighbor_table_tmp->neighbor_2_list.next;
	      list_2 != &neighbor_table_tmp->neighbor_2_list;
	      list_2 = list_2->next)
	    {
	      size += sprintf(&buf[size], "\t-> %s\n", olsr_ip_to_string(&list_2->neighbor_2->neighbor_2_addr));
	    }
	      
	}
    }

  size += sprintf(&buf[size], "</table><hr>\n");

  return size;
}



int
build_topo_body(char *buf, olsr_u32_t bufsize)
{
  int size = 0;
  olsr_u8_t index;
  struct tc_entry *entry;
  struct topo_dst *dst_entry;


  size += sprintf(&buf[size], "<table width=100% BORDER=0 CELLSPACING=0 CELLPADDING=0 ALIGN=center><tr BGCOLOR=\"#AAAAAA\"><td>Source IP addr</td><td>Dest IP addr</td><td>LQ</td><td>ILQ</td><td>ETX</td></tr>\n");


  /* Topology */  
  for(index=0;index<HASHSIZE;index++)
    {
      /* For all TC entries */
      entry = tc_table[index].next;
      while(entry != &tc_table[index])
	{
	  /* For all destination entries of that TC entry */
	  dst_entry = entry->destinations.next;
	  while(dst_entry != &entry->destinations)
	    {
	      size += sprintf(&buf[size], "%s -> %s\n", olsr_ip_to_string(&entry->T_last_addr), olsr_ip_to_string(&dst_entry->T_dest_addr));
	      dst_entry = dst_entry->next;
	    }
	  entry = entry->next;
	}
    }

  size += sprintf(&buf[size], "</table><hr>\n");

}



int
build_hna_body(char *buf, olsr_u32_t bufsize)
{
  int size;
  olsr_u8_t index;
  struct tc_entry *entry;
  struct hna_entry *tmp_hna;
  struct hna_net *tmp_net;

  size = 0;

  size += sprintf(&buf[size], "Remote HNA entries\n");
  size += sprintf(&buf[size], "<table width=100% BORDER=0 CELLSPACING=0 CELLPADDING=0 ALIGN=center><tr BGCOLOR=\"#AAAAAA\"><td>Network</td><td>Netmask</td><td>Gateway</td></tr>\n");

  /* HNA entries */
  for(index=0;index<HASHSIZE;index++)
    {
      tmp_hna = hna_set[index].next;
      /* Check all entrys */
      while(tmp_hna != &hna_set[index])
	{
	  /* Check all networks */
	  tmp_net = tmp_hna->networks.next;
	      
	  while(tmp_net != &tmp_hna->networks)
	    {
	      size += sprintf(&buf[size], "GW: %s NET: %s\n", olsr_ip_to_string(&tmp_hna->A_gateway_addr), olsr_ip_to_string(&tmp_net->A_network_addr)/*, olsr_ip_to_string(&tmp_net->A_netmask.v4)*/);
	      tmp_net = tmp_net->next;
	    }
	      
	  tmp_hna = tmp_hna->next;
	}
    }


  size += sprintf(&buf[size], "</table><hr>\n");
  size += sprintf(&buf[size], "Local(announced) HNA entries\n");
  size += sprintf(&buf[size], "<table width=100% BORDER=0 CELLSPACING=0 CELLPADDING=0 ALIGN=center><tr BGCOLOR=\"#AAAAAA\"><td>Network</td><td>Netmask</td></tr>\n");
  size += sprintf(&buf[size], "</table><hr>\n");



}


int
build_mid_body(char *buf, olsr_u32_t bufsize)
{

    sprintf(buf, "No MID entries registered<br>");
}

/**
 *Converts a olsr_ip_addr to a string
 *Goes for both IPv4 and IPv6
 *
 *NON REENTRANT! If you need to use this
 *function twice in e.g. the same printf
 *it will not work.
 *You must use it in different calls e.g.
 *two different printfs
 *
 *@param the IP to convert
 *@return a pointer to a static string buffer
 *representing the address in "dots and numbers"
 *
 */
char *
olsr_ip_to_string(union olsr_ip_addr *addr)
{
  static int index = 0;
  static char buff[4][100];
  char *ret;
  struct in_addr in;
 
  if(ipversion == AF_INET)
    {
      in.s_addr=addr->v4;
      ret = inet_ntoa(in);
    }
  else
    {
      /* IPv6 */
      ret = (char *)inet_ntop(AF_INET6, &addr->v6, ipv6_buf, sizeof(ipv6_buf));
    }

  strncpy(buff[index], ret, 100);

  ret = buff[index];

  index = (index + 1) & 3;

  return ret;
}




/**
 *This function is just as bad as the previous one :-(
 */
char *
olsr_netmask_to_string(union hna_netmask *mask)
{
  char *ret;
  struct in_addr in;
  
  if(ipversion == AF_INET)
    {
      in.s_addr = mask->v4;
      ret = inet_ntoa(in);
      return ret;

    }
  else
    {
      /* IPv6 */
      sprintf(netmask, "%d", mask->v6);
      return netmask;
    }

  return ret;
}



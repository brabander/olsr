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
 * $Id: olsrd_dot_draw.c,v 1.8 2004/12/03 20:52:27 kattemat Exp $
 */

/*
 * Dynamic linked library for the olsr.org olsr daemon
 */

#include "olsrd_dot_draw.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#ifdef WIN32
#define close(x) closesocket(x)
#endif

int ipc_socket;
int ipc_open;
int ipc_connection;
int ipc_socket_up;

/**
 *Do initialization here
 *
 *This function is called by the my_init
 *function in uolsrd_plugin.c
 */
int
olsr_plugin_init()
{

  /* Initial IPC value */
  ipc_open = 0;
  ipc_socket_up = 0;

  /* Register the "ProcessChanges" function */
  register_pcf(&pcf_event);

  return 1;
}

int
plugin_ipc_init()
{
  struct sockaddr_in sin;
  olsr_u32_t yes = 1;

  /* Init ipc socket */
  if ((ipc_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
    {
      olsr_printf(1, "(DOT DRAW)IPC socket %s\n", strerror(errno));
      return 0;
    }
  else
    {
      if (setsockopt(ipc_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(yes)) < 0) 
      {
	perror("SO_REUSEADDR failed");
	return 0;
      }

      /* Bind the socket */
      
      /* complete the socket structure */
      memset(&sin, 0, sizeof(sin));
      sin.sin_family = AF_INET;
      sin.sin_addr.s_addr = INADDR_ANY;
      sin.sin_port = htons(ipc_port);
      
      /* bind the socket to the port number */
      if (bind(ipc_socket, (struct sockaddr *) &sin, sizeof(sin)) == -1) 
	{
	  olsr_printf(1, "(DOT DRAW)IPC bind %s\n", strerror(errno));
	  return 0;
	}
      
      /* show that we are willing to listen */
      if (listen(ipc_socket, 1) == -1) 
	{
	  olsr_printf(1, "(DOT DRAW)IPC listen %s\n", strerror(errno));
	  return 0;
	}


      /* Register with olsrd */
      add_olsr_socket(ipc_socket, &ipc_action);
      ipc_socket_up = 1;
    }

  return 1;
}

void
ipc_action(int fd)
{
  struct sockaddr_in pin;
  socklen_t addrlen;
  char *addr;  

  addrlen = sizeof(struct sockaddr_in);

  if ((ipc_connection = accept(ipc_socket, (struct sockaddr *)  &pin, &addrlen)) == -1)
    {
      olsr_printf(1, "(DOT DRAW)IPC accept: %s\n", strerror(errno));
      exit(1);
    }
  else
    {
      addr = inet_ntoa(pin.sin_addr);
      if(ntohl(pin.sin_addr.s_addr) != ntohl(ipc_accept_ip.s_addr))
	{
	  olsr_printf(1, "Front end-connection from foregin host(%s) not allowed!\n", addr);
	  close(ipc_connection);
	  return;
	}
      else
	{
	  ipc_open = 1;
	  olsr_printf(1, "(DOT DRAW)IPC: Connection from %s\n",addr);
	  pcf_event(1, 1, 1);
	}
    }

}

/*
 * destructor - called at unload
 */
void
olsr_plugin_exit()
{
  if(ipc_open)
    close(ipc_socket);
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




/**
 *Scheduled event
 */
int
pcf_event(int changes_neighborhood,
	  int changes_topology,
	  int changes_hna)
{
  int res;
  olsr_u8_t index;
  struct neighbor_entry *neighbor_table_tmp;
  struct neighbor_2_list_entry *list_2;
  struct tc_entry *entry;
  struct topo_dst *dst_entry;
  struct hna_entry *tmp_hna;
  struct hna_net *tmp_net;

  res = 0;

  if(changes_neighborhood || changes_topology || changes_hna)
    {
      /* Print tables to IPC socket */

      ipc_send("digraph topology\n{\n", strlen("digraph topology\n{\n"));

      /* Neighbors */
      for(index=0;index<HASHSIZE;index++)
	{
	  
	  for(neighbor_table_tmp = neighbortable[index].next;
	      neighbor_table_tmp != &neighbortable[index];
	      neighbor_table_tmp = neighbor_table_tmp->next)
	    {
	      if(neighbor_table_tmp->is_mpr)
		{
		  ipc_print_mpr_link(main_addr, &neighbor_table_tmp->neighbor_main_addr);		  
		  ipc_print_mpr_link(&neighbor_table_tmp->neighbor_main_addr, main_addr);
		}
	      else
		{
		  ipc_print_neigh_link(main_addr, &neighbor_table_tmp->neighbor_main_addr);		  
		  ipc_print_neigh_link(&neighbor_table_tmp->neighbor_main_addr, main_addr);
		}

	      for(list_2 = neighbor_table_tmp->neighbor_2_list.next;
		  list_2 != &neighbor_table_tmp->neighbor_2_list;
		  list_2 = list_2->next)
		{
		  ipc_print_2h_link(&neighbor_table_tmp->neighbor_main_addr, 
				    &list_2->neighbor_2->neighbor_2_addr);
		}
	      
	    }
	}

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
		  ipc_print_tc_link(&entry->T_last_addr, &dst_entry->T_dest_addr);
		  dst_entry = dst_entry->next;
		}
	      entry = entry->next;
	    }
	}

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
		  ipc_print_net(&tmp_hna->A_gateway_addr, 
				&tmp_net->A_network_addr, 
				&tmp_net->A_netmask);
		  tmp_net = tmp_net->next;
		}
	      
	      tmp_hna = tmp_hna->next;
	    }
	}


      ipc_send("}\n\n", strlen("}\n\n"));

      res = 1;
    }


  if(!ipc_socket_up)
    plugin_ipc_init();

  return res;
}




static void inline
ipc_print_neigh_link(union olsr_ip_addr *from, union olsr_ip_addr *to)
{
  char *adr;

  adr = olsr_ip_to_string(from);
  ipc_send("\"", 1);
  ipc_send(adr, strlen(adr));
  ipc_send("\" -> \"", strlen("\" -> \""));
  adr = olsr_ip_to_string(to);
  ipc_send(adr, strlen(adr));
  ipc_send("\"[label=\"neigh\", style=dashed];\n", strlen("\"[label=\"neigh\", style=dashed];\n"));

}

static void inline
ipc_print_2h_link(union olsr_ip_addr *from, union olsr_ip_addr *to)
{
  char *adr;

  adr = olsr_ip_to_string(from);
  ipc_send("\"", 1);
  ipc_send(adr, strlen(adr));
  ipc_send("\" -> \"", strlen("\" -> \""));
  adr = olsr_ip_to_string(to);
  ipc_send(adr, strlen(adr));
  ipc_send("\"[label=\"2 hop\"];\n", strlen("\"[label=\"2 hop\"];\n"));

}

static void inline
ipc_print_mpr_link(union olsr_ip_addr *from, union olsr_ip_addr *to)
{
  char *adr;

  adr = olsr_ip_to_string(from);
  ipc_send("\"", 1);
  ipc_send(adr, strlen(adr));
  ipc_send("\" -> \"", strlen("\" -> \""));
  adr = olsr_ip_to_string(to);
  ipc_send(adr, strlen(adr));
  ipc_send("\"[label=\"MPR\", style=dashed];\n", strlen("\"[label=\"MPR\", style=dashed]];\n"));

}

static void inline
ipc_print_tc_link(union olsr_ip_addr *from, union olsr_ip_addr *to)
{
  char *adr;

  adr = olsr_ip_to_string(from);
  ipc_send("\"", 1);
  ipc_send(adr, strlen(adr));
  ipc_send("\" -> \"", strlen("\" -> \""));
  adr = olsr_ip_to_string(to);
  ipc_send(adr, strlen(adr));
  ipc_send("\"[label=\"TC\"];\n", strlen("\"[label=\"TC\"];\n"));

}

static void inline
ipc_print_net(union olsr_ip_addr *gw, union olsr_ip_addr *net, union hna_netmask *mask)
{
  char *adr;

  adr = olsr_ip_to_string(gw);
  ipc_send("\"", 1);
  ipc_send(adr, strlen(adr));
  ipc_send("\" -> \"", strlen("\" -> \""));
  adr = olsr_ip_to_string(net);
  ipc_send(adr, strlen(adr));
  ipc_send("/", 1);
  adr = olsr_netmask_to_string(mask);
  ipc_send(adr, strlen(adr));
  ipc_send("\"[label=\"HNA\"];\n", strlen("\"[label=\"HNA\"];\n"));
  ipc_send("\"", 1);
  adr = olsr_ip_to_string(net);
  ipc_send(adr, strlen(adr));
  ipc_send("/", 1);
  adr = olsr_netmask_to_string(mask);
  ipc_send(adr, strlen(adr));
  ipc_send("\"", 1);
  ipc_send("[shape=diamond];\n", strlen("[shape=diamond];\n"));
}



int
ipc_send(char *data, int size)
{
  if(!ipc_open)
    return 0;

  if (send(ipc_connection, data, size, MSG_NOSIGNAL) < 0) 
    {
      olsr_printf(1, "(DOT DRAW)IPC connection lost!\n");
      close(ipc_connection);
      ipc_open = 0;
      return -1;
    }

  return 1;
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



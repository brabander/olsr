/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2003 Andreas Tønnesen (andreto@ifi.uio.no)
 *
 * This file is part of the olsr.org OLSR daemon.
 *
 * olsr.org is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * olsr.org is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with olsr.org; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*
 *Andreas Tønnesen (andreto@ifi.uio.no)
 *
 *IPC - interprocess communication
 *for the OLSRD - GUI front-end
 *
 */

#include "ipc_frontend.h"
#include "link_set.h"
#include "olsr.h"
#include "parser.h"
#include "local_hna_set.h"

#ifdef WIN32
#define close(x) closesocket(x)
#define perror(x) WinSockPError(x)
#define MSG_NOSIGNAL 0
void 
WinSockPError(char *);
#endif

pthread_t accept_thread;

/**
 *Create the socket to use for IPC to the
 *GUI front-end
 *
 *@return the socket FD
 */
int
ipc_init()
{
  //int flags;
  struct   sockaddr_in sin;

  /* Add parser function */
  olsr_parser_add_function(&frontend_msgparser, PROMISCUOUS, 0);

  /* get an internet domain socket */
  if ((ipc_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
    {
      perror("IPC socket");
      olsr_exit("IPC socket", EXIT_FAILURE);
    }

  /* complete the socket structure */
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_port = htons(IPC_PORT);

  /* bind the socket to the port number */
  if (bind(ipc_sock, (struct sockaddr *) &sin, sizeof(sin)) == -1) 
    {
      perror("IPC bind");
      olsr_exit("IPC bind", EXIT_FAILURE);
    }

  /* show that we are willing to listen */
  if (listen(ipc_sock, 5) == -1) 
    {
      perror("IPC listen");
      olsr_exit("IPC listen", EXIT_FAILURE);
    }


  /* Start the accept thread */

  pthread_create(&accept_thread, NULL, (void *)&ipc_accept_thread, NULL);

  return ipc_sock;
}


void
ipc_accept_thread()
{
  int 	             addrlen;
  struct sockaddr_in pin;
  char *addr;  

  while(use_ipc)
    {
      olsr_printf(2, "\nFront-end accept thread initiated(socket %d)\n\n", ipc_sock);

      addrlen = sizeof (struct sockaddr_in);

      if ((ipc_connection = accept(ipc_sock, (struct sockaddr *)  &pin, &addrlen)) == -1)
	{
	  perror("IPC accept");
	  olsr_exit("IPC accept", EXIT_FAILURE);
	}
      else
	{
	  olsr_printf(1, "Front end connected\n");
	  addr = inet_ntoa(pin.sin_addr);
	  if(ntohl(pin.sin_addr.s_addr) != INADDR_LOOPBACK)
	    {
	      olsr_printf(1, "Front end-connection from foregin host(%s) not allowed!\n", addr);
	      olsr_syslog(OLSR_LOG_ERR, "OLSR: Front end-connection from foregin host(%s) not allowed!\n", addr);
	      close(ipc_connection);
	    }
	  else
	    {
	      ipc_active = 1;
	      ipc_send_net_info();
	      ipc_send_all_routes();
	      olsr_printf(1, "Connection from %s\n",addr);
	    }
	}

      sleep(2);
    }
}



/**
 *Read input from the IPC socket. Not in use.
 *
 *@todo for future use
 *@param sock the IPC socket
 *@return 1
 */
int
ipc_input(int sock)
{
  /*
  union 
  {
    char	buf[MAXPACKETSIZE+1];
    struct	olsr olsr;
  } inbuf;


  if (recv(sock, dir, sizeof(dir), 0) == -1) 
    {
      perror("recv");
      exit(1);
    }
*/
  return 1;
}


/**
 *Sends a olsr packet on the IPC socket.
 *
 *@param olsr the olsr struct representing the packet
 *
 *@return negative on error
 */
void
frontend_msgparser(union olsr_message *msg, struct interface *in_if, union olsr_ip_addr *from_addr)
{
  int size;

  if(!ipc_active)
    return;
  
  if(ipversion == AF_INET)
    size = ntohs(msg->v4.olsr_msgsize);
  else
    size = ntohs(msg->v6.olsr_msgsize);
  
  if (send(ipc_connection, (void *)msg, size, MSG_NOSIGNAL) < 0) 
    {
      olsr_printf(1, "(OUTPUT)IPC connection lost!\n");
      close(ipc_connection);
      //use_ipc = 0;
      ipc_active = 0;
      return;
    }
  
  return;
}


/**
 *Send a route table update to the front-end.
 *
 *@param kernel_route a rtentry describing the route update
 *@param add 1 if the route is to be added 0 if it is to be deleted
 *@param int_name the name of the interface the route is set to go by
 *
 *@return negative on error
 */
int
ipc_route_send_rtentry(union olsr_kernel_route *kernel_route, int add, char *int_name)
{
  struct ipcmsg packet;
  //int i, x;
  char *tmp;

  if(!ipc_active)
    return 0;

  packet.size = htons(IPC_PACK_SIZE);
  packet.msgtype = ROUTE_IPC;

  if(ipversion == AF_INET)
    COPY_IP(&packet.target_addr, &((struct sockaddr_in *)&kernel_route->v4.rt_dst)->sin_addr.s_addr);
  else
    COPY_IP(&packet.target_addr, &kernel_route->v6.rtmsg_dst);

  packet.add = add;
  if(add)
    {
      if(ipversion == AF_INET)
	{
	  packet.metric = kernel_route->v4.rt_metric - 1;
	  COPY_IP(&packet.gateway_addr, &((struct sockaddr_in *)&kernel_route->v4.rt_gateway)->sin_addr.s_addr);
	}
      else
	{
	  packet.metric = kernel_route->v6.rtmsg_metric;
	  COPY_IP(&packet.gateway_addr, &kernel_route->v6.rtmsg_gateway);
	}

      if(int_name != NULL)
	memcpy(&packet.device[0], int_name, 4);
      else
	memset(&packet.device[0], 0, 4);
    }
  else
    {
      memset(&packet.metric, 0, 1);
      memset(&packet.gateway_addr, 0, 4);
      memset(&packet.device[0], 0, 4);
    }


  tmp = (char *) &packet;
  /*
  x = 0;
  for(i = 0; i < IPC_PACK_SIZE;i++)
    {
      if(x == 4)
	{
	  x = 0;
	  printf("\n\t");
	}
      x++;
      printf(" %03i", (u_char) tmp[i]);
    }
  
  printf("\n");
  */
  
  if (send(ipc_connection, tmp, IPC_PACK_SIZE, MSG_NOSIGNAL) < 0) // MSG_NOSIGNAL to avoid sigpipe
    {
      olsr_printf(1, "(RT_ENTRY)IPC connection lost!\n");
      close(ipc_connection);
      //use_ipc = 0;
      ipc_active = 0;
      return -1;
    }

  return 1;
}



int
ipc_send_all_routes()
{
  struct rt_entry  *destination;
  struct interface *ifn;
  olsr_u8_t        index;
  struct ipcmsg packet;
  char *tmp;
  

  if(!ipc_active)
    return 0;
  
  for(index=0;index<HASHSIZE;index++)
    {
      for(destination = routingtable[index].next;
	  destination != &routingtable[index];
	  destination = destination->next)
	{
	  ifn = get_interface_link_set(&destination->rt_router);
	  

	  
	  packet.size = htons(IPC_PACK_SIZE);
	  packet.msgtype = ROUTE_IPC;
	  
	  COPY_IP(&packet.target_addr, &destination->rt_dst);
	  
	  packet.add = 1;

	  if(ipversion == AF_INET)
	    {
	      packet.metric = (olsr_u8_t)(destination->rt_metric - 1);
	    }
	  else
	    {
	      packet.metric = (olsr_u8_t)destination->rt_metric;
	    }
	  COPY_IP(&packet.gateway_addr, &destination->rt_router);

	  if(ifn)
	    memcpy(&packet.device[0], ifn->int_name, 4);
	  else
	    memset(&packet.device[0], 0, 4);


	  tmp = (char *) &packet;
  
	  if (send(ipc_connection, tmp, IPC_PACK_SIZE, MSG_NOSIGNAL) < 0) // MSG_NOSIGNAL to avoid sigpipe
	    {
	      olsr_printf(1, "(RT_ENTRY)IPC connection lost!\n");
	      close(ipc_connection);
	      //use_ipc = 0;
	      ipc_active = 0;
	      return -1;
	    }

	}
    }

  for(index=0;index<HASHSIZE;index++)
    {
      for(destination = hna_routes[index].next;
	  destination != &hna_routes[index];
	  destination = destination->next)
	{
	  ifn = get_interface_link_set(&destination->rt_router);

	  packet.size = htons(IPC_PACK_SIZE);
	  packet.msgtype = ROUTE_IPC;
	  
	  COPY_IP(&packet.target_addr, &destination->rt_dst);
	  
	  packet.add = 1;

	  if(ipversion == AF_INET)
	    {
	      packet.metric = (olsr_u8_t)(destination->rt_metric - 1);
	    }
	  else
	    {
	      packet.metric = (olsr_u8_t)destination->rt_metric;
	    }
	  COPY_IP(&packet.gateway_addr, &destination->rt_router);

	  if(ifn)
	    memcpy(&packet.device[0], ifn->int_name, 4);
	  else
	    memset(&packet.device[0], 0, 4);


	  tmp = (char *) &packet;
  
	  if (send(ipc_connection, tmp, IPC_PACK_SIZE, MSG_NOSIGNAL) < 0) // MSG_NOSIGNAL to avoid sigpipe
	    {
	      olsr_printf(1, "(RT_ENTRY)IPC connection lost!\n");
	      close(ipc_connection);
	      //use_ipc = 0;
	      ipc_active = 0;
	      return -1;
	    }

	}
    }


  return 1;
}



/**
 *Sends OLSR info to the front-end. This info consists of
 *the different time intervals and holding times, number
 *of interfaces, HNA routes and main address.
 *
 *@return negative on error
 */
int
ipc_send_net_info()
{
  struct ipc_net_msg *net_msg;
  //int x, i;
  char *msg;
  

  net_msg = olsr_malloc(sizeof(struct ipc_net_msg), "send net info");

  msg = (char *)net_msg;

  olsr_printf(1, "Sending net-info to front end...\n");
  
  memset(net_msg, 0, sizeof(struct ipc_net_msg));
  
  /* Message size */
  net_msg->size = htons(sizeof(struct ipc_net_msg));
  /* Message type */
  net_msg->msgtype = NET_IPC;
  
  /* MIDs */
  net_msg->mids = nbinterf - 1;
  
  /* HNAs */
  if(ipversion == AF_INET6)
    {
      if(local_hna6_set.next == &local_hna6_set)
	net_msg->hnas = 0;
      else
	net_msg->hnas = 1;
    }

  if(ipversion == AF_INET)
    {
      if(local_hna4_set.next == &local_hna4_set)
	net_msg->hnas = 0;
      else
	net_msg->hnas = 1;
    }

  /* Different values */
  net_msg->hello_int = htons((olsr_u16_t)hello_int);
  net_msg->hello_lan_int = htons((olsr_u16_t)hello_int_nw);
  net_msg->tc_int = htons((olsr_u16_t)tc_int);
  net_msg->neigh_hold = htons((olsr_u16_t)neighbor_hold_time);
  net_msg->topology_hold = htons((olsr_u16_t)topology_hold_time);

  if(ipversion == AF_INET)
    net_msg->ipv6 = 0;
  else
    net_msg->ipv6 = 1;
 
  /* Main addr */
  COPY_IP(&net_msg->main_addr, &main_addr);


  /*
  printf("\t");
  x = 0;
  for(i = 0; i < sizeof(struct ipc_net_msg);i++)
    {
      if(x == 4)
	{
	  x = 0;
	  printf("\n\t");
	}
      x++;
      printf(" %03i", (u_char) msg[i]);
    }
  
  printf("\n");
  */


  if (send(ipc_connection, (char *)net_msg, sizeof(struct ipc_net_msg), MSG_NOSIGNAL) < 0) 
    {
      olsr_printf(1, "(NETINFO)IPC connection lost!\n");
      close(ipc_connection);
      use_ipc = 0;
      return -1;
    }

  free(net_msg);
  return 0;
}



int
shutdown_ipc()
{

  pthread_kill(accept_thread, SIGTERM);
  close(ipc_sock);
  
  if(ipc_active)
    close(ipc_connection);
  
  return 1;
}

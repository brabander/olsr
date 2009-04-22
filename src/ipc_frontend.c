
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
 *
 *IPC - interprocess communication
 *for the OLSRD - GUI front-end
 *
 */

#include "ipc_frontend.h"
#include "link_set.h"
#include "olsr.h"
#include "log.h"
#include "parser.h"
#include "scheduler.h"
#include "net_olsr.h"
#include "ipcalc.h"
#include "olsr_ip_prefix_list.h"
#include "olsr_logging.h"

#include <unistd.h>
#include <stdlib.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

/*
 *IPC message sent to the front-end
 *at every route update. Both delete
 *and add
 */
struct ipcmsg {
  uint8_t msgtype;
  uint16_t size;
  uint8_t metric;
  uint8_t add;
  union olsr_ip_addr target_addr;
  union olsr_ip_addr gateway_addr;
  char device[4];
};


struct ipc_net_msg {
  uint8_t msgtype;
  uint16_t size;
  uint8_t mids;                        /* No. of extra interfaces */
  uint8_t hnas;                        /* No. of HNA nets */
  uint8_t unused1;
  uint16_t hello_int;
  uint16_t hello_lan_int;
  uint16_t tc_int;
  uint16_t neigh_hold;
  uint16_t topology_hold;
  uint8_t ipv6;
  union olsr_ip_addr main_addr;
};

static int ipc_sock = -1;
static int ipc_conn = -1;

static int
  ipc_send_all_routes(int fd);

static int
  ipc_send_net_info(int fd);

static void
  ipc_accept(int, void *, unsigned int);

#if 0
static int
  ipc_input(int);
#endif

static bool ipc_check_allowed_ip(union olsr_ip_addr *);

static bool frontend_msgparser(union olsr_message *, struct interface *, union olsr_ip_addr *);


/**
 *Create the socket to use for IPC to the
 *GUI front-end
 *
 *@return the socket FD
 */
int
ipc_init(void)
{
  struct sockaddr_in sin4;
  int yes;

  /* Add parser function */
  olsr_parser_add_function(&frontend_msgparser, PROMISCUOUS);

  /* get an internet domain socket */
  ipc_sock = socket(AF_INET, SOCK_STREAM, 0);
  if (ipc_sock == -1) {
    OLSR_ERROR(LOG_IPC, "Could not open IPC socket\n");
    olsr_exit(EXIT_FAILURE);
  }

  yes = 1;
  if (setsockopt(ipc_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(yes)) < 0) {
    OLSR_WARN(LOG_IPC, "Could not allow socket to reuse port\n");
    return 0;
  }

  /* complete the socket structure */
  memset(&sin4, 0, sizeof(sin4));
  sin4.sin_family = AF_INET;
  sin4.sin_addr.s_addr = INADDR_ANY;
  sin4.sin_port = htons(IPC_PORT);

  /* bind the socket to the port number */
  if (bind(ipc_sock, (struct sockaddr *)&sin4, sizeof(sin4)) == -1) {
    OLSR_WARN(LOG_IPC, "Could not bind IPC socket, will retry in 10 seconds...\n");
    sleep(10);
    if (bind(ipc_sock, (struct sockaddr *)&sin4, sizeof(sin4)) == -1) {
      OLSR_ERROR(LOG_IPC, "Second attempt to bind IPC socket failed\n");
      olsr_exit(EXIT_FAILURE);
    }
  }

  /* show that we are willing to listen */
  if (listen(ipc_sock, olsr_cnf->ipc_connections) == -1) {
    OLSR_ERROR(LOG_IPC, "Could not listen to IPC socket\n");
    olsr_exit(EXIT_FAILURE);
  }

  OLSR_INFO(LOG_IPC, "Initializing IPC connection...\n");

  /* Register the socket with the socket parser */
  add_olsr_socket(ipc_sock, &ipc_accept, NULL, NULL, SP_PR_READ);

  return ipc_sock;
}

static bool
ipc_check_allowed_ip(union olsr_ip_addr *addr)
{
  if (addr->v4.s_addr == ntohl(INADDR_LOOPBACK)) {
    return true;
  }

  /* check nets */
  return ip_acl_acceptable(&olsr_cnf->ipc_nets, addr, olsr_cnf->ip_version);
}

static void
ipc_accept(int fd, void *data __attribute__ ((unused)), unsigned int flags __attribute__ ((unused)))
{
  struct sockaddr_in pin;
  char *addr;
  socklen_t addrlen = sizeof(struct sockaddr_in);

  ipc_conn = accept(fd, (struct sockaddr *)&pin, &addrlen);
  if (ipc_conn == -1) {
    OLSR_ERROR(LOG_IPC, "Could not accept incoming connection to IPC socket\n");
    olsr_exit(EXIT_FAILURE);
  } else {
    addr = inet_ntoa(pin.sin_addr);
    if (ipc_check_allowed_ip((union olsr_ip_addr *)&pin.sin_addr.s_addr)) {
      ipc_send_net_info(ipc_conn);
      ipc_send_all_routes(ipc_conn);
      OLSR_INFO(LOG_IPC, "Front end connected from %s\n", addr);
    } else {
      OLSR_WARN(LOG_IPC, "Front end-connection from foregin host(%s) not allowed!\n", addr);
      CLOSESOCKET(ipc_conn);
    }
  }
}

#if 0

/**
 *Read input from the IPC socket. Not in use.
 *
 *@todo for future use
 *@param sock the IPC socket
 *@return 1
 */
static int
ipc_input(int sock)
{
  union {
    char buf[MAXPACKETSIZE + 1];
    struct olsr olsr;
  } inbuf;

  if (recv(sock, dir, sizeof(dir), 0) == -1) {
    perror("recv");
    exit(1);
  }
  return 1;
}
#endif

/**
 *Sends a olsr packet on the IPC socket.
 *
 *@param olsr the olsr struct representing the packet
 *
 *@return true for not preventing forwarding
 */
static bool
frontend_msgparser(union olsr_message *msg,
                   struct interface *in_if __attribute__ ((unused)), union olsr_ip_addr *from_addr __attribute__ ((unused)))
{
  if (ipc_conn >= 0) {
    const size_t len = olsr_cnf->ip_version == AF_INET ? ntohs(msg->v4.olsr_msgsize) : ntohs(msg->v6.olsr_msgsize);
    if (send(ipc_conn, (void *)msg, len, MSG_NOSIGNAL) < 0) {
      OLSR_WARN(LOG_IPC, "(OUTPUT)IPC connection lost!\n");
      CLOSESOCKET(ipc_conn);
    }
  }
  return true;
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
ipc_route_send_rtentry(const union olsr_ip_addr *dst, const union olsr_ip_addr *gw, int met, int add, const char *int_name)
{
  struct ipcmsg packet;

  if (olsr_cnf->ipc_connections <= 0) {
    return -1;
  }

  if (ipc_conn < 0) {
    return 0;
  }
  memset(&packet, 0, sizeof(packet));
  packet.size = htons(IPC_PACK_SIZE);
  packet.msgtype = ROUTE_IPC;

  packet.target_addr = *dst;

  packet.add = add;
  if (add && gw) {
    packet.metric = met;
    packet.gateway_addr = *gw;
  }

  if (int_name != NULL) {
    memcpy(&packet.device[0], int_name, 4);
  } else {
    memset(&packet.device[0], 0, 4);
  }

  /*
     x = 0;
     for(i = 0; i < IPC_PACK_SIZE;i++)
     {
     if (x == 4)
     {
     x = 0;
     printf("\n\t");
     }
     x++;
     printf(" %03i", (u_char) tmp[i]);
     }

     printf("\n");
   */

  if (send(ipc_conn, (void *)&packet, IPC_PACK_SIZE, MSG_NOSIGNAL) < 0) {       // MSG_NOSIGNAL to avoid sigpipe
    OLSR_WARN(LOG_IPC, "(RT_ENTRY)IPC connection lost!\n");
    CLOSESOCKET(ipc_conn);
    return -1;
  }

  return 1;
}



static int
ipc_send_all_routes(int fd)
{
  struct rt_entry *rt;

  if (ipc_conn < 0) {
    return 0;
  }

  OLSR_FOR_ALL_RT_ENTRIES(rt) {
    struct ipcmsg packet;

    memset(&packet, 0, sizeof(packet));
    packet.size = htons(IPC_PACK_SIZE);
    packet.msgtype = ROUTE_IPC;

    packet.target_addr = rt->rt_dst.prefix;

    packet.add = 1;
    packet.metric = rt->rt_best->rtp_metric.hops;

    packet.gateway_addr = rt->rt_nexthop.gateway;

    memcpy(&packet.device[0], rt->rt_nexthop.interface->int_name, 4);

    /* MSG_NOSIGNAL to avoid sigpipe */
    if (send(fd, (void *)&packet, IPC_PACK_SIZE, MSG_NOSIGNAL) < 0) {
      OLSR_WARN(LOG_IPC, "(RT_ENTRY)IPC connection lost!\n");
      CLOSESOCKET(ipc_conn);
      return -1;
    }
  }
  OLSR_FOR_ALL_RT_ENTRIES_END(rt);
  return 1;
}



/**
 *Sends OLSR info to the front-end. This info consists of
 *the different time intervals and holding times, number
 *of interfaces, HNA routes and main address.
 *
 *@return negative on error
 */
static int
ipc_send_net_info(int fd)
{
  //int x, i;
  struct ipc_net_msg net_msg;

  OLSR_DEBUG(LOG_IPC, "Sending net-info to front end...\n");

  memset(&net_msg, 0, sizeof(net_msg));

  /* Message size */
  net_msg.size = htons(sizeof(net_msg));
  /* Message type */
  net_msg.msgtype = NET_IPC;

  /* MIDs */
  /* XXX fix IPC MIDcnt */
  net_msg.mids = (!list_is_empty(&interface_head)) ? 1 : 0;

  /* HNAs */
  net_msg.hnas = list_is_empty(&olsr_cnf->hna_entries) ? 0 : 1;

  /* Different values */
  /* Temporary fixes */
  /* XXX fix IPC intervals */
  net_msg.hello_int = 0;        //htons((uint16_t)hello_int);
  net_msg.hello_lan_int = 0;    //htons((uint16_t)hello_int_nw);
  net_msg.tc_int = 0;           //htons((uint16_t)tc_int);
  net_msg.neigh_hold = 0;       //htons((uint16_t)neighbor_hold_time);
  net_msg.topology_hold = 0;    //htons((uint16_t)topology_hold_time);

  net_msg.ipv6 = olsr_cnf->ip_version == AF_INET ? 0 : 1;

  /* Main addr */
  net_msg.main_addr = olsr_cnf->router_id;

  /*
     printf("\t");
     x = 0;
     for(i = 0; i < sizeof(struct ipc_net_msg);i++)
     {
     if (x == 4)
     {
     x = 0;
     printf("\n\t");
     }
     x++;
     printf(" %03i", (u_char) msg[i]);
     }

     printf("\n");
   */

  if (send(fd, (void *)&net_msg, sizeof(net_msg), MSG_NOSIGNAL) < 0) {
    OLSR_WARN(LOG_IPC, "(NETINFO)IPC connection lost!\n");
    CLOSESOCKET(ipc_conn);
    return -1;
  }
  return 0;
}



void
shutdown_ipc(void)
{
  OLSR_INFO(LOG_IPC, "Shutting down IPC...\n");
  CLOSESOCKET(ipc_conn);
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

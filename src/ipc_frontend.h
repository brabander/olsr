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
 * 
 * $ Id $
 *
 */
/*
 *Andreas Tønnesen (andreto@ifi.uio.no)
 *
 *IPC - interprocess communication
 *for the OLSRD - GUI front-end
 *
 */

#ifndef _OLSR_IPC
#define _OLSR_IPC

#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <signal.h>

#include "defs.h"

#define IPC_PORT 1212
#define IPC_PACK_SIZE 44 /* Size of the IPC_ROUTE packet */
#define	ROUTE_IPC 11    /* IPC to front-end telling of route changes */
#define NET_IPC 12      /* IPC to front end net-info */

/*
 *IPC message sent to the front-end
 *at every route update. Both delete
 *and add
 */

struct ipcmsg 
{
  olsr_u8_t          msgtype;
  olsr_u16_t         size;
  olsr_u8_t          metric;
  olsr_u8_t          add;
  union olsr_ip_addr target_addr;
  union olsr_ip_addr gateway_addr;
  char               device[4];
};


struct ipc_net_msg
{
  olsr_u8_t            msgtype;
  olsr_u16_t           size;
  olsr_u8_t            mids; /* No. of extra interfaces */
  olsr_u8_t            hnas; /* No. of HNA nets */
  olsr_u8_t            unused1;
  olsr_u16_t           hello_int;
  olsr_u16_t           hello_lan_int;
  olsr_u16_t           tc_int;
  olsr_u16_t           neigh_hold;
  olsr_u16_t           topology_hold;
  olsr_u8_t            ipv6;
  union olsr_ip_addr   main_addr;
};


int ipc_connection;
int ipc_sock;
int ipc_active;

void
ipc_accept_thread();

int
ipc_send_all_routes();

void
frontend_msgparser(union olsr_message *, struct interface *, union olsr_ip_addr *);

#endif

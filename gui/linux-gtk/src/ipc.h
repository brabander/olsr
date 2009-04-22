
/*
 * OLSR ad-hoc routing table management protocol GUI front-end
 * Copyright (C) 2003 Andreas Tonnesen (andreto@ifi.uio.no)
 *
 * This file is part of olsr.org.
 *
 * uolsrGUI is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * uolsrGUI is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with olsr.org; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "olsr_protocol.h"
#include "packet.h"

#define IPC_PORT 1212
#define	IPC_MESSAGE 11          /* IPC to front-end telling of route changes */
#define IPC_NET 12              /* IPC to front end net-info */


//int ipc_socket;
int connected;


/*
 *AND:
 *IPC message sent to the front-end
 *at every route update. Both delete
 *and add
 */

struct routemsg {
  uint8_t msgtype;
  uint16_t size;
  uint8_t metric;
  uint8_t add;
  union olsr_ip_addr target_addr;
  union olsr_ip_addr gateway_addr;
  char device[4];
};


struct netmsg {
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

/*
 *Private functions
 */

int
  ipc_get_socket();

int
  ipc_evaluate_message(union olsr_message *);

int
  ipc_eval_route_packet(struct routemsg *);

int
  ipc_eval_net_info(struct netmsg *);

int
  process_hello(int, uint8_t, union olsr_ip_addr *, union hello_message *);

int
  process_tc(int, uint8_t, union olsr_ip_addr *, union tc_message *);

int
  process_mid(int, uint8_t, union olsr_ip_addr *, union mid_message *);

int
  process_hna(int, uint8_t, union olsr_ip_addr *, union hna_message *);

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

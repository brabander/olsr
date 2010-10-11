/*
 * kernel_tunnel.h
 *
 *  Created on: 08.02.2010
 *      Author: henning
 */

#ifndef KERNEL_TUNNEL_H_
#define KERNEL_TUNNEL_H_

#include <net/if.h>
#ifdef WIN32
/* compat for win32 */
#include <iprtrmib.h>
#define IF_NAMESIZE MAX_INTERFACE_NAME_LEN
#endif

#include "defs.h"
#include "olsr_types.h"
#include "common/avl.h"

#define TUNNEL_ENDPOINT_IF "tunl0"
#define TUNNEL_ENDPOINT_IF6 "ip6tnl0"

struct olsr_iptunnel_entry {
  struct avl_node node;
  union olsr_ip_addr target;

  char if_name[IF_NAMESIZE];
  int if_index;

  int usage;
};

int os_iptunnel_init(void);
void os_iptunnel_cleanup(void);

struct olsr_iptunnel_entry *os_iptunnel_add_ipip(union olsr_ip_addr *target, bool transportV4);
void os_iptunnel_del_ipip(struct olsr_iptunnel_entry *);

#endif /* KERNEL_TUNNEL_H_ */

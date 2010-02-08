/*
 * kernel_tunnel.h
 *
 *  Created on: 08.02.2010
 *      Author: henning
 */

#ifndef KERNEL_TUNNEL_H_
#define KERNEL_TUNNEL_H_

#include "defs.h"
#include "olsr_types.h"

#ifdef linux
int olsr_os_add_ipip_tunnel(const char *name, union olsr_ip_addr *target, bool transportV4);
int olsr_os_change_ipip_tunnel(const char *name, union olsr_ip_addr *target, bool transportV4);
int olsr_os_del_ipip_tunnel(const char *name, bool transportV4);
#endif

#endif /* KERNEL_TUNNEL_H_ */

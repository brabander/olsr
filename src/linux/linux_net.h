/*
 * linux_net.h
 *
 *  Created on: Oct 7, 2010
 *      Author: rogge
 */

#ifndef LINUX_NET_H_
#define LINUX_NET_H_

#include "defs.h"
#include "interfaces.h"

/*
 * these functions are used by the common unix code, but are not
 * exported to the OLSR core
 */
void os_init_global_ifoptions(void);
void os_cleanup_global_ifoptions(void);
void net_os_restore_ifoption(struct interface *ifs);
int net_os_set_ifoptions(const char *if_name, struct interface *iface);

#endif /* LINUX_NET_H_ */

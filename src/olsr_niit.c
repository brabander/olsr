/*
 * olsr_niit.c
 *
 *  Created on: 02.02.2010
 *      Author: henning
 */

#include "defs.h"
#include "kernel_routes.h"
#include "net_os.h"
#include "olsr_niit.h"

#include <net/if.h>

#ifdef linux
int olsr_init_niit(void) {
  olsr_cnf->niit4to6_if_index = if_nametoindex(DEF_NIIT4TO6_IFNAME);
  if (olsr_cnf->niit4to6_if_index <= 0 || !olsr_if_isup(DEF_NIIT4TO6_IFNAME)) {
    OLSR_PRINTF(1, "Warning, %s device is not available, deactivating NIIT\n", DEF_NIIT4TO6_IFNAME);
    olsr_cnf->use_niit = false;
    return 0;
  }
  olsr_cnf->niit6to4_if_index = if_nametoindex(DEF_NIIT6TO4_IFNAME);
  if (olsr_cnf->niit6to4_if_index <= 0 || !olsr_if_isup(DEF_NIIT6TO4_IFNAME)) {
    OLSR_PRINTF(1, "Warning, %s device is not available, deactivating NIIT\n", DEF_NIIT6TO4_IFNAME);
    olsr_cnf->use_niit = false;
    return 0;
  }
  return 0;
}

void olsr_setup_niit_routes(void) {
  struct ip_prefix_list *h;
  for (h = olsr_cnf->hna_entries; h != NULL; h = h->next) {
    olsr_netlink_static_niit_routes(&h->net, true);
  }
}

void olsr_cleanup_niit_routes(void) {
  struct ip_prefix_list *h;
  for (h = olsr_cnf->hna_entries; h != NULL; h = h->next) {
    olsr_netlink_static_niit_routes(&h->net, false);
  }
}
#endif

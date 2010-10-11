/*
 * dummy.c
 *
 *  Created on: 12.02.2010
 *      Author: henning
 */

#include "../defs.h"
#include "../kernel_routes.h"
#include "../kernel_tunnel.h"
#include "../net_os.h"

/* prototypes: have them here or disable the warnings about missing prototypes! */
int olsr_if_setip(const char *dev __attribute__ ((unused)), union olsr_ip_addr *ip __attribute__ ((unused)), int ipversion __attribute__ ((unused))); 



int os_iptunnel_init(void) {
  return -1;
}

void os_iptunnel_cleanup(void) {
}

struct olsr_iptunnel_entry *os_iptunnel_add_ipip(union olsr_ip_addr *target __attribute__ ((unused)),
    bool transportV4 __attribute__ ((unused))) {
  return NULL;
}

void os_iptunnel_del_ipip(struct olsr_iptunnel_entry *t __attribute__ ((unused))) {
  return;
}

bool os_is_interface_up(const char * dev __attribute__ ((unused))) {
  return false;
}

int olsr_if_setip(const char *dev __attribute__ ((unused)),
    union olsr_ip_addr *ip __attribute__ ((unused)),
    int ipversion __attribute__ ((unused))) {
  return -1;
}

void olsr_os_niit_4to6_route(const struct olsr_ip_prefix *dst_v4 __attribute__ ((unused)),
    bool set __attribute__ ((unused))) {
}
void olsr_os_niit_6to4_route(const struct olsr_ip_prefix *dst_v6 __attribute__ ((unused)),
    bool set __attribute__ ((unused))) {
}
void olsr_os_inetgw_tunnel_route(uint32_t if_idx __attribute__ ((unused)),
    bool ipv4 __attribute__ ((unused)),
    bool set __attribute__ ((unused))) {
}

int olsr_os_policy_rule(int family __attribute__ ((unused)),
    int rttable __attribute__ ((unused)),
    uint32_t priority __attribute__ ((unused)),
    const char *if_name __attribute__ ((unused)),
    bool set __attribute__ ((unused))) {
  return -1;
}

int olsr_os_localhost_if(union olsr_ip_addr *ip __attribute__ ((unused)),
    bool create __attribute__ ((unused))) {
  return -1;
}

int olsr_os_ifip(int ifindex __attribute__ ((unused)),
    union olsr_ip_addr *ip __attribute__ ((unused)), bool create __attribute__ ((unused))) {
  return -1;
}

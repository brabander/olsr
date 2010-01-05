/*
 * gateway.c
 *
 *  Created on: 05.01.2010
 *      Author: henning
 */

#include "common/avl.h"
#include "defs.h"
#include "ipcalc.h"
#include "olsr.h"
#include "olsr_cfg.h"
#include "olsr_cookie.h"
#include "scheduler.h"
#include "gateway.h"

struct avl_tree gateway_tree;
struct olsr_cookie_info *gw_mem_cookie = NULL;
union olsr_ip_addr smart_gateway_netmask;

static uint32_t deserialize_gw_speed(uint8_t value) {
  uint32_t speed, exp;

  speed = value >> 3;
  exp = value & 7;
  while (exp-- > 0) {
    speed *= 10;
  }
  return speed;
}

static uint8_t serialize_gw_speed(uint32_t speed) {
  uint8_t exp = 0;

  if (speed == 0 || speed > 310000000) {
    return 0;
  }

  while (speed > 32 || (speed % 10) == 0) {
    speed /= 10;
    exp ++;
  }
  return (speed << 3) | exp;
}

void
olsr_init_gateways(void) {
  uint8_t *ip;
  gw_mem_cookie = olsr_alloc_cookie("Gateway cookie", OLSR_COOKIE_TYPE_MEMORY);
  olsr_cookie_set_memory_size(gw_mem_cookie, sizeof(struct gateway_entry));

  avl_init(&gateway_tree, avl_comp_default);

  memset(&smart_gateway_netmask, 0, sizeof(smart_gateway_netmask));

  if (olsr_cnf->smart_gateway_active) {
    union olsr_ip_addr gw_net;
    int prefix;

    memset(&gw_net, 0, sizeof(gw_net));

    /*
     * hack for Vienna network to remove 0.0.0.0/128.0.0.0 and 128.0.0.0/128.0.0.0 routes
     * just set MAXIMUM_GATEWAY_PREFIX_LENGTH to 1
     */
    for (prefix = 1; prefix <= MAXIMUM_GATEWAY_PREFIX_LENGTH; prefix++) {
      while (ip_prefix_list_remove(&olsr_cnf->hna_entries, &gw_net, prefix));
    }

    ip = (uint8_t *) &smart_gateway_netmask;
    ip[olsr_cnf->ipsize - 2] = serialize_gw_speed(olsr_cnf->smart_gateway_uplink);
    ip[olsr_cnf->ipsize - 1] = serialize_gw_speed(olsr_cnf->smart_gateway_downlink);
  }
}

struct gateway_entry *
olsr_find_gateway(union olsr_ip_addr *originator) {
  struct avl_node *node = avl_find(&gateway_tree, originator);

  return node == NULL ? NULL : node2gateway(node);
}

void
olsr_set_gateway(union olsr_ip_addr *originator, union olsr_ip_addr *subnetmask) {
  struct gateway_entry *gw;
  uint8_t *ip;

  gw = olsr_find_gateway(originator);
  if (!gw) {
    gw = olsr_cookie_malloc(gw_mem_cookie);

    gw->originator = *originator;
    gw->node.key = &gw->originator;

    avl_insert(&gateway_tree, &gw->node, AVL_DUP_NO);
  }

  ip = (uint8_t *)subnetmask;
  gw->uplink = deserialize_gw_speed(ip[olsr_cnf->ipsize - 2]);
  gw->downlink = deserialize_gw_speed(ip[olsr_cnf->ipsize - 1]);
}

void
olsr_delete_gateway(union olsr_ip_addr *originator) {
  struct gateway_entry *gw;

  gw = olsr_find_gateway(originator);
  if (gw) {
    avl_delete(&gateway_tree, &gw->node);

    olsr_cookie_free(gw_mem_cookie, gw);
  }
}

bool olsr_is_smart_gateway(union olsr_ip_addr *net, union olsr_ip_addr *mask) {
  uint8_t i;
  uint8_t *ip;

  ip = (uint8_t *)net;
  for (i=0; i<olsr_cnf->ipsize; i++) {
    if (*ip++) {
      return false;
    }
  }

  ip = (uint8_t *)mask;
  for (i=0; i<olsr_cnf->ipsize-2; i++) {
    if (*ip++ != 0) {
      return false;
    }
  }

  return ip[0] > 0 && ip[1] > 0;
}

void
olsr_print_gateway(void) {
#ifndef NODEBUG
  struct ipaddr_str buf;
  struct gateway_entry *gw;
  const int addrsize = olsr_cnf->ip_version == AF_INET ? 15 : 39;

  OLSR_PRINTF(0, "\n--- %s ---------------------------------------------------- GATEWAYS\n\n", olsr_wallclock_string());
  OLSR_PRINTF(0, "%-*s  %-9s %-9s\n", addrsize, "IP address", "Uplink", "Downlink");

  OLSR_FOR_ALL_GATEWAY_ENTRIES(gw) {
    OLSR_PRINTF(0, "%-*s  %-9u %-9u\n", addrsize, olsr_ip_to_string(&buf, &gw->originator), gw->uplink, gw->downlink);
  } OLSR_FOR_ALL_GATEWAY_ENTRIES_END(gw)
#endif
}

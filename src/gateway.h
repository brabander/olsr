/*
 * gateway.h
 *
 *  Created on: 05.01.2010
 *      Author: henning
 */

#ifndef GATEWAY_H_
#define GATEWAY_H_

#include "common/avl.h"
#include "defs.h"
#include "olsr.h"

#define MAXIMUM_GATEWAY_PREFIX_LENGTH 0

enum gateway_hna_flags {
  GW_HNA_FLAG_SMART      = 1<<0,
  GW_HNA_FLAG_UPLINK     = 1<<1,
  GW_HNA_FLAG_DOWNLINK   = 1<<2,
  GW_HNA_FLAG_IPV6PREFIX = 1<<3
};

enum gateway_hna_fields {
  GW_HNA_PAD         = 0,
  GW_HNA_FLAGS       = 1,
  GW_HNA_UPLINK      = 2,
  GW_HNA_DOWNLINK    = 3,
  GW_HNA_V6PREFIXLEN = 4,
  GW_HNA_V6PREFIX    = 8
};

struct gateway_entry {
  struct avl_node node;
  union olsr_ip_addr originator;
  struct olsr_ip_prefix external_prefix;
  uint32_t uplink;
  uint32_t downlink;
};

AVLNODE2STRUCT(node2gateway, struct gateway_entry, node);

#define OLSR_FOR_ALL_GATEWAY_ENTRIES(gw) \
{ \
  struct avl_node *gw_node, *next_gw_node; \
  for (gw_node = avl_walk_first(&gateway_tree); \
    gw_node; gw_node = next_gw_node) { \
    next_gw_node = avl_walk_next(gw_node); \
    gw = node2gateway(gw_node);
#define OLSR_FOR_ALL_GATEWAY_ENTRIES_END(gw) }}

extern struct avl_tree gateway_tree;

void olsr_init_gateways(void);
struct gateway_entry *olsr_find_gateway(union olsr_ip_addr *originator);
void olsr_set_gateway(union olsr_ip_addr *originator, union olsr_ip_addr *subnetmask);
void olsr_delete_gateway(union olsr_ip_addr *originator);
bool olsr_is_smart_gateway(union olsr_ip_addr *net, union olsr_ip_addr *mask);
void olsr_print_gateway(void);

extern union olsr_ip_addr smart_gateway_netmask;

#endif /* GATEWAY_H_ */

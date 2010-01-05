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

struct gateway_entry {
  struct avl_node node;
  union olsr_ip_addr originator;
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

#endif /* GATEWAY_H_ */

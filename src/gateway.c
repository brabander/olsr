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
static struct olsr_cookie_info *gw_mem_cookie = NULL;
static uint8_t smart_gateway_netmask[sizeof(union olsr_ip_addr)];

static uint32_t deserialize_gw_speed(uint8_t value) {
  uint32_t speed, exp;

  speed = (value >> 3)+1;
  exp = value & 7;
  while (exp-- > 0) {
    speed *= 10;
  }
  return speed;
}

static uint8_t serialize_gw_speed(uint32_t speed) {
  uint8_t exp = 0;

  if (speed == 0 || speed > 320000000) {
    return 0;
  }

  while (speed > 32 || (speed % 10) == 0) {
    speed /= 10;
    exp ++;
  }
  return ((speed-1) << 3) | exp;
}

void
olsr_init_gateways(void) {
  uint8_t *ip;
  gw_mem_cookie = olsr_alloc_cookie("Gateway cookie", OLSR_COOKIE_TYPE_MEMORY);
  olsr_cookie_set_memory_size(gw_mem_cookie, sizeof(struct gateway_entry));

  avl_init(&gateway_tree, avl_comp_default);

  memset(&smart_gateway_netmask, 0, sizeof(smart_gateway_netmask));

  if (olsr_cnf->smart_gw_active) {
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

    if (olsr_cnf->smart_gw_uplink > 0 || olsr_cnf->smart_gw_downlink > 0) {
      ip[GW_HNA_FLAGS] |= GW_HNA_FLAG_LINKSPEED;
      ip[GW_HNA_DOWNLINK] = serialize_gw_speed(olsr_cnf->smart_gw_downlink);
      ip[GW_HNA_UPLINK] = serialize_gw_speed(olsr_cnf->smart_gw_uplink);
    }
    if (olsr_cnf->ip_version == AF_INET6 && olsr_cnf->smart_gw_prefix.prefix_len > 0) {
      ip[GW_HNA_FLAGS] |= GW_HNA_FLAG_IPV6PREFIX;
      ip[GW_HNA_V6PREFIXLEN] = olsr_cnf->smart_gw_prefix.prefix_len;
      memcpy(&ip[GW_HNA_V6PREFIX], &olsr_cnf->smart_gw_prefix.prefix, 8);
    }
  }
}

struct gateway_entry *
olsr_find_gateway(union olsr_ip_addr *originator) {
  struct avl_node *node = avl_find(&gateway_tree, originator);

  return node == NULL ? NULL : node2gateway(node);
}

void
olsr_set_gateway(union olsr_ip_addr *originator, union olsr_ip_addr *mask, int prefixlen) {
  struct gateway_entry *gw;
  uint8_t *ptr = ((uint8_t *)mask) + ((prefixlen+7)/8);

  gw = olsr_find_gateway(originator);
  if (!gw) {
    gw = olsr_cookie_malloc(gw_mem_cookie);

    gw->originator = *originator;
    gw->node.key = &gw->originator;

    avl_insert(&gateway_tree, &gw->node, AVL_DUP_NO);
  }

  if ((ptr[GW_HNA_FLAGS] & GW_HNA_FLAG_LINKSPEED) != 0) {
    gw->uplink = deserialize_gw_speed(ptr[GW_HNA_UPLINK]);
    gw->downlink = deserialize_gw_speed(ptr[GW_HNA_DOWNLINK]);
  }
  else {
    gw->uplink = 1;
    gw->downlink = 1;
  }

  gw->ipv4 = (ptr[GW_HNA_FLAGS] & GW_HNA_FLAG_IPV4) != 0;
  gw->ipv4nat = (ptr[GW_HNA_FLAGS] & GW_HNA_FLAG_IPV4_NAT) != 0;

  if (olsr_cnf->ip_version == AF_INET6) {
    gw->ipv6 = (ptr[GW_HNA_FLAGS] & GW_HNA_FLAG_IPV6) != 0;

    /* do not reset prefixlength for ::ffff:0:0 HNAs */
    if (prefixlen == ipv6_internet_route.prefix_len) {
      memset(&gw->external_prefix, 0, sizeof(gw->external_prefix));

      if ((ptr[GW_HNA_FLAGS] & GW_HNA_FLAG_IPV6PREFIX) != 0
          && memcmp(mask->v6.s6_addr, &ipv6_internet_route.prefix, olsr_cnf->ipsize) == 0) {
        /* this is the right prefix (2000::/3), so we can copy the prefix */
        gw->external_prefix.prefix_len = ptr[GW_HNA_V6PREFIXLEN];
        memcpy(&gw->external_prefix.prefix, &ptr[GW_HNA_V6PREFIX], 8);
      }
    }
  }
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

bool olsr_is_smart_gateway(struct olsr_ip_prefix *prefix, union olsr_ip_addr *mask) {
  uint8_t *ptr;

  if (!ip_is_inetgw_prefix(prefix)) {
    return false;
  }

  ptr = ((uint8_t *)mask) + ((prefix->prefix_len+7)/8);
  return ptr[GW_HNA_PAD] == 0 && ptr[GW_HNA_FLAGS] != 0;
}

void olsr_modifiy_inetgw_netmask(union olsr_ip_addr *mask, int prefixlen) {
  uint8_t *ptr = ((uint8_t *)mask) + ((prefixlen+7)/8);

  memcpy(ptr, &smart_gateway_netmask, sizeof(smart_gateway_netmask) - prefixlen/8);
  if (olsr_cnf->has_ipv4_gateway) {
    ptr[GW_HNA_FLAGS] |= GW_HNA_FLAG_IPV4;

    if (olsr_cnf->smart_gw_uplink_nat) {
      ptr[GW_HNA_FLAGS] |= GW_HNA_FLAG_IPV4_NAT;
    }
  }
  if (olsr_cnf->has_ipv6_gateway) {
    ptr[GW_HNA_FLAGS] |= GW_HNA_FLAG_IPV6;
  }
  if (!olsr_cnf->has_ipv6_gateway || prefixlen != ipv6_internet_route.prefix_len){
    ptr[GW_HNA_FLAGS] &= ~GW_HNA_FLAG_IPV6PREFIX;
  }
}

void
olsr_print_gateway(void) {
#ifndef NODEBUG
  struct ipaddr_str buf;
  struct gateway_entry *gw;
  const int addrsize = olsr_cnf->ip_version == AF_INET ? 15 : 39;

  OLSR_PRINTF(0, "\n--- %s ---------------------------------------------------- GATEWAYS\n\n",
      olsr_wallclock_string());
  OLSR_PRINTF(0, "%-*s %-6s %-9s %-9s %s\n", addrsize, "IP address", "Type", "Uplink", "Downlink",
      olsr_cnf->ip_version == AF_INET ? "" : "External Prefix");

  OLSR_FOR_ALL_GATEWAY_ENTRIES(gw) {
    OLSR_PRINTF(0, "%-*s %s%c%s%c%c %-9u %-9u %s\n", addrsize, olsr_ip_to_string(&buf, &gw->originator),
        gw->ipv4nat ? "" : "   ",
        gw->ipv4 ? '4' : ' ',
        gw->ipv4nat ? "(N)" : "",
        (gw->ipv4 && gw->ipv6) ? ',' : ' ',
        gw->ipv6 ? '6' : ' ',
        gw->uplink, gw->downlink,
        gw->external_prefix.prefix_len == 0 ? "" : olsr_ip_prefix_to_string(&gw->external_prefix));
  } OLSR_FOR_ALL_GATEWAY_ENTRIES_END(gw)
#endif
}

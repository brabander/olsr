/*
 * avl_olsr_comp.c
 *
 *  Created on: 10.07.2010
 *      Author: henning
 */

#include <common/avl_olsr_comp.h>
#include <ipcalc.h>

avl_tree_comp avl_comp_default = NULL;
avl_tree_comp avl_comp_addr_origin_default = NULL;
avl_tree_comp avl_comp_prefix_default = NULL;
avl_tree_comp avl_comp_prefix_origin_default = NULL;

int
avl_comp_ipv4(const void *ip1, const void *ip2)
{
  return ip4cmp(ip1, ip2);
}

int
avl_comp_ipv6(const void *ip1, const void *ip2)
{
  return ip6cmp(ip1, ip2);
}

int
avl_comp_mac(const void *ip1, const void *ip2)
{
  return memcmp(ip1, ip2, 6);
}

int avl_comp_strcasecmp(const void *txt1, const void *txt2) {
  return strcasecmp(txt1, txt2);
}

int avl_comp_int(const void *p1, const void *p2) {
  const int *i1 = p1, *i2 = p2;
  return *i1 - *i2;
}

int avl_comp_interface_id(const void *p1, const void *p2) {
  const struct olsr_interface_id *id1 = p1, *id2 = p2;
  int diff;

  diff = olsr_ipcmp(&id1->ip, &id2->ip);
  if (diff != 0)
    return diff;
  return (int)(id1->if_index) - (int)(id2->if_index);
}


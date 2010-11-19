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
avl_comp_ipv4(const void *ip1, const void *ip2, void *ptr __attribute__ ((unused)))
{
  return ip4cmp(ip1, ip2);
}

int
avl_comp_ipv6(const void *ip1, const void *ip2, void *ptr __attribute__ ((unused)))
{
  return ip6cmp(ip1, ip2);
}

int
avl_comp_mac(const void *ip1, const void *ip2, void *ptr __attribute__ ((unused)))
{
  return memcmp(ip1, ip2, 6);
}

int avl_comp_strcasecmp(const void *txt1, const void *txt2, void *ptr __attribute__ ((unused))) {
  return strcasecmp(txt1, txt2);
}

int avl_comp_int(const void *p1, const void *p2, void *ptr __attribute__ ((unused))) {
  const int *i1 = p1, *i2 = p2;

  if (*i1 > *i2) {
    return 1;
  }
  if (*i1 < *i2) {
    return -1;
  }
  return 0;
}

int avl_comp_interface_id(const void *p1, const void *p2, void *ptr) {
  const struct olsr_interface_id *id1 = p1, *id2 = p2;
  int diff;

  diff = avl_comp_prefix_default(&id1->addr, &id2->addr, ptr);
  if (diff != 0)
    return diff;

  if (id1->if_index > id2->if_index) {
    return 1;
  }
  if (id1->if_index < id2->if_index) {
    return -1;
  }
  return 0;
}

/**
 * avl_comp_ipv4_prefix_origin
 *
 * compare two ipv4 prefixes.
 * first compare the prefixes, then
 *  then compare the prefix lengths,
 *  then compare origin codes
 *
 * return 0 if there is an exact match and
 * -1 / +1 depending on being smaller or bigger.
 */
int
avl_comp_ipv4_prefix_origin(const void *prefix1, const void *prefix2,
    void *ptr __attribute__ ((unused)))
{
  const struct olsr_ip_prefix *pfx1 = prefix1;
  const struct olsr_ip_prefix *pfx2 = prefix2;
  const uint32_t addr1 = ntohl(pfx1->prefix.v4.s_addr);
  const uint32_t addr2 = ntohl(pfx2->prefix.v4.s_addr);
  int diff;

  /* prefix */
  diff = addr2 - addr1;
  if (diff) {
    return diff;
  }

  /* prefix length */
  diff = (int)pfx2->prefix_len - (int)pfx1->prefix_len;
  if (diff) {
    return diff;
  }

  /* prefix origin */
  return (int)pfx2->prefix_origin - (int)pfx1->prefix_origin;
}

/**
 * avl_comp_ipv6_prefix_origin
 *
 * compare two ipv6 prefixes.
 * first compare the prefixes, then
 *  then compare the prefix lengths,
 *  then compare origin codes
 *
 * return 0 if there is an exact match and
 * -1 / +1 depending on being smaller or bigger.
 */
int
avl_comp_ipv6_prefix_origin(const void *prefix1, const void *prefix2,
    void *ptr __attribute__ ((unused)))
{
  int diff;
  const struct olsr_ip_prefix *pfx1 = prefix1;
  const struct olsr_ip_prefix *pfx2 = prefix2;

  /* prefix */
  diff = ip6cmp(&pfx1->prefix.v6, &pfx2->prefix.v6);
  if (diff) {
    return diff;
  }

  /* prefix length */
  diff = (int)pfx2->prefix_len - (int)pfx1->prefix_len;
  if (diff) {
    return diff;
  }

  /* prefix origin */
  return (int)pfx2->prefix_origin - (int)pfx1->prefix_origin;
}

/**
 * avl_comp_ipv4_prefix
 *
 * compare two ipv4 prefixes.
 * first compare the prefixes, then
 *  then compare the prefix lengths.
 *
 * return 0 if there is an exact match and
 * -1 / +1 depending on being smaller or bigger.
 */
int
avl_comp_ipv4_prefix(const void *prefix1, const void *prefix2,
    void *ptr __attribute__ ((unused)))
{
  const struct olsr_ip_prefix *pfx1 = prefix1;
  const struct olsr_ip_prefix *pfx2 = prefix2;
  int diff;

  /* prefix */
  diff = ip4cmp(&pfx1->prefix.v4, &pfx2->prefix.v4);
  if (diff) {
    return diff;
  }

  /* prefix length */
  return (int)pfx2->prefix_len - (int)pfx1->prefix_len;
}

/**
 * avl_comp_ipv6_prefix
 *
 * compare two ipv6 prefixes.
 * first compare the prefixes, then
 *  then compare the prefix lengths.
 *
 * return 0 if there is an exact match and
 * -1 / +1 depending on being smaller or bigger.
 */
int
avl_comp_ipv6_prefix(const void *prefix1, const void *prefix2,
    void *ptr __attribute__ ((unused)))
{
  int diff;
  const struct olsr_ip_prefix *pfx1 = prefix1;
  const struct olsr_ip_prefix *pfx2 = prefix2;

  /* prefix */
  diff = ip6cmp(&pfx1->prefix.v6, &pfx2->prefix.v6);
  if (diff) {
    return diff;
  }

  /* prefix length */
  return (int)pfx2->prefix_len - (int)pfx1->prefix_len;
}

/**
 * avl_comp_ipv4_addr_origin
 *
 * first compare the addresses, then compare the origin code.
 *
 * return 0 if there is an exact match and
 * -1 / +1 depending on being smaller or bigger.
 */
int
avl_comp_ipv4_addr_origin(const void *prefix1, const void *prefix2,
    void *ptr __attribute__ ((unused)))
{
  const struct olsr_ip_prefix *pfx1 = prefix1;
  const struct olsr_ip_prefix *pfx2 = prefix2;
  const uint32_t addr1 = ntohl(pfx1->prefix.v4.s_addr);
  const uint32_t addr2 = ntohl(pfx2->prefix.v4.s_addr);
  int diff;

  /* prefix */
  diff = addr2 - addr1;
  if (diff) {
    return diff;
  }

  /* prefix origin */
  return (int)pfx2->prefix_origin - (int)pfx1->prefix_origin;
}

/**
 * avl_comp_ipv6_addr_origin
 *
 * first compare the addresses, then compare the origin code.
 *
 * return 0 if there is an exact match and
 * -1 / +1 depending on being smaller or bigger.
 */
int
avl_comp_ipv6_addr_origin(const void *prefix1, const void *prefix2,
    void *ptr __attribute__ ((unused)))
{
  int diff;
  const struct olsr_ip_prefix *pfx1 = prefix1;
  const struct olsr_ip_prefix *pfx2 = prefix2;

  /* prefix */
  diff = ip6cmp(&pfx1->prefix.v6, &pfx2->prefix.v6);
  if (diff) {
    return diff;
  }

  /* prefix origin */
  return (int)pfx2->prefix_origin - (int)pfx1->prefix_origin;
}

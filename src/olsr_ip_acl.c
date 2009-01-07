
/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004, Andreas Tonnesen(andreto@olsr.org)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of olsr.org, olsrd nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Visit http://www.olsr.org for more information.
 *
 * If you find this software useful feel free to make a donation
 * to the project. For more information see the website or contact
 * the copyright holders.
 *
 */

#include "ipcalc.h"
#include "olsr_ip_prefix_list.h"
#include "olsr_ip_acl.h"

void
ip_acl_init(struct ip_acl *acl)
{
  list_head_init(&acl->accept);
  list_head_init(&acl->reject);
  acl->default_accept = false;
  acl->first_accept = false;
}

void
ip_acl_flush(struct ip_acl *acl)
{
  ip_prefix_list_flush(&acl->accept);
  ip_prefix_list_flush(&acl->reject);
}

void
ip_acl_add(struct ip_acl *acl, const union olsr_ip_addr *net, uint8_t prefix_len, bool reject)
{
  ip_prefix_list_add(reject ? &acl->reject : &acl->accept, net, prefix_len);
}

void
ip_acl_remove(struct ip_acl *acl, const union olsr_ip_addr *net, uint8_t prefix_len, bool reject)
{
  ip_prefix_list_remove(reject ? &acl->reject : &acl->accept, net, prefix_len);
}

bool
ip_acl_acceptable(struct ip_acl *acl, const union olsr_ip_addr *ip)
{
  struct list_node *first, *second;
  struct ip_prefix_entry *entry;

  first = acl->first_accept ? &acl->accept : &acl->reject;
  second = acl->first_accept ? &acl->reject : &acl->accept;

  /* first run */
  OLSR_FOR_ALL_IPPREFIX_ENTRIES(first, entry) {
    if (ip_in_net(ip, &entry->net)) {
      return acl->first_accept;
    }
  }
  OLSR_FOR_ALL_IPPREFIX_ENTRIES_END();

  /* second run */
  OLSR_FOR_ALL_IPPREFIX_ENTRIES(second, entry) {
    if (ip_in_net(ip, &entry->net)) {
      return !acl->first_accept;
    }
  }
  OLSR_FOR_ALL_IPPREFIX_ENTRIES_END();

  /* just use default */
  return acl->default_accept;
}

static int
ip_acl_plugin_parse(const char *value, union olsr_ip_addr *addr)
{
  /* space for txt representation of ipv6 + prefixlength */
  static char arg[INET6_ADDRSTRLEN + 5];

  char *c, *slash;
  bool ipv4 = false;
  bool ipv6 = false;
  int prefix;

  prefix = olsr_cnf->ip_version == AF_INET ? 32 : 128;

  strncpy(arg, value, sizeof(arg));
  arg[sizeof(arg) - 1] = 0;

  c = arg;
  slash = NULL;
  /* parse first word */
  while (*c && *c != ' ' && *c != '\t') {
    switch (*c) {
    case '.':
      ipv4 = true;
      break;
    case ':':
      ipv6 = true;
      break;
    case '/':
      slash = c;
      break;
    }
    c++;
  }

  /* look for second word */
  while (*c == ' ' || *c == '\t')
    c++;

  if (ipv4 == ipv6) {
    OLSR_PRINTF(0, "Error, illegal ip net '%s'\n", value);
    return -1;
  }

  if (slash) {
    /* split prefixlength from ip */
    *slash++ = 0;
  }

  if (inet_pton(olsr_cnf->ip_version, arg, addr) < 0) {
    OLSR_PRINTF(0, "Error, illegal ip net '%s'\n", value);
    return -1;
  }

  if (ipv4 && prefix == 128) {
    /* translate to ipv6 if neccessary */
    memmove(&addr->v6.s6_addr[12], &addr->v4.s_addr, sizeof(in_addr_t));
    memset(&addr->v6.s6_addr[0], 0x00, 10 * sizeof(uint8_t));
    memset(&addr->v6.s6_addr[10], 0xff, 2 * sizeof(uint8_t));
  } else if (ipv6 && olsr_cnf->ip_version == AF_INET) {
    OLSR_PRINTF(0, "Ignore Ipv6 address '%s' in ipv4 mode\n", value);
    return -1;
  }

  if (slash) {
    /* handle numeric netmask */
    prefix = (int)strtoul(slash, NULL, 10);
    if (prefix < 0 || prefix > (olsr_cnf->ip_version == AF_INET ? 32 : 128)) {
      OLSR_PRINTF(0, "Error, illegal prefix length in '%s'\n", value);
      return -1;
    }
  } else if (ipv4 && *c) {
    /* look for explicit netmask */
    union olsr_ip_addr netmask;

    if (inet_pton(AF_INET, c, &netmask) > 0) {
      prefix = olsr_netmask_to_prefix(&netmask);
      if (olsr_cnf->ip_version == AF_INET6) {
        prefix += 96;
      }
    }
  }
  return prefix;
}

int
ip_acl_add_plugin_accept(const char *value, void *data, set_plugin_parameter_addon addon __attribute__ ((unused)))
{
  union olsr_ip_addr ip;
  int prefix;

  prefix = ip_acl_plugin_parse(value, &ip);
  if (prefix == -1)
    return -1;

  ip_acl_add(data, &ip, prefix, false);
  return 0;
}

int
ip_acl_add_plugin_reject(const char *value, void *data, set_plugin_parameter_addon addon __attribute__ ((unused)))
{
  union olsr_ip_addr ip;
  int prefix;

  prefix = ip_acl_plugin_parse(value, &ip);
  if (prefix == -1)
    return -1;

  ip_acl_add(data, &ip, prefix, true);
  return 0;
}

int
ip_acl_add_plugin_checkFirst(const char *value, void *data, set_plugin_parameter_addon addon __attribute__ ((unused)))
{
  struct ip_acl *acl = data;
  acl->first_accept = strcasecmp(value, "accept") == 0;
  return 0;
}

int
ip_acl_add_plugin_defaultPolicy(const char *value, void *data, set_plugin_parameter_addon addon __attribute__ ((unused)))
{
  struct ip_acl *acl = data;
  acl->default_accept = strcasecmp(value, "accept") == 0;
  return 0;
}


/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2007, Bernd Petrovitsch <bernd-at-firmix.at>
 * Copyright (c) 2007, Hannes Gredler <hannes-at-gredler.at>
 * Copyright (c) 2008, aaron-at-localhost.lan
 * Copyright (c) 2008, Bernd Petrovitsch <bernd-at-firmix.at>
 * Copyright (c) 2008, Sven-Ola Tuecke <sven-ola-at-gmx.de>
 * Copyright (c) 2009, Sven-Ola Tuecke <sven-ola-at-gmx.de>
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

#include "plugin_util.h"
#include "ipcalc.h"
#include "olsr.h"
#include "defs.h"
#include "common/string.h"
#include "olsr_logging.h"

#include <arpa/inet.h>

int
set_plugin_port(const char *value, void *data, set_plugin_parameter_addon addon __attribute__ ((unused)))
{
  char *endptr;
  const unsigned int port = strtoul(value, &endptr, 0);
  if (*endptr != '\0' || endptr == value) {
    OLSR_WARN(LOG_PLUGINS, "Illegal port number \"%s\"", value);
    return 1;
  }
  if (port > 65535) {
    OLSR_WARN(LOG_PLUGINS, "Port number %u out of range", port);
    return 1;
  }
  if (data != NULL) {
    int *v = data;
    *v = port;
    OLSR_INFO(LOG_PLUGINS, "%s port number %u\n", "Got", port);
  } else {
    OLSR_INFO(LOG_PLUGINS, "%s port number %u\n", "Ignored", port);
  }
  return 0;
}

int
set_plugin_ipaddress(const char *value, void *data, set_plugin_parameter_addon addon __attribute__ ((unused)))
{
  char buf[INET6_ADDRSTRLEN];
  union olsr_ip_addr ip_addr;
  if (inet_pton(olsr_cnf->ip_version, value, &ip_addr) <= 0) {
    OLSR_WARN(LOG_PLUGINS, "Illegal IP address \"%s\"", value);
    return 1;
  }
  inet_ntop(olsr_cnf->ip_version, &ip_addr, buf, sizeof(buf));
  if (data != NULL) {
    union olsr_ip_addr *v = data;
    *v = ip_addr;
    OLSR_INFO(LOG_PLUGINS, "%s IP address %s\n", "Got", buf);
  } else {
    OLSR_INFO(LOG_PLUGINS, "%s IP address %s\n", "Ignored", buf);
  }
  return 0;
}


int
set_plugin_boolean(const char *value, void *data, set_plugin_parameter_addon addon __attribute__ ((unused)))
{
  int *v = data;
  if (strcasecmp(value, "yes") == 0 || strcasecmp(value, "true") == 0) {
    *v = 1;
  } else if (strcasecmp(value, "no") == 0 || strcasecmp(value, "false") == 0) {
    *v = 0;
  } else {
    return 1;
  }
  return 0;
}

int
set_plugin_int(const char *value, void *data, set_plugin_parameter_addon addon __attribute__ ((unused)))
{
  char *endptr;
  const int theint = strtol(value, &endptr, 0);
  if (*endptr != '\0' || endptr == value) {
    OLSR_WARN(LOG_PLUGINS, "Illegal int \"%s\"", value);
    return 1;
  }
  if (data != NULL) {
    int *v = data;
    *v = theint;
    OLSR_INFO(LOG_PLUGINS, "%s int %d\n", "Got", theint);
  } else {
    OLSR_INFO(LOG_PLUGINS, "%s int %d\n", "Ignored", theint);
  }
  return 0;
}

int
set_plugin_string(const char *value, void *data, set_plugin_parameter_addon addon)
{
  if (data != NULL) {
    char *v = data;
    if ((unsigned)strlen(value) >= addon.ui) {
      OLSR_WARN(LOG_PLUGINS, "String too long \"%s\"", value);
      return 1;
    }
    strscpy(v, value, addon.ui);
    OLSR_INFO(LOG_PLUGINS, "%s string %s\n", "Got", value);
  } else {
    OLSR_INFO(LOG_PLUGINS, "%s string %s\n", "Ignored", value);
  }
  return 0;
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
    OLSR_WARN(LOG_PLUGINS, "Error, illegal ip net '%s'\n", value);
    return -1;
  }

  if (slash) {
    /* split prefixlength from ip */
    *slash++ = 0;
  }

  if (inet_pton(ipv4 ? AF_INET : AF_INET6, arg, addr) < 0) {
    OLSR_WARN(LOG_PLUGINS, "Error, illegal ip net '%s'\n", value);
    return -1;
  }

  if (ipv4 && prefix == 128) {
    /* translate to ipv6 if neccessary */
    ip_map_4to6(addr);
  } else if (ipv6 && olsr_cnf->ip_version == AF_INET) {
    OLSR_WARN(LOG_PLUGINS, "Ignore Ipv6 address '%s' in ipv4 mode\n", value);
    return -1;
  }

  if (slash) {
    /* handle numeric netmask */
    prefix = (int)strtoul(slash, NULL, 10);
    if (prefix < 0 || prefix > (olsr_cnf->ip_version == AF_INET ? 32 : 128)) {
      OLSR_WARN(LOG_PLUGINS, "Error, illegal prefix length in '%s'\n", value);
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

/*
 * Local Variables:
 * mode: c
 * style: linux
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */

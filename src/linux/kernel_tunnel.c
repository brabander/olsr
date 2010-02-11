/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004, Andreas Tonnesen(andreto@olsr.org)
 * Copyright (c) 2007, Sven-Ola for the policy routing stuff
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

#include "kernel_tunnel.h"
#include "log.h"
#include "olsr_types.h"
#include "net_os.h"

#include <assert.h>

//ipip includes
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/ip.h>
#include <linux/if_tunnel.h>
#include <linux/ip6_tunnel.h>

//ifup includes
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <net/if.h>

static const char DEV_IPV4_TUNNEL[IFNAMSIZ] = "tunl0";
static const char DEV_IPV6_TUNNEL[IFNAMSIZ] = "ip6tnl0";

static bool store_iptunnel_state;

int olsr_os_init_iptunnel(void) {
  const char *dev = olsr_cnf->ip_version == AF_INET ? DEV_IPV4_TUNNEL : DEV_IPV6_TUNNEL;

  store_iptunnel_state = olsr_if_isup(dev);
fprintf(stderr, "device %s was %s\n", dev, store_iptunnel_state ? "up" : "down");
  if (store_iptunnel_state) {
    return 0;
  }
  return olsr_if_set_state(dev, true);
}

void olsr_os_cleanup_iptunnel(void) {
  if (!store_iptunnel_state) {
fprintf(stderr, "ifdown: %s\n", olsr_cnf->ip_version == AF_INET ? DEV_IPV4_TUNNEL : DEV_IPV6_TUNNEL);
    olsr_if_set_state(olsr_cnf->ip_version == AF_INET ? DEV_IPV4_TUNNEL : DEV_IPV6_TUNNEL, false);
  }
}

static const char *get_tunnelcmd_name(uint32_t cmd) {
  static const char ADD[] = "add";
  static const char CHANGE[] = "change";
  static const char DELETE[] = "delete";

  switch (cmd) {
    case SIOCADDTUNNEL:
      return ADD;
    case SIOCCHGTUNNEL:
      return CHANGE;
    case SIOCDELTUNNEL:
      return DELETE;
    default:
      return NULL;
  }
}

static int os_ip4_tunnel(const char *name, in_addr_t *target, uint32_t cmd)
{
  struct ifreq ifr;
  int err;
  struct ip_tunnel_parm p;

  /* no IPIP tunnel if OLSR runs with IPv6 */
  assert (olsr_cnf->ip_version == AF_INET);
  memset(&p, 0, sizeof(p));
  p.iph.version = 4;
  p.iph.ihl = 5;
  p.iph.protocol = IPPROTO_IPIP;
  if (target) {
    p.iph.daddr = *target;
  }
  strncpy(p.name, name, IFNAMSIZ);

  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, cmd == SIOCADDTUNNEL ? DEV_IPV4_TUNNEL : name, IFNAMSIZ);
  ifr.ifr_ifru.ifru_data = (void *) &p;

  if ((err = ioctl(olsr_cnf->ioctl_s, cmd, &ifr))) {
    olsr_syslog(OLSR_LOG_ERR, "Cannot %s a tunnel %s: %s (%d)\n",
        get_tunnelcmd_name(cmd), name, strerror(errno), errno);
  }
  return err;
}

static int os_ip6_tunnel(const char *name, struct in6_addr *target, uint32_t cmd, uint8_t proto)
{
  struct ifreq ifr;
  int err;
  struct ip6_tnl_parm p;

  /* no IPIP tunnel if OLSR runs with IPv6 */
  assert (olsr_cnf->ip_version == AF_INET6);
  memset(&p, 0, sizeof(p));
  p.proto = proto; //  ? IPPROTO_IPV6 : IPPROTO_IPIP;
  if (target) {
    p.raddr = *target;
  }
  strncpy(p.name, name, IFNAMSIZ);

  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, cmd == SIOCADDTUNNEL ? DEV_IPV6_TUNNEL : name, IFNAMSIZ);
  ifr.ifr_ifru.ifru_data = (void *) &p;

  if ((err = ioctl(olsr_cnf->ioctl_s, cmd, &ifr))) {
    olsr_syslog(OLSR_LOG_ERR, "Cannot %s a tunnel %s: %s (%d)\n",
        get_tunnelcmd_name(cmd), name, strerror(errno), errno);
  }
  return err;
}

int olsr_os_add_ipip_tunnel(const char *name, union olsr_ip_addr *target, bool transportV4) {
  if (olsr_cnf->ip_version == AF_INET) {
    assert(transportV4);

    return os_ip4_tunnel(name, &target->v4.s_addr, SIOCADDTUNNEL);
  }
  return os_ip6_tunnel(name, &target->v6, SIOCADDTUNNEL, transportV4 ? IPPROTO_IPIP : IPPROTO_IPV6);
}

int olsr_os_change_ipip_tunnel(const char *name, union olsr_ip_addr *target, bool transportV4) {
  if (olsr_cnf->ip_version == AF_INET) {
    assert(transportV4);

    return os_ip4_tunnel(name, &target->v4.s_addr, SIOCCHGTUNNEL);
  }
  return os_ip6_tunnel(name, &target->v6, SIOCCHGTUNNEL, transportV4 ? IPPROTO_IPIP : IPPROTO_IPV6);
}

int olsr_os_del_ipip_tunnel(const char *name, bool transportV4) {
  if (olsr_cnf->ip_version == AF_INET) {
    assert(transportV4);

    return os_ip4_tunnel(name, NULL, SIOCDELTUNNEL);
  }
  return os_ip6_tunnel(name, NULL, SIOCDELTUNNEL, transportV4 ? IPPROTO_IPIP : IPPROTO_IPV6);
}


/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004-2009, the olsr.org team - see HISTORY file
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


#if defined __FreeBSD__ || defined __MacOSX__ || defined __NetBSD__ || defined __OpenBSD__
#define ifr_netmask ifr_addr
#endif

#include "ifnet.h"
#include "ipcalc.h"
#include "interfaces.h"
#include "defs.h"
#include "olsr.h"
#include "net_os.h"
#include "net_olsr.h"
#include "parser.h"
#include "scheduler.h"
#include "generate_msg.h"
#include "olsr_time.h"
#include "lq_packet.h"
#include "log.h"
#include "link_set.h"
#include "../common/string.h"
#include "../valgrind/valgrind.h"

#include <signal.h>
#include <sys/types.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

#define BUFSPACE  (127*1024)    /* max. input buffer size to request */

#if 0
int
set_flag(char *ifname, short flag __attribute__ ((unused)))
{
  struct ifreq ifr;

  /* Get flags */
  strscpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
  if (ioctl(olsr_cnf->ioctl_s, SIOCGIFFLAGS, &ifr) < 0) {
    fprintf(stderr, "ioctl (get interface flags)");
    return -1;
  }

  strscpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
  //printf("Setting flags for if \"%s\"\n", ifr.ifr_name);
  if ((ifr.ifr_flags & (IFF_UP | IFF_RUNNING)) == 0) {
    /* Add UP */
    ifr.ifr_flags |= (IFF_UP | IFF_RUNNING);
    /* Set flags + UP */
    if (ioctl(olsr_cnf->ioctl_s, SIOCSIFFLAGS, &ifr) < 0) {
      fprintf(stderr, "ERROR(%s): %s\n", ifr.ifr_name, strerror(errno));
      return -1;
    }
  }
  return 1;
}
#endif

/**
 * Checks if an initialized interface is changed
 * that is if it has been set down or the address
 * has been changed.
 *
 *@param iface the olsr_if_config struct describing the interface
 */
int
chk_if_changed(struct olsr_if_config *iface)
{
#if !defined REMOVE_LOG_DEBUG || !defined REMOVE_LOG_INFO
  struct ipaddr_str buf;
#endif
  struct interface *ifp;
  struct ifreq ifr;
  int if_changes = 0;

  OLSR_DEBUG(LOG_INTERFACE, "Checking if %s is set down or changed\n", iface->name);

  ifp = iface->interf;
  if (!ifp) {
    return 0;
  }

  memset(&ifr, 0, sizeof(ifr));
  strscpy(ifr.ifr_name, iface->name, sizeof(ifr.ifr_name));
  /* Get flags (and check if interface exists) */
  if (ioctl(olsr_cnf->ioctl_s, SIOCGIFFLAGS, &ifr) < 0) {
    OLSR_WARN(LOG_INTERFACE, "No such interface: %s\n", iface->name);
    remove_interface(&iface->interf);
    return 0;
  }
  ifp->int_flags = ifr.ifr_flags;

  /*
   * First check if the interface is set DOWN
   */
  if ((ifp->int_flags & IFF_UP) == 0) {
    OLSR_DEBUG(LOG_INTERFACE, "\tInterface %s not up - removing it...\n", iface->name);
    remove_interface(&iface->interf);
    return 0;
  }

  /*
   * We do all the interface type checks over.
   * This is because the interface might be a PCMCIA card. Therefore
   * it might not be the same physical interface as we configured earlier.
   */

  /* Check broadcast */
  if (olsr_cnf->ip_version == AF_INET && !iface->cnf->ipv4_broadcast.v4.s_addr &&       /* Skip if fixed bcast */
      ((ifp->int_flags & IFF_BROADCAST)) == 0) {
    OLSR_DEBUG(LOG_INTERFACE, "\tNo broadcast - removing\n");
    remove_interface(&iface->interf);
    return 0;
  }

  if (ifp->int_flags & IFF_LOOPBACK) {
    OLSR_DEBUG(LOG_INTERFACE, "\tThis is a loopback interface - removing it...\n");
    remove_interface(&iface->interf);
    return 0;
  }

  /* trying to detect if interface is wireless. */
  ifp->is_wireless = check_wireless_interface(ifr.ifr_name);

  /* Set interface metric */
  ifp->int_metric = iface->cnf->weight.fixed ? iface->cnf->weight.value : calculate_if_metric(ifr.ifr_name);

  /* Get MTU */
  if (ioctl(olsr_cnf->ioctl_s, SIOCGIFMTU, &ifr) < 0) {
    ifp->int_mtu = 0;
  } else {
    ifr.ifr_mtu -= olsr_cnf->ip_version == AF_INET6 ? UDP_IPV6_HDRSIZE : UDP_IPV4_HDRSIZE;
    if (ifp->int_mtu != ifr.ifr_mtu) {
      ifp->int_mtu = ifr.ifr_mtu;
      /* Create new outputbuffer */
      net_remove_buffer(ifp);   /* Remove old */
      net_add_buffer(ifp);
    }
  }

  /* Get interface index */
  ifp->if_index = if_nametoindex(ifr.ifr_name);

  /*
   * Now check if the IP has changed
   */

  /* IP version 6 */
  if (olsr_cnf->ip_version == AF_INET6) {
    struct sockaddr_in6 tmp_saddr6;

    /* Get interface address */
    if (get_ipv6_address(ifr.ifr_name, &tmp_saddr6, iface->cnf->ipv6_addrtype) <= 0) {
      if (iface->cnf->ipv6_addrtype == OLSR_IP6T_SITELOCAL)
        OLSR_WARN(LOG_INTERFACE, "\tCould not find site-local IPv6 address for %s\n", ifr.ifr_name);
      else if (iface->cnf->ipv6_addrtype == OLSR_IP6T_UNIQUELOCAL)
        OLSR_WARN(LOG_INTERFACE, "\tCould not find unique-local IPv6 address for %s\n", ifr.ifr_name);
      else if (iface->cnf->ipv6_addrtype == OLSR_IP6T_GLOBAL)
        OLSR_WARN(LOG_INTERFACE, "\tCould not find global IPv6 address for %s\n", ifr.ifr_name);
      else
        OLSR_WARN(LOG_INTERFACE, "\tCould not find an IPv6 address for %s\n", ifr.ifr_name);
      remove_interface(&iface->interf);
      return 0;
    }

    OLSR_DEBUG(LOG_INTERFACE, "\tAddress: %s\n", ip6_to_string(&buf, &tmp_saddr6.sin6_addr));

    if (ip6cmp(&tmp_saddr6.sin6_addr, &ifp->int6_addr.sin6_addr) != 0) {
      OLSR_DEBUG(LOG_INTERFACE, "New IP address for %s:\n"
                 "\tOld: %s\n\tNew: %s\n",
                 ifr.ifr_name, ip6_to_string(&buf, &ifp->int6_addr.sin6_addr), ip6_to_string(&buf, &tmp_saddr6.sin6_addr));

      /* Check main addr */
      if (!olsr_cnf->fixed_origaddr && ip6cmp(&olsr_cnf->router_id.v6, &tmp_saddr6.sin6_addr) != 0) {
        /* Update main addr */
        olsr_cnf->router_id.v6 = tmp_saddr6.sin6_addr;
      }

      /* Update address */
      ifp->int6_addr.sin6_addr = tmp_saddr6.sin6_addr;
      ifp->ip_addr.v6 = tmp_saddr6.sin6_addr;

      if_changes = 1;
    }
  } else {
    /* IP version 4 */
    const struct sockaddr_in *tmp_saddr4 = (struct sockaddr_in *)&ifr.ifr_addr;

    /* Check interface address (IPv4) */
    if (ioctl(olsr_cnf->ioctl_s, SIOCGIFADDR, &ifr) < 0) {
      OLSR_DEBUG(LOG_INTERFACE, "\tCould not get address of interface - removing it\n");
      remove_interface(&iface->interf);
      return 0;
    }

    OLSR_DEBUG(LOG_INTERFACE, "\tAddress:%s\n", ip4_to_string(&buf, ifp->int_addr.sin_addr));

    if (ip4cmp(&ifp->int_addr.sin_addr, &tmp_saddr4->sin_addr) != 0) {
      /* New address */
      OLSR_DEBUG(LOG_INTERFACE, "IPv4 address changed for %s\n"
                 "\tOld:%s\n\tNew:%s\n",
                 ifr.ifr_name, ip4_to_string(&buf, ifp->int_addr.sin_addr), ip4_to_string(&buf, tmp_saddr4->sin_addr));

      ifp->int_addr = *(struct sockaddr_in *)&ifr.ifr_addr;
      if (!olsr_cnf->fixed_origaddr && ip4cmp(&olsr_cnf->router_id.v4, &ifp->ip_addr.v4) == 0) {
        OLSR_INFO(LOG_INTERFACE, "New main address: %s\n", ip4_to_string(&buf, tmp_saddr4->sin_addr));
        olsr_cnf->router_id.v4 = tmp_saddr4->sin_addr;
      }

      ifp->ip_addr.v4 = tmp_saddr4->sin_addr;

      if_changes = 1;
    }

    /* Check netmask */
    if (ioctl(olsr_cnf->ioctl_s, SIOCGIFNETMASK, &ifr) < 0) {
      OLSR_WARN(LOG_INTERFACE, "%s: ioctl (get broadaddr) failed", ifr.ifr_name);
      remove_interface(&iface->interf);
      return 0;
    }

    OLSR_DEBUG(LOG_INTERFACE, "\tNetmask:%s\n", ip4_to_string(&buf, ((struct sockaddr_in *)&ifr.ifr_netmask)->sin_addr));

    if (ip4cmp(&ifp->int_netmask.sin_addr, &((struct sockaddr_in *)&ifr.ifr_netmask)->sin_addr) != 0) {
      /* New address */
      OLSR_DEBUG(LOG_INTERFACE, "IPv4 netmask changed for %s\n"
                 "\tOld:%s\n\tNew:%s\n",
                 ifr.ifr_name,
                 ip4_to_string(&buf, ifp->int_netmask.sin_addr),
                 ip4_to_string(&buf, ((struct sockaddr_in *)&ifr.ifr_netmask)->sin_addr));

      ifp->int_netmask = *(struct sockaddr_in *)&ifr.ifr_netmask;

      if_changes = 1;
    }

    if (!iface->cnf->ipv4_broadcast.v4.s_addr) {
      /* Check broadcast address */
      if (ioctl(olsr_cnf->ioctl_s, SIOCGIFBRDADDR, &ifr) < 0) {
        OLSR_WARN(LOG_INTERFACE, "%s: ioctl (get broadaddr) failed", ifr.ifr_name);
        return 0;
      }

      OLSR_DEBUG(LOG_INTERFACE, "\tBroadcast address:%s\n",
                 ip4_to_string(&buf, ((struct sockaddr_in *)&ifr.ifr_broadaddr)->sin_addr));

      if (ifp->int_broadaddr.sin_addr.s_addr != ((struct sockaddr_in *)&ifr.ifr_broadaddr)->sin_addr.s_addr) {
        /* New address */
        OLSR_DEBUG(LOG_INTERFACE, "IPv4 broadcast changed for %s\n"
                   "\tOld:%s\n\tNew:%s\n",
                   ifr.ifr_name,
                   ip4_to_string(&buf, ifp->int_broadaddr.sin_addr),
                   ip4_to_string(&buf, ((struct sockaddr_in *)&ifr.ifr_broadaddr)->sin_addr));

        ifp->int_broadaddr = *(struct sockaddr_in *)&ifr.ifr_broadaddr;
        if_changes = 1;
      }
    }
  }
  if (if_changes) {
    run_ifchg_cbs(ifp, IFCHG_IF_UPDATE);
  }
  return if_changes;
}

static char basenamestr[32];
static const char *if_basename(const char *name);
static const char *
if_basename(const char *name)
{
  const char *p = strchr(name, ':');
  if (NULL == p || p - name >= (int)(ARRAYSIZE(basenamestr) - 1)) {
    return name;
  }
  memcpy(basenamestr, name, p - name);
  basenamestr[p - name] = 0;
  return basenamestr;
}

/**
 * Initializes a interface described by iface,
 * if it is set up and is of the correct type.
 *
 *@param iface the olsr_if_config struct describing the interface
 *@param so the socket to use for ioctls
 *
 */
int
chk_if_up(struct olsr_if_config *iface)
{
  struct interface *ifp;
  struct ifreq ifr;
  const char *ifr_basename;
#if !defined REMOVE_LOG_DEBUG || !defined REMOVE_LOG_INFO
  struct ipaddr_str buf;
#endif

  /*
   * Sanity check.
   */
  if (iface->interf) {
    return -1;
  }
  ifp = olsr_cookie_malloc(interface_mem_cookie);

   /*
   * Setup query block.
   */
  memset(&ifr, 0, sizeof(ifr));
  strscpy(ifr.ifr_name, iface->name, sizeof(ifr.ifr_name));

  OLSR_DEBUG(LOG_INTERFACE, "Checking %s:\n", ifr.ifr_name);

  /* Get flags (and check if interface exists) */
  if (ioctl(olsr_cnf->ioctl_s, SIOCGIFFLAGS, &ifr) < 0) {
    OLSR_DEBUG(LOG_INTERFACE, "\tNo such interface!\n");
    goto cleanup;
  }

  ifp->int_flags = ifr.ifr_flags;
  if ((ifp->int_flags & IFF_UP) == 0) {
    OLSR_DEBUG(LOG_INTERFACE, "\tInterface not up - skipping it...\n");
    goto cleanup;
  }

  /* Check broadcast */
  if (olsr_cnf->ip_version == AF_INET && !iface->cnf->ipv4_broadcast.v4.s_addr &&       /* Skip if fixed bcast */
      (ifp->int_flags & IFF_BROADCAST) == 0) {
    OLSR_DEBUG(LOG_INTERFACE, "\tNo broadcast - skipping\n");
    goto cleanup;
  }

  if (ifp->int_flags & IFF_LOOPBACK) {
    OLSR_DEBUG(LOG_INTERFACE, "\tThis is a loopback interface - skipping it...\n");
    goto cleanup;
  }

  /* trying to detect if interface is wireless. */
  ifp->is_wireless = check_wireless_interface(ifr.ifr_name);
  OLSR_DEBUG(LOG_INTERFACE, ifp->is_wireless ? "\tWireless interface detected\n" : "\tNot a wireless interface\n");

  ifr_basename = if_basename(ifr.ifr_name);

  /* IP version 6 */
  if (olsr_cnf->ip_version == AF_INET6) {
    /* Get interface address */
    int result;

    if (iface->cnf->ipv6_addrtype == OLSR_IP6T_AUTO) {
      if ((result = get_ipv6_address(ifr.ifr_name, &ifp->int6_addr, OLSR_IP6T_SITELOCAL)) > 0) {
        iface->cnf->ipv6_addrtype = OLSR_IP6T_SITELOCAL;
      } else {
        if ((result = get_ipv6_address(ifr.ifr_name, &ifp->int6_addr, OLSR_IP6T_UNIQUELOCAL)) > 0) {
          iface->cnf->ipv6_addrtype = OLSR_IP6T_UNIQUELOCAL;
        } else {
          if ((result = get_ipv6_address(ifr.ifr_name, &ifp->int6_addr, OLSR_IP6T_GLOBAL)) > 0) {
            iface->cnf->ipv6_addrtype = OLSR_IP6T_GLOBAL;
          }
        }
      }
    } else {
      result = get_ipv6_address(ifr.ifr_name, &ifp->int6_addr, iface->cnf->ipv6_addrtype);
    }

    if (result <= 0) {
      if (iface->cnf->ipv6_addrtype == OLSR_IP6T_SITELOCAL)
        OLSR_DEBUG(LOG_INTERFACE, "\tCould not find site-local IPv6 address for %s\n", ifr.ifr_name);
      else if (iface->cnf->ipv6_addrtype == OLSR_IP6T_UNIQUELOCAL)
        OLSR_DEBUG(LOG_INTERFACE, "\tCould not find unique-local IPv6 address for %s\n", ifr.ifr_name);
      else if (iface->cnf->ipv6_addrtype == OLSR_IP6T_GLOBAL)
        OLSR_DEBUG(LOG_INTERFACE, "\tCould not find global IPv6 address for %s\n", ifr.ifr_name);
      else
        OLSR_DEBUG(LOG_INTERFACE, "\tCould not find an IPv6 address for %s\n", ifr.ifr_name);
      goto cleanup;
    }

    OLSR_DEBUG(LOG_INTERFACE, "\tAddress: %s\n", ip6_to_string(&buf, &ifp->int6_addr.sin6_addr));

    /* Multicast */
    memset(&ifp->int6_multaddr, 0, sizeof(ifp->int6_multaddr));
    ifp->int6_multaddr.sin6_family = AF_INET6;
    ifp->int6_multaddr.sin6_flowinfo = htonl(0);
    ifp->int6_multaddr.sin6_scope_id = if_nametoindex(ifr.ifr_name);
    ifp->int6_multaddr.sin6_port = htons(olsr_cnf->olsr_port);
    ifp->int6_multaddr.sin6_addr = iface->cnf->ipv6_addrtype == OLSR_IP6T_SITELOCAL
      ? iface->cnf->ipv6_multi_site.v6 : iface->cnf->ipv6_multi_glbl.v6;

#ifdef __MacOSX__
    ifp->int6_multaddr.sin6_scope_id = 0;
#endif

    OLSR_DEBUG(LOG_INTERFACE, "\tMulticast: %s\n", ip6_to_string(&buf, &ifp->int6_multaddr.sin6_addr));

    ifp->ip_addr.v6 = ifp->int6_addr.sin6_addr;
  } else {
    /* IP version 4 */

    /* Get interface address (IPv4) */
    if (ioctl(olsr_cnf->ioctl_s, SIOCGIFADDR, &ifr) < 0) {
      OLSR_WARN(LOG_INTERFACE, "\tCould not get address of interface - skipping it\n");
      goto cleanup;
    }

    ifp->int_addr = *(struct sockaddr_in *)&ifr.ifr_addr;

    /* Find netmask */
    if (ioctl(olsr_cnf->ioctl_s, SIOCGIFNETMASK, &ifr) < 0) {
      OLSR_WARN(LOG_INTERFACE, "%s: ioctl (get netmask) failed", ifr.ifr_name);
      goto cleanup;
    }
    ifp->int_netmask = *(struct sockaddr_in *)&ifr.ifr_netmask;

    /* Find broadcast address */
    if (iface->cnf->ipv4_broadcast.v4.s_addr) {
      /* Specified broadcast */
      ifp->int_broadaddr.sin_addr = iface->cnf->ipv4_broadcast.v4;
    } else {
      /* Autodetect */
      if (ioctl(olsr_cnf->ioctl_s, SIOCGIFBRDADDR, &ifr) < 0) {
        OLSR_WARN(LOG_INTERFACE, "%s: ioctl (get broadaddr) failed", ifr.ifr_name);
        goto cleanup;
      }

      ifp->int_broadaddr = *(struct sockaddr_in *)&ifr.ifr_broadaddr;
    }

    /* Deactivate IP spoof filter */
    deactivate_spoof(ifr_basename, ifp, olsr_cnf->ip_version);

    /* Disable ICMP redirects */
    disable_redirects(ifr_basename, ifp, olsr_cnf->ip_version);

    ifp->ip_addr.v4 = ifp->int_addr.sin_addr;
  }

  /* check if interface with this IP already exists */
  if (if_ifwithaddr(&ifp->ip_addr)) {
    OLSR_ERROR(LOG_INTERFACE, "Warning, multiple interfaces with the same IP are deprecated. Use 'OriginatorAddress'"
               " option if you fear a changing main address. Future versions of OLSR might block using multiple"
               " interfaces with the same IP\n");
  }

  /* Get interface index */
  ifp->if_index = if_nametoindex(ifr.ifr_name);

  /* Set interface metric */
  ifp->int_metric = iface->cnf->weight.fixed ? iface->cnf->weight.value : calculate_if_metric(ifr.ifr_name);
  OLSR_DEBUG(LOG_INTERFACE, "\tMetric: %d\n", ifp->int_metric);

  /* Get MTU */
  if (ioctl(olsr_cnf->ioctl_s, SIOCGIFMTU, &ifr) < 0) {
    ifp->int_mtu = OLSR_DEFAULT_MTU;
  } else {
    ifp->int_mtu = ifr.ifr_mtu;
  }
  ifp->int_mtu -= olsr_cnf->ip_version == AF_INET6 ? UDP_IPV6_HDRSIZE : UDP_IPV4_HDRSIZE;

  ifp->ttl_index = -32;         /* For the first 32 TC's, fish-eye is disabled */

  /* Set up buffer */
  net_add_buffer(ifp);

  OLSR_DEBUG(LOG_INTERFACE, "\tMTU - IPhdr: %d\n", ifp->int_mtu);

  OLSR_INFO(LOG_INTERFACE, "Adding interface %s\n", iface->name);
  OLSR_DEBUG(LOG_INTERFACE, "\tIndex %d\n", ifp->if_index);

  if (olsr_cnf->ip_version == AF_INET) {
    OLSR_DEBUG(LOG_INTERFACE, "\tAddress:%s\n", ip4_to_string(&buf, ifp->int_addr.sin_addr));
    OLSR_DEBUG(LOG_INTERFACE, "\tNetmask:%s\n", ip4_to_string(&buf, ifp->int_netmask.sin_addr));
    OLSR_DEBUG(LOG_INTERFACE, "\tBroadcast address:%s\n", ip4_to_string(&buf, ifp->int_broadaddr.sin_addr));
  } else {
    OLSR_DEBUG(LOG_INTERFACE, "\tAddress: %s\n", ip6_to_string(&buf, &ifp->int6_addr.sin6_addr));
    OLSR_DEBUG(LOG_INTERFACE, "\tMulticast: %s\n", ip6_to_string(&buf, &ifp->int6_multaddr.sin6_addr));
  }

  /*
   * Clone interface name.
   */
  ifp->int_name = olsr_strdup(ifr_basename);

  ifp->immediate_send_tc = olsr_cnf->tc_params.emission_interval < iface->cnf->hello_params.emission_interval;
#if 0
  ifp->gen_properties = NULL;
#endif

  if (olsr_cnf->ip_version == AF_INET) {
    /* IP version 4 */
    /*
     * We create one socket for each interface and bind
     * the socket to it. This to ensure that we can control
     * on what interface the message is transmitted
     */
    ifp->olsr_socket = getsocket(BUFSPACE, ifp->int_name);
    if (ifp->olsr_socket < 0) {
      OLSR_ERROR(LOG_INTERFACE, "Could not initialize socket... exiting!\n\n");
      olsr_exit(EXIT_FAILURE);
    }
  } else {
    /* IP version 6 */

    /*
     * We create one socket for each interface and bind
     * the socket to it. This to ensure that we can control
     * on what interface the message is transmitted
     */
    ifp->olsr_socket = getsocket6(BUFSPACE, ifp->int_name);
    join_mcast(ifp, ifp->olsr_socket);
  }

  set_buffer_timer(ifp);

  /* Register socket */
  add_olsr_socket(ifp->olsr_socket, &olsr_input, NULL, NULL, SP_PR_READ);

#ifdef linux
  {
    /* Set TOS */
    int data = IPTOS_PREC(olsr_cnf->tos);
    if (setsockopt(ifp->olsr_socket, SOL_SOCKET, SO_PRIORITY, (char *)&data, sizeof(data)) < 0) {
      OLSR_WARN(LOG_INTERFACE, "setsockopt(SO_PRIORITY) error %s", strerror(errno));
    }
    data = IPTOS_TOS(olsr_cnf->tos);
    if (setsockopt(ifp->olsr_socket, SOL_IP, IP_TOS, (char *)&data, sizeof(data)) < 0) {
      OLSR_WARN(LOG_INTERFACE, "setsockopt(IP_TOS) error %s", strerror(errno));
    }
  }
#endif

  /*
   *Initialize sequencenumber as a random 16bit value
   */
  ifp->olsr_seqnum = random() & 0xFFFF;

  /*
   * Set main address if this is the only interface
   */
  if (!olsr_cnf->fixed_origaddr && olsr_ipcmp(&all_zero, &olsr_cnf->router_id) == 0) {
    olsr_cnf->router_id = ifp->ip_addr;
    OLSR_INFO(LOG_INTERFACE, "New main address: %s\n", olsr_ip_to_string(&buf, &olsr_cnf->router_id));
  }

  /*
   * Register functions for periodic message generation
   */
  ifp->hello_gen_timer =
    olsr_start_timer(iface->cnf->hello_params.emission_interval,
                     HELLO_JITTER, OLSR_TIMER_PERIODIC, &olsr_output_lq_hello, ifp, hello_gen_timer_cookie);
  ifp->tc_gen_timer =
    olsr_start_timer(olsr_cnf->tc_params.emission_interval,
                     TC_JITTER, OLSR_TIMER_PERIODIC, &olsr_output_lq_tc, ifp, tc_gen_timer_cookie);
  ifp->mid_gen_timer =
    olsr_start_timer(olsr_cnf->mid_params.emission_interval,
                     MID_JITTER, OLSR_TIMER_PERIODIC, &generate_mid, ifp, mid_gen_timer_cookie);
  ifp->hna_gen_timer =
    olsr_start_timer(olsr_cnf->hna_params.emission_interval,
                     HNA_JITTER, OLSR_TIMER_PERIODIC, &generate_hna, ifp, hna_gen_timer_cookie);

  ifp->hello_interval = iface->cnf->hello_params.emission_interval;
  ifp->hello_validity = iface->cnf->hello_params.validity_time;

  ifp->mode = iface->cnf->mode;

  /*
   * Call possible ifchange functions registered by plugins
   */
  run_ifchg_cbs(ifp, IFCHG_IF_ADD);

  /*
   * The interface is ready, lock it.
   */
  lock_interface(ifp);

  /*
   * Link to config.
   */
  iface->interf = ifp;
  lock_interface(iface->interf);

  /* Queue */
  list_node_init(&ifp->int_node);
  list_add_before(&interface_head, &ifp->int_node);

  return 1;

cleanup:
  olsr_cookie_free(interface_mem_cookie, ifp);
  return 0;
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */


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

#include "ipcalc.h"
#include "interfaces.h"
#include "defs.h"
#include "olsr.h"
#include "os_net.h"
#include "net_olsr.h"
#include "parser.h"
#include "scheduler.h"
#include "olsr_time.h"
#include "lq_packet.h"
#include "link_set.h"
#include "mid_set.h"
#include "hna_set.h"
#include "common/string.h"

#ifdef linux
#include "linux/linux_net.h"
#endif

#include <assert.h>
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
#include <fcntl.h>

#define BUFSPACE  (127*1024)    /* max. input buffer size to request */

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
  int int_flags;

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
    remove_interface(iface->interf);
    return 0;
  }
  int_flags = ifr.ifr_flags;

  /*
   * First check if the interface is set DOWN
   */
  if ((int_flags & IFF_UP) == 0) {
    OLSR_DEBUG(LOG_INTERFACE, "\tInterface %s not up - removing it...\n", iface->name);
    remove_interface(iface->interf);
    return 0;
  }

  /*
   * We do all the interface type checks over.
   * This is because the interface might be a PCMCIA card. Therefore
   * it might not be the same physical interface as we configured earlier.
   */

  /* Check broadcast */
  if (olsr_cnf->ip_version == AF_INET && !iface->cnf->ipv4_broadcast.v4.s_addr &&       /* Skip if fixed bcast */
      ((int_flags & IFF_BROADCAST)) == 0) {
    OLSR_DEBUG(LOG_INTERFACE, "\tNo broadcast - removing\n");
    remove_interface(iface->interf);
    return 0;
  }

  if (int_flags & IFF_LOOPBACK) {
    OLSR_DEBUG(LOG_INTERFACE, "\tThis is a loopback interface - removing it...\n");
    remove_interface(iface->interf);
    return 0;
  }

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
      remove_interface(iface->interf);
      return 0;
    }

    OLSR_DEBUG(LOG_INTERFACE, "\tAddress: %s\n", ip6_to_string(&buf, &tmp_saddr6.sin6_addr));

    if (ip6cmp(&tmp_saddr6.sin6_addr, &ifp->int_src.v6.sin6_addr) != 0) {
      OLSR_DEBUG(LOG_INTERFACE, "New IP address for %s:\n"
                 "\tOld: %s\n\tNew: %s\n",
                 ifr.ifr_name, ip6_to_string(&buf, &ifp->int_src.v6.sin6_addr), ip6_to_string(&buf, &tmp_saddr6.sin6_addr));

      /* Update address */
      ifp->int_src.v6.sin6_addr = tmp_saddr6.sin6_addr;
      ifp->ip_addr.v6 = tmp_saddr6.sin6_addr;

      if_changes = 1;
    }
  } else {
    /* IP version 4 */
    const struct sockaddr_in *tmp_saddr4 = (struct sockaddr_in *)(ARM_NOWARN_ALIGN(&ifr.ifr_addr));

    /* Check interface address (IPv4) */
    if (ioctl(olsr_cnf->ioctl_s, SIOCGIFADDR, &ifr) < 0) {
      OLSR_DEBUG(LOG_INTERFACE, "\tCould not get address of interface - removing it\n");
      remove_interface(iface->interf);
      return 0;
    }

    OLSR_DEBUG(LOG_INTERFACE, "\tAddress:%s\n", ip4_to_string(&buf, ifp->int_src.v4.sin_addr));

    if (ip4cmp(&ifp->int_src.v4.sin_addr, &tmp_saddr4->sin_addr) != 0) {
      /* New address */
      OLSR_DEBUG(LOG_INTERFACE, "IPv4 address changed for %s\n"
                 "\tOld:%s\n\tNew:%s\n",
                 ifr.ifr_name, ip4_to_string(&buf, ifp->int_src.v4.sin_addr), ip4_to_string(&buf, tmp_saddr4->sin_addr));

      ifp->int_src.v4 = *(struct sockaddr_in *)(ARM_NOWARN_ALIGN(&ifr.ifr_addr));
      ifp->ip_addr.v4 = tmp_saddr4->sin_addr;

      if_changes = 1;
    }

    /* Check netmask */
    if (ioctl(olsr_cnf->ioctl_s, SIOCGIFNETMASK, &ifr) < 0) {
      OLSR_WARN(LOG_INTERFACE, "%s: ioctl (get broadaddr) failed", ifr.ifr_name);
      remove_interface(iface->interf);
      return 0;
    }

    OLSR_DEBUG(LOG_INTERFACE, "\tNetmask:%s\n", ip4_to_string(&buf, ((struct sockaddr_in *)(ARM_NOWARN_ALIGN(&ifr.ifr_netmask)))->sin_addr));

    if (!iface->cnf->ipv4_broadcast.v4.s_addr) {
      /* Check broadcast address */
      if (ioctl(olsr_cnf->ioctl_s, SIOCGIFBRDADDR, &ifr) < 0) {
        OLSR_WARN(LOG_INTERFACE, "%s: ioctl (get broadaddr) failed", ifr.ifr_name);
        return 0;
      }

      OLSR_DEBUG(LOG_INTERFACE, "\tBroadcast address:%s\n",
                 ip4_to_string(&buf, ((struct sockaddr_in *)(ARM_NOWARN_ALIGN(&ifr.ifr_broadaddr)))->sin_addr));

      if (ifp->int_multicast.v4.sin_addr.s_addr != ((struct sockaddr_in *)(ARM_NOWARN_ALIGN(&ifr.ifr_broadaddr)))->sin_addr.s_addr) {
        /* New address */
        OLSR_DEBUG(LOG_INTERFACE, "IPv4 broadcast changed for %s\n"
                   "\tOld:%s\n\tNew:%s\n",
                   ifr.ifr_name,
                   ip4_to_string(&buf, ifp->int_multicast.v4.sin_addr),
                   ip4_to_string(&buf, ((struct sockaddr_in *)(ARM_NOWARN_ALIGN(&ifr.ifr_broadaddr)))->sin_addr));

        ifp->int_multicast.v4 = *(struct sockaddr_in *)(ARM_NOWARN_ALIGN(&ifr.ifr_broadaddr));
        if_changes = 1;
      }
    }
  }
  if (if_changes) {
    run_ifchg_cbs(ifp, IFCHG_IF_UPDATE);
  }
  return if_changes;
}

static const char *
if_basename(const char *name)
{
  static char basenamestr[32];
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
os_init_interface(struct interface *ifp, struct olsr_if_config *iface)
{
  struct ifreq ifr;
  const char *ifr_basename;
  int int_flags;
#if !defined REMOVE_LOG_DEBUG || !defined REMOVE_LOG_INFO
  struct ipaddr_str buf;
#endif

  /*
   * Sanity check.
   */
  assert (iface->interf == NULL);
  assert (ifp);

   /*
   * Setup query block.
   */
  memset(&ifr, 0, sizeof(ifr));
  strscpy(ifr.ifr_name, iface->name, sizeof(ifr.ifr_name));

  OLSR_DEBUG(LOG_INTERFACE, "Checking %s:\n", ifr.ifr_name);

  /* Get flags (and check if interface exists) */
  if (ioctl(olsr_cnf->ioctl_s, SIOCGIFFLAGS, &ifr) < 0) {
    OLSR_DEBUG(LOG_INTERFACE, "\tNo such interface!\n");
    return -1;
  }

  int_flags = ifr.ifr_flags;
  if ((int_flags & IFF_UP) == 0) {
    OLSR_DEBUG(LOG_INTERFACE, "\tInterface not up - skipping it...\n");
    return -1;
  }

  /* Check broadcast */
  if (olsr_cnf->ip_version == AF_INET && !iface->cnf->ipv4_broadcast.v4.s_addr &&       /* Skip if fixed bcast */
      (int_flags & IFF_BROADCAST) == 0) {
    OLSR_DEBUG(LOG_INTERFACE, "\tNo broadcast - skipping\n");
    return -1;
  }

  if (int_flags & IFF_LOOPBACK) {
    OLSR_DEBUG(LOG_INTERFACE, "\tThis is a loopback interface - skipping it...\n");
    return -1;
  }

  ifr_basename = if_basename(ifr.ifr_name);

  /* IP version 6 */
  if (olsr_cnf->ip_version == AF_INET6) {
    /* Get interface address */
    int result;

    memset(&ifp->int_src, 0, sizeof(ifp->int_multicast));
    ifp->int_src.v6.sin6_family = AF_INET6;
    ifp->int_src.v6.sin6_flowinfo = htonl(0);
    ifp->int_src.v6.sin6_scope_id = if_nametoindex(ifr.ifr_name);
    ifp->int_src.v6.sin6_port = htons(olsr_cnf->olsr_port);

    if (iface->cnf->ipv6_addrtype == OLSR_IP6T_AUTO) {
      if ((result = get_ipv6_address(ifr.ifr_name, &ifp->int_src.v6, OLSR_IP6T_SITELOCAL)) > 0) {
        iface->cnf->ipv6_addrtype = OLSR_IP6T_SITELOCAL;
      } else {
        if ((result = get_ipv6_address(ifr.ifr_name, &ifp->int_src.v6, OLSR_IP6T_UNIQUELOCAL)) > 0) {
          iface->cnf->ipv6_addrtype = OLSR_IP6T_UNIQUELOCAL;
        } else {
          if ((result = get_ipv6_address(ifr.ifr_name, &ifp->int_src.v6, OLSR_IP6T_GLOBAL)) > 0) {
            iface->cnf->ipv6_addrtype = OLSR_IP6T_GLOBAL;
          }
        }
      }
    } else {
      result = get_ipv6_address(ifr.ifr_name, &ifp->int_src.v6, iface->cnf->ipv6_addrtype);
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
      return -1;
    }

    OLSR_DEBUG(LOG_INTERFACE, "\tAddress: %s\n", ip6_to_string(&buf, &ifp->int_src.v6.sin6_addr));

    /* Multicast */
    memset(&ifp->int_multicast, 0, sizeof(ifp->int_multicast));
    ifp->int_multicast.v6.sin6_family = AF_INET6;
    ifp->int_multicast.v6.sin6_flowinfo = htonl(0);
    ifp->int_multicast.v6.sin6_scope_id = if_nametoindex(ifr.ifr_name);
    ifp->int_multicast.v6.sin6_port = htons(olsr_cnf->olsr_port);
    ifp->int_multicast.v6.sin6_addr = iface->cnf->ipv6_addrtype == OLSR_IP6T_SITELOCAL
      ? iface->cnf->ipv6_multi_site.v6 : iface->cnf->ipv6_multi_glbl.v6;

#ifdef __MacOSX__
    ifp->int6_multaddr.sin6_scope_id = 0;
#endif

    OLSR_DEBUG(LOG_INTERFACE, "\tMulticast: %s\n", ip6_to_string(&buf, &ifp->int_multicast.v6.sin6_addr));

    ifp->ip_addr.v6 = ifp->int_src.v6.sin6_addr;
  } else {
    /* IP version 4 */
    memset(&ifp->int_src, 0, sizeof(ifp->int_src));
    memset(&ifp->int_multicast, 0, sizeof(ifp->int_multicast));

    /* Get interface address (IPv4) */
    if (ioctl(olsr_cnf->ioctl_s, SIOCGIFADDR, &ifr) < 0) {
      OLSR_WARN(LOG_INTERFACE, "\tCould not get address of interface - skipping it\n");
      return -1;
    }

    ifp->int_src.v4 = *(struct sockaddr_in *)(ARM_NOWARN_ALIGN(&ifr.ifr_addr));

    /* Find broadcast address */
    if (iface->cnf->ipv4_broadcast.v4.s_addr) {
      /* Specified broadcast */
      ifp->int_multicast.v4.sin_addr = iface->cnf->ipv4_broadcast.v4;
    } else {
      /* Autodetect */
      if (ioctl(olsr_cnf->ioctl_s, SIOCGIFBRDADDR, &ifr) < 0) {
        OLSR_WARN(LOG_INTERFACE, "%s: ioctl (get broadaddr) failed", ifr.ifr_name);
        return -1;
      }

      ifp->int_multicast.v4 = *(struct sockaddr_in *)(ARM_NOWARN_ALIGN(&ifr.ifr_broadaddr));
    }

    ifp->int_src.v4.sin_family = AF_INET;
    ifp->int_src.v4.sin_port = htons(olsr_cnf->olsr_port);
    ifp->int_multicast.v4.sin_family = AF_INET;
    ifp->int_multicast.v4.sin_port = htons(olsr_cnf->olsr_port);

    ifp->ip_addr.v4 = ifp->int_src.v4.sin_addr;
  }

  /* check if interface with this IP already exists */
  if (if_ifwithaddr(&ifp->ip_addr)) {
    OLSR_ERROR(LOG_INTERFACE, "Warning, multiple interfaces with the same IP are deprecated. Use 'OriginatorAddress'"
               " option if you fear a changing main address. Future versions of OLSR might block using multiple"
               " interfaces with the same IP\n");
  }

  /* Get interface index */
  ifp->if_index = if_nametoindex(ifr.ifr_name);

  /* Get MTU */
  if (ioctl(olsr_cnf->ioctl_s, SIOCGIFMTU, &ifr) < 0) {
    ifp->int_mtu = OLSR_DEFAULT_MTU;
  } else {
    ifp->int_mtu = ifr.ifr_mtu;
  }
  ifp->int_mtu -= olsr_cnf->ip_version == AF_INET6 ? UDP_IPV6_HDRSIZE : UDP_IPV4_HDRSIZE;

  OLSR_DEBUG(LOG_INTERFACE, "\tMTU - IPhdr: %d\n", ifp->int_mtu);

  OLSR_INFO(LOG_INTERFACE, "Adding interface %s\n", iface->name);
  OLSR_DEBUG(LOG_INTERFACE, "\tIndex %d\n", ifp->if_index);

  if (olsr_cnf->ip_version == AF_INET) {
    OLSR_DEBUG(LOG_INTERFACE, "\tAddress:%s\n", ip4_to_string(&buf, ifp->int_src.v4.sin_addr));
    OLSR_DEBUG(LOG_INTERFACE, "\tBroadcast address:%s\n", ip4_to_string(&buf, ifp->int_multicast.v4.sin_addr));
  } else {
    OLSR_DEBUG(LOG_INTERFACE, "\tAddress: %s\n", ip6_to_string(&buf, &ifp->int_src.v6.sin6_addr));
    OLSR_DEBUG(LOG_INTERFACE, "\tMulticast: %s\n", ip6_to_string(&buf, &ifp->int_multicast.v6.sin6_addr));
  }

  /*
   * Clone interface name.
   */
  ifp->int_name = olsr_strdup(ifr_basename);

  /* Set interface options */
#ifdef linux
  net_os_set_ifoptions(ifr_basename, ifp);
#endif

  return 0;
}

void
os_cleanup_interface(struct interface *ifp) {
#ifdef linux
  net_os_restore_ifoption(ifp);
#endif
}

int
os_socket_set_nonblocking(int fd)
{
  /* make the fd non-blocking */
  int socket_flags = fcntl(fd, F_GETFL);
  if (socket_flags < 0) {
    OLSR_WARN(LOG_NETWORKING, "Cannot get the socket flags: %s", strerror(errno));
    return -1;
  }
  if (fcntl(fd, F_SETFL, socket_flags | O_NONBLOCK) < 0) {
    OLSR_WARN(LOG_NETWORKING, "Cannot set the socket flags: %s", strerror(errno));
    return -1;
  }
  return 0;
}


/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

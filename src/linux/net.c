
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

/*
 * Linux spesific code
 */

#include "ipcalc.h"
#include "common/string.h"
#include "olsr_protocol.h"
#include "olsr_logging.h"
#include "olsr.h"
#include "os_kernel_tunnel.h"
#include "os_net.h"
#include "linux/linux_net.h"

#include <net/if.h>
#include <netinet/ip.h>

#include <sys/ioctl.h>
#include <sys/utsname.h>

#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <syslog.h>
#include <unistd.h>

#define IPV6_ADDR_LOOPBACK      0x0010U
#define IPV6_ADDR_LINKLOCAL     0x0020U
#define IPV6_ADDR_SITELOCAL     0x0040U

/* ip forwarding */
#define PROC_IPFORWARD_V4 "/proc/sys/net/ipv4/ip_forward"
#define PROC_IPFORWARD_V6 "/proc/sys/net/ipv6/conf/all/forwarding"

/* Redirect proc entry */
#define PROC_IF_REDIRECT "/proc/sys/net/ipv4/conf/%s/send_redirects"
#define PROC_ALL_REDIRECT "/proc/sys/net/ipv4/conf/all/send_redirects"

/* IP spoof proc entry */
#define PROC_IF_SPOOF "/proc/sys/net/ipv4/conf/%s/rp_filter"
#define PROC_ALL_SPOOF "/proc/sys/net/ipv4/conf/all/rp_filter"


/* list of IPv6 interfaces */
#define PATH_PROCNET_IFINET6           "/proc/net/if_inet6"

/*
 *Wireless definitions for ioctl calls
 *(from linux/wireless.h)
 */
#define SIOCGIWNAME	0x8B01  /* get name == wireless protocol */
#define SIOCGIWRATE	0x8B21  /* get default bit rate (bps) */

/* The original state of the IP forwarding proc entry */
static char orig_fwd_state;
static char orig_global_redirect_state;
static char orig_global_rp_filter;
static char orig_tunnel_rp_filter;
#if 0 // should not be necessary for IPv6 */
static char orig_tunnel6_rp_filter;
#endif

static int writeToProc(const char *file, char *old, char value) {
  int fd;
  char rv;

  if ((fd = open(file, O_RDWR)) < 0) {
    OLSR_WARN(LOG_INTERFACE, "Cannot open proc entry %s: %s (%d)\n", file, strerror(errno), errno);
    return -1;
  }

  if (read(fd, &rv, 1) != 1) {
    OLSR_WARN(LOG_INTERFACE, "Cannot read proc entry %s: %s (%d)\n", file, strerror(errno), errno);
    return -1;
  }

  if (rv != value) {
    if (lseek(fd, SEEK_SET, 0) == -1) {
      OLSR_WARN(LOG_INTERFACE, "Cannot rewind proc entry %s: %s (%d)\n", file, strerror(errno), errno);
      return -1;
    }

    if (write(fd, &value, 1) != 1) {
      OLSR_WARN(LOG_INTERFACE, "Cannot write proc entry %s: %s (%d)\n", file, strerror(errno), errno);
      return -1;
    }
  }

  if (close(fd) != 0) {
    OLSR_WARN(LOG_INTERFACE, "Cannot close proc entry %s: %s (%d)\n", file, strerror(errno), errno);
    return -1;
  }

  if (old) {
    *old = rv;
  }
  OLSR_DEBUG(LOG_INTERFACE, "Writing '%c' (was %c) to %s", value, rv, file);
  return 0;
}

static bool is_at_least_linuxkernel_2_6_31(void) {
  struct utsname uts;

  memset(&uts, 0, sizeof(uts));
  if (uname(&uts)) {
    OLSR_WARN(LOG_NETWORKING, "Cannot not read kernel version: %s (%d)\n", strerror(errno), errno);
    return false;
  }

  if (strncmp(uts.release, "2.6.",4) != 0) {
    return false;
  }
  return atoi(&uts.release[4]) >= 31;
}

/**
 * Setup global interface options (icmp redirect, ip forwarding, rp_filter)
 * @return 1 on success 0 on failure
 */
void
os_init_global_ifoptions(void) {
  if (writeToProc(olsr_cnf->ip_version == AF_INET ? PROC_IPFORWARD_V4 : PROC_IPFORWARD_V6, &orig_fwd_state, '1')) {
    OLSR_WARN(LOG_INTERFACE, "Warning, could not enable IP forwarding!\n"
        "you should manually ensure that IP forwarding is enabled!\n\n");
    // TODO olsr_startup_sleep(3);
  }

  if (olsr_cnf->smart_gw_active) {
    char procfile[FILENAME_MAX];

    /* Generate the procfile name */
    if (olsr_cnf->ip_version == AF_INET || olsr_cnf->use_niit) {
      snprintf(procfile, sizeof(procfile), PROC_IF_SPOOF, TUNNEL_ENDPOINT_IF);
      if (writeToProc(procfile, &orig_tunnel_rp_filter, '0')) {
        OLSR_WARN(LOG_INTERFACE, "WARNING! Could not disable the IP spoof filter for tunnel!\n"
            "you should mannually ensure that IP spoof filtering is disabled!\n\n");

        // TODO olsr_startup_sleep(3);
      }
    }
  }

  if (olsr_cnf->ip_version == AF_INET) {
    if (writeToProc(PROC_ALL_REDIRECT, &orig_global_redirect_state, '0')) {
      OLSR_WARN(LOG_INTERFACE, "WARNING! Could not disable ICMP redirects!\n"
          "you should manually ensure that ICMP redirects are disabled!\n\n");

      // TODO olsr_startup_sleep(3);
    }

    /* check kernel version and disable global rp_filter */
    if (is_at_least_linuxkernel_2_6_31()) {
      if (writeToProc(PROC_ALL_SPOOF, &orig_global_rp_filter, '0')) {
        OLSR_WARN(LOG_INTERFACE, "WARNING! Could not disable global rp_filter (necessary for kernel 2.6.31 and higher!\n"
            "you should manually ensure that rp_filter is disabled!\n\n");

        // TODO olsr_startup_sleep(3);
      }
    }
  }
  return;
}

/**
 *
 *@return 1 on sucess 0 on failiure
 */
int
net_os_set_ifoptions(const char *if_name, struct interface *iface)
{
  char procfile[FILENAME_MAX];
  if (olsr_cnf->ip_version == AF_INET6)
    return -1;

  /* Generate the procfile name */
  snprintf(procfile, sizeof(procfile), PROC_IF_REDIRECT, if_name);

  if (writeToProc(procfile, &iface->nic_state.redirect, '0')) {
    OLSR_WARN(LOG_INTERFACE, "WARNING! Could not disable ICMP redirects!\n"
        "you should mannually ensure that ICMP redirects are disabled!\n\n");
    // TODO olsr_startup_sleep(3);
    return 0;
  }

  /* Generate the procfile name */
  snprintf(procfile, sizeof(procfile), PROC_IF_SPOOF, if_name);

  if (writeToProc(procfile, &iface->nic_state.spoof, '0')) {
    OLSR_WARN(LOG_INTERFACE, "WARNING! Could not disable the IP spoof filter!\n"
        "you should mannually ensure that IP spoof filtering is disabled!\n\n");

    // TODO olsr_startup_sleep(3);
    return 0;
  }
  return 1;
}

void net_os_restore_ifoption(struct interface *ifs) {
  char procfile[FILENAME_MAX];

  /* ICMP redirects */
  snprintf(procfile, sizeof(procfile), PROC_IF_REDIRECT, ifs->int_name);
  if (writeToProc(procfile, NULL, ifs->nic_state.redirect)) {
    OLSR_WARN(LOG_INTERFACE, "Could not restore icmp_redirect for interface %s\n", ifs->int_name);
  }

  /* Spoof filter */
  sprintf(procfile, PROC_IF_SPOOF, ifs->int_name);
  if (writeToProc(procfile, NULL, ifs->nic_state.spoof)) {
    OLSR_WARN(LOG_INTERFACE, "Could not restore rp_filter for interface %s\n", ifs->int_name);
  }
}
/**
 *Resets the spoof filter and ICMP redirect settings
 */
void
os_cleanup_global_ifoptions(void)
{
  char procfile[FILENAME_MAX];
  OLSR_DEBUG(LOG_INTERFACE, "Restoring network state\n");

  /* Restore IP forwarding to "off" */
  if (writeToProc(olsr_cnf->ip_version == AF_INET ? PROC_IPFORWARD_V4 : PROC_IPFORWARD_V6, NULL, orig_fwd_state)) {
    OLSR_WARN(LOG_INTERFACE, "Could not restore ip_forward settings\n");
  }

  if (olsr_cnf->smart_gw_active && (olsr_cnf->ip_version == AF_INET || olsr_cnf->use_niit)) {
    /* Generate the procfile name */
    snprintf(procfile, sizeof(procfile), PROC_IF_SPOOF, TUNNEL_ENDPOINT_IF);
    if (writeToProc(procfile, NULL, orig_tunnel_rp_filter)) {
      OLSR_WARN(LOG_INTERFACE, "WARNING! Could not restore the IP spoof filter for tunnel!\n");
    }
  }

  if (olsr_cnf->ip_version == AF_INET) {
    /* Restore global ICMP redirect setting */
    if (writeToProc(PROC_ALL_REDIRECT, NULL, orig_global_redirect_state)) {
      OLSR_WARN(LOG_INTERFACE, "Could not restore global icmp_redirect setting\n");
    }

    /* Restore global rp_filter setting for linux 2.6.31+ */
    if (is_at_least_linuxkernel_2_6_31()) {
      if (writeToProc(PROC_ALL_SPOOF, NULL, orig_global_rp_filter)) {
        OLSR_WARN(LOG_INTERFACE, "Could not restore global rp_filter setting\n");
      }
    }
  }
}

/**
 *Bind a socket to a device
 *
 *@param sock the socket to bind
 *@param dev_name name of the device
 *
 *@return negative if error
 */

static int
bind_socket_to_device(int sock, const char *dev_name)
{
  /*
   *Bind to device using the SO_BINDTODEVICE flag
   */
  OLSR_DEBUG(LOG_NETWORKING, "Binding socket %d to device %s\n", sock, dev_name);
  return setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, dev_name, strlen(dev_name) + 1);
}


/**
 *Creates a nonblocking broadcast socket.
 *@param sa sockaddr struct. Used for bind(2).
 *@return the FD of the socket or -1 on error.
 */
int
os_getsocket4(const char *if_name, uint16_t port, int bufspace, union olsr_sockaddr *bindto)
{
  struct sockaddr_in sin4;
  int on;
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    OLSR_ERROR(LOG_NETWORKING, "Cannot open socket for OLSR PDUs (%s)\n", strerror(errno));
    olsr_exit(EXIT_FAILURE);
  }

  on = 1;
#ifdef SO_BROADCAST
  if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) < 0) {
    OLSR_ERROR(LOG_NETWORKING, "Cannot set socket for OLSR PDUs to broadcast mode (%s)\n", strerror(errno));
    close(sock);
    olsr_exit(EXIT_FAILURE);
  }
#endif

  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
    OLSR_ERROR(LOG_NETWORKING, "Cannot reuse address for OLSR PDUs (%s)\n", strerror(errno));
    close(sock);
    olsr_exit(EXIT_FAILURE);
  }
#ifdef SO_RCVBUF
  if(bufspace > 0) {
    for (on = bufspace;; on -= 1024) {
      if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &on, sizeof(on)) == 0) {
        OLSR_DEBUG(LOG_NETWORKING, "Set socket buffer space to %d\n", on);
        break;
      }
      if (on <= 8 * 1024) {
        OLSR_WARN(LOG_NETWORKING, "Could not set a socket buffer space for OLSR PDUs (%s)\n", strerror(errno));
        break;
      }
    }
  }
#endif

  /*
   * WHEN USING KERNEL 2.6 THIS MUST HAPPEN PRIOR TO THE PORT BINDING!!!!
   */

  /* Bind to device */
  if (bind_socket_to_device(sock, if_name) < 0) {
    OLSR_ERROR(LOG_NETWORKING, "Cannot bind socket for OLSR PDUs to interface %s: %s (%d)\n", if_name, strerror(errno), errno);
    close(sock);
    olsr_exit(EXIT_FAILURE);
  }

  if (bindto == NULL) {
    memset(&sin4, 0, sizeof(sin4));
    sin4.sin_family = AF_INET;
    sin4.sin_port = htons(port);
    sin4.sin_addr.s_addr = 0;
    bindto = (union olsr_sockaddr *)&sin4;
  }
  if (bind(sock, &bindto->std, sizeof(*bindto)) < 0) {
    OLSR_ERROR(LOG_NETWORKING, "Coult not bind socket for OLSR PDUs to port (%s)\n", strerror(errno));
    close(sock);
    olsr_exit(EXIT_FAILURE);
  }

  os_socket_set_nonblocking(sock);
  return sock;
}


/**
 *Creates a nonblocking IPv6 socket
 *@param sin sockaddr_in6 struct. Used for bind(2).
 *@return the FD of the socket or -1 on error.
 */
int
os_getsocket6(const char *if_name, uint16_t port, int bufspace, union olsr_sockaddr *bindto)
{
  struct sockaddr_in6 sin6;
  int on;
  int sock = socket(AF_INET6, SOCK_DGRAM, 0);
  if (sock < 0) {
    OLSR_ERROR(LOG_NETWORKING, "Cannot open socket for OLSR PDUs (%s)\n", strerror(errno));
    olsr_exit(EXIT_FAILURE);
  }
#ifdef IPV6_V6ONLY
  on = 1;
  if (setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on)) < 0) {
    OLSR_WARN(LOG_NETWORKING, "Cannot set socket for OLSR PDUs to ipv6 only (%s)\n", strerror(errno));
  }
#endif


  //#ifdef SO_BROADCAST
  /*
     if (setsockopt(sock, SOL_SOCKET, SO_MULTICAST, &on, sizeof(on)) < 0)
     {
     perror("setsockopt");
     syslog(LOG_ERR, "setsockopt SO_BROADCAST: %m");
     close(sock);
     return (-1);
     }
   */
  //#endif

#ifdef SO_RCVBUF
  if(bufspace > 0) {
    for (on = bufspace;; on -= 1024) {
      if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &on, sizeof(on)) == 0) {
        OLSR_DEBUG(LOG_NETWORKING, "Set socket buffer space to %d\n", on);
        break;
      }
      if (on <= 8 * 1024) {
        OLSR_WARN(LOG_NETWORKING, "Could not set a socket buffer space for OLSR PDUs (%s)\n", strerror(errno));
        break;
      }
    }
  }
#endif

  on = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
    OLSR_ERROR(LOG_NETWORKING, "Cannot reuse address for socket for OLSR PDUs (%s)\n", strerror(errno));
    close(sock);
    olsr_exit(EXIT_FAILURE);
  }

  /*
   * we are abusing "on" here. The value is 1 which is our intended
   * hop limit value.
   */
  if (setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &on, sizeof(on)) < 0) {
    OLSR_ERROR(LOG_NETWORKING, "Cannot set multicast hops to 1 for socket for OLSR PDUs (%s)\n", strerror(errno));
    close(sock);
    olsr_exit(EXIT_FAILURE);
  }


  /*
   * WHEN USING KERNEL 2.6 THIS MUST HAPPEN PRIOR TO THE PORT BINDING!!!!
   */

  /* Bind to device */
  if (bind_socket_to_device(sock, if_name) < 0) {
    OLSR_ERROR(LOG_NETWORKING, "Cannot bind socket for OLSR PDUs to interface %s: %s (%d)\n", if_name, strerror(errno), errno);
    close(sock);
    olsr_exit(EXIT_FAILURE);
  }

  if (bindto == NULL) {
    memset(&sin6, 0, sizeof(sin6));
    sin6.sin6_family = AF_INET6;
    sin6.sin6_port = htons(port);
    bindto = (union olsr_sockaddr *)&sin6;
  }
  if (bind(sock, &bindto->std, sizeof(*bindto)) < 0) {
    OLSR_ERROR(LOG_NETWORKING, "Cannot bind socket for OLSR PDUs (%s)\n", strerror(errno));
    close(sock);
    olsr_exit(EXIT_FAILURE);
  }

  os_socket_set_nonblocking(sock);
  return sock;
}


static int
join_mcast(struct interface *ifs, int sock)
{
  /* See linux/in6.h */
#if !defined REMOVE_LOG_INFO
  struct ipaddr_str buf;
#endif
  struct ipv6_mreq mcastreq;

  mcastreq.ipv6mr_multiaddr = ifs->int_multicast.v6.sin6_addr;
  mcastreq.ipv6mr_interface = ifs->if_index;

  OLSR_INFO(LOG_NETWORKING, "Interface %s joining multicast %s\n", ifs->int_name,
            olsr_sockaddr_to_string(&buf, &ifs->int_multicast));
  /* Send multicast */
  if (setsockopt(sock, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, (char *)&mcastreq, sizeof(struct ipv6_mreq))
      < 0) {
    OLSR_WARN(LOG_NETWORKING, "Cannot join multicast group (%s)\n", strerror(errno));
    return -1;
  }
#if 0
  /* Old libc fix */
#ifdef IPV6_JOIN_GROUP
  /* Join reciever group */
  if (setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, (char *)&mcastreq, sizeof(struct ipv6_mreq))
      < 0)
#else
  /* Join reciever group */
  if (setsockopt(sock, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, (char *)&mcastreq, sizeof(struct ipv6_mreq))
      < 0)
#endif
  {
    perror("Join multicast send");
    return -1;
  }
#endif
  if (setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_IF, (char *)&mcastreq.ipv6mr_interface, sizeof(mcastreq.ipv6mr_interface))
      < 0) {
    OLSR_WARN(LOG_NETWORKING, "Cannot set multicast interface (%s)\n", strerror(errno));
    return -1;
  }

  return 0;
}

void
os_socket_set_olsr_options(struct interface * ifs, int sock) {
  /* Set TOS */
  int data = IPTOS_PREC(olsr_cnf->tos);
  if (setsockopt(sock, SOL_SOCKET, SO_PRIORITY, (char *)&data, sizeof(data)) < 0) {
    OLSR_WARN(LOG_INTERFACE, "setsockopt(SO_PRIORITY) error %s", strerror(errno));
  }
  data = IPTOS_TOS(olsr_cnf->tos);
  if (setsockopt(sock, SOL_IP, IP_TOS, (char *)&data, sizeof(data)) < 0) {
    OLSR_WARN(LOG_INTERFACE, "setsockopt(IP_TOS) error %s", strerror(errno));
  }

  join_mcast(ifs, sock);
}

/*
 *From net-tools lib/interface.c
 *
 */
int
get_ipv6_address(char *ifname, struct sockaddr_in6 *saddr6, int addrtype6)
{
  int rv = 0;
  FILE *f = fopen(_PATH_PROCNET_IFINET6, "r");
  if (f != NULL) {
    char devname[IFNAMSIZ];
    char addr6p[8][5];
    int plen, scope, dad_status, if_idx;
    bool found = false;
    while (fscanf(f, "%4s%4s%4s%4s%4s%4s%4s%4s %02x %02x %02x %02x %20s\n",
                  addr6p[0], addr6p[1], addr6p[2], addr6p[3],
                  addr6p[4], addr6p[5], addr6p[6], addr6p[7], &if_idx, &plen, &scope, &dad_status, devname) != EOF) {
      if (strcmp(devname, ifname) == 0) {
        char addr6[40];
        sprintf(addr6, "%s:%s:%s:%s:%s:%s:%s:%s",
                addr6p[0], addr6p[1], addr6p[2], addr6p[3], addr6p[4], addr6p[5], addr6p[6], addr6p[7]);

        if (addrtype6 == OLSR_IP6T_SITELOCAL && scope == IPV6_ADDR_SITELOCAL)
          found = true;
        else if (addrtype6 == OLSR_IP6T_UNIQUELOCAL && scope == IPV6_ADDR_GLOBAL)
          found = true;
        else if (addrtype6 == OLSR_IP6T_GLOBAL && scope == IPV6_ADDR_GLOBAL)
          found = true;

        if (found) {
          found = false;
          if (addr6p[0][0] == 'F' || addr6p[0][0] == 'f') {
            if (addr6p[0][1] == 'C' || addr6p[0][1] == 'c' || addr6p[0][1] == 'D' || addr6p[0][1] == 'd')
              found = true;
          }
          if (addrtype6 == OLSR_IP6T_SITELOCAL)
            found = true;
          else if (addrtype6 == OLSR_IP6T_UNIQUELOCAL && found)
            found = true;
          else if (addrtype6 == OLSR_IP6T_GLOBAL && !found)
            found = true;
          else
            found = false;
        }

        if (found) {
          inet_pton(AF_INET6, addr6, &saddr6->sin6_addr);
          rv = 1;
          break;
        }
      }
    }
    fclose(f);
  }
  return rv;
}


/**
 * Wrapper for sendto(2)
 */
ssize_t
os_sendto(int s, const void *buf, size_t len, int flags, const union olsr_sockaddr *sockaddr)
{
  return sendto(s, buf, len, flags, &sockaddr->std, sizeof(*sockaddr));
}

/**
 * Wrapper for recvfrom(2)
 */

ssize_t
os_recvfrom(int s, void *buf, size_t len, int flags,
    union olsr_sockaddr *sockaddr, socklen_t *socklen)
{
  return recvfrom(s, buf, len, flags, &sockaddr->std, socklen);
}

/**
 * Wrapper for select(2)
 */

int
os_select(int nfds, fd_set * readfds, fd_set * writefds, fd_set * exceptfds, struct timeval *timeout)
{
  return select(nfds, readfds, writefds, exceptfds, timeout);
}

bool os_is_interface_up(const char * dev)
{
  struct ifreq ifr;

  memset(&ifr, 0, sizeof(ifr));
  strscpy(ifr.ifr_name, dev, IFNAMSIZ);

  if (ioctl(olsr_cnf->ioctl_s, SIOCGIFFLAGS, &ifr) < 0) {
    OLSR_WARN(LOG_INTERFACE, "ioctl SIOCGIFFLAGS (get flags) error on device %s: %s (%d)\n",
        dev, strerror(errno), errno);
    return 1;
  }
  return (ifr.ifr_flags & IFF_UP) != 0;
}

int os_interface_set_state(const char *dev, bool up) {
  int oldflags;
  struct ifreq ifr;

  memset(&ifr, 0, sizeof(ifr));
  strscpy(ifr.ifr_name, dev, IFNAMSIZ);

  if (ioctl(olsr_cnf->ioctl_s, SIOCGIFFLAGS, &ifr) < 0) {
    OLSR_WARN(LOG_INTERFACE, "ioctl SIOCGIFFLAGS (get flags) error on device %s: %s (%d)\n",
        dev, strerror(errno), errno);
    return 1;
  }

  oldflags = ifr.ifr_flags;
  if (up) {
    ifr.ifr_flags |= IFF_UP;
  }
  else {
    ifr.ifr_flags &= ~IFF_UP;
  }

  if (oldflags == ifr.ifr_flags) {
    /* interface is already up/down */
    return 0;
  }

  if (ioctl(olsr_cnf->ioctl_s, SIOCSIFFLAGS, &ifr) < 0) {
    OLSR_WARN(LOG_INTERFACE, "ioctl SIOCSIFFLAGS (set flags %s) error on device %s: %s (%d)\n",
        up ? "up" : "down", dev, strerror(errno), errno);
    return 1;
  }
  return 0;
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

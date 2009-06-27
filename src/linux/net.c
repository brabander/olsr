
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


/*
 * Linux spesific code
 */

#include "../net_os.h"
#include "../ipcalc.h"
#include "../common/string.h"
#include "../olsr_protocol.h"
#include "../misc.h"
#include "../olsr_logging.h"
#include "../olsr.h"

#include <net/if.h>

#include <sys/ioctl.h>

#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

/* Redirect proc entry */
#define REDIRECT_PROC "/proc/sys/net/ipv4/conf/%s/send_redirects"

/* IP spoof proc entry */
#define SPOOF_PROC "/proc/sys/net/ipv4/conf/%s/rp_filter"

/*
 *Wireless definitions for ioctl calls
 *(from linux/wireless.h)
 */
#define SIOCGIWNAME	0x8B01  /* get name == wireless protocol */
#define SIOCGIWRATE	0x8B21  /* get default bit rate (bps) */

/* The original state of the IP forwarding proc entry */
static char orig_fwd_state;
static char orig_global_redirect_state;

/**
 *Bind a socket to a device
 *
 *@param sock the socket to bind
 *@param dev_name name of the device
 *
 *@return negative if error
 */

static int
bind_socket_to_device(int sock, char *dev_name)
{
  /*
   *Bind to device using the SO_BINDTODEVICE flag
   */
  OLSR_DEBUG(LOG_NETWORKING, "Binding socket %d to device %s\n", sock, dev_name);
  return setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, dev_name, strlen(dev_name) + 1);
}


/**
 *Enable IP forwarding.
 *Just writing "1" to the /proc/sys/net/ipv4/ip_forward
 *if using IPv4 or /proc/sys/net/ipv6/conf/all/forwarding
 *if using IPv6.
 *Could probably drop the check for
 *"0" here and write "1" anyways.
 *
 *@param version IP version.
 *
 *@return 1 on sucess 0 on failiure
 */
int
enable_ip_forwarding(int version)
{
  const char *const procfile = version == AF_INET ? "/proc/sys/net/ipv4/ip_forward" : "/proc/sys/net/ipv6/conf/all/forwarding";
  FILE *proc_fwd = fopen(procfile, "r");

  if (proc_fwd == NULL) {
    OLSR_WARN(LOG_NETWORKING,
              "WARNING! Could not open the %s file to check/enable IP forwarding!\n"
              "Are you using the procfile filesystem?\nDoes your system support IPv%d?\n"
              "I will continue(in 3 sec) - but you should manually ensure that IP forwarding is enabled!\n\n",
              procfile, version == AF_INET ? 4 : 6);
    sleep(3);
    return 0;
  }
  orig_fwd_state = fgetc(proc_fwd);
  fclose(proc_fwd);

  if (orig_fwd_state == '1') {
    OLSR_INFO(LOG_NETWORKING, "\nIP forwarding is enabled on this system\n");
  } else {
    proc_fwd = fopen(procfile, "w");
    if (proc_fwd == NULL) {
      OLSR_WARN(LOG_NETWORKING, "Could not open %s for writing!\n"
                "I will continue(in 3 sec) - but you should manually ensure that IP forwarding is enabeled!\n\n", procfile);
      sleep(3);
      return 0;
    }
    fputs("1", proc_fwd);
    fclose(proc_fwd);
  }
  return 1;
}

int
disable_redirects_global(int version)
{
  FILE *proc_redirect;
  const char *const procfile = "/proc/sys/net/ipv4/conf/all/send_redirects";

  if (version == AF_INET6) {
    return -1;
  }
  proc_redirect = fopen(procfile, "r");
  if (proc_redirect == NULL) {
    OLSR_WARN(LOG_NETWORKING,
              "WARNING! Could not open the %s file to check/disable ICMP redirects!\n"
              "Are you using the procfile filesystem?\n"
              "Does your system support IPv4?\n"
              "I will continue(in 3 sec) - but you should manually ensure that ICMP redirects are disabled!\n\n", procfile);
    sleep(3);
    return -1;
  }
  orig_global_redirect_state = fgetc(proc_redirect);
  fclose(proc_redirect);

  if (orig_global_redirect_state == '0') {
    return 0;
  }
  proc_redirect = fopen(procfile, "w");
  if (proc_redirect == NULL) {
    OLSR_WARN(LOG_NETWORKING, "Could not open %s for writing!\n"
              "I will continue(in 3 sec) - but you should manually ensure that ICMP redirect is disabled!\n\n", procfile);
    sleep(3);
    return 0;
  }
  fputs("0", proc_redirect);
  fclose(proc_redirect);
  return 1;
}

/**
 *
 *@return 1 on sucess 0 on failiure
 */
int
disable_redirects(const char *if_name, struct interface *iface, int version)
{
  FILE *proc_redirect;
  char procfile[FILENAME_MAX];

  if (version == AF_INET6) {
    return -1;
  }

  /* Generate the procfile name */
  snprintf(procfile, sizeof(procfile), REDIRECT_PROC, if_name);

  proc_redirect = fopen(procfile, "r");
  if (proc_redirect == NULL) {
    OLSR_WARN(LOG_NETWORKING,
              "WARNING! Could not open the %s file to check/disable ICMP redirects!\n"
              "Are you using the procfile filesystem?\n"
              "Does your system support IPv4?\n"
              "I will continue(in 3 sec) - but you should manually ensure that ICMP redirects are disabled!\n\n", procfile);
    sleep(3);
    return 0;
  }
  iface->nic_state.redirect = fgetc(proc_redirect);
  fclose(proc_redirect);

  proc_redirect = fopen(procfile, "w");
  if (proc_redirect == NULL) {
    OLSR_WARN(LOG_NETWORKING, "Could not open %s for writing!\n"
              "I will continue(in 3 sec) - but you should manually ensure that ICMP redirect is disabled!\n\n", procfile);
    sleep(3);
    return 0;
  }
  fputs("0", proc_redirect);
  fclose(proc_redirect);
  return 1;
}

/**
 *
 *@return 1 on sucess 0 on failiure
 */
int
deactivate_spoof(const char *if_name, struct interface *iface, int version)
{
  FILE *proc_spoof;
  char procfile[FILENAME_MAX];

  if (version == AF_INET6) {
    return -1;
  }

  /* Generate the procfile name */
  sprintf(procfile, SPOOF_PROC, if_name);

  proc_spoof = fopen(procfile, "r");
  if (proc_spoof == NULL) {
    OLSR_WARN(LOG_NETWORKING,
              "WARNING! Could not open the %s file to check/disable the IP spoof filter!\n"
              "Are you using the procfile filesystem?\n"
              "Does your system support IPv4?\n"
              "I will continue(in 3 sec) - but you should manually ensure that IP spoof filtering is disabled!\n\n", procfile);
    sleep(3);
    return 0;
  }
  iface->nic_state.spoof = fgetc(proc_spoof);
  fclose(proc_spoof);

  proc_spoof = fopen(procfile, "w");
  if (proc_spoof == NULL) {
    OLSR_WARN(LOG_NETWORKING, "Could not open %s for writing!\n"
              "I will continue(in 3 sec) - but you should manually ensure that IP spoof filtering is disabled!\n\n", procfile);
    sleep(3);
    return 0;
  }
  fputs("0", proc_spoof);
  fclose(proc_spoof);
  return 1;
}

/**
 *Resets the spoof filter and ICMP redirect settings
 */
int
restore_settings(int version)
{
  struct interface *ifs;

  OLSR_INFO(LOG_NETWORKING, "Restoring network state\n");

  /* Restore IP forwarding to "off" */
  if (orig_fwd_state == '0') {
    const char *const procfile = version == AF_INET ? "/proc/sys/net/ipv4/ip_forward" : "/proc/sys/net/ipv6/conf/all/forwarding";
    FILE *proc_fd = fopen(procfile, "w");

    if (proc_fd == NULL) {
      OLSR_WARN(LOG_NETWORKING, "Could not open %s for writing!\nSettings not restored!\n", procfile);
    } else {
      fputc(orig_fwd_state, proc_fd);
      fclose(proc_fd);
    }
  }

  /* Restore global ICMP redirect setting */
  if (orig_global_redirect_state != '0') {
    if (version == AF_INET) {
      const char *const procfile = "/proc/sys/net/ipv4/conf/all/send_redirects";
      FILE *proc_fd = fopen(procfile, "w");

      if (proc_fd == NULL) {
        OLSR_WARN(LOG_NETWORKING, "Could not open %s for writing!\nSettings not restored!\n", procfile);
      } else {
        fputc(orig_global_redirect_state, proc_fd);
        fclose(proc_fd);
      }
    }
  }

  if (version == AF_INET6) {
    return 0;
  }

  OLSR_FOR_ALL_INTERFACES(ifs) {
    char procfile[FILENAME_MAX];
    FILE *proc_fd;
    /* ICMP redirects */

    /* Generate the procfile name */
    snprintf(procfile, sizeof(procfile), REDIRECT_PROC, ifs->int_name);
    proc_fd = fopen(procfile, "w");
    if (proc_fd == NULL) {
      OLSR_WARN(LOG_NETWORKING, "Could not open %s for writing!\nSettings not restored!\n", procfile);
    } else {
      fputc(ifs->nic_state.redirect, proc_fd);
      fclose(proc_fd);
    }

    /* Spoof filter */

    /* Generate the procfile name */
    sprintf(procfile, SPOOF_PROC, ifs->int_name);
    proc_fd = fopen(procfile, "w");
    if (proc_fd == NULL) {
      OLSR_WARN(LOG_NETWORKING, "Could not open %s for writing!\nSettings not restored!\n", procfile);
    } else {
      fputc(ifs->nic_state.spoof, proc_fd);
      fclose(proc_fd);
    }
  }
  OLSR_FOR_ALL_INTERFACES_END(ifs);

  return 1;
}

/**
 *Creates a nonblocking broadcast socket.
 *@param sa sockaddr struct. Used for bind(2).
 *@return the FD of the socket or -1 on error.
 */
int
getsocket(int bufspace, char *int_name)
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
#endif

  /*
   * WHEN USING KERNEL 2.6 THIS MUST HAPPEN PRIOR TO THE PORT BINDING!!!!
   */

  /* Bind to device */
  if (bind_socket_to_device(sock, int_name) < 0) {
    OLSR_ERROR(LOG_NETWORKING, "Could not bind socket for OLSR PDUs to device (%s)\n", strerror(errno));
    close(sock);
    olsr_exit(EXIT_FAILURE);
  }

  memset(&sin4, 0, sizeof(sin4));
  sin4.sin_family = AF_INET;
  sin4.sin_port = htons(olsr_cnf->olsr_port);
  assert(sin4.sin_addr.s_addr == INADDR_ANY);
  if (bind(sock, (struct sockaddr *)&sin4, sizeof(sin4)) < 0) {
    OLSR_ERROR(LOG_NETWORKING, "Coult not bind socket for OLSR PDUs to port (%s)\n", strerror(errno));
    close(sock);
    olsr_exit(EXIT_FAILURE);
  }

  set_nonblocking(sock);
  return sock;
}


/**
 *Creates a nonblocking IPv6 socket
 *@param sin sockaddr_in6 struct. Used for bind(2).
 *@return the FD of the socket or -1 on error.
 */
int
getsocket6(int bufspace, char *int_name)
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
  if (bind_socket_to_device(sock, int_name) < 0) {
    OLSR_ERROR(LOG_NETWORKING, "Cannot bind socket for OLSR PDUs to interface %s (%s)\n", int_name, strerror(errno));
    close(sock);
    olsr_exit(EXIT_FAILURE);
  }

  memset(&sin6, 0, sizeof(sin6));
  sin6.sin6_family = AF_INET6;
  sin6.sin6_port = htons(olsr_cnf->olsr_port);
  assert(0 == memcmp(&sin6.sin6_addr, &in6addr_any, sizeof(sin6.sin6_addr)));   /* == IN6ADDR_ANY_INIT */
  if (bind(sock, (struct sockaddr *)&sin6, sizeof(sin6)) < 0) {
    OLSR_ERROR(LOG_NETWORKING, "Cannot bind socket for OLSR PDUs (%s)\n", strerror(errno));
    close(sock);
    olsr_exit(EXIT_FAILURE);
  }

  set_nonblocking(sock);
  return sock;
}

int
join_mcast(struct interface *ifs, int sock)
{
  /* See linux/in6.h */
#if !defined REMOVE_LOG_INFO
  struct ipaddr_str buf;
#endif
  struct ipv6_mreq mcastreq;

  mcastreq.ipv6mr_multiaddr = ifs->int6_multaddr.sin6_addr;
  mcastreq.ipv6mr_interface = ifs->if_index;

  OLSR_INFO(LOG_NETWORKING, "Interface %s joining multicast %s\n", ifs->int_name,
            ip6_to_string(&buf, &ifs->int6_multaddr.sin6_addr));
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
olsr_sendto(int s, const void *buf, size_t len, int flags, const struct sockaddr * to, socklen_t tolen)
{
  return sendto(s, buf, len, flags, to, tolen);
}

/**
 * Wrapper for recvfrom(2)
 */

ssize_t
olsr_recvfrom(int s, void *buf, size_t len, int flags, struct sockaddr * from, socklen_t * fromlen)
{
  return recvfrom(s, buf, len, flags, from, fromlen);
}

/**
 * Wrapper for select(2)
 */

int
olsr_select(int nfds, fd_set * readfds, fd_set * writefds, fd_set * exceptfds, struct timeval *timeout)
{
  return select(nfds, readfds, writefds, exceptfds, timeout);
}

int
check_wireless_interface(char *ifname)
{
  struct ifreq ifr;

  memset(&ifr, 0, sizeof(ifr));
  strscpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));

  return (ioctl(olsr_cnf->ioctl_s, SIOCGIWNAME, &ifr) >= 0) ? 1 : 0;
}

#if 0

#include <linux/sockios.h>
#include <linux/types.h>

/* This data structure is used for all the MII ioctl's */
struct mii_data {
  __u16 phy_id;
  __u16 reg_num;
  __u16 val_in;
  __u16 val_out;
};


/* Basic Mode Control Register */
#define MII_BMCR		0x00
#define  MII_BMCR_RESET		0x8000
#define  MII_BMCR_LOOPBACK	0x4000
#define  MII_BMCR_100MBIT	0x2000
#define  MII_BMCR_AN_ENA	0x1000
#define  MII_BMCR_ISOLATE	0x0400
#define  MII_BMCR_RESTART	0x0200
#define  MII_BMCR_DUPLEX	0x0100
#define  MII_BMCR_COLTEST	0x0080

/* Basic Mode Status Register */
#define MII_BMSR		0x01
#define  MII_BMSR_CAP_MASK	0xf800
#define  MII_BMSR_100BASET4	0x8000
#define  MII_BMSR_100BASETX_FD	0x4000
#define  MII_BMSR_100BASETX_HD	0x2000
#define  MII_BMSR_10BASET_FD	0x1000
#define  MII_BMSR_10BASET_HD	0x0800
#define  MII_BMSR_NO_PREAMBLE	0x0040
#define  MII_BMSR_AN_COMPLETE	0x0020
#define  MII_BMSR_REMOTE_FAULT	0x0010
#define  MII_BMSR_AN_ABLE	0x0008
#define  MII_BMSR_LINK_VALID	0x0004
#define  MII_BMSR_JABBER	0x0002
#define  MII_BMSR_EXT_CAP	0x0001

int
calculate_if_metric(char *ifname)
{
  if (check_wireless_interface(ifname)) {
    struct ifreq ifr;
    strscpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));

    /* Get bit rate */
    if (ioctl(olsr_cnf->ioctl_s, SIOCGIWRATE, &ifr) < 0) {
      OLSR_PRINTF(1, "Not able to find rate for WLAN interface %s\n", ifname);
      return WEIGHT_WLAN_11MB;
    }

    OLSR_PRINTF(1, "Bitrate %d\n", ifr.ifr_ifru.ifru_ivalue);

    //WEIGHT_WLAN_LOW,          /* <11Mb WLAN     */
    //WEIGHT_WLAN_11MB,         /* 11Mb 802.11b   */
    //WEIGHT_WLAN_54MB,         /* 54Mb 802.11g   */
    return WEIGHT_WLAN_LOW;
  } else {
    /* Ethernet */
    /* Mii wizardry */
    struct ifreq ifr;
    struct mii_data *mii = (struct mii_data *)&ifr.ifr_data;
    int bmcr;
    memset(&ifr, 0, sizeof(ifr));
    strscpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));

    if (ioctl(olsr_cnf->ioctl_s, SIOCGMIIPHY, &ifr) < 0) {
      if (errno != ENODEV)
        OLSR_PRINTF(1, "SIOCGMIIPHY on '%s' failed: %s\n", ifr.ifr_name, strerror(errno));
      return WEIGHT_ETHERNET_DEFAULT;
    }

    mii->reg_num = MII_BMCR;
    if (ioctl(olsr_cnf->ioctl_s, SIOCGMIIREG, &ifr) < 0) {
      OLSR_PRINTF(1, "SIOCGMIIREG on %s failed: %s\n", ifr.ifr_name, strerror(errno));
      return WEIGHT_ETHERNET_DEFAULT;
    }
    bmcr = mii->val_out;


    OLSR_PRINTF(1, "%s: ", ifr.ifr_name);
    OLSR_PRINTF(1, "%s Mbit, %s duplex\n", (bmcr & MII_BMCR_100MBIT) ? "100" : "10", (bmcr & MII_BMCR_DUPLEX) ? "full" : "half");

    is_if_link_up(ifname);

    if (mii->val_out & MII_BMCR_100MBIT)
      return WEIGHT_ETHERNET_100MB;
    else
      return WEIGHT_ETHERNET_10MB;
    //WEIGHT_ETHERNET_1GB,      /* Ethernet 1Gb   */

  }
}


bool
is_if_link_up(char *ifname)
{
  if (check_wireless_interface(ifname)) {
    /* No link checking on wireless devices */
    return true;
  } else {
    /* Mii wizardry */
    struct ifreq ifr;
    struct mii_data *mii = (struct mii_data *)&ifr.ifr_data;
    int bmsr;
    memset(&ifr, 0, sizeof(ifr));
    strscpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));

    if (ioctl(olsr_cnf->ioctl_s, SIOCGMIIPHY, &ifr) < 0) {
      if (errno != ENODEV)
        OLSR_PRINTF(1, "SIOCGMIIPHY on '%s' failed: %s\n", ifr.ifr_name, strerror(errno));
      return WEIGHT_ETHERNET_DEFAULT;
    }
    mii->reg_num = MII_BMSR;
    if (ioctl(olsr_cnf->ioctl_s, SIOCGMIIREG, &ifr) < 0) {
      OLSR_PRINTF(1, "SIOCGMIIREG on %s failed: %s\n", ifr.ifr_name, strerror(errno));
      return WEIGHT_ETHERNET_DEFAULT;
    }
    bmsr = mii->val_out;

    OLSR_PRINTF(1, "%s: ", ifr.ifr_name);
    OLSR_PRINTF(1, "%s\n", (bmsr & MII_BMSR_LINK_VALID) ? "link ok " : "no link ");

    return (bmsr & MII_BMSR_LINK_VALID);

  }
}

#else
int
calculate_if_metric(char *ifname)
{
  return check_wireless_interface(ifname);
}
#endif

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

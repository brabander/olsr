/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004, Andreas Tønnesen(andreto@olsr.org)
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
 * $Id: net.c,v 1.14 2005/02/19 21:50:21 kattemat Exp $
 */

#include "../defs.h"
#include "../net_os.h"
#include "../parser.h" /* dnc: needed for call to packet_parser() */
#include "net.h"

#ifdef __NetBSD__
#include <sys/param.h>
#endif

#include <net/if.h>
#include <net/if_var.h>
#include <net/ethernet.h>

#include <net80211/ieee80211.h>
#include <net80211/ieee80211_ioctl.h>
#include <dev/wi/if_wavelan_ieee.h>
#include <dev/wi/if_wireg.h>

//#define	SIOCGIFGENERIC	_IOWR('i', 58, struct ifreq)	/* generic IF get op */
//#define SIOCGWAVELAN SIOCGIFGENERIC

#include <sys/sysctl.h>

static int ignore_redir;
static int send_redir;
static int gateway;

static int first_time = 1;

static int set_sysctl_int(char *name, int new)
{
  int old;
  unsigned int len = sizeof (old);

  if (sysctlbyname(name, &old, &len, &new, sizeof (new)) < 0)
    return -1;

  return old;
}

int enable_ip_forwarding(int version)
{
  char *name;

  if (olsr_cnf->ip_version == AF_INET)
    name = "net.inet.ip.forwarding";

  else
    name = "net.inet6.ip6.forwarding";

  gateway = set_sysctl_int(name, 1);

  if (gateway < 0)
    {
      fprintf(stderr, "Cannot enable IP forwarding. Please enable IP forwarding manually. Continuing in 3 seconds...\n");
      sleep(3);
    }

  return 1;
}

int disable_redirects(char *if_name, int index, int version)
{
  char *name;

  // this function gets called for each interface olsrd uses; however,
  // FreeBSD can only globally control ICMP redirects, and not on a
  // per-interface basis; hence, only disable ICMP redirects on the first
  // invocation

  if (first_time == 0)
    return 1;

  first_time = 0;

  // do not accept ICMP redirects

  if (olsr_cnf->ip_version == AF_INET)
    name = "net.inet.icmp.drop_redirect";

  else
    name = "net.inet6.icmp6.drop_redirect";

  ignore_redir = set_sysctl_int(name, 1);

  if (ignore_redir < 0)
    {
      fprintf(stderr, "Cannot disable incoming ICMP redirect messages. Please disable them manually. Continuing in 3 seconds...\n");
      sleep(3);
    }

  // do not send ICMP redirects

  if (olsr_cnf->ip_version == AF_INET)
    name = "net.inet.ip.redirect";

  else
    name = "net.inet6.ip6.redirect";

  send_redir = set_sysctl_int(name, 0);

  if (send_redir < 0)
    {
      fprintf(stderr, "Cannot disable outgoing ICMP redirect messages. Please disable them manually. Continuing in 3 seconds...\n");
      sleep(3);
    }

  return 1;
}

int deactivate_spoof(char *if_name, int index, int version)
{
  return 1;
}

int restore_settings(int version)
{
  char *name;

  // reset IP forwarding

  if (olsr_cnf->ip_version == AF_INET)
    name = "net.inet.ip.forwarding";

  else
    name = "net.inet6.ip6.forwarding";

  set_sysctl_int(name, gateway);

  // reset incoming ICMP redirects

  if (olsr_cnf->ip_version == AF_INET)
    name = "net.inet.icmp.drop_redirect";

  else
    name = "net.inet6.icmp6.drop_redirect";

  set_sysctl_int(name, ignore_redir);

  // reset outgoing ICMP redirects

  if (olsr_cnf->ip_version == AF_INET)
    name = "net.inet.ip.redirect";

  else
    name = "net.inet6.ip6.redirect";

  set_sysctl_int(name, send_redir);

  return 1;
}

int
getsocket(struct sockaddr *sa, int bufspace, char *int_name)
{
  struct sockaddr_in *sin = (struct sockaddr_in *)sa;
  int sock, on = 1;

  if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
    {
      perror("socket");
      syslog(LOG_ERR, "socket: %m");
      return (-1);
    }

  if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &on, sizeof (on)) < 0)
    {
      perror("setsockopt");
      syslog(LOG_ERR, "setsockopt SO_BROADCAST: %m");
      close(sock);
      return (-1);
    }

  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) 
    {
      perror("SO_REUSEADDR failed");
      return (-1);
    }

  for (on = bufspace; ; on -= 1024) 
    {
      if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF,
		     &on, sizeof (on)) == 0)
	break;
      if (on <= 8*1024) 
	{
	  perror("setsockopt");
	  syslog(LOG_ERR, "setsockopt SO_RCVBUF: %m");
	  break;
	}
    }

  if (bind(sock, (struct sockaddr *)sin, sizeof (*sin)) < 0) 
    {
      perror("bind");
      syslog(LOG_ERR, "bind: %m");
      close(sock);
      return (-1);
    }

  if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1)
    syslog(LOG_ERR, "fcntl O_NONBLOCK: %m\n");

  return (sock);
}

int getsocket6(struct sockaddr_in6 *sin, int bufspace, char *int_name)
{
  int sock, on = 1;

  if ((sock = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) 
    {
      perror("socket");
      syslog(LOG_ERR, "socket: %m");
      return (-1);
    }

  for (on = bufspace; ; on -= 1024) 
    {
      if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF,
		     &on, sizeof (on)) == 0)
	break;
      if (on <= 8*1024) 
	{
	  perror("setsockopt");
	  syslog(LOG_ERR, "setsockopt SO_RCVBUF: %m");
	  break;
	}
    }

  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) 
    {
      perror("SO_REUSEADDR failed");
      return (-1);
    }

  if (bind(sock, (struct sockaddr *)sin, sizeof (*sin)) < 0) 
    {
      perror("bind");
      syslog(LOG_ERR, "bind: %m");
      close(sock);
      return (-1);
    }

  if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1)
    syslog(LOG_ERR, "fcntl O_NONBLOCK: %m\n");

  return (sock);
}

int get_ipv6_address(char *ifname, struct sockaddr_in6 *saddr6, int scope_in)
{
  return 0;
}




/**
 * Wrapper for sendto(2)
 */

ssize_t
olsr_sendto(int s, 
	    const void *buf, 
	    size_t len, 
	    int flags, 
	    const struct sockaddr *to, 
	    socklen_t tolen)
{
  return sendto(s, buf, len, flags, to, tolen);
}


/**
 * Wrapper for recvfrom(2)
 */

ssize_t  
olsr_recvfrom(int  s, 
	      void *buf, 
	      size_t len, 
	      int flags, 
	      struct sockaddr *from,
	      socklen_t *fromlen)
{
  return recvfrom(s, 
		  buf, 
		  len, 
		  0, 
		  from, 
		  fromlen);
}


int 
check_wireless_interface(char *ifname)
{
  struct wi_req	wreq;
  struct ifreq ifr;

  memset((char *)&wreq, 0, sizeof(wreq));
  memset((char *)&ifr, 0, sizeof(ifr));

  wreq.wi_len = WI_MAX_DATALEN;
  wreq.wi_type = WI_RID_IFACE_STATS;

  strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
  ifr.ifr_data = (caddr_t)&wreq;

  return (ioctl(ioctl_s, SIOCGWAVELAN, &ifr) >= 0) ? 1 : 0;
}

#include <sys/sockio.h>

int
calculate_if_metric(char *ifname)
{
  if(check_wireless_interface(ifname))
    {
      /* Wireless */
      return 1;
    }
  else
    {
      /* Ethernet */
#if 0
      /* Andreas: Perhaps SIOCGIFMEDIA is the way to do this? */
      struct ifmediareq ifm;

      memset(&ifm, 0, sizeof(ifm));
      strlcpy(ifm.ifm_name, ifname, sizeof(ifm.ifm_name));

      if(ioctl(ioctl_s, SIOCGIFMEDIA, &ifm) < 0)
	{
	  olsr_printf(1, "Error SIOCGIFMEDIA(%s)\n", ifm.ifm_name);
	  return WEIGHT_ETHERNET_DEFAULT;
	}

      printf("%s: STATUS 0x%08x\n", ifm.ifm_name, ifm.ifm_status);
#endif
      return WEIGHT_ETHERNET_DEFAULT;
    }
}

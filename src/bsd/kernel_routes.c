/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2004 Thomas Lopatic (thomas@lopatic.de)
 *
 * This file is part of olsr.org.
 *
 * olsr.org is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * olsr.org is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with olsr.org; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 * 
 * $Id: kernel_routes.c,v 1.2 2004/11/15 12:18:49 tlopatic Exp $
 *
 */

#include "../kernel_routes.h"
#include "../olsr.h"

#include <net/if_dl.h>
#include <ifaddrs.h>

static unsigned int seq = 0;

static int add_del_route(struct rt_entry *dest, int add)
{
  struct rt_msghdr *rtm;
  unsigned char buff[512];
  unsigned char *walker;
  struct sockaddr_in sin;
  struct sockaddr_dl *sdl;
  struct ifaddrs *addrs;
  struct ifaddrs *awalker;
  int step, step2;
  int len;
  char Str1[16], Str2[16], Str3[16];
  int flags;

  inet_ntop(AF_INET, &dest->rt_dst.v4, Str1, 16);
  inet_ntop(AF_INET, &dest->rt_mask.v4, Str2, 16);
  inet_ntop(AF_INET, &dest->rt_router.v4, Str3, 16);

  olsr_printf(1, "%s IPv4 route to %s/%s via %s.\n",
    (add != 0) ? "Adding" : "Removing", Str1, Str2, Str3);

  memset(buff, 0, sizeof (buff));
  memset(&sin, 0, sizeof (sin));

  sin.sin_len = sizeof (sin);
  sin.sin_family = AF_INET;

  step = 1 + ((sizeof (struct sockaddr_in) - 1) | 3);
  step2 = 1 + ((sizeof (struct sockaddr_dl) - 1) | 3);

  rtm = (struct rt_msghdr *)buff;

  flags = dest->rt_flags;

  // the host is directly reachable, so use cloning and a /32 net
  // routing table entry

  if ((flags & RTF_GATEWAY) == 0)
  {
    flags |= RTF_CLONING;
    flags &= ~RTF_HOST;
  }

  rtm->rtm_version = RTM_VERSION;
  rtm->rtm_type = (add != 0) ? RTM_ADD : RTM_DELETE;
  rtm->rtm_index = 0;
  rtm->rtm_flags = flags;
  rtm->rtm_addrs = RTA_DST | RTA_NETMASK | RTA_GATEWAY;
  rtm->rtm_seq = ++seq;

  walker = buff + sizeof (struct rt_msghdr);

  sin.sin_addr.s_addr = dest->rt_dst.v4;

  memcpy(walker, &sin, sizeof (sin));
  walker += step;

  if ((flags & RTF_GATEWAY) != 0)
  {
    sin.sin_addr.s_addr = dest->rt_router.v4;

    memcpy(walker, &sin, sizeof (sin));
    walker += step;
  }

  // the host is directly reachable, so add the output interface's
  // MAC address

  else
  {
    if (getifaddrs(&addrs))
    {
      fprintf(stderr, "getifaddrs() failed\n");
      return -1;
    }

    for (awalker = addrs; awalker != NULL; awalker = awalker->ifa_next)
      if (awalker->ifa_addr->sa_family == AF_LINK &&
          strcmp(awalker->ifa_name, dest->rt_if->int_name) == 0)
        break;

    if (awalker == NULL)
    {
      fprintf(stderr, "interface %s not found\n", dest->rt_if->int_name);
      freeifaddrs(addrs);
      return -1;
    }

    sdl = (struct sockaddr_dl *)awalker->ifa_addr;

    memcpy(walker, sdl, sdl->sdl_len);
    walker += step2;

    freeifaddrs(addrs);
  }

  sin.sin_addr.s_addr = dest->rt_mask.v4;

  memcpy(walker, &sin, sizeof (sin));
  walker += step;

  rtm->rtm_msglen = (unsigned short)(walker - buff);

  len = write(rts, buff, rtm->rtm_msglen);

  if (len < rtm->rtm_msglen)
    fprintf(stderr, "cannot write to routing socket: %s\n", strerror(errno));

  return 0;
}

int olsr_ioctl_add_route(struct rt_entry *dest)
{
  return add_del_route(dest, 1);
}

int olsr_ioctl_del_route(struct rt_entry *dest)
{
  return add_del_route(dest, 0);
}

int olsr_ioctl_add_route6(struct rt_entry *dest)
{
  return 0;
}

int olsr_ioctl_del_route6(struct rt_entry *dest)
{
  return 0;
}

int add_tunnel_route(union olsr_ip_addr *gw)
{
  return 0;
}

int delete_tunnel_route()
{
  return 0;
}


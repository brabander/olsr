/*
 * $Id: route.h,v 1.2 2004/09/15 11:18:42 tlopatic Exp $
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
 */

#if !defined TL_NET_ROUTE_H_INCLUDED

#define TL_NET_ROUTE_H_INCLUDED

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#undef interface

struct in6_rtmsg
{
  struct in6_addr rtmsg_dst;
  struct in6_addr rtmsg_src;
  struct in6_addr rtmsg_gateway;
  unsigned int rtmsg_type;
  unsigned short rtmsg_dst_len;
  unsigned short rtmsg_src_len;
  unsigned int rtmsg_metric;
  unsigned long rtmsg_info;
  unsigned int rtmsg_flags;
  int rtmsg_ifindex;
};

struct rtentry
{
  unsigned long rt_pad1;
  struct sockaddr rt_dst;
  struct sockaddr rt_gateway;
  struct sockaddr rt_genmask;
  unsigned short rt_flags;
  short rt_pad2;
  unsigned long rt_pad3;
  unsigned char rt_tos;
  unsigned char rt_class;
  short rt_pad4;
  short rt_metric;
  char *rt_dev;
  unsigned long rt_mtu;
  unsigned long rt_window;
  unsigned short rt_irtt;
};

#define RTF_UP 1
#define RTF_HOST 2
#define RTF_GATEWAY 4

#endif

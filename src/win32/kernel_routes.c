/* 
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2004 Thomas Lopatic (thomas@lopatic.de)
 *
 * This file is part of the olsr.org OLSR daemon.
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
 * $Id: kernel_routes.c,v 1.5 2004/10/19 21:44:56 tlopatic Exp $
 *
 */

#include <stdio.h>
#include "net/route.h"

#include "../kernel_routes.h"
#include "../defs.h"

#define WIN32_LEAN_AND_MEAN
#include <iprtrmib.h>
#include <iphlpapi.h>
#undef interface

char *StrError(unsigned int ErrNo);

int olsr_ioctl_add_route(struct rt_entry *Dest)
{
  MIB_IPFORWARDROW Row;
  unsigned long Res;
  union olsr_kernel_route Route;
  char *IntString;
  char Str1[16], Str2[16], Str3[16];

  inet_ntop(AF_INET, &Dest->rt_dst.v4, Str1, 16);
  inet_ntop(AF_INET, &Dest->rt_mask.v4, Str2, 16);
  inet_ntop(AF_INET, &Dest->rt_router.v4, Str3, 16);

  olsr_printf(1, "Adding IPv4 route to %s/%s via %s.\n", Str1, Str2, Str3);

  memset(&Row, 0, sizeof (MIB_IPFORWARDROW));

  Row.dwForwardDest = Dest->rt_dst.v4;
  Row.dwForwardMask = Dest->rt_mask.v4;
  Row.dwForwardNextHop = Dest->rt_router.v4;
  Row.dwForwardIfIndex = Dest->rt_if->if_index;
  Row.dwForwardType = (Dest->rt_dst.v4 == Dest->rt_router.v4) ? 3 : 4;
  Row.dwForwardProto = 3; // PROTO_IP_NETMGMT

  Res = SetIpForwardEntry(&Row);

  if (Res != NO_ERROR)
  {
    fprintf(stderr, "SetIpForwardEntry() = %08lx, %s", Res, StrError(Res));
    Res = CreateIpForwardEntry(&Row);
  }

  if (Res != NO_ERROR)
  {
    fprintf(stderr, "CreateIpForwardEntry() = %08lx, %s", Res, StrError(Res));

    // XXX - report error in a different way

    errno = Res;

    return -1;
  }

  if(olsr_cnf->open_ipc)
  {
    memset(&Route, 0, sizeof (Route));

    Route.v4.rt_metric = Dest->rt_metric;

    ((struct sockaddr_in *)&Route.v4.rt_gateway)->sin_addr.s_addr =
      Dest->rt_router.v4;

    ((struct sockaddr_in *)&Route.v4.rt_dst)->sin_addr.s_addr =
      Dest->rt_dst.v4;

    IntString = (Dest->rt_router.v4 == 0) ? NULL : Dest->rt_if->int_name;
    ipc_route_send_rtentry(&Route, 1, IntString);
  }

  return 0;
}

// XXX - to be implemented

int olsr_ioctl_add_route6(struct rt_entry *Dest)
{
  return 0;
}

int olsr_ioctl_del_route(struct rt_entry *Dest)
{
  MIB_IPFORWARDROW Row;
  unsigned long Res;
  union olsr_kernel_route Route;
  char Str1[16], Str2[16], Str3[16];

  inet_ntop(AF_INET, &Dest->rt_dst.v4, Str1, 16);
  inet_ntop(AF_INET, &Dest->rt_mask.v4, Str2, 16);
  inet_ntop(AF_INET, &Dest->rt_router.v4, Str3, 16);

  olsr_printf(1, "Deleting IPv4 route to %s/%s via %s.\n", Str1, Str2, Str3);

  memset(&Row, 0, sizeof (MIB_IPFORWARDROW));

  Row.dwForwardDest = Dest->rt_dst.v4;
  Row.dwForwardMask = Dest->rt_mask.v4;
  Row.dwForwardNextHop = Dest->rt_router.v4;
  Row.dwForwardIfIndex = Dest->rt_if->if_index;
  Row.dwForwardType = (Dest->rt_dst.v4 == Dest->rt_router.v4) ? 3 : 4;
  Row.dwForwardProto = 3; // PROTO_IP_NETMGMT

  Res = DeleteIpForwardEntry(&Row);

  if (Res != NO_ERROR)
  {
    fprintf(stderr, "DeleteIpForwardEntry() = %08lx, %s", Res, StrError(Res));

    // XXX - report error in a different way

    errno = Res;

    return -1;
  }

  if(olsr_cnf->open_ipc)
  {
    memset(&Route, 0, sizeof (Route));

    Route.v4.rt_metric = Dest->rt_metric;

    ((struct sockaddr_in *)&Route.v4.rt_gateway)->sin_addr.s_addr =
      Dest->rt_router.v4;

    ((struct sockaddr_in *)&Route.v4.rt_dst)->sin_addr.s_addr =
      Dest->rt_dst.v4;

    ipc_route_send_rtentry(&Route, 0, NULL);
  }

  return 0;
}

// XXX - to be implemented

int olsr_ioctl_del_route6(struct rt_entry *Dest)
{
  return 0;
}

// XXX - to be implemented

int delete_tunnel_route()
{
  return 0;
}

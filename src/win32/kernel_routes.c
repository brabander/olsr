
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

#include <stdio.h>
#include "net/route.h"

#include "kernel_routes.h"
#include "net_olsr.h"
#include "ipcalc.h"
#include "routing_table.h"
#include "olsr_logging.h"

#define WIN32_LEAN_AND_MEAN
#include <iprtrmib.h>
#include <iphlpapi.h>

#include <errno.h>

char *StrError(unsigned int ErrNo);

/*
 * Insert a route in the kernel routing table
 * @param destination the route to add
 * @return negative on error
 */
int
olsr_kernel_add_route(const struct rt_entry *rt, int ip_version)
{
  MIB_IPFORWARDROW Row;
  union olsr_ip_addr mask;
  unsigned long Res;
  struct interface *iface = rt->rt_best->rtp_nexthop.interface;

  if (AF_INET != ip_version) {
    /*
     * Not implemented
     */
    return -1;
  }


  OLSR_DEBUG(LOG_ROUTING, "KERN: Adding %s\n", olsr_rt_to_string(rt));

  memset(&Row, 0, sizeof(MIB_IPFORWARDROW));

  Row.dwForwardDest = rt->rt_dst.prefix.v4.s_addr;

  if (!olsr_prefix_to_netmask(&mask, rt->rt_dst.prefix_len)) {
    return -1;
  } else {
    Row.dwForwardMask = mask.v4.s_addr;
  }

  Row.dwForwardPolicy = 0;
  Row.dwForwardNextHop = rt->rt_best->rtp_nexthop.gateway.v4.s_addr;
  Row.dwForwardIfIndex = iface->if_index;
  // MIB_IPROUTE_TYPE_DIRECT and MIB_IPROUTE_TYPE_INDIRECT
  Row.dwForwardType = (rt->rt_dst.prefix.v4.s_addr == rt->rt_best->rtp_nexthop.gateway.v4.s_addr) ? 3 : 4;
  Row.dwForwardProto = 3;       // MIB_IPPROTO_NETMGMT
  Row.dwForwardAge = INFINITE;
  Row.dwForwardNextHopAS = 0;
  Row.dwForwardMetric1 = iface ? iface->int_metric : 0 + olsr_fib_metric(&rt->rt_best->rtp_metric);
  Row.dwForwardMetric2 = -1;
  Row.dwForwardMetric3 = -1;
  Row.dwForwardMetric4 = -1;
  Row.dwForwardMetric5 = -1;

  Res = SetIpForwardEntry(&Row);

  if (Res != NO_ERROR) {
    if (Res != ERROR_NOT_FOUND)
      OLSR_WARN(LOG_ROUTING, "SetIpForwardEntry() = %08lx, %s", Res, StrError(Res));

    Res = CreateIpForwardEntry(&Row);
  }

  if (Res != NO_ERROR) {
    OLSR_WARN(LOG_ROUTING, "CreateIpForwardEntry() = %08lx, %s", Res, StrError(Res));

    // XXX - report error in a different way

    errno = Res;

    return -1;
  }

  return 0;
}


/*
 * Remove a route from the kernel
 * @param destination the route to remove
 * @return negative on error
 */
int
olsr_kernel_del_route(const struct rt_entry *rt, int ip_version)
{
  MIB_IPFORWARDROW Row;
  union olsr_ip_addr mask;
  unsigned long Res;
  struct interface *iface = rt->rt_nexthop.interface;

  if (AF_INET != ip_version) {
    /*
     * Not implemented
     */
    return -1;
  }

  OLSR_DEBUG(LOG_NETWORKING, "KERN: Deleting %s\n", olsr_rt_to_string(rt));

  memset(&Row, 0, sizeof(Row));

  Row.dwForwardDest = rt->rt_dst.prefix.v4.s_addr;

  if (!olsr_prefix_to_netmask(&mask, rt->rt_dst.prefix_len)) {
    return -1;
  }
  Row.dwForwardMask = mask.v4.s_addr;
  Row.dwForwardPolicy = 0;
  Row.dwForwardNextHop = rt->rt_nexthop.gateway.v4.s_addr;
  Row.dwForwardIfIndex = iface->if_index;
  // MIB_IPROUTE_TYPE_DIRECT and MIB_IPROUTE_TYPE_INDIRECT
  Row.dwForwardType = (rt->rt_dst.prefix.v4.s_addr == rt->rt_nexthop.gateway.v4.s_addr) ? 3 : 4;
  Row.dwForwardProto = 3;       // MIB_IPPROTO_NETMGMT
  Row.dwForwardAge = INFINITE;
  Row.dwForwardNextHopAS = 0;
  Row.dwForwardMetric1 = iface ? iface->int_metric : 0 + olsr_fib_metric(&rt->rt_metric);
  Row.dwForwardMetric2 = -1;
  Row.dwForwardMetric3 = -1;
  Row.dwForwardMetric4 = -1;
  Row.dwForwardMetric5 = -1;

  Res = DeleteIpForwardEntry(&Row);

  if (Res != NO_ERROR) {
    OLSR_WARN(LOG_NETWORKING, "DeleteIpForwardEntry() = %08lx, %s", Res, StrError(Res));

    // XXX - report error in a different way

    errno = Res;

    return -1;
  }
  return 0;
}

int
olsr_lo_interface(union olsr_ip_addr *ip __attribute__ ((unused)), bool create __attribute__ ((unused)))
{
  return 0;
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

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

#include "kernel_routes.h"
#include "ipc_frontend.h"
#include <assert.h>
#include <errno.h>
#include <linux/types.h>
#include <linux/rtnetlink.h>

#include "olsr_logging.h"

struct olsr_rtreq {
  struct nlmsghdr n;
  struct rtmsg    r;
  char            buf[512];
};

struct olsr_ipadd_req {
  struct nlmsghdr   n;
  struct ifaddrmsg  ifa;
  char        buf[256];
};


static void olsr_netlink_addreq(struct nlmsghdr *n, size_t reqSize __attribute__((unused)), int type, const void *data, int len)
{
  struct rtattr *rta = (struct rtattr*)(((char*)n) + NLMSG_ALIGN(n->nlmsg_len));
  n->nlmsg_len = NLMSG_ALIGN(n->nlmsg_len) + RTA_LENGTH(len);
  assert(n->nlmsg_len < reqSize);
  rta->rta_type = type;
  rta->rta_len = RTA_LENGTH(len);
  memcpy(RTA_DATA(rta), data, len);
}

static int olsr_netlink_send(struct nlmsghdr *n, char *buf, size_t bufSize) {
  struct iovec iov;
  struct sockaddr_nl nladdr = { .nl_family = AF_NETLINK };
  struct msghdr msg = {
    .msg_name = &nladdr,
    .msg_namelen = sizeof(nladdr),
    .msg_iov = &iov,
    .msg_iovlen = 1,
    .msg_control = NULL,
    .msg_controllen = 0,
    .msg_flags = 0
  };
  int ret;

  iov.iov_base = n;
  iov.iov_len = n->nlmsg_len;
  ret = sendmsg(olsr_cnf->rts_linux, &msg, 0);
  if (0 <= ret) {
    iov.iov_base = buf;
    iov.iov_len = bufSize;
    ret = recvmsg(olsr_cnf->rts_linux, &msg, 0);
    if (0 < ret) {
      struct nlmsghdr* h = (struct nlmsghdr*)buf;
      while (NLMSG_OK(h, (unsigned int)ret)) {
        if (NLMSG_DONE == h->nlmsg_type) {
          break;
        }
        if (NLMSG_ERROR == h->nlmsg_type) {
          if (NLMSG_LENGTH(sizeof(struct nlmsgerr) <= h->nlmsg_len)) {
            const struct nlmsgerr *l_err = (struct nlmsgerr*)NLMSG_DATA(h);
            errno = -l_err->error;
            if (0 != errno) {
              ret = -1;
            }
          }
          break;
        }
        h = NLMSG_NEXT(h, ret);
      }
    }
  }
  return ret;
}

static int olsr_netlink_route(const struct rt_entry *rt, uint8_t family, uint8_t rttable, __u16 cmd)
{
  int ret = 0;
  struct olsr_rtreq req;
  uint32_t metric = FIBM_FLAT != olsr_cnf->fib_metric
    ? (RTM_NEWROUTE == cmd
       ? rt->rt_best->rtp_metric.hops
       : rt->rt_metric.hops)
    : RT_METRIC_DEFAULT;
  const struct rt_nexthop* nexthop = RTM_NEWROUTE == cmd
    ? &rt->rt_best->rtp_nexthop
    : &rt->rt_nexthop;

  memset(&req, 0, sizeof(req));
  req.n.nlmsg_len = NLMSG_LENGTH(sizeof(req.r));
  req.n.nlmsg_flags = NLM_F_REQUEST|NLM_F_CREATE|NLM_F_EXCL|NLM_F_ACK;
  req.n.nlmsg_type = cmd;
  req.r.rtm_family = family;
  req.r.rtm_table = rttable;
  req.r.rtm_protocol = olsr_cnf->rtproto > 0 ? olsr_cnf->rtproto : RTPROT_BOOT;
  req.r.rtm_scope = RT_SCOPE_LINK;
  req.r.rtm_type = RTN_UNICAST;
  req.r.rtm_dst_len = rt->rt_dst.prefix_len;

  /* source ip here is based on now static olsr_cnf->main_addr in this olsr-0.5.6-r4, should be based on orignator-id in newer olsrds*/
  if (AF_INET == family) {
    olsr_netlink_addreq(&req.n, sizeof(req), RTA_PREFSRC, &olsr_cnf->router_id.v4, sizeof(olsr_cnf->router_id.v4));
  }
  else {
    olsr_netlink_addreq(&req.n, sizeof(req), RTA_PREFSRC, &olsr_cnf->router_id.v6, sizeof(olsr_cnf->router_id.v6));
  }

  if (NULL != nexthop->interface) {
    if (AF_INET == family) {
      if (ip4cmp(&rt->rt_dst.prefix.v4, &nexthop->gateway.v4) != 0) {
        olsr_netlink_addreq(&req.n, sizeof(req), RTA_GATEWAY, &nexthop->gateway.v4, sizeof(nexthop->gateway.v4));
        req.r.rtm_scope = RT_SCOPE_UNIVERSE;
      }
      olsr_netlink_addreq(&req.n, sizeof(req), RTA_DST, &rt->rt_dst.prefix.v4, sizeof(rt->rt_dst.prefix.v4));
    } else {
      if (ip6cmp(&rt->rt_dst.prefix.v6, &nexthop->gateway.v6) != 0) {
        olsr_netlink_addreq(&req.n, sizeof(req), RTA_GATEWAY, &nexthop->gateway.v6, sizeof(nexthop->gateway.v6));
        req.r.rtm_scope = RT_SCOPE_UNIVERSE;
      }
      olsr_netlink_addreq(&req.n, sizeof(req), RTA_DST, &rt->rt_dst.prefix.v6, sizeof(rt->rt_dst.prefix.v6));
    }
    if (FIBM_APPROX != olsr_cnf->fib_metric || RTM_NEWROUTE == cmd) {
      olsr_netlink_addreq(&req.n, sizeof(req), RTA_PRIORITY, &metric, sizeof(metric));
    }
    olsr_netlink_addreq(&req.n, sizeof(req), RTA_OIF, &nexthop->interface->if_index,
                        sizeof(nexthop->interface->if_index));
  }
  else {
    /*
     * No interface means: remove unspecificed default route
     */
    req.r.rtm_scope = RT_SCOPE_NOWHERE;
  }
  ret = olsr_netlink_send(&req.n, req.buf, sizeof(req.buf));
  if (0 <= ret && olsr_cnf->ipc_connections > 0) {
    ipc_route_send_rtentry(&rt->rt_dst.prefix,
         &nexthop->gateway,
         metric,
         RTM_NEWROUTE == cmd,
         nexthop->interface->int_name);
  }
  return ret;
}

/*
 * Insert a route in the kernel routing table
 * @param destination the route to add
 * @return negative on error
 */
int
olsr_kernel_add_route(const struct rt_entry *rt, int ip_version)
{
  int rslt;
  int rttable;

  OLSR_DEBUG(LOG_ROUTING, "KERN: Adding %s\n", olsr_rtp_to_string(rt->rt_best));

  if (0 == olsr_cnf->rttable_default && 0 == rt->rt_dst.prefix_len && 253 > olsr_cnf->rttable)
  {
    /*
     * Users start whining about not having internet with policy
     * routing activated and no static default route in table 254.
     * We maintain a fallback defroute in the default=253 table.
     */
    olsr_netlink_route(rt, AF_INET, 253, RTM_NEWROUTE);
  }
  rttable = 0 == rt->rt_dst.prefix_len && olsr_cnf->rttable_default != 0
    ? olsr_cnf->rttable_default
    : olsr_cnf->rttable;
  rslt = olsr_netlink_route(rt, ip_version, rttable, RTM_NEWROUTE);

  if (rslt >= 0) {
    /*
     * Send IPC route update message
     */
    ipc_route_send_rtentry(&rt->rt_dst.prefix, &rt->rt_best->rtp_nexthop.gateway,
                           rt->rt_best->rtp_metric.hops, 1,
                           rt->rt_best->rtp_nexthop.interface->int_name);
  }
  return rslt;
}


/*
 * Remove a route from the kernel
 * @param destination the route to remove
 * @return negative on error
 */
int
olsr_kernel_del_route(const struct rt_entry *rt, int ip_version)
{
  int rslt;
  int rttable;

  OLSR_DEBUG(LOG_ROUTING, "KERN: Deleting %s\n", olsr_rt_to_string(rt));

  if (0 == olsr_cnf->rttable_default && 0 == rt->rt_dst.prefix_len && 253 > olsr_cnf->rttable)
  {
    /*
     * Also remove the fallback default route
     */
    olsr_netlink_route(rt, AF_INET, 253, RTM_DELROUTE);
  }
  rttable = 0 == rt->rt_dst.prefix_len && olsr_cnf->rttable_default != 0
    ? olsr_cnf->rttable_default
    : olsr_cnf->rttable;
  rslt = olsr_netlink_route(rt, ip_version, rttable, RTM_DELROUTE);
  if (rslt >= 0) {
    /*
     * Send IPC route update message
     */
    ipc_route_send_rtentry(&rt->rt_dst.prefix, NULL, 0, 0, NULL);
  }

  return rslt;
}


int olsr_create_lo_interface(union olsr_ip_addr *ip) {
  struct olsr_ipadd_req req;
  static char  l[] = "lo:olsr";

  memset(&req, 0, sizeof(req));

  req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
  req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_REPLACE;
  req.n.nlmsg_type = RTM_NEWADDR;
  req.ifa.ifa_family = olsr_cnf->ip_version;

  olsr_netlink_addreq(&req.n, sizeof(req), IFA_LABEL, l, strlen(l)+1);
  olsr_netlink_addreq(&req.n, sizeof(req), IFA_LOCAL, ip, olsr_cnf->ipsize);

  req.ifa.ifa_prefixlen = olsr_cnf->ipsize * 8;

  req.ifa.ifa_index = if_nametoindex("lo");

  return olsr_netlink_send(&req.n, req.buf, sizeof(req.buf));
}

int olsr_delete_lo_interface(union olsr_ip_addr *ip) {
  struct olsr_ipadd_req req;
  static char  l[] = "lo:olsr";

  memset(&req, 0, sizeof(req));

  req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
  req.n.nlmsg_flags = NLM_F_REQUEST;
  req.n.nlmsg_type = RTM_DELADDR;
  req.ifa.ifa_family = olsr_cnf->ip_version;

  olsr_netlink_addreq(&req.n, sizeof(req), IFA_LABEL, l, strlen(l)+1);
  olsr_netlink_addreq(&req.n, sizeof(req), IFA_LOCAL, ip, olsr_cnf->ipsize);

  req.ifa.ifa_prefixlen = olsr_cnf->ipsize * 8;

  req.ifa.ifa_index = if_nametoindex("lo");

  return olsr_netlink_send(&req.n, req.buf, sizeof(req.buf));
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

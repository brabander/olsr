
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

static int olsr_netlink_route_int(const struct rt_entry *, uint8_t, uint8_t, __u16, uint8_t);

/* values for control flag to handle recursive route corrections
 *  currently only requires in linux specific kernel_routes.c */

#define RT_NONE 0
#define RT_ORIG_REQUEST 1
#define RT_RETRY_AFTER_ADD_GATEWAY 2
#define RT_RETRY_AFTER_DELETE_SIMILAR 3
#define RT_DELETE_SIMILAR_ROUTE 4
#define RT_AUTO_ADD_GATEWAY_ROUTE 5
#define RT_DELETE_SIMILAR_AUTO_ROUTE 6
#define RT_LO_IP 7

struct olsr_rtreq {
  struct nlmsghdr n;
  struct rtmsg r;
  char buf[512];
};

struct olsr_ipadd_req {
  struct nlmsghdr n;
  struct ifaddrmsg ifa;
  char buf[256];
};


static void
olsr_netlink_addreq(struct nlmsghdr *n, size_t reqSize __attribute__ ((unused)), int type, const void *data, int len)
{
  struct rtattr *rta = (struct rtattr *)(((char *)n) + NLMSG_ALIGN(n->nlmsg_len));
  n->nlmsg_len = NLMSG_ALIGN(n->nlmsg_len) + RTA_LENGTH(len);
  //produces strange compile error
  //assert(n->nlmsg_len < reqSize);
  rta->rta_type = type;
  rta->rta_len = RTA_LENGTH(len);
  memcpy(RTA_DATA(rta), data, len);
}

/*rt_entry and nexthop and family and table must only be specified with an flag != RT_NONE*/
static int
olsr_netlink_send(struct nlmsghdr *n, char *buf, size_t bufSize, uint8_t flag, const struct rt_entry *rt,
                  const struct rt_nexthop *nexthop, uint8_t family, uint8_t rttable)
{
  struct iovec iov;
  struct sockaddr_nl nladdr = {.nl_family = AF_NETLINK };
  struct msghdr msg = {
    .msg_name = &nladdr,
    .msg_namelen = sizeof(nladdr),
    .msg_iov = &iov,
    .msg_iovlen = 1,
    .msg_control = NULL,
    .msg_controllen = 0,
    .msg_flags = 0
  };
  int ret = 1;                         /* helper variable for rtnetlink_message processing */
  int rt_ret = -2;                     /* if no response from rtnetlink it must be considered as failed! */

  iov.iov_base = n;
  iov.iov_len = n->nlmsg_len;
  ret = sendmsg(olsr_cnf->rts_linux, &msg, 0);
  if (0 <= ret) {
    iov.iov_base = buf;
    iov.iov_len = bufSize;
    ret = recvmsg(olsr_cnf->rts_linux, &msg, 0);
    if (0 < ret) {
      struct nlmsghdr *h = (struct nlmsghdr *)buf;
      while (NLMSG_OK(h, (unsigned int)ret)) {
        if (NLMSG_DONE == h->nlmsg_type) {
          //seems to be never reached
          //log this if it ever happens !!??
          break;
        }
        if (NLMSG_ERROR == h->nlmsg_type) {
          if (NLMSG_LENGTH(sizeof(struct nlmsgerr) <= h->nlmsg_len)) {
#ifndef REMOVE_LOG_DEBUG
            struct ipaddr_str ibuf;
            struct ipaddr_str gbuf;
#endif
            const struct nlmsgerr *l_err = (struct nlmsgerr *)NLMSG_DATA(h);
            errno = -l_err->error;
            if (0 != errno) {
#ifndef REMOVE_LOG_DEBUG
              const char *const err_msg = strerror(errno);
#endif
              ret = -1;
              rt_ret = -1;
#ifndef REMOVE_LOG_DEBUG
              if (flag != RT_NONE) {
                /* debug output for various situations */
                if (n->nlmsg_type == RTM_NEWRULE) {
                  OLSR_DEBUG(LOG_ROUTING, "Error '%s' (%d) on inserting empty policy rule aimed to activate RtTable %u!", err_msg,
                             errno, rttable);
                } else if (n->nlmsg_type == RTM_DELRULE) {
                  OLSR_DEBUG(LOG_ROUTING, "Error '%s' (%d) on deleting empty policy rule aimed to activate rtTable %u!", err_msg,
                             errno, rttable);
                } else if (flag <= RT_RETRY_AFTER_DELETE_SIMILAR) {
                  if (rt->rt_dst.prefix.v4.s_addr != nexthop->gateway.v4.s_addr)
                    OLSR_DEBUG(LOG_ROUTING, "error '%s' (%d) %s route to %s/%d via %s dev %s", err_msg, errno,
                               (n->nlmsg_type == RTM_NEWROUTE) ? "add" : "del", olsr_ip_to_string(&ibuf, &rt->rt_dst.prefix),
                               rt->rt_dst.prefix_len, olsr_ip_to_string(&gbuf, &nexthop->gateway), nexthop->interface->int_name);
                  else
                    OLSR_DEBUG(LOG_ROUTING, "error '%s' (%d) %s route to %s/%d dev %s", err_msg, errno,
                               (n->nlmsg_type == RTM_NEWROUTE) ? "add" : "del", olsr_ip_to_string(&ibuf, &rt->rt_dst.prefix),
                               rt->rt_dst.prefix_len, nexthop->interface->int_name);
                } else if (flag == RT_AUTO_ADD_GATEWAY_ROUTE)
                  OLSR_DEBUG(LOG_ROUTING, ". error '%s' (%d) auto-add route to %s dev %s", err_msg, errno,
                             olsr_ip_to_string(&ibuf, &nexthop->gateway), nexthop->interface->int_name);
                else if (flag == RT_DELETE_SIMILAR_ROUTE)
                  OLSR_DEBUG(LOG_ROUTING, ". error '%s' (%d) auto-delete route to %s gw %s", err_msg, errno,
                             olsr_ip_to_string(&ibuf, &rt->rt_dst.prefix), olsr_ip_to_string(&gbuf, &nexthop->gateway));
                else if (flag == RT_DELETE_SIMILAR_AUTO_ROUTE)
                  OLSR_DEBUG(LOG_ROUTING, ". . error '%s' (%d) auto-delete similar route to %s gw %s", err_msg, errno,
                             olsr_ip_to_string(&ibuf, &nexthop->gateway), olsr_ip_to_string(&gbuf, &nexthop->gateway));
                else {          /* should never happen */
                  OLSR_DEBUG(LOG_ROUTING, "# invalid internal route delete/add flag (%d) used!", flag);
                }
              }
              else { /*at least give some information*/
                OLSR_DEBUG(LOG_NETWORKING,"rtnetlink returned: %s (%d)",err_msg,errno);
              }
#endif /*REMOVE_LOG_DEBUG*/
            } else {            /* netlink acks requests with an errno=0 NLMSG_ERROR response! */
              rt_ret = 1;
            }
            if (flag != RT_NONE) {
              /* ignore file exist when inserting Addr to lo:olsr*/
              if ((errno == 17) & (flag == RT_LO_IP)) {
               OLSR_DEBUG(LOG_ROUTING, "ignoring 'File exists' (17) while adding addr to lo!");      
               rt_ret = 1;
              }
              /* resolve "File exist" (17) propblems (on orig and autogen routes) */
              if ((errno == 17) & ((flag == RT_ORIG_REQUEST) | (flag == RT_AUTO_ADD_GATEWAY_ROUTE)) & (n->nlmsg_type ==
                                                                                                       RTM_NEWROUTE)) {
                /* a similar route going over another gateway may be present, which has to be deleted! */
                OLSR_DEBUG(LOG_ROUTING, ". auto-deleting similar routes to resolve 'File exists' (17) while adding route!");
                rt_ret = RT_DELETE_SIMILAR_ROUTE;       /* processing will contiune after this loop */
              }
              /* report success on "No such process" (3) */
              else if ((errno == 3) & (n->nlmsg_type == RTM_DELROUTE) & (flag == RT_ORIG_REQUEST)) {
                /* another similar (but slightly different) route may be present at this point
                 * , if so this will get solved when adding new route to this destination */
                OLSR_DEBUG(LOG_ROUTING, ". ignoring 'No such process' (3) while deleting route!");
                rt_ret = 0;
              }
              /* insert route to gateway on the fly if "Network unreachable" (128) on 2.4 kernels
               * or on 2.6 kernel No such process (3) is reported in rtnetlink response
               * do this only with flat metric, as using metric values inherited from
               * a target behind the gateway is really strange, and could lead to multiple routes!
               * anyways if invalid gateway ips may happen we are f*cked up!!
               * but if not, these on the fly generated routes are no problem, and will only get used when needed
               * warning currently only for ipv4 */
              else if (((errno == 3) | (errno == 128)) & (flag == RT_ORIG_REQUEST) & (FIBM_FLAT == olsr_cnf->fib_metric)
                       & (n->nlmsg_type == RTM_NEWROUTE) & (rt->rt_dst.prefix.v4.s_addr != nexthop->gateway.v4.s_addr)) {
                if (errno == 128)
                  OLSR_DEBUG(LOG_ROUTING, ". autogenerating route to handle 'Network unreachable' (128) while adding route!");
                else
                  OLSR_DEBUG(LOG_ROUTING, ". autogenerating route to handle 'No such process' (3) while adding route!");

                rt_ret = RT_AUTO_ADD_GATEWAY_ROUTE;     /* processing will contiune after this loop */
              }
            }
          }
          break;
        }
        h = NLMSG_NEXT(h, ret);
      }
    }
  }
  if (flag != RT_NONE) {
    if (rt_ret == RT_DELETE_SIMILAR_ROUTE) {    //delete all routes that may collide
      /* recursive call to delete simlar routes, using flag 2 to invoke deletion of similar, not only exact matches */
      rt_ret = olsr_netlink_route_int(rt, family, rttable, RTM_DELROUTE,
                                      flag == RT_AUTO_ADD_GATEWAY_ROUTE ? RT_DELETE_SIMILAR_AUTO_ROUTE : RT_DELETE_SIMILAR_ROUTE);

      /* retry insert original route, if deleting similar succeeded, using flag=1 to prevent recursions */
      if (rt_ret > 0)
        rt_ret = olsr_netlink_route_int(rt, family, rttable, RTM_NEWROUTE, RT_RETRY_AFTER_DELETE_SIMILAR);
      else
        OLSR_DEBUG(LOG_ROUTING, ". failed on auto-deleting similar route conflicting with above route!");

      /* set appropriate return code for original request, while returning simple -1/1 if called recursive */
      if (flag != RT_AUTO_ADD_GATEWAY_ROUTE) {
        if (rt_ret > 0)
          rt_ret = 0;           /* successful recovery */
        else
          rt_ret = -1;          /* unrecoverable error */
      }
    }
    if (rt_ret == RT_AUTO_ADD_GATEWAY_ROUTE) {  /* autoadd route via gateway */
      /* recursive call to invoke creation of a route to the gateway */
      rt_ret = olsr_netlink_route_int(rt, family, rttable, RTM_NEWROUTE, RT_AUTO_ADD_GATEWAY_ROUTE);

      /* retry insert original route, if above succeeded without problems */
      if (rt_ret > 0)
        rt_ret = olsr_netlink_route_int(rt, family, rttable, RTM_NEWROUTE, RT_RETRY_AFTER_ADD_GATEWAY);
      else
        OLSR_DEBUG(LOG_ROUTING, ". failed on inserting auto-generated route to gateway of above route!");

      /* set appropriate return code for original request */
      if (rt_ret > 0)
        rt_ret = 0;             /* successful recovery */
      else
        rt_ret = -1;            /* unrecoverable error */
    }
    /* send ipc update on success (deprecated!?)
       if ( ( n->nlmsg_type != RTM_NEWRULE ) & ( n->nlmsg_type != RTM_DELRULE ) & (flag = RT_ORIG_REQUEST) & (0 <= rt_ret && olsr_cnf->ipc_connections > 0)) {
       ipc_route_send_rtentry(&rt->rt_dst.prefix, &nexthop->gateway, metric, RTM_NEWROUTE == n->nlmsg_type,
       if_ifwithindex_name(nexthop->iif_index));
       } */
    if (rt_ret == -2)
      OLSR_ERROR(LOG_ROUTING, "no rtnetlink response! (no system ressources left?, everything may happen now ...)");
    return rt_ret;
  } else return ret; 
}

//external wrapper function for above patched multi purpose rtnetlink function
int
olsr_netlink_rule(uint8_t family, uint8_t rttable, uint16_t cmd)
{
  struct rt_entry rt;
  return olsr_netlink_route_int(&rt, family, rttable, cmd, RT_ORIG_REQUEST);
}

/*internal wrapper function for above patched function*/
static int
olsr_netlink_route(const struct rt_entry *rt, uint8_t family, uint8_t rttable, __u16 cmd)
{
  return olsr_netlink_route_int(rt, family, rttable, cmd, RT_ORIG_REQUEST);
}

/* returns
 *  -1 on unrecoverable error (calling function will have to handle it)
 *  0 on unexpected but recoverable rtnetlink behaviour
 *  but some of the implemented recovery methods only cure symptoms,
 *  not the cause, like unintelligent ordering of inserted routes.
 *  1 on success */
static int
olsr_netlink_route_int(const struct rt_entry *rt, uint8_t family, uint8_t rttable, __u16 cmd, uint8_t flag)
{
  int ret = 0;
  struct olsr_rtreq req;
  uint32_t metric = ((cmd != RTM_NEWRULE) | (cmd != RTM_DELRULE)) ?
    FIBM_FLAT != olsr_cnf->fib_metric ? ((RTM_NEWROUTE == cmd) ? rt->rt_best->rtp_metric.hops : rt->rt_metric.hops)
    : RT_METRIC_DEFAULT : 0;
  const struct rt_nexthop *nexthop = ((cmd != RTM_NEWRULE) | (cmd != RTM_DELRULE)) ?
    (RTM_NEWROUTE == cmd) ? &rt->rt_best->rtp_nexthop : &rt->rt_nexthop : NULL;

  memset(&req, 0, sizeof(req));
  req.n.nlmsg_len = NLMSG_LENGTH(sizeof(req.r));
  req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL | NLM_F_ACK;
  req.n.nlmsg_type = cmd;
  req.r.rtm_family = family;
  req.r.rtm_table = rttable;
  /* RTN_UNSPEC would be the wildcard, but blackhole broadcast or nat roules should usually not conflict */
  req.r.rtm_type = RTN_UNICAST; /* -> olsr only adds/deletes unicast routes */
  req.r.rtm_protocol = RTPROT_UNSPEC;   /* wildcard to delete routes of all protos if no simlar-delete correct proto will get set below */
  req.r.rtm_scope = RT_SCOPE_NOWHERE;   /* as wildcard for deletion */
  req.r.rtm_dst_len = rt->rt_dst.prefix_len;

  if (NULL != nexthop->interface) {
    if ((cmd != RTM_NEWRULE) & (cmd != RTM_DELRULE)) {
      req.r.rtm_dst_len = rt->rt_dst.prefix_len;

      /* do not specify much as we wanna delete similar/conflicting routes */
      if ((flag != RT_DELETE_SIMILAR_ROUTE) & (flag != RT_DELETE_SIMILAR_AUTO_ROUTE)) {
        /* 0 gets replaced by OS-specifc default (3)
         * 1 is reserved so we take 0 instead (this really makes some sense)
         * other numbers are used 1:1 */
        req.r.rtm_protocol = ((olsr_cnf->rtproto < 1) ? RTPROT_BOOT : ((olsr_cnf->rtproto == 1) ? 0 : olsr_cnf->rtproto));
        req.r.rtm_scope = RT_SCOPE_LINK;

        /*add interface */
        olsr_netlink_addreq(&req.n, sizeof(req), RTA_OIF, &nexthop->interface->if_index, sizeof(nexthop->interface->if_index));

        if (olsr_cnf->source_ip_mode) {
          if (AF_INET == family)
            olsr_netlink_addreq(&req.n, sizeof(req), RTA_PREFSRC, &olsr_cnf->router_id.v4, sizeof(olsr_cnf->router_id.v4));
          else
            olsr_netlink_addreq(&req.n, sizeof(req), RTA_PREFSRC, &olsr_cnf->router_id.v6, sizeof(&olsr_cnf->router_id.v6));
	}
      }

      /* metric is specified always as we can only delete one route per iteration, and wanna hit the correct one first */
      if (FIBM_APPROX != olsr_cnf->fib_metric || (RTM_NEWROUTE == cmd)) {
        olsr_netlink_addreq(&req.n, sizeof(req), RTA_PRIORITY, &metric, sizeof(metric));
      }

      /* make sure that netmask = /32 as this is an autogenarated route */
      if ((flag == RT_AUTO_ADD_GATEWAY_ROUTE) | (flag == RT_DELETE_SIMILAR_AUTO_ROUTE))
        req.r.rtm_dst_len = 32;

      /* for ipv4 or ipv6 we add gateway if one is specified,
       * or leave gateway away if we want to delete similar routes aswell,
       * or even use the gateway as target if we add a auto-generated route,
       * or if delete-similar to make insertion of auto-generated route possible */
      if (AF_INET == family) {
        if ((flag != RT_AUTO_ADD_GATEWAY_ROUTE) & (flag != RT_DELETE_SIMILAR_ROUTE) &
            (flag != RT_DELETE_SIMILAR_AUTO_ROUTE) & (rt->rt_dst.prefix.v4.s_addr != nexthop->gateway.v4.s_addr)) {
          olsr_netlink_addreq(&req.n, sizeof(req), RTA_GATEWAY, &nexthop->gateway.v4, sizeof(nexthop->gateway.v4));
          req.r.rtm_scope = RT_SCOPE_UNIVERSE;
        }
        olsr_netlink_addreq(&req.n, sizeof(req), RTA_DST,
                            ((flag == RT_AUTO_ADD_GATEWAY_ROUTE) | (flag ==
                                                                    RT_DELETE_SIMILAR_AUTO_ROUTE)) ? &nexthop->gateway.v4 : &rt->
                            rt_dst.prefix.v4, sizeof(rt->rt_dst.prefix.v4));
      } else {
        if ((flag != RT_AUTO_ADD_GATEWAY_ROUTE) & (flag != RT_DELETE_SIMILAR_ROUTE) & (flag != RT_DELETE_SIMILAR_AUTO_ROUTE)
            & (0 != memcmp(&rt->rt_dst.prefix.v6, &nexthop->gateway.v6, sizeof(nexthop->gateway.v6)))) {
          olsr_netlink_addreq(&req.n, sizeof(req), RTA_GATEWAY, &nexthop->gateway.v6, sizeof(nexthop->gateway.v6));
          req.r.rtm_scope = RT_SCOPE_UNIVERSE;
        }
        olsr_netlink_addreq(&req.n, sizeof(req), RTA_DST,
                            ((flag == RT_AUTO_ADD_GATEWAY_ROUTE) | (flag ==
                                                                    RT_DELETE_SIMILAR_AUTO_ROUTE)) ? &nexthop->gateway.v6 : &rt->
                            rt_dst.prefix.v6, sizeof(rt->rt_dst.prefix.v6));
      }
    } else {                    //add or delete a rule
      static uint32_t priority = 65535;
      req.r.rtm_scope = RT_SCOPE_UNIVERSE;
      olsr_netlink_addreq(&req.n, sizeof(req), RTA_PRIORITY, &priority, sizeof(priority));
    }
  } else {
    /*
     * No interface means: remove unspecificed default route
     */
    req.r.rtm_scope = RT_SCOPE_NOWHERE;
  }
  ret = olsr_netlink_send(&req.n, req.buf, sizeof(req.buf), flag, rt, nexthop, family, rttable);
  if (0 <= ret && olsr_cnf->ipc_connections > 0) {
    ipc_route_send_rtentry(&rt->rt_dst.prefix, &nexthop->gateway, metric, RTM_NEWROUTE == cmd, nexthop->interface->int_name);
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

  if (0 == olsr_cnf->rttable_default && 0 == rt->rt_dst.prefix_len && 253 > olsr_cnf->rttable) {
    /*
     * Users start whining about not having internet with policy
     * routing activated and no static default route in table 254.
     * We maintain a fallback defroute in the default=253 table.
     */
    olsr_netlink_route(rt, AF_INET, 253, RTM_NEWROUTE);
  }
  rttable = 0 == rt->rt_dst.prefix_len && olsr_cnf->rttable_default != 0 ? olsr_cnf->rttable_default : olsr_cnf->rttable;
  rslt = olsr_netlink_route(rt, ip_version, rttable, RTM_NEWROUTE);

  if (rslt >= 0) {
    /*
     * Send IPC route update message
     */
    ipc_route_send_rtentry(&rt->rt_dst.prefix, &rt->rt_best->rtp_nexthop.gateway,
                           rt->rt_best->rtp_metric.hops, 1, rt->rt_best->rtp_nexthop.interface->int_name);
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

  if (0 == olsr_cnf->rttable_default && 0 == rt->rt_dst.prefix_len && 253 > olsr_cnf->rttable) {
    /*
     * Also remove the fallback default route
     */
    olsr_netlink_route(rt, AF_INET, 253, RTM_DELROUTE);
  }
  rttable = 0 == rt->rt_dst.prefix_len && olsr_cnf->rttable_default != 0 ? olsr_cnf->rttable_default : olsr_cnf->rttable;
  rslt = olsr_netlink_route(rt, ip_version, rttable, RTM_DELROUTE);
  if (rslt >= 0) {
    /*
     * Send IPC route update message
     */
    ipc_route_send_rtentry(&rt->rt_dst.prefix, NULL, 0, 0, NULL);
  }

  return rslt;
}


int
olsr_lo_interface(union olsr_ip_addr *ip, bool create)
{
  struct olsr_ipadd_req req;
  static char l[] = "lo:olsr";

  memset(&req, 0, sizeof(req));

  req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
  if (create) {
   req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_REPLACE | NLM_F_ACK;
   req.n.nlmsg_type = RTM_NEWADDR;
  } else {
   req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
   req.n.nlmsg_type = RTM_DELADDR;
  }
  req.ifa.ifa_family = olsr_cnf->ip_version;

  olsr_netlink_addreq(&req.n, sizeof(req), IFA_LABEL, l, strlen(l) + 1);
  olsr_netlink_addreq(&req.n, sizeof(req), IFA_LOCAL, ip, olsr_cnf->ipsize);

  req.ifa.ifa_prefixlen = olsr_cnf->ipsize * 8;

  req.ifa.ifa_index = if_nametoindex("lo");

  return olsr_netlink_send(&req.n, req.buf, sizeof(req.buf), RT_LO_IP, NULL, NULL, 0, 0);
}
/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

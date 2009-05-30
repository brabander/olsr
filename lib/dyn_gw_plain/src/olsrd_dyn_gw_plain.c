
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

#include "olsrd_dyn_gw_plain.h"
#include "olsr_types.h"
#include "ipcalc.h"
#include "scheduler.h"
#include "olsr.h"
#include "olsr_cookie.h"
#include "olsr_ip_prefix_list.h"
#include "olsr_logging.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <net/route.h>
#include <unistd.h>
#include <errno.h>

#define PLUGIN_INTERFACE_VERSION 5

static int has_inet_gateway;
static struct olsr_cookie_info *event_timer_cookie;
static union olsr_ip_addr gw_net;
static union olsr_ip_addr gw_netmask;

/**
 * Plugin interface version
 * Used by main olsrd to check plugin interface version
 */
int
olsrd_plugin_interface_version(void)
{
  return PLUGIN_INTERFACE_VERSION;
}

static const struct olsrd_plugin_parameters plugin_parameters[] = {
};

void
olsrd_get_plugin_parameters(const struct olsrd_plugin_parameters **params, int *size)
{
  *params = plugin_parameters;
  *size = ARRAYSIZE(plugin_parameters);
}

/**
 * Initialize plugin
 * Called after all parameters are passed
 */
int
olsrd_plugin_init(void)
{
  OLSR_INFO(LOG_PLUGINS, "OLSRD dyn_gw_plain plugin by Sven-Ola\n");

  gw_net.v4.s_addr = INET_NET;
  gw_netmask.v4.s_addr = INET_PREFIX;

  has_inet_gateway = 0;

  /* Remove all local Inet HNA entries */
  while (ip_prefix_list_remove(&olsr_cnf->hna_entries, &gw_net, olsr_netmask_to_prefix(&gw_netmask), olsr_cnf->ip_version)) {
    OLSR_DEBUG(LOG_PLUGINS, "HNA Internet gateway deleted\n");
  }

  /* create the cookie */
  event_timer_cookie = olsr_alloc_cookie("DynGW Plain: Event", OLSR_COOKIE_TYPE_TIMER);

  /* Register the GW check */
  olsr_start_timer(3 * MSEC_PER_SEC, 0, OLSR_TIMER_PERIODIC, &olsr_event, NULL, event_timer_cookie);

  return 1;
}

int
check_gw(union olsr_ip_addr *net, union olsr_ip_addr *mask)
{
  char buff[1024], iface[17];
  uint32_t gate_addr, dest_addr, netmask;
  unsigned int iflags;
  int num, metric, refcnt, use;
  int retval = 0;

  FILE *fp = fopen(PROCENTRY_ROUTE, "r");

  if (!fp) {
    OLSR_WARN(LOG_PLUGINS, "Cannot read proc file %s: %s\n", PROCENTRY_ROUTE, strerror(errno));
    return -1;
  }

  rewind(fp);

  /*
     OLSR_PRINTF(DEBUGLEV, "Genmask         Destination     Gateway         "
     "Flags Metric Ref    Use Iface\n");
   */
  while (fgets(buff, 1023, fp)) {
#if !defined REMOVE_LOG_DEBUG
    struct ipaddr_str buf;
#endif
    num =
      sscanf(buff, "%16s %128X %128X %X %d %d %d %128X \n", iface, &dest_addr, &gate_addr, &iflags, &refcnt, &use, &metric,
             &netmask);

    if (num < 8) {
      continue;
    }
    OLSR_DEBUG(LOG_PLUGINS, "%-15s %-15s %-15s %-6d %-2d %7d %s\n",
               olsr_ip_to_string(&buf, (union olsr_ip_addr *)&netmask),
               olsr_ip_to_string(&buf, (union olsr_ip_addr *)&dest_addr),
               olsr_ip_to_string(&buf, (union olsr_ip_addr *)&gate_addr), metric, refcnt, use, iface);

    if (                        /* (iflags & RTF_GATEWAY) && */
         (iflags & RTF_UP) && (metric == 0) && (netmask == mask->v4.s_addr) && (dest_addr == net->v4.s_addr)) {
      OLSR_DEBUG(LOG_PLUGINS, "INTERNET GATEWAY VIA %s detected in routing table.\n", iface);
      retval = 1;
    }

  }

  fclose(fp);

  if (retval == 0) {
    OLSR_DEBUG(LOG_PLUGINS, "No Internet GWs detected...\n");
  }

  return retval;
}

/**
 * Scheduled event to update the hna table,
 * called from olsrd main thread to keep the hna table thread-safe
 */
void
olsr_event(void *foo __attribute__ ((unused)))
{
  int res = check_gw(&gw_net, &gw_netmask);
  if (1 == res && 0 == has_inet_gateway) {
    OLSR_DEBUG(LOG_PLUGINS, "Adding OLSR local HNA entry for Internet\n");
    ip_prefix_list_add(&olsr_cnf->hna_entries, &gw_net, olsr_netmask_to_prefix(&gw_netmask));
    has_inet_gateway = 1;
  } else if (0 == res && 1 == has_inet_gateway) {
    /* Remove all local Inet HNA entries */
    while (ip_prefix_list_remove(&olsr_cnf->hna_entries, &gw_net, olsr_netmask_to_prefix(&gw_netmask), olsr_cnf->ip_version)) {
      OLSR_DEBUG(LOG_PLUGINS, "Removing OLSR local HNA entry for Internet\n");
    }
    has_inet_gateway = 0;
  }
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

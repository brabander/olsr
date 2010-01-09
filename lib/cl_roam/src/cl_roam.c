
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

#include "cl_roam.h"
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




int host_there=0;
int route_announced=0;



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

  OLSR_INFO(LOG_PLUGINS, "OLSRD automated Client Roaming Plugin\n");

  gw_net.v4.s_addr = inet_addr("10.0.0.134");
  gw_netmask.v4.s_addr = inet_addr("255.255.255.255");

  has_inet_gateway = 0;


  /* create the cookie */
  event_timer_cookie = olsr_alloc_cookie("cl roam: Event", OLSR_COOKIE_TYPE_TIMER);

  /* Register the GW check */
  olsr_start_timer(3 * MSEC_PER_SEC, 0, OLSR_TIMER_PERIODIC, &olsr_event, NULL, event_timer_cookie);




  return 1;
}














int do_ping(void)
{
    char ping_command[50];
    
    snprintf(ping_command, sizeof(ping_command), "ping -I ath0 -c 1 -q %s", "10.0.0.134");
    //snprintf(ping_command, sizeof(ping_command), "ping -c 1 -q %s", "137.0.0.1");
    if (system(ping_command) == 0) {
      system("echo \"ping erfolgreich\" ");
      OLSR_DEBUG(LOG_PLUGINS, "\nDo ping on %s ... ok\n", "10.0.0.134");
      host_there=1;
    }
    else
    {
      system("echo \"ping erfolglos\" ");
      OLSR_DEBUG(LOG_PLUGINS, "\nDo ping on %s ... failed\n", "10.0.0.134");
      host_there=0;
    }
}



/**
 * Scheduled event to update the hna table,
 * called from olsrd main thread to keep the hna table thread-safe
 */
void
olsr_event(void *foo __attribute__ ((unused)))
{
    do_ping();


    if (host_there && ! route_announced ) {
      OLSR_DEBUG(LOG_PLUGINS, "Adding Route\n");
      system("echo \"Setze route\" ");
      ip_prefix_list_add(&olsr_cnf->hna_entries, &gw_net, olsr_netmask_to_prefix(&gw_netmask));
      route_announced=1;
    }
    else if ((! host_there) &&  route_announced )
    {
      OLSR_DEBUG(LOG_PLUGINS, "Removing Route\n");
      system("echo \"Entferne route\" ");
      ip_prefix_list_remove(&olsr_cnf->hna_entries, &gw_net, olsr_netmask_to_prefix(&gw_netmask), olsr_cnf->ip_version);
      route_announced=0;
    }

}




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
 * $Id: olsrd_dyn_gw.c,v 1.8 2004/12/01 07:32:44 kattemat Exp $
 */

/*
 * Threaded ping code added by Jens Nachitgall
 *
 */

#include "olsrd_dyn_gw.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <net/route.h>
#include <linux/in_route.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>


static int has_inet_gateway;
static int has_available_gw;

/* set default interval, in case none is given in the config file */
static int interval = 5;

/* list to store the Ping IP addresses given in the config file */
struct ping_list {
  char *ping_address;
  struct ping_list *next;
};
struct ping_list *the_ping_list = NULL;

int
register_olsr_param(char *key, char *value)
{
  /* foo_addr is only used for call to inet_aton */ 
  struct in_addr foo_addr;
  int retval = -1;
 
  if (!strcmp(key, "Interval")) {
    if (sscanf(value, "%d", &interval) == 1) {
      retval = 1;
    }
  }
  if (!strcmp(key, "Ping")) {
    /* if value contains a valid IPaddr, then add it to the list */
    if (inet_aton(strdup(value), &foo_addr)) {
      the_ping_list = add_to_ping_list(value, the_ping_list);
      retval = 1;
    }
  }
  return retval;
}

/* add the valid IPs to the head of the list */
struct ping_list *
add_to_ping_list(char *ping_address, struct ping_list *the_ping_list)
{
  struct ping_list *new = (struct ping_list *) malloc(sizeof(struct ping_list));
  new->ping_address = strdup(ping_address);
  new->next = the_ping_list;
  return new;
}    

/**
 *Do initialization here
 *
 *This function is called by the my_init
 *function in uolsrd_plugin.c
 */
int
olsr_plugin_init()
{
  pthread_t ping_thread;
  
  gw_net.v4 = INET_NET;
  gw_netmask.v4 = INET_PREFIX;

  has_inet_gateway = 0;
  has_available_gw = 0;
  
  /* Remove all local Inet HNA entries */
  while(remove_local_hna4_entry(&gw_net, &gw_netmask))
    {
      olsr_printf(1, "HNA Internet gateway deleted\n");
    }

  pthread_create(&ping_thread, NULL, olsr_event, NULL);
  
  /* Register the GW check */
  olsr_register_scheduler_event(&olsr_event_doing_hna, NULL, 3, 4, NULL);

  return 1;
}



/*
 * destructor - called at unload
 */
void
olsr_plugin_exit()
{

}



/* Mulitpurpose funtion */
int
plugin_io(int cmd, void *data, size_t size)
{

  switch(cmd)
    {
    default:
      return 0;
    }
  
  return 1;
}




/**
 * the threaded function which happens within an endless loop,
 * reiterated every "Interval" sec (as given in the config or 
 * the default value)
 */
void *
olsr_event(void *foo)
{
  for(;;) {
    struct timespec remainder_spec;
    /* the time to wait in "Interval" sec (see connfig), default=5sec */
    struct timespec sleeptime_spec  = {(time_t) interval, 0L };

    /* check for gw in table entry and if Ping IPs are given also do pings */
    has_available_gw = check_gw(&gw_net, &gw_netmask);

    while(nanosleep(&sleeptime_spec, &remainder_spec) < 0)
      sleeptime_spec = remainder_spec;
  }
}


int
check_gw(union olsr_ip_addr *net, union hna_netmask *mask)
{
    char buff[1024], iface[16];
    olsr_u32_t gate_addr, dest_addr, netmask;
    unsigned int iflags;
    int num, metric, refcnt, use;
    int retval = 0;

    FILE *fp = fopen(PROCENTRY_ROUTE, "r");

    if (!fp) 
      {
        perror(PROCENTRY_ROUTE);
        olsr_printf(1, "INET (IPv4) not configured in this system.\n");
	return -1;
      }
    
    rewind(fp);

    /*
    olsr_printf(1, "Genmask         Destination     Gateway         "
                "Flags Metric Ref    Use Iface\n");
    */
    while (fgets(buff, 1023, fp)) 
      {	
	num = sscanf(buff, "%16s %128X %128X %X %d %d %d %128X \n",
		     iface, &dest_addr, &gate_addr,
		     &iflags, &refcnt, &use, &metric, &netmask);

	if (num < 8)
	  {
	    continue;
	  }

	/*
	olsr_printf(1, "%-15s ", olsr_ip_to_string((union olsr_ip_addr *)&netmask));

	olsr_printf(1, "%-15s ", olsr_ip_to_string((union olsr_ip_addr *)&dest_addr));

	olsr_printf(1, "%-15s %-6d %-2d %7d %s\n",
		    olsr_ip_to_string((union olsr_ip_addr *)&gate_addr),
		    metric, refcnt, use, iface);
	*/

	if((iflags & RTF_GATEWAY) &&
	   (iflags & RTF_UP) &&
	   (metric == 0) &&
	   (netmask == mask->v4) && 
	   (dest_addr == net->v4))
	  {
      /* don't ping, if there was no "Ping" IP addr in the config file */
      if (the_ping_list != NULL) {  
        /*validate the found inet gw by pinging*/ 
        if (ping_is_possible()) {
          olsr_printf(1, "INTERNET GATEWAY (ping is possible) VIA %s detected in routing table.\n", iface);
          retval=1;      
        }
      } else {
        olsr_printf(1, "INTERNET GATEWAY VIA %s detected in routing table.\n", iface);
        retval=1;      
      }
	  }

    }

    fclose(fp);  
  
    if(retval == 0)
      {
	olsr_printf(1, "No Internet GWs detected...\n");
      }
  
    return retval;
}

int
ping_is_possible() 
{
  struct ping_list *list;
  for (list = the_ping_list; list != NULL; list = list->next) {
    char ping_command[50] = "ping -c 1 -q ";
    strcat(ping_command, list->ping_address);
    olsr_printf(1, "\nDo ping on %s ...\n", list->ping_address);
    if (system(ping_command) == 0) {
      olsr_printf(1, "...OK\n\n");
      return 1;      
    } else {
      olsr_printf(1, "...FAILED\n\n");
    }
  }
  return 0;
}

/**
 * Scheduled event to update the hna table,
 * called from olsrd main thread to keep the hna table thread-safe
 */
void
olsr_event_doing_hna()
{
  if (has_available_gw == 1 && has_inet_gateway == 0) {
    olsr_printf(1, "Adding OLSR local HNA entry for Internet\n");
    add_local_hna4_entry(&gw_net, &gw_netmask);
    has_inet_gateway = 1;
  } else if ((has_available_gw == 0) && (has_inet_gateway == 1)) {
    /* Remove all local Inet HNA entries */
    while(remove_local_hna4_entry(&gw_net, &gw_netmask)) {
      olsr_printf(1, "Removing OLSR local HNA entry for Internet\n");
    }
    has_inet_gateway = 0;
  }
}
  


/*
 * OLSR plugin
 * Copyright (C) 2004 Andreas Tønnesen (andreto@olsr.org)
 *
 * This file is part of the olsrd dynamic gateway detection.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This plugin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with olsrd-unik; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 * 
 * $Id: olsrd_dyn_gw.c,v 1.6 2004/11/07 10:57:54 kattemat Exp $
 *
 */

/*
 * Dynamic linked library for UniK OLSRd
 */

#include "olsrd_dyn_gw.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <net/route.h>
#include <linux/in_route.h>
#include <unistd.h>
#include <errno.h>

static int has_inet_gateway;

/**
 *Do initialization here
 *
 *This function is called by the my_init
 *function in uolsrd_plugin.c
 */
int
olsr_plugin_init()
{
  gw_net.v4 = INET_NET;
  gw_netmask.v4 = INET_PREFIX;

  has_inet_gateway = 0;
 
  /* Remove all local Inet HNA entries */
  while(remove_local_hna4_entry(&gw_net, &gw_netmask))
    {
      olsr_printf(1, "HNA Internet gateway deleted\n");
    }

  /* Register the GW check */
  olsr_register_scheduler_event(&olsr_event, NULL, 5, 4, NULL);

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
 *Scheduled event
 */
void
olsr_event(void *foo)
{
  int res;

  res = check_gw(&gw_net, &gw_netmask);

  if((res == 1) && (has_inet_gateway == 0))
    {
      olsr_printf(1, "Adding OLSR local HNA entry for Internet\n");
      add_local_hna4_entry(&gw_net, &gw_netmask);
      has_inet_gateway = 1;
    }
  else
    {
      if((res == 0) && (has_inet_gateway == 1))
	{
	  /* Remove all local Inet HNA entries */
	  while(remove_local_hna4_entry(&gw_net, &gw_netmask))
	    {
	      olsr_printf(1, "Removing OLSR local HNA entry for Internet\n");
	    }
	  has_inet_gateway = 0;
	}
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
	    olsr_printf(1, "INTERNET GATEWAY VIA %s detected.\n", iface);
	    retval = 1;
	  }

    }

    fclose(fp);  
  
    if(retval == 0)
      {
	olsr_printf(1, "No Internet GWs detected...\n");
      }
  
    return retval;
}









/*************************************************************
 *                 TOOLS DERIVED FROM OLSRD                  *
 *************************************************************/


/**
 *Converts a olsr_ip_addr to a string
 *Goes for both IPv4 and IPv6
 *
 *NON REENTRANT! If you need to use this
 *function twice in e.g. the same printf
 *it will not work.
 *You must use it in different calls e.g.
 *two different printfs
 *
 *@param the IP to convert
 *@return a pointer to a static string buffer
 *representing the address in "dots and numbers"
 *
 */
char *
olsr_ip_to_string(union olsr_ip_addr *addr)
{

  char *ret;
  struct in_addr in;
  
  if(ipversion == AF_INET)
    {
      in.s_addr=addr->v4;
      ret = inet_ntoa(in);
    }
  else
    {
      /* IPv6 */
      ret = (char *)inet_ntop(AF_INET6, &addr->v6, ipv6_buf, sizeof(ipv6_buf));
    }

  return ret;
}



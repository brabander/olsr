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

int ipc_socket;
int ipc_open;
int ipc_connection;
int ipc_socket_up;

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


  /* Initial IPC value */
  ipc_open = 0;
  ipc_socket_up = 0;

  /* Register the GW check */
  olsr_register_scheduler_event(&olsr_event, 5, 4, NULL);

  return 1;
}

int
plugin_ipc_init()
{
  struct sockaddr_in sin;

  /* Init ipc socket */
  if ((ipc_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
    {
      olsr_printf(1, "(DYN GW)IPC socket %s\n", strerror(errno));
      return 0;
    }
  else
    {
      /* Bind the socket */
      
      /* complete the socket structure */
      memset(&sin, 0, sizeof(sin));
      sin.sin_family = AF_INET;
      sin.sin_addr.s_addr = INADDR_ANY;
      sin.sin_port = htons(9999);
      
      /* bind the socket to the port number */
      if (bind(ipc_socket, (struct sockaddr *) &sin, sizeof(sin)) == -1) 
	{
	  olsr_printf(1, "(DYN GW)IPC bind %s\n", strerror(errno));
	  return 0;
	}
      
      /* show that we are willing to listen */
      if (listen(ipc_socket, 1) == -1) 
	{
	  olsr_printf(1, "(DYN GW)IPC listen %s\n", strerror(errno));
	  return 0;
	}


      /* Register with olsrd */
      add_olsr_socket(ipc_socket, &ipc_action);
      ipc_socket_up = 1;
    }

  return 1;
}

void
ipc_action(int fd)
{
  struct sockaddr_in pin;
  socklen_t addrlen;
  char *addr;  

  addrlen = sizeof(struct sockaddr_in);

  if ((ipc_connection = accept(ipc_socket, (struct sockaddr *)  &pin, &addrlen)) == -1)
    {
      olsr_printf(1, "(DYN GW)IPC accept: %s\n", strerror(errno));
      exit(1);
    }
  else
    {
      addr = inet_ntoa(pin.sin_addr);
      if(ntohl(pin.sin_addr.s_addr) != INADDR_LOOPBACK)
	{
	  olsr_printf(1, "Front end-connection from foregin host(%s) not allowed!\n", addr);
	  close(ipc_connection);
	  return;
	}
      else
	{
	  ipc_open = 1;
	  olsr_printf(1, "(DYN GW)IPC: Connection from %s\n",addr);
	}
    }

}

/*
 * destructor - called at unload
 */
void
olsr_plugin_exit()
{
  if(ipc_open)
    close(ipc_socket);
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
olsr_event()
{
  int res;

  res = check_gw(&gw_net, &gw_netmask);

  if((res == 1) && (has_inet_gateway == 0))
    {
      ipc_send("Adding OLSR local HNA entry for Internet\n", strlen("Adding OLSR local HNA entry for Internet\n"));
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
	      ipc_send("Removing OLSR local HNA entry for Internet\n", strlen("Removing OLSR local HNA entry for Internet\n"));
	    }
	  has_inet_gateway = 0;
	}
    }

  if(!ipc_socket_up)
    plugin_ipc_init();

}






int
ipc_send(char *data, int size)
{
  if(!ipc_open)
    return 0;

  if (send(ipc_connection, data, size, MSG_NOSIGNAL) < 0) 
    {
      olsr_printf(1, "(DYN GW)IPC connection lost!\n");
      close(ipc_connection);
      ipc_open = 0;
      return -1;
    }

  return 1;
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
        ipc_send("INET (IPv4) not configured in this system.\n", strlen("INET (IPv4) not configured in this system.\n"));
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
	    sprintf(buff, "INTERNET GATEWAY VIA %s detected.\n", iface);
	    ipc_send(buff, strlen(buff));
	    retval = 1;
	  }

    }

    fclose(fp);  
  
    if(retval == 0)
      {
	ipc_send("No Internet GWs detected...\n", strlen("No Internet GWs detected...\n"));
      }
  
    return retval;
}









/*************************************************************
 *                 TOOLS DERIVED FROM OLSRD                  *
 *************************************************************/


/**
 *Hashing function. Creates a key based on
 *an 32-bit address.
 *@param address the address to hash
 *@return the hash(a value in the 0-31 range)
 */
olsr_u32_t
olsr_hashing(union olsr_ip_addr *address)
{
  olsr_u32_t hash;
  char *tmp;

  if(ipversion == AF_INET)
    /* IPv4 */  
    hash = (ntohl(address->v4));
  else
    {
      /* IPv6 */
      tmp = (char *) &address->v6;
      hash = (ntohl(*tmp));
    }

  //hash &= 0x7fffffff; 
  hash &= HASHMASK;

  return hash;
}



/**
 *Checks if a timer has times out. That means
 *if it is smaller than present time.
 *@param timer the timeval struct to evaluate
 *@return positive if the timer has not timed out,
 *0 if it matches with present time and negative
 *if it is timed out.
 */
int
olsr_timed_out(struct timeval *timer)
{
  return(timercmp(timer, now, <));
}



/**
 *Initiates a "timer", wich is a timeval structure,
 *with the value given in time_value.
 *@param time_value the value to initialize the timer with
 *@param hold_timer the timer itself
 *@return nada
 */
void
olsr_init_timer(olsr_u32_t time_value, struct timeval *hold_timer)
{ 
  olsr_u16_t  time_value_sec;
  olsr_u16_t  time_value_msec;

  time_value_sec = time_value/1000;
  time_value_msec = time_value-(time_value_sec*1000);

  hold_timer->tv_sec = time_value_sec;
  hold_timer->tv_usec = time_value_msec*1000;   
}





/**
 *Generaties a timestamp a certain number of milliseconds
 *into the future.
 *
 *@param time_value how many milliseconds from now
 *@param hold_timer the timer itself
 *@return nada
 */
void
olsr_get_timestamp(olsr_u32_t delay, struct timeval *hold_timer)
{ 
  olsr_u16_t  time_value_sec;
  olsr_u16_t  time_value_msec;

  time_value_sec = delay/1000;
  time_value_msec= delay - (delay*1000);

  hold_timer->tv_sec = now->tv_sec + time_value_sec;
  hold_timer->tv_usec = now->tv_usec + (time_value_msec*1000);   
}


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



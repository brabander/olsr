/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2004 Andreas Tønnesen (andreto@ifi.uio.no)
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
 * 
 * $Id: kernel_routes.c,v 1.8 2004/10/19 20:03:15 kattemat Exp $
 *
 */


#include "../kernel_routes.h"
#include "../link_set.h"
#include "../olsr.h"
#include <net/if.h>
#include <sys/ioctl.h>

/**
 *Insert a route in the kernel routing table
 *
 *@param destination the route to add
 *
 *@return negative on error
 */
int
olsr_ioctl_add_route(struct rt_entry *destination)
{

  struct rtentry kernel_route;
  int tmp;

  olsr_printf(1, "(ioctl)Adding route: %s ", 
	      olsr_ip_to_string(&destination->rt_dst));
  olsr_printf(1, "gw %s (hopc %d)\n", 
	      olsr_ip_to_string(&destination->rt_router), 
	      destination->rt_metric);

  memset(&kernel_route, 0, sizeof(struct rtentry));

  ((struct sockaddr_in*)&kernel_route.rt_dst)->sin_family = AF_INET;
  ((struct sockaddr_in*)&kernel_route.rt_gateway)->sin_family = AF_INET;
  ((struct sockaddr_in*)&kernel_route.rt_genmask)->sin_family = AF_INET;

  ((struct sockaddr_in *)&kernel_route.rt_dst)->sin_addr.s_addr = destination->rt_dst.v4;
  ((struct sockaddr_in *)&kernel_route.rt_genmask)->sin_addr.s_addr = destination->rt_mask.v4;

  if(destination->rt_dst.v4 != destination->rt_router.v4)
    {
      ((struct sockaddr_in *)&kernel_route.rt_gateway)->sin_addr.s_addr=destination->rt_router.v4;
    }

  kernel_route.rt_flags = destination->rt_flags;
  
  kernel_route.rt_metric = destination->rt_metric + 1;

  /* 
   * Thales Internet GW fix
   */

  if((del_gws) &&
     (destination->rt_dst.v4 == INADDR_ANY) &&
     (destination->rt_dst.v4 == INADDR_ANY))
    {
      delete_all_inet_gws();
      del_gws = 0;
    }

  /*
   * Set interface
   */
  if((kernel_route.rt_dev = malloc(strlen(destination->rt_if->int_name) + 1)) == 0)
    {
      fprintf(stderr, "Out of memory!\n%s\n", strerror(errno));
      olsr_exit(__func__, EXIT_FAILURE);
    }

  strcpy(kernel_route.rt_dev, destination->rt_if->int_name);

  
  //printf("Inserting route entry on device %s\n\n", kernel_route.rt_dev);
  
  /*
  printf("Adding route:\n\tdest: %s\n", olsr_ip_to_string(&destination->rt_dst));    
  printf("\trouter: %s\n", olsr_ip_to_string(&destination->rt_router));    
  printf("\tmask: %s\n", olsr_ip_to_string((union olsr_ip_addr *)&destination->rt_mask));    
  printf("\tmetric: %d\n", destination->rt_metric);    
  */

  //printf("\tiface: %s\n", kernel_route.rt_dev);    
  
  tmp = ioctl(ioctl_s,SIOCADDRT,&kernel_route);
  /*  kernel_route.rt_dev=*/

  /*
   *Send IPC route update message
   */
  
  if(olsr_cnf->open_ipc)
      {
	if(destination->rt_router.v4)
	  ipc_route_send_rtentry((union olsr_kernel_route *)&kernel_route, 1, destination->rt_if->int_name); /* Send interface name */
	else
	  ipc_route_send_rtentry((union olsr_kernel_route *)&kernel_route, 1, NULL);
      }
  
  
  if (ifnet && kernel_route.rt_dev)
    {
      free(kernel_route.rt_dev);
    }
  
  
  return tmp;
}




/**
 *Insert a route in the kernel routing table
 *
 *@param destination the route to add
 *
 *@return negative on error
 */
int
olsr_ioctl_add_route6(struct rt_entry *destination)
{

  struct in6_rtmsg kernel_route;
  int tmp;
  struct in6_addr zeroaddr;

  olsr_printf(2, "(ioctl)Adding route: %s(hopc %d)\n", 
	      olsr_ip_to_string(&destination->rt_dst), 
	      destination->rt_metric + 1);
  

  memset(&zeroaddr, 0, ipsize); /* Use for comparision */


  memset(&kernel_route, 0, sizeof(struct in6_rtmsg));

  COPY_IP(&kernel_route.rtmsg_dst, &destination->rt_dst);

  kernel_route.rtmsg_flags = destination->rt_flags;
  kernel_route.rtmsg_metric = destination->rt_metric;
  
  kernel_route.rtmsg_dst_len =   kernel_route.rtmsg_dst_len = destination->rt_mask.v6;

  if(memcmp(&destination->rt_dst, &destination->rt_router, ipsize) != 0)
    {
      COPY_IP(&kernel_route.rtmsg_gateway, &destination->rt_router);
    }
  else
    {
      COPY_IP(&kernel_route.rtmsg_gateway, &destination->rt_dst);
    }

      /*
       * set interface
       */
  kernel_route.rtmsg_ifindex = destination->rt_if->if_index;


  
  //olsr_printf(3, "Adding route to %s using gw ", olsr_ip_to_string((union olsr_ip_addr *)&kernel_route.rtmsg_dst));
  //olsr_printf(3, "%s\n", olsr_ip_to_string((union olsr_ip_addr *)&kernel_route.rtmsg_gateway));

  if((tmp = ioctl(ioctl_s, SIOCADDRT, &kernel_route)) < 0)
    {
      olsr_printf(1, "Add route: %s\n", strerror(errno));
      olsr_syslog(OLSR_LOG_ERR, "Add route:%m");
    }
  else
    {
      if(olsr_cnf->open_ipc)
	{
	  if(memcmp(&destination->rt_router, &null_addr6, ipsize) != 0)
	    ipc_route_send_rtentry((union olsr_kernel_route *)&kernel_route, 1, destination->rt_if->int_name); // Send interface name
	  else
	    ipc_route_send_rtentry((union olsr_kernel_route *)&kernel_route, 1, NULL);
	}
    }
    return(tmp);
}



/**
 *Remove a route from the kernel
 *
 *@param destination the route to remove
 *
 *@return negative on error
 */
int
olsr_ioctl_del_route(struct rt_entry *destination)
{

  struct rtentry kernel_route;
  int tmp;

  olsr_printf(1, "(ioctl)Deleting route: %s(hopc %d)\n", 
	      olsr_ip_to_string(&destination->rt_dst), 
	      destination->rt_metric + 1);

  memset(&kernel_route,0,sizeof(struct rtentry));

  ((struct sockaddr_in*)&kernel_route.rt_dst)->sin_family = AF_INET;
  ((struct sockaddr_in*)&kernel_route.rt_gateway)->sin_family = AF_INET;
  ((struct sockaddr_in*)&kernel_route.rt_genmask)->sin_family = AF_INET;

  ((struct sockaddr_in *)&kernel_route.rt_dst)->sin_addr.s_addr = destination->rt_dst.v4;
  if(destination->rt_dst.v4 != destination->rt_router.v4)
    ((struct sockaddr_in *)&kernel_route.rt_gateway)->sin_addr.s_addr = destination->rt_router.v4;
  ((struct sockaddr_in *)&kernel_route.rt_genmask)->sin_addr.s_addr = destination->rt_mask.v4;


  kernel_route.rt_dev = NULL;

  kernel_route.rt_flags = destination->rt_flags;
  
  kernel_route.rt_metric = destination->rt_metric + 1;

  /*
  printf("Deleteing route:\n\tdest: %s\n", olsr_ip_to_string(&destination->rt_dst));    
  printf("\trouter: %s\n", olsr_ip_to_string(&destination->rt_router));    
  printf("\tmask: %s\n", olsr_ip_to_string((union olsr_ip_addr *)&destination->rt_mask));    
  printf("\tmetric: %d\n", destination->rt_metric);    
  //printf("\tiface: %s\n", kernel_route.rt_dev);    
  */

  tmp = ioctl(ioctl_s, SIOCDELRT, &kernel_route);


    /*
     *Send IPC route update message
     */

  if(olsr_cnf->open_ipc)
    ipc_route_send_rtentry((union olsr_kernel_route *)&kernel_route, 0, NULL);

  return tmp;
}






/**
 *Remove a route from the kernel
 *
 *@param destination the route to remove
 *
 *@return negative on error
 */
int
olsr_ioctl_del_route6(struct rt_entry *destination)
{

  struct in6_rtmsg kernel_route;
  int tmp;

  union olsr_ip_addr tmp_addr = destination->rt_dst;

  olsr_printf(2, "(ioctl)Deleting route: %s(hopc %d)\n", 
	      olsr_ip_to_string(&destination->rt_dst), 
	      destination->rt_metric + 1);


  olsr_printf(1, "Deleting route: %s\n", olsr_ip_to_string(&tmp_addr));

  memset(&kernel_route,0,sizeof(struct in6_rtmsg));

  kernel_route.rtmsg_dst_len = destination->rt_mask.v6;

  memcpy(&kernel_route.rtmsg_dst, &destination->rt_dst, ipsize);

  memcpy(&kernel_route.rtmsg_gateway, &destination->rt_router, ipsize);

  kernel_route.rtmsg_flags = destination->rt_flags;
  kernel_route.rtmsg_metric = destination->rt_metric;


  tmp = ioctl(ioctl_s, SIOCDELRT,&kernel_route);


    /*
     *Send IPC route update message
     */

  if(olsr_cnf->open_ipc)
    ipc_route_send_rtentry((union olsr_kernel_route *)&kernel_route, 0, NULL);

  return tmp;
}



/**
 *Add a IP in IP tunnel route to a Internet gateway
 *First add acess to the gateway node trough the tunnel
 *then add the Internet gateway
 *
 *@return negative on error
 */

int
add_tunnel_route(union olsr_ip_addr *gw)
{

  struct rtentry kernel_route;
  int tmp;
  //olsr_u32_t adr, netmask;

  /* Get gw netaddress */
  /*
  adr = ntohl(gw->v4);
  if (IN_CLASSA(adr))
    netmask = IN_CLASSA_NET;
  else if (IN_CLASSB(adr))
    netmask = IN_CLASSB_NET;
  else
    netmask = IN_CLASSC_NET;

  netmask = htonl(netmask);
  */
  /* Global values */
  /*
  tunl_netmask = netmask;
  tunl_gw = gw->v4;

  printf("Adding route to gateway trough tunnel.\n\tNode %s\n", ip_to_string(&tunl_gw));
  printf("\tMask %s\n", ip_to_string(&netmask));
  */
  /* Adding net */
  /*
  memset(&kernel_route,0,sizeof(struct rtentry));

  ((struct sockaddr_in *)&kernel_route.rt_dst)->sin_addr.s_addr = tunl_gw;
  ((struct sockaddr_in *)&kernel_route.rt_dst)->sin_family = AF_INET;
  ((struct sockaddr_in *)&kernel_route.rt_genmask)->sin_addr.s_addr = netmask;
  ((struct sockaddr_in *)&kernel_route.rt_genmask)->sin_family = AF_INET;
  //((struct sockaddr_in *)&kernel_route.rt_gateway)->sin_addr.s_addr = INADDR_ANY;
  //((struct sockaddr_in *)&kernel_route.rt_gateway)->sin_family = AF_INET;

  //memcpy(&kernel_route.rt_gateway, gw, ipsize);

  kernel_route.rt_flags = RTF_UP;// | RTF_HOST;

  kernel_route.rt_metric = 1;

  if((kernel_route.rt_dev = malloc(6)) == 0)
    {
      fprintf(stderr, "Out of memory!\n%s\n", strerror(errno));
      olsr_exit(__func__, EXIT_FAILURE);
    }

  strcpy(kernel_route.rt_dev, "tunl1");

  
  //printf("Inserting route entry on device %s\n\n", kernel_route.rt_dev);

  if((tmp = ioctl(ioctl_s, SIOCADDRT, &kernel_route)) < 0)
    perror("Add default Internet route net");


  free(kernel_route.rt_dev);
  */

  /* Adding gateway */


  memset(&kernel_route,0,sizeof(struct rtentry));

  ((struct sockaddr_in *)&kernel_route.rt_dst)->sin_addr.s_addr = INADDR_ANY;
  ((struct sockaddr_in *)&kernel_route.rt_dst)->sin_family=AF_INET;
  ((struct sockaddr_in *)&kernel_route.rt_genmask)->sin_addr.s_addr = INADDR_ANY;
  ((struct sockaddr_in *)&kernel_route.rt_genmask)->sin_family=AF_INET;
  ((struct sockaddr_in *)&kernel_route.rt_gateway)->sin_addr.s_addr = INADDR_ANY;//tunl_gw;
  ((struct sockaddr_in *)&kernel_route.rt_gateway)->sin_family=AF_INET;


  //olsr_printf(1, "Adding kernel route for Internet router trough tunnel\n");
  
  kernel_route.rt_metric = 0;

  kernel_route.rt_flags = RTF_UP;// | RTF_GATEWAY;

  if((kernel_route.rt_dev = malloc(6)) == 0)
    {
      fprintf(stderr, "Out of memory!\n%s\n", strerror(errno));
      olsr_exit(__func__, EXIT_FAILURE);
    }

  strcpy(kernel_route.rt_dev, "tunl1");

  
  olsr_printf(1, "Inserting route entry on device %s\n\n", kernel_route.rt_dev);

  if((tmp = ioctl(ioctl_s,SIOCADDRT,&kernel_route)) < 0)
    {
      olsr_printf(1, "Add tunnel route: %s\n", strerror(errno));
      olsr_syslog(OLSR_LOG_ERR, "Add tunnel route:%m");
    }

  free(kernel_route.rt_dev);
  
  return(tmp);


}



int
delete_tunnel_route()
{
  struct rtentry kernel_route;
  int tmp;


  /* Delete gateway */

  olsr_printf(1, "Deleting tunnel GW route\n");

  memset(&kernel_route,0,sizeof(struct rtentry));

  ((struct sockaddr_in *)&kernel_route.rt_dst)->sin_addr.s_addr = INADDR_ANY;
  ((struct sockaddr_in *)&kernel_route.rt_dst)->sin_family=AF_INET;
  ((struct sockaddr_in *)&kernel_route.rt_genmask)->sin_addr.s_addr = INADDR_ANY;
  ((struct sockaddr_in *)&kernel_route.rt_genmask)->sin_family=AF_INET;
  
  ((struct sockaddr_in *)&kernel_route.rt_gateway)->sin_addr.s_addr = tunl_gw;
  ((struct sockaddr_in *)&kernel_route.rt_gateway)->sin_family=AF_INET;
  
  kernel_route.rt_metric = 1;
  
  kernel_route.rt_flags = RTF_UP | RTF_GATEWAY;

  if((kernel_route.rt_dev = malloc(6)) == 0)
    {
      fprintf(stderr, "Out of memory!\n%s\n", strerror(errno));
      olsr_exit(__func__, EXIT_FAILURE);
    }

  strcpy(kernel_route.rt_dev, "tunl1");

  
  //printf("Inserting route entry on device %s\n\n", kernel_route.rt_dev);

  if((tmp = ioctl(ioctl_s,SIOCDELRT,&kernel_route)) < 0)
    {
      olsr_printf(1, "Del tunnel route: %s\n", strerror(errno));
      olsr_syslog(OLSR_LOG_ERR, "Del tunnel route:%m");
    }
  
  free(kernel_route.rt_dev);



  olsr_printf(1, "Deleting route gateway trough tunnel.\n\tNet %s\n", ip_to_string(&tunl_gw));
  olsr_printf(1, "\tMask %s\n", ip_to_string(&tunl_netmask));

  /* Adding net */

  memset(&kernel_route,0,sizeof(struct rtentry));

  ((struct sockaddr_in *)&kernel_route.rt_dst)->sin_addr.s_addr = tunl_gw;
  ((struct sockaddr_in *)&kernel_route.rt_dst)->sin_family=AF_INET;
  ((struct sockaddr_in *)&kernel_route.rt_genmask)->sin_addr.s_addr = tunl_netmask;
  ((struct sockaddr_in *)&kernel_route.rt_genmask)->sin_family=AF_INET;

  //memcpy(&kernel_route.rt_gateway, gw, ipsize);

  

  kernel_route.rt_flags = RTF_UP | RTF_HOST;


  if((kernel_route.rt_dev = malloc(6)) == 0)
    {
      fprintf(stderr, "Out of memory!\n%s\n", strerror(errno));
      olsr_exit(__func__, EXIT_FAILURE);
    }

  strcpy(kernel_route.rt_dev, "tunl1");

  
  //printf("Inserting route entry on device %s\n\n", kernel_route.rt_dev);

  if((tmp = ioctl(ioctl_s,SIOCDELRT,&kernel_route)) < 0)
    {
      olsr_printf(1, "Del tunnel route: %s\n", strerror(errno));
      olsr_syslog(OLSR_LOG_ERR, "Del tunnel route:%m");
    }
  

  free(kernel_route.rt_dev);

  
  return(tmp);
}






int
delete_all_inet_gws()
{
  struct rtentry kernel_route;
  
  int s;
  char buf[BUFSIZ], *cp, *cplim;
  struct ifconf ifc;
  struct ifreq *ifr;
  
  olsr_printf(1, "Internet gateway detected...\nTrying to delete default gateways\n");
  
  /* Get a socket */
  if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
    {
      olsr_syslog(OLSR_LOG_ERR, "socket: %m");
      close(s);
      return -1;
    }
  
  ifc.ifc_len = sizeof (buf);
  ifc.ifc_buf = buf;
  if (ioctl(s, SIOCGIFCONF, (char *)&ifc) < 0) 
    {
      olsr_syslog(OLSR_LOG_ERR, "ioctl (get interface configuration)");
      close(s);
      return -1;
    }

  ifr = ifc.ifc_req;
#define size(p) (sizeof (p))
  cplim = buf + ifc.ifc_len; /*skip over if's with big ifr_addr's */
  for (cp = buf; cp < cplim;cp += sizeof (ifr->ifr_name) + size(ifr->ifr_addr)) 
    {
      ifr = (struct ifreq *)cp;
      
      
      if(strcmp(ifr->ifr_ifrn.ifrn_name, "lo") == 0)
	{
	  olsr_printf(1, "Skipping loopback...\n");
	  continue;
	}

      olsr_printf(1, "Trying 0.0.0.0/0 %s...", ifr->ifr_ifrn.ifrn_name);
      
      
      memset(&kernel_route,0,sizeof(struct rtentry));
      
      ((struct sockaddr_in *)&kernel_route.rt_dst)->sin_addr.s_addr = 0;
      ((struct sockaddr_in *)&kernel_route.rt_dst)->sin_family=AF_INET;
      ((struct sockaddr_in *)&kernel_route.rt_genmask)->sin_addr.s_addr = 0;
      ((struct sockaddr_in *)&kernel_route.rt_genmask)->sin_family=AF_INET;

      ((struct sockaddr_in *)&kernel_route.rt_gateway)->sin_addr.s_addr = INADDR_ANY;
      ((struct sockaddr_in *)&kernel_route.rt_gateway)->sin_family=AF_INET;
      
      //memcpy(&kernel_route.rt_gateway, gw, ipsize);
      
	   
	   
      kernel_route.rt_flags = RTF_UP | RTF_GATEWAY;
	   
	   
      if((kernel_route.rt_dev = malloc(6)) == 0)
	{
	  fprintf(stderr, "Out of memory!\n%s\n", strerror(errno));
	  olsr_exit(__func__, EXIT_FAILURE);
	}
	   
      strncpy(kernel_route.rt_dev, ifr->ifr_ifrn.ifrn_name, 6);

  
      //printf("Inserting route entry on device %s\n\n", kernel_route.rt_dev);
      
      if((ioctl(s, SIOCDELRT, &kernel_route)) < 0)
	olsr_printf(1, "NO\n");
      else
	olsr_printf(1, "YES\n");


      free(kernel_route.rt_dev);
      
    }
  
  close(s);
  
  return 0;
       
}

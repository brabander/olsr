/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2004 Andreas Tønnesen (andreto@ifi.uio.no)
 *
 * This file is part of olsr.org.
 *
 * UniK olsrd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * UniK olsrd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with olsr.org; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */


#include <net/if.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>

#include "tunnel.h"
#include "../defs.h"
#include "../olsr.h"
#include "../ifnet.h"

#include "../kernel_routes.h"

#include <errno.h>

/**
 *Set up a IP in IP tunnel to a Internet gateway
 *
 *@param p the ip_tunnel_parm struct containing 
 *the tunnel info
 *
 *@return negative on error
 */

int
add_ip_tunnel(struct ip_tunnel_parm *p)
{
  struct ifreq ifr;
  int err;
  
  /* Copy param for deletion */
  memcpy(&ipt, p, sizeof(struct ip_tunnel_parm));
  
  /* Create tunnel endpoint */
  
  strcpy(ifr.ifr_name, "tunl0");
  ifr.ifr_ifru.ifru_data = (void*)p;
  
  err = ioctl(ioctl_s, SIOCCHGTUNNEL, &ifr);
  if (err)
    {
      perror("change IPv4 tunnel ioctl");
      /* Try new tunnel */
      err = ioctl(ioctl_s, SIOCADDTUNNEL, &ifr);
      if(err)
	perror("add IPv4 tunnel ioctl");
    }
  
  
  /* Set local address */
  
  
  memcpy(&((struct sockaddr_in *)&ifr.ifr_dstaddr)->sin_addr, 
	 &main_addr, 
	 ipsize);
  
  ((struct sockaddr_in *)&ifr.ifr_addr)->sin_family = AF_INET; 
  
  
  strcpy(ifr.ifr_name, "tunl1");
  
  if(ioctl(ioctl_s, SIOCSIFADDR, &ifr) < 0)
    {
      fprintf(stderr, "\tERROR setting tunnel address!\n\t%s\n", 
	      strerror(errno));
    }
  
  
  /* Set local address */
  
  memset(&ifr, 0, sizeof(struct ifreq));
  
  
  memcpy(&((struct sockaddr_in *)&ifr.ifr_dstaddr)->sin_addr, 
	 &p->iph.saddr, 
	 ipsize);
  
  ((struct sockaddr_in *)&ifr.ifr_addr)->sin_family = AF_INET; 
  
  
  strcpy(ifr.ifr_name, "tunl1");
  
  if(ioctl(ioctl_s, SIOCSIFADDR, &ifr) < 0)
    {
      fprintf(stderr, "\tERROR setting tunnel address!\n\t%s\n", 
	      strerror(errno));
    }
  
  
  /* Set remote address */
  
  memcpy(&((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr, 
	 &p->iph.daddr, 
	 ipsize);
  
  ((struct sockaddr_in *)&ifr.ifr_addr)->sin_family = AF_INET; 
  
  if(ioctl(ioctl_s, SIOCSIFDSTADDR, &ifr) < 0)
    {
      fprintf(stderr, "\tERROR setting tunnel remote address!\n\t%s\n", 
	      strerror(errno));
    }
  
  
  /* Set up */
  set_flag("tunl1", IFF_POINTOPOINT);
  
  add_tunnel_route((union olsr_ip_addr *)&p->iph.daddr);
  
  /* Store the gateway address */
  memcpy(&tnl_addr, &p->iph.daddr, ipsize);
  inet_tnl_added = 1;
  
  /* Enable tunnel forwarding for gateways */
  enable_tunl_forwarding();
  
  return err;
}




int
del_ip_tunnel(struct ip_tunnel_parm *p)
{


  return 1;
}


/**
 *Set up a source side endpoint for a IP in IP
 *tunnel
 *
 *@param my_addr local address
 *@param dst destination address
 *@param if_index interface index to set up
 *
 *@return nada
 */

void
set_up_source_tnl(union olsr_ip_addr *my_addr, union olsr_ip_addr *dst, int if_index)
{
  struct ip_tunnel_parm itp;
  
  memset(&itp, 0, sizeof(struct ip_tunnel_parm));
  
  /*
   * Tunnel info 
   */
  /* Name - CAN'T BE tunl0 !!!!!!!!!!!!!!!!!!! */
  strcpy(itp.name, "tunl1");
  /* IP version */
  itp.iph.version = 4;
  itp.iph.ihl = 5;
  /* Tunnel type IPinIP */
  itp.iph.protocol = IPPROTO_IPIP;
  /* Time to live - 255 */
  itp.iph.ttl = 255;
  /* TOS - 1 */
  itp.iph.tos = 1;
  /* Fragmentation - from iptools */
  itp.iph.frag_off = htons(IP_DF);
  /* Source address */
  itp.iph.saddr = (u_int32_t) my_addr->v4;
  /* Destination address */
  itp.iph.daddr = (u_int32_t) dst->v4;

  olsr_printf(1, "\tsource   : %s\n", ip_to_string(&itp.iph.saddr));
  olsr_printf(1, "\tdest     : %s\n", ip_to_string(&itp.iph.daddr));

  /* Interface */
  olsr_printf(1, "\tInterface: %d\n", if_index);
  itp.link = if_index;

  /* Add IPv4 tunnel */
  add_ip_tunnel(&itp);

  return;
}





/**
 *Set up a gateway side IP in IP tunnel with foregin
 *endpoint address set to ANY.
 *
 *@return negative on error
 */

int
set_up_gw_tunnel(union olsr_ip_addr *adr)
{

  struct ifreq ifr;

  printf("Setting up a GW side IP tunnel...\n");

  gw_tunnel = 1;

  memset(&ifr, 0, sizeof(struct ifreq));

  /* Set address */
  strcpy(ifr.ifr_name, "tunl0");
    
  memcpy(&((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr, 
	 adr, 
	 ipsize);
  
  ((struct sockaddr_in *)&ifr.ifr_addr)->sin_family = AF_INET; 
  
  printf("Setting GW tunnel address %s\n", sockaddr_to_string(&ifr.ifr_addr));

  /* Set address */

  if(ioctl(ioctl_s, SIOCSIFADDR, &ifr) < 0)
    {
      fprintf(stderr, "\tERROR setting gw tunnel address!\n\t%s\n", 
	      strerror(errno));
    }

  /* Set tunnel UP */

  memset(&ifr, 0, sizeof(struct ifreq));

  /* Set address */
  strcpy(ifr.ifr_name, "tunl0");

  /* Get flags */
  if (ioctl(ioctl_s, SIOCGIFFLAGS, &ifr) < 0) 
    {
      fprintf(stderr,"ioctl (get interface flags)");
      return -1;
    }

  strcpy(ifr.ifr_name, "tunl0");

   if(!(ifr.ifr_flags & IFF_UP & IFF_RUNNING))
    {
      /* Add UP */
      ifr.ifr_flags |= (IFF_UP | IFF_RUNNING);
      /* Set flags + UP */
      if(ioctl(ioctl_s, SIOCSIFFLAGS, &ifr) < 0)
	{
	  fprintf(stderr, "ERROR(%s): %s\n", ifr.ifr_name, strerror(errno));
	  return -1;
	}
    }
   

   /* FIX THIS !!! */
   //enable_tunl_forwarding();

   return 1;
}





int
enable_tunl_forwarding()
{
  FILE *proc_fwd;
  int ans = 0;
  char procfile[FILENAME_MAX];

  strcpy(procfile, TUNL_PROC_FILE);


  if ((proc_fwd=fopen(procfile, "r"))==NULL)
    {
      fprintf(stderr, "WARINING!! Could not open %s for writing!!\n", procfile);      
      return 0;
    }
  
  else
    {
      ans = fgetc(proc_fwd);
      fclose(proc_fwd);
      if(ans == '1')
	{
	  printf("\nTunnel forwarding is enabeled\n");
	}
      else
	{
	  if ((proc_fwd=fopen(procfile, "w"))==NULL)
	    {
	      fprintf(stderr, "Could not open %s for writing!\n", procfile);
	      return 0;
	    }
	  else
	    {
	      printf("Enabling TUNNEL-forwarding by writing \"1\" to the %s file\nThis file will not be restored to its original state!\n", procfile);
	      fputs("1", proc_fwd);
	    }
	  fclose(proc_fwd);

	}
    }
  return 1;
      
}











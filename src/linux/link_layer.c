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
 */

/* Ugly fix to make this compile on wireless extentions < 16 */
#define _LINUX_ETHTOOL_H

#include "../link_layer.h"
#include "../olsr_protocol.h"
#include "../scheduler.h"
#include "../interfaces.h"
#include <linux/wireless.h>
#include <linux/icmp.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <net/if_arp.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>


extern char *
sockaddr_to_string(struct sockaddr *);

extern char *
olsr_ip_to_string(union olsr_ip_addr *);

extern int
olsr_printf(int, char *, ...);


#define	MAXIPLEN	60
#define	MAXICMPLEN	76

extern size_t ipsize;

extern int ioctl_s;

float poll_int = 0.2;

int
iw_get_range_info(char *, struct iw_range *);

int
clear_spy_list(char *);

int
convert_ip_to_mac(union olsr_ip_addr *, struct sockaddr *, char *);

void
ping_thread(void *);


void
init_link_layer_notification()
{
  struct interface *ifd;

  olsr_printf(1, "Initializing link-layer notification...\n");


  for (ifd = ifnet; ifd ; ifd = ifd->int_next) 
    {
      if(ifd->is_wireless)
	clear_spy_list(ifd->int_name);
    }

  olsr_register_scheduler_event(&poll_link_layer, poll_int, 0, NULL);

  return;
}

int
clear_spy_list(char *ifname)
{
  struct iwreq	wrq;

  /* Time to do send addresses to the driver */
  wrq.u.data.pointer = NULL;//(caddr_t) hw_address;
  wrq.u.data.length = 0;
  wrq.u.data.flags = 0;

  /* Set device name */
  strncpy(wrq.ifr_name, ifname, IFNAMSIZ);

  if(ioctl(ioctl_s, SIOCSIWSPY, &wrq) < 0)
    {
      olsr_printf(1, "Could not clear spylist %s\n", strerror(errno));
      return -1;
    }

  return 1;
}



int
add_spy_node(union olsr_ip_addr *addr, char *interface)
{
  struct sockaddr       new_node;
  struct iwreq		wrq;
  int			nbr;		/* Number of valid addresses */
  struct sockaddr	hw_address[IW_MAX_SPY];
  char	buffer[(sizeof(struct iw_quality) +
		sizeof(struct sockaddr)) * IW_MAX_SPY];
  
  olsr_printf(1, "Adding spynode!\n\n");
  
  /* get all addresses already in the driver */

  wrq.u.data.pointer = (caddr_t) buffer;
  wrq.u.data.length = IW_MAX_SPY;
  wrq.u.data.flags = 0;

  strncpy(wrq.ifr_name, interface, IFNAMSIZ);

  if(ioctl(ioctl_s, SIOCGIWSPY, &wrq) < 0)
    {
      olsr_printf(1, "Could not get old spylist %s\n", strerror(errno));
      return 0;
    }

  /* Copy old addresses */
  nbr = wrq.u.data.length;
  memcpy(hw_address, buffer, nbr * sizeof(struct sockaddr));

  olsr_printf(1, "Old addresses: %d\n\n", nbr);

  /* Check upper limit */
  if(nbr >= IW_MAX_SPY)
    return 0;

  /* Add new address if MAC exists in ARP cache */
  if(convert_ip_to_mac(addr, &new_node, interface) > 0)
    {
      memcpy(&hw_address[nbr], &new_node, sizeof(struct sockaddr));
      nbr++;
    }
  else
    return 0;
  
  /* Add all addresses */
  wrq.u.data.pointer = (caddr_t) hw_address;
  wrq.u.data.length = nbr;
  wrq.u.data.flags = 0;
  
  /* Set device name */
  strncpy(wrq.ifr_name, interface, IFNAMSIZ);
  
  if(ioctl(ioctl_s, SIOCSIWSPY, &wrq) < 0)
    {
      olsr_printf(1, "Could not clear spylist %s\n", strerror(errno));
      return 0;
    }


  return 1;
}


int
convert_ip_to_mac(union olsr_ip_addr *ip, struct sockaddr *mac, char *interface)
{
  struct arpreq	arp_query;
  struct sockaddr_in tmp_sockaddr;
  pthread_t ping_thr;


  memset(&arp_query, 0, sizeof(struct arpreq));

  olsr_printf(1, "\nARP conversion for %s interface %s\n", 
	      olsr_ip_to_string(ip),
	      interface);

  tmp_sockaddr.sin_family = AF_INET;
  tmp_sockaddr.sin_port = 0;

  memcpy(&tmp_sockaddr.sin_addr, ip, ipsize);

  /* Translate IP addresses to MAC addresses */
  memcpy(&arp_query.arp_pa, &tmp_sockaddr, sizeof(struct sockaddr_in));
  arp_query.arp_ha.sa_family = 0;
  arp_query.arp_flags = 0;

  strncpy(arp_query.arp_dev, interface, IFNAMSIZ);
  
  if((ioctl(ioctl_s, SIOCGARP, &arp_query) < 0) ||
     !(arp_query.arp_flags & ATF_COM)) /* ATF_COM - hw addr valid */
    {
      olsr_printf(1, "Arp failed: (%s) - trying lookup\n", strerror(errno));

      /* No address - create a thread that sends a PING */
      pthread_create(&ping_thr, NULL, (void *)&ping_thread, ip);
  
      return -1;
    }

  olsr_printf(1, "Arp success!\n");

  memcpy(mac, &arp_query.arp_ha, sizeof(struct sockaddr));

  return 1;
}



/**
 *A thread that sends a ICMP echo "ping" packet
 *to a given destination to force the ARP cache
 *to be updated... kind of a kludge....
 *
 *@param _ip the IP address to ping
 */
/* ONLY IPv4 FOR NOW!!! */

void
ping_thread(void *_ip)
{
  union olsr_ip_addr *ip;
  int ping_s;
  struct sockaddr dst;
  struct sockaddr_in *dst_in;
  char *packet;
  struct icmphdr *icp;

  dst_in = (struct sockaddr_in *) &dst;
  ip = (union olsr_ip_addr *)_ip;

  dst_in->sin_family = AF_INET;
  memcpy(&dst_in->sin_addr, ip, ipsize);

  olsr_printf(1, "pinging %s\n\n", olsr_ip_to_string(ip));

  if ((ping_s = socket(AF_INET, SOCK_RAW, PF_INET)) < 0) 
    {
      olsr_printf(1, "Could not create RAW socket for ping!\n%s\n", strerror(errno));
      return;
    }

  /* Create packet */
  packet = malloc(MAXIPLEN + MAXICMPLEN);
  
  
  icp = (struct icmphdr *)packet;
  icp->type = ICMP_ECHO;
  icp->code = 0;
  icp->checksum = 0;
  icp->un.echo.sequence = 1;
  icp->un.echo.id = getpid() & 0xFFFF;

  if((sendto(ping_s, packet, MAXIPLEN + MAXICMPLEN + 8, 0, &dst, sizeof(struct sockaddr))) !=
     MAXIPLEN + MAXICMPLEN + 8)
    {
      olsr_printf(1, "Error PING: %s\n", strerror(errno));
    }

  /* Nevermind the pong ;-) */

  olsr_printf(1, "Ping complete...\n");
  close(ping_s);

  free(packet);

  return;
}

void
poll_link_layer()
{
  struct iwreq		wrq;
  char		        buffer[(sizeof(struct iw_quality) +
			       sizeof(struct sockaddr)) * IW_MAX_SPY];
  struct sockaddr       *hwa;
  struct iw_quality     *qual;
  int		        n;
  struct iw_range	range;
  int		        i, j;
  int                   has_range = 0;
  struct interface      *iflist;

  //olsr_printf(1, "Polling link-layer notification...\n");

  for(iflist = ifnet; iflist != NULL; iflist = iflist->int_next)
    {
      if(!iflist->is_wireless)
	continue;

      /* Collect stats */
      wrq.u.data.pointer = (caddr_t) buffer;
      wrq.u.data.length = IW_MAX_SPY;
      wrq.u.data.flags = 0;
      
      /* Set device name */
      strncpy(wrq.ifr_name, iflist->int_name, IFNAMSIZ);
      
      /* Do the request */
      if(ioctl(ioctl_s, SIOCGIWSPY, &wrq) < 0)
	{
	  olsr_printf(1, "%-8.16s  Interface doesn't support wireless statistic collection\n\n", iflist->int_name);
	  return;
	}
      
      /* Get range info if we can */
      if(iw_get_range_info(iflist->int_name, &(range)) >= 0)
	has_range = 1;
      
      /* Number of addresses */
      n = wrq.u.data.length;
      
      /* The two lists */
      hwa = (struct sockaddr *) buffer;
      qual = (struct iw_quality *) (buffer + (sizeof(struct sockaddr) * n));
      
      for(i = 0; i < n; i++)
	{
	  if(!(qual->updated & 0x7))
	    continue;
	  
	  /* Print stats for each address */
	  olsr_printf(1, "MAC");
	  for(j = 0; j < 6; j++)
	    {
	      olsr_printf(1, ":%02x", (hwa[i].sa_data[j] % 0xffffff00));
	    }
	  if(!has_range)
	    olsr_printf(1, " : Quality:%d  Signal level:%d dBm  Noise level:%d dBm",
			qual[i].qual,
			qual[i].level - 0x100, 
			qual[i].noise - 0x100);
	  else
	    olsr_printf(1, " : Quality:%d/%d  Signal level:%d dBm  Noise level:%d dBm",
			qual[i].qual,
			range.max_qual.qual,
			qual[i].level - 0x100, 
			qual[i].noise - 0x100);
	  
	  olsr_printf(1, "\n");
	  
	}
    }

  //olsr_printf(1, "\n");
  return;
}





/*
 * Get the range information out of the driver
 */
int
iw_get_range_info(char            *ifname,
		  struct iw_range *range)
{
  struct iwreq		wrq;
  char			buffer[sizeof(struct iw_range) * 2];	/* Large enough */
  union iw_range_raw    *range_raw;

  /* Cleanup */
  bzero(buffer, sizeof(buffer));

  wrq.u.data.pointer = (caddr_t) buffer;
  wrq.u.data.length = sizeof(buffer);
  wrq.u.data.flags = 0;

  /* Set device name */
  strncpy(wrq.ifr_name, ifname, IFNAMSIZ);

  if(ioctl(ioctl_s, SIOCGIWRANGE, &wrq) < 0)
    {
      olsr_printf(1, "NO RANGE\n");
      return -1;
    }

  /* Point to the buffer */
  range_raw = (union iw_range_raw *) buffer;

  memcpy((char *) range, buffer, sizeof(struct iw_range));

  return 1;
}

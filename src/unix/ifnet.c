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
 * $Id: ifnet.c,v 1.14 2005/01/05 15:35:16 kattemat Exp $
 */


#ifdef linux
/*
 *Wireless definitions for ioctl calls
 *(from linux/wireless.h)
 */
#define SIOCGIWNAME	0x8B01		/* get name == wireless protocol */
#define SIOCSIWNWID	0x8B02		/* set network id (the cell) */
#define SIOCGIWNWID	0x8B03		/* get network id */
#define SIOCSIWFREQ	0x8B04		/* set channel/frequency (Hz) */
#define SIOCGIWFREQ	0x8B05		/* get channel/frequency (Hz) */
#define SIOCSIWMODE	0x8B06		/* set operation mode */
#define SIOCGIWMODE	0x8B07		/* get operation mode */
#define SIOCSIWSENS	0x8B08		/* set sensitivity (dBm) */
#define SIOCGIWSENS	0x8B09		/* get sensitivity (dBm) */
#elif defined __FreeBSD__ || defined __MacOSX__ || defined __NetBSD__
#define ifr_netmask ifr_addr
#endif

#include "interfaces.h"
#include "ifnet.h"
#include "defs.h"
#include "net_os.h"
#include "socket_parser.h"
#include "parser.h"
#include "scheduler.h"
#include "generate_msg.h"
#include "mantissa.h"
#include <signal.h>
#include <sys/types.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

static int bufspace = 127*1024;	/* max. input buffer size to request */


int
set_flag(char *ifname, short flag)
{
  struct ifreq ifr;

  strncpy(ifr.ifr_name, ifname, IFNAMSIZ);

  /* Get flags */
  if (ioctl(ioctl_s, SIOCGIFFLAGS, &ifr) < 0) 
    {
      fprintf(stderr,"ioctl (get interface flags)");
      return -1;
    }

  strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
  
  //printf("Setting flags for if \"%s\"\n", ifr.ifr_name);

  if(!(ifr.ifr_flags & (IFF_UP | IFF_RUNNING)))
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
  return 1;

}




void
check_interface_updates(void *foo)
{
  struct olsr_if *tmp_if;

#ifdef DEBUG
  olsr_printf(3, "Checking for updates in the interface set\n");
#endif

  for(tmp_if = olsr_cnf->interfaces; tmp_if != NULL; tmp_if = tmp_if->next)
    {

      if(tmp_if->configured)
	chk_if_changed(tmp_if);
      else
	chk_if_up(tmp_if, 3);
    }

  return;
}

/**
 * Checks if an initialized interface is changed
 * that is if it has been set down or the address
 * has been changed.
 *
 *@param iface the olsr_if struct describing the interface
 */
int
chk_if_changed(struct olsr_if *iface)
{
  struct interface *ifp, *tmp_ifp;
  struct ifreq ifr;
  struct sockaddr_in6 tmp_saddr6;
  int if_changes;
  struct ifchgf *tmp_ifchgf_list;
  if_changes = 0;

#ifdef DEBUG
  olsr_printf(3, "Checking if %s is set down or changed\n", iface->name);
#endif

  ifp = iface->interf;

  if(ifp == NULL)
    {
      /* Should not happen */
      iface->configured = 0;
      return 0;
    }

  memset(&ifr, 0, sizeof(struct ifreq));
  strncpy(ifr.ifr_name, iface->name, IFNAMSIZ);


  /* Get flags (and check if interface exists) */
  if (ioctl(ioctl_s, SIOCGIFFLAGS, &ifr) < 0) 
    {
      olsr_printf(3, "No such interface: %s\n", iface->name);
      goto remove_interface;
    }

  ifp->int_flags = ifr.ifr_flags | IFF_INTERFACE;

  /*
   * First check if the interface is set DOWN
   */

  if ((ifp->int_flags & IFF_UP) == 0)
    {
      olsr_printf(1, "\tInterface %s not up - removing it...\n", iface->name);
      goto remove_interface;
    }

  /*
   * We do all the interface type checks over.
   * This is because the interface might be a PCMCIA card. Therefore
   * It might not be the same physical interface as we configured earlier.
   */

  /* Check broadcast */
  if ((olsr_cnf->ip_version == AF_INET) && 
      (iface->cnf->ipv4_broadcast.v4) && /* Skip if fixed bcast */ 
      (!(ifp->int_flags & IFF_BROADCAST))) 
    {
      olsr_printf(3, "\tNo broadcast - removing\n");
      goto remove_interface;
    }

  if (ifp->int_flags & IFF_LOOPBACK)
    {
      olsr_printf(3, "\tThis is a loopback interface - removing it...\n");
      goto remove_interface;
    }


  /* trying to detect if interface is wireless. */
  ifp->is_wireless = check_wireless_interface(&ifr);

  /* Set interface metric */
  ifp->int_metric = ifp->is_wireless;

  /* Get MTU */
  if (ioctl(ioctl_s, SIOCGIFMTU, &ifr) < 0)
    ifp->int_mtu = 0;
  else
    {
      if(ifp->int_mtu != ifr.ifr_mtu)
	{
	  ifp->int_mtu = ifr.ifr_mtu;
	  /* Create new outputbuffer */
	  net_remove_buffer(ifp); /* Remove old */
	  net_add_buffer(ifp);
	}
    }


  /* Get interface index */
  ifp->if_index = if_nametoindex(ifr.ifr_name);

  /*
   * Now check if the IP has changed
   */
  
  /* IP version 6 */
  if(olsr_cnf->ip_version == AF_INET6)
    {
      /* Get interface address */
      
      if(get_ipv6_address(ifr.ifr_name, &tmp_saddr6, iface->cnf->ipv6_addrtype) <= 0)
	{
	  if(iface->cnf->ipv6_addrtype == IPV6_ADDR_SITELOCAL)
	    olsr_printf(3, "\tCould not find site-local IPv6 address for %s\n", ifr.ifr_name);
	  else
	    olsr_printf(3, "\tCould not find global IPv6 address for %s\n", ifr.ifr_name);
	  
	  
	  goto remove_interface;
	}
      
#ifdef DEBUG
      olsr_printf(3, "\tAddress: %s\n", ip6_to_string(&tmp_saddr6.sin6_addr));
#endif

      if(memcmp(&tmp_saddr6.sin6_addr, &ifp->int6_addr.sin6_addr, ipsize) != 0)
	{
	  olsr_printf(1, "New IP address for %s:\n", ifr.ifr_name);
	  olsr_printf(1, "\tOld: %s\n", ip6_to_string(&ifp->int6_addr.sin6_addr));
	  olsr_printf(1, "\tNew: %s\n", ip6_to_string(&tmp_saddr6.sin6_addr));

	  /* Check main addr */
	  if(memcmp(&main_addr, &tmp_saddr6.sin6_addr, ipsize) == 0)
	    {
	      /* Update main addr */
	      memcpy(&main_addr, &tmp_saddr6.sin6_addr, ipsize);
	    }

	  /* Update address */
	  memcpy(&ifp->int6_addr.sin6_addr, &tmp_saddr6.sin6_addr, ipsize);
	  memcpy(&ifp->ip_addr, &tmp_saddr6.sin6_addr, ipsize);

	  /*
	   *Call possible ifchange functions registered by plugins  
	   */
	  tmp_ifchgf_list = ifchgf_list;
	  while(tmp_ifchgf_list != NULL)
	    {
	      tmp_ifchgf_list->function(ifp, IFCHG_IF_UPDATE);
	      tmp_ifchgf_list = tmp_ifchgf_list->next;
	    }

	  return 1;	  	  
	}
      return 0;

    }
  else
  /* IP version 4 */
    {
      /* Check interface address (IPv4)*/
      if(ioctl(ioctl_s, SIOCGIFADDR, &ifr) < 0) 
	{
	  olsr_printf(1, "\tCould not get address of interface - removing it\n");
	  goto remove_interface;
	}

#ifdef DEBUG
      olsr_printf(3, "\tAddress:%s\n", sockaddr_to_string(&ifr.ifr_addr));
#endif

      if(memcmp(&((struct sockaddr_in *)&ifp->int_addr)->sin_addr.s_addr,
		&((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr, 
		ipsize) != 0)
	{
	  /* New address */
	  olsr_printf(1, "IPv4 address changed for %s\n", ifr.ifr_name);
	  olsr_printf(1, "\tOld:%s\n", sockaddr_to_string(&ifp->int_addr));
	  olsr_printf(1, "\tNew:%s\n", sockaddr_to_string(&ifr.ifr_addr));
	  
	  if(memcmp(&main_addr, 
		    &((struct sockaddr_in *)&ifp->int_addr)->sin_addr.s_addr, 
		    ipsize) == 0)
	    {
	      olsr_printf(1, "New main address: %s\n", sockaddr_to_string(&ifr.ifr_addr));
	      olsr_syslog(OLSR_LOG_INFO, "New main address: %s\n", sockaddr_to_string(&ifr.ifr_addr));
	      memcpy(&main_addr, 
		     &((struct sockaddr_in *)&ifp->int_addr)->sin_addr.s_addr, 
		     ipsize);
	    }

	  ifp->int_addr = ifr.ifr_addr;
	  memcpy(&ifp->ip_addr, 
		 &((struct sockaddr_in *)&ifp->int_addr)->sin_addr.s_addr, 
		 ipsize);

	  if_changes = 1;
	}

      /* Check netmask */
      if (ioctl(ioctl_s, SIOCGIFNETMASK, &ifr) < 0) 
	{
	  olsr_syslog(OLSR_LOG_ERR, "%s: ioctl (get broadaddr)", ifr.ifr_name);
	  goto remove_interface;
	}

#ifdef DEBUG
      olsr_printf(3, "\tNetmask:%s\n", sockaddr_to_string(&ifr.ifr_netmask));
#endif

      if(memcmp(&((struct sockaddr_in *)&ifp->int_netmask)->sin_addr.s_addr,
		&((struct sockaddr_in *)&ifr.ifr_netmask)->sin_addr.s_addr, 
		ipsize) != 0)
	{
	  /* New address */
	  olsr_printf(1, "IPv4 netmask changed for %s\n", ifr.ifr_name);
	  olsr_printf(1, "\tOld:%s\n", sockaddr_to_string(&ifp->int_netmask));
	  olsr_printf(1, "\tNew:%s\n", sockaddr_to_string(&ifr.ifr_netmask));

	  ifp->int_netmask = ifr.ifr_netmask;

	  if_changes = 1;
	}
      
      if(!iface->cnf->ipv4_broadcast.v4)
	{
	  /* Check broadcast address */      
	  if (ioctl(ioctl_s, SIOCGIFBRDADDR, &ifr) < 0) 
	    {
	      olsr_syslog(OLSR_LOG_ERR, "%s: ioctl (get broadaddr)", ifr.ifr_name);
	      goto remove_interface;
	    }
	  
#ifdef DEBUG
	  olsr_printf(3, "\tBroadcast address:%s\n", sockaddr_to_string(&ifr.ifr_broadaddr));
#endif
	  
	  if(memcmp(&((struct sockaddr_in *)&ifp->int_broadaddr)->sin_addr.s_addr,
		    &((struct sockaddr_in *)&ifr.ifr_broadaddr)->sin_addr.s_addr, 
		    ipsize) != 0)
	    {
	      
	      /* New address */
	      olsr_printf(1, "IPv4 broadcast changed for %s\n", ifr.ifr_name);
	      olsr_printf(1, "\tOld:%s\n", sockaddr_to_string(&ifp->int_broadaddr));
	      olsr_printf(1, "\tNew:%s\n", sockaddr_to_string(&ifr.ifr_broadaddr));
	      
	      ifp->int_broadaddr = ifr.ifr_broadaddr;
	      if_changes = 1;
	    }            
	}
    }

  if(if_changes)
    {
      /*
       *Call possible ifchange functions registered by plugins  
       */
      tmp_ifchgf_list = ifchgf_list;
      while(tmp_ifchgf_list != NULL)
	{
	  tmp_ifchgf_list->function(ifp, IFCHG_IF_UPDATE);
	  tmp_ifchgf_list = tmp_ifchgf_list->next;
	}
    }
  return if_changes;


 remove_interface:
  olsr_printf(1, "Removing interface %s\n", iface->name);
  olsr_syslog(OLSR_LOG_INFO, "Removing interface %s\n", iface->name);

  /*
   *Call possible ifchange functions registered by plugins  
   */
  tmp_ifchgf_list = ifchgf_list;
  while(tmp_ifchgf_list != NULL)
    {
      tmp_ifchgf_list->function(ifp, IFCHG_IF_REMOVE);
      tmp_ifchgf_list = tmp_ifchgf_list->next;
    }
  
  /* Dequeue */
  if(ifp == ifnet)
    {
      ifnet = ifp->int_next;
    }
  else
    {
      tmp_ifp = ifnet;
      while(tmp_ifp->int_next != ifp)
	{
	  tmp_ifp = tmp_ifp->int_next;
	}
      tmp_ifp->int_next = ifp->int_next;
    }


  /* Remove output buffer */
  net_remove_buffer(ifp);

  /* Check main addr */
  if(COMP_IP(&main_addr, &ifp->ip_addr))
    {
      if(ifnet == NULL)
	{
	  /* No more interfaces */
	  memset(&main_addr, 0, ipsize);
	  olsr_printf(1, "No more interfaces...\n");
	}
      else
	{
	  COPY_IP(&main_addr, &ifnet->ip_addr);
	  olsr_printf(1, "New main address: %s\n", olsr_ip_to_string(&main_addr));
	  olsr_syslog(OLSR_LOG_INFO, "New main address: %s\n", olsr_ip_to_string(&main_addr));
	}
    }


  /*
   * Deregister scheduled functions 
   */

  if (olsr_cnf->lq_level == 0)
    {
      olsr_remove_scheduler_event(&generate_hello, 
                                  ifp, 
                                  iface->cnf->hello_params.emission_interval, 
                                  0, 
                                  NULL);
      olsr_remove_scheduler_event(&generate_tc, 
                                  ifp, 
                                  iface->cnf->tc_params.emission_interval,
                                  0, 
                                  NULL);
    }

  else
    {
      olsr_remove_scheduler_event(&olsr_output_lq_hello, 
                                  ifp, 
                                  iface->cnf->hello_params.emission_interval, 
                                  0, 
                                  NULL);
      olsr_remove_scheduler_event(&olsr_output_lq_tc, 
                                  ifp, 
                                  iface->cnf->tc_params.emission_interval,
                                  0, 
                                  NULL);
    }

  olsr_remove_scheduler_event(&generate_mid, 
			      ifp, 
			      iface->cnf->mid_params.emission_interval,
			      0, 
			      NULL);
  olsr_remove_scheduler_event(&generate_hna, 
			      ifp, 
			      iface->cnf->hna_params.emission_interval,
			      0, 
			      NULL);



  iface->configured = 0;
  iface->interf = NULL;
  /* Close olsr socket */
  close(ifp->olsr_socket);
  remove_olsr_socket(ifp->olsr_socket, &olsr_input);
  /* Free memory */
  free(ifp->int_name);
  free(ifp);

  if((ifnet == NULL) && (!olsr_cnf->allow_no_interfaces))
    {
      olsr_printf(1, "No more active interfaces - exiting.\n");
      olsr_syslog(OLSR_LOG_INFO, "No more active interfaces - exiting.\n");
      exit_value = EXIT_FAILURE;
      kill(getpid(), SIGINT);
    }

  return 0;

}



/**
 * Initializes a interface described by iface,
 * if it is set up and is of the correct type.
 *
 *@param iface the olsr_if struct describing the interface
 *@param so the socket to use for ioctls
 *
 */
int
chk_if_up(struct olsr_if *iface, int debuglvl)
{
  struct interface ifs, *ifp;
  struct ifreq ifr;
  union olsr_ip_addr null_addr;
  struct ifchgf *tmp_ifchgf_list;
#ifdef linux
  int precedence = IPTOS_PREC(olsr_cnf->tos);
  int tos_bits = IPTOS_TOS(olsr_cnf->tos);
#endif

  memset(&ifr, 0, sizeof(struct ifreq));
  strncpy(ifr.ifr_name, iface->name, IFNAMSIZ);

  olsr_printf(debuglvl, "Checking %s:\n", ifr.ifr_name);

  /* Get flags (and check if interface exists) */
  if (ioctl(ioctl_s, SIOCGIFFLAGS, &ifr) < 0) 
    {
      olsr_printf(debuglvl, "\tNo such interface!\n");
      return 0;
    }

  ifs.int_flags = ifr.ifr_flags | IFF_INTERFACE;      


  if ((ifs.int_flags & IFF_UP) == 0)
    {
      olsr_printf(debuglvl, "\tInterface not up - skipping it...\n");
      return 0;
    }

  /* Check broadcast */
  if ((olsr_cnf->ip_version == AF_INET) &&
      (iface->cnf->ipv4_broadcast.v4) && /* Skip if fixed bcast */ 
      (!(ifs.int_flags & IFF_BROADCAST))) 
    {
      olsr_printf(debuglvl, "\tNo broadcast - skipping\n");
      return 0;
    }


  if (ifs.int_flags & IFF_LOOPBACK)
    {
      olsr_printf(debuglvl, "\tThis is a loopback interface - skipping it...\n");
      return 0;
    }

  /* trying to detect if interface is wireless. */
  if(check_wireless_interface(&ifr))
    {
      olsr_printf(debuglvl, "\tWireless interface detected\n");
      ifs.is_wireless = 1;
    }
  else
    {
      olsr_printf(debuglvl, "\tNot a wireless interface\n");
      ifs.is_wireless = 0;
    }

  
  /* IP version 6 */
  if(olsr_cnf->ip_version == AF_INET6)
    {
      /* Get interface address */
      
      if(get_ipv6_address(ifr.ifr_name, &ifs.int6_addr, iface->cnf->ipv6_addrtype) <= 0)
	{
	  if(iface->cnf->ipv6_addrtype == IPV6_ADDR_SITELOCAL)
	    olsr_printf(debuglvl, "\tCould not find site-local IPv6 address for %s\n", ifr.ifr_name);
	  else
	    olsr_printf(debuglvl, "\tCould not find global IPv6 address for %s\n", ifr.ifr_name);
	  
	  return 0;
	}
      
      olsr_printf(debuglvl, "\tAddress: %s\n", ip6_to_string(&ifs.int6_addr.sin6_addr));
      
      /* Multicast */
      ifs.int6_multaddr.sin6_addr = (iface->cnf->ipv6_addrtype == IPV6_ADDR_SITELOCAL) ? 
	iface->cnf->ipv6_multi_site.v6 :
	iface->cnf->ipv6_multi_glbl.v6;
      /* Set address family */
      ifs.int6_multaddr.sin6_family = AF_INET6;
      /* Set port */
      ifs.int6_multaddr.sin6_port = olsr_udp_port;
      
      olsr_printf(debuglvl, "\tMulticast: %s\n", ip6_to_string(&ifs.int6_multaddr.sin6_addr));
      
    }
  /* IP version 4 */
  else
    {
      /* Get interface address (IPv4)*/
      if(ioctl(ioctl_s, SIOCGIFADDR, &ifr) < 0) 
	{
	  olsr_printf(debuglvl, "\tCould not get address of interface - skipping it\n");
	  return 0;
	}
      
      ifs.int_addr = ifr.ifr_addr;
      
      /* Find netmask */
      
      if (ioctl(ioctl_s, SIOCGIFNETMASK, &ifr) < 0) 
	{
	  olsr_syslog(OLSR_LOG_ERR, "%s: ioctl (get netmask)", ifr.ifr_name);
	  return 0;
	}
      
      ifs.int_netmask = ifr.ifr_netmask;
      
      /* Find broadcast address */
      if(iface->cnf->ipv4_broadcast.v4)
	{
	  /* Specified broadcast */
	  memset(&ifs.int_broadaddr, 0, sizeof(struct sockaddr));
	  memcpy(&((struct sockaddr_in *)&ifs.int_broadaddr)->sin_addr.s_addr, 
		 &iface->cnf->ipv4_broadcast.v4, 
		 sizeof(olsr_u32_t));
	}
      else
	{
	  /* Autodetect */
	  if (ioctl(ioctl_s, SIOCGIFBRDADDR, &ifr) < 0) 
	    {
	      olsr_syslog(OLSR_LOG_ERR, "%s: ioctl (get broadaddr)", ifr.ifr_name);
	      return 0;
	    }
	  
	  ifs.int_broadaddr = ifr.ifr_broadaddr;
	}
      
      /* Deactivate IP spoof filter */
      deactivate_spoof(ifr.ifr_name, iface->index, olsr_cnf->ip_version);
      
      /* Disable ICMP redirects */
      disable_redirects(ifr.ifr_name, iface->index, olsr_cnf->ip_version);
      
    }
  
  
  /* Get interface index */
  
  ifs.if_index = if_nametoindex(ifr.ifr_name);
  
  /* Set interface metric */
  ifs.int_metric = ifs.is_wireless;
  
  /* setting the interfaces number*/
  ifs.if_nr = iface->index;


  /* Get MTU */
  if (ioctl(ioctl_s, SIOCGIFMTU, &ifr) < 0)
    ifs.int_mtu = OLSR_DEFAULT_MTU;
  else
    ifs.int_mtu = ifr.ifr_mtu;

  /* Set up buffer */
  net_add_buffer(&ifs);
	       
  olsr_printf(1, "\tMTU: %d\n", ifs.int_mtu);

  olsr_syslog(OLSR_LOG_INFO, "Adding interface %s\n", iface->name);
  olsr_printf(1, "\tIndex %d\n", ifs.if_nr);

  if(olsr_cnf->ip_version == AF_INET)
    {
      olsr_printf(1, "\tAddress:%s\n", sockaddr_to_string(&ifs.int_addr));
      olsr_printf(1, "\tNetmask:%s\n", sockaddr_to_string(&ifs.int_netmask));
      olsr_printf(1, "\tBroadcast address:%s\n", sockaddr_to_string(&ifs.int_broadaddr));
    }
  else
    {
      olsr_printf(1, "\tAddress: %s\n", ip6_to_string(&ifs.int6_addr.sin6_addr));
      olsr_printf(1, "\tMulticast: %s\n", ip6_to_string(&ifs.int6_multaddr.sin6_addr));
    }
  
  ifp = olsr_malloc(sizeof (struct interface), "Interface update 2");
  
  iface->configured = 1;
  iface->interf = ifp;
  
  memcpy(ifp, &ifs, sizeof(struct interface));
  
  ifp->int_name = olsr_malloc(strlen(ifr.ifr_name) + 1, "Interface update 3");
      
  strcpy(ifp->int_name, ifr.ifr_name);
  /* Segfaults if using strncpy(IFNAMSIZ) why oh why?? */
  ifp->int_next = ifnet;
  ifnet = ifp;

  if(olsr_cnf->ip_version == AF_INET)
    {
      /* IP version 4 */
      ifp->ip_addr.v4 = ((struct sockaddr_in *)&ifp->int_addr)->sin_addr.s_addr;
      
      /*
       *We create one socket for each interface and bind
       *the socket to it. This to ensure that we can control
       *on what interface the message is transmitted
       */
      
      ifp->olsr_socket = getsocket((struct sockaddr *)&addrsock, bufspace, ifp->int_name);
      
      if (ifp->olsr_socket < 0)
	{
	  fprintf(stderr, "Could not initialize socket... exiting!\n\n");
	  olsr_syslog(OLSR_LOG_ERR, "Could not initialize socket... exiting!\n\n");
	  exit_value = EXIT_FAILURE;
	  kill(getpid(), SIGINT);
	}
    }
  else
    {
      /* IP version 6 */
      memcpy(&ifp->ip_addr, &ifp->int6_addr.sin6_addr, ipsize);

      
      /*
       *We create one socket for each interface and bind
       *the socket to it. This to ensure that we can control
       *on what interface the message is transmitted
       */
      
      ifp->olsr_socket = getsocket6(&addrsock6, bufspace, ifp->int_name);
      
      join_mcast(ifp, ifp->olsr_socket);
      
      if (ifp->olsr_socket < 0)
	{
	  fprintf(stderr, "Could not initialize socket... exiting!\n\n");
	  olsr_syslog(OLSR_LOG_ERR, "Could not initialize socket... exiting!\n\n");
	  exit_value = EXIT_FAILURE;
	  kill(getpid(), SIGINT);
	}
      
    }
  
  /* Register socket */
  add_olsr_socket(ifp->olsr_socket, &olsr_input);
  
#ifdef linux 
  /* Set TOS */
  
  if (setsockopt(ifp->olsr_socket, SOL_SOCKET, SO_PRIORITY, (char*)&precedence, sizeof(precedence)) < 0)
    {
      perror("setsockopt(SO_PRIORITY)");
      olsr_syslog(OLSR_LOG_ERR, "OLSRD: setsockopt(SO_PRIORITY) error %m");
    }
  if (setsockopt(ifp->olsr_socket, SOL_IP, IP_TOS, (char*)&tos_bits, sizeof(tos_bits)) < 0)    
    {
      perror("setsockopt(IP_TOS)");
      olsr_syslog(OLSR_LOG_ERR, "setsockopt(IP_TOS) error %m");
    }
#endif
  
  /*
   *Initialize sequencenumber as a random 16bit value
   */
  ifp->olsr_seqnum = random() & 0xFFFF;

  /*
   * Set main address if this is the only interface
   */
  memset(&null_addr, 0, ipsize);
  if(COMP_IP(&null_addr, &main_addr))
    {
      COPY_IP(&main_addr, &ifp->ip_addr);
      olsr_printf(1, "New main address: %s\n", olsr_ip_to_string(&main_addr));
      olsr_syslog(OLSR_LOG_INFO, "New main address: %s\n", olsr_ip_to_string(&main_addr));
    }
  
  /*
   * Register scheduled functions 
   */

  if (olsr_cnf->lq_level == 0)
    {
      olsr_register_scheduler_event(&generate_hello, 
                                    ifp, 
                                    iface->cnf->hello_params.emission_interval, 
                                    0, 
                                    NULL);
      olsr_register_scheduler_event(&generate_tc, 
                                    ifp, 
                                    iface->cnf->tc_params.emission_interval,
                                    0, 
                                    NULL);
    }

  else
    {
      olsr_register_scheduler_event(&olsr_output_lq_hello, 
                                    ifp, 
                                    iface->cnf->hello_params.emission_interval, 
                                    0, 
                                    NULL);
      olsr_register_scheduler_event(&olsr_output_lq_tc, 
                                    ifp, 
                                    iface->cnf->tc_params.emission_interval,
                                    0, 
                                    NULL);
    }

  olsr_register_scheduler_event(&generate_mid, 
				ifp, 
				iface->cnf->mid_params.emission_interval,
				0, 
				NULL);
  olsr_register_scheduler_event(&generate_hna, 
				ifp, 
				iface->cnf->hna_params.emission_interval,
				0, 
				NULL);

  /* Recalculate max jitter */

  if((max_jitter == 0) || ((iface->cnf->hello_params.emission_interval / 4) < max_jitter))
    max_jitter = iface->cnf->hello_params.emission_interval / 4;

  /* Recalculate max topology hold time */
  if(max_tc_vtime < iface->cnf->tc_params.emission_interval)
    max_tc_vtime = iface->cnf->tc_params.emission_interval;

  ifp->hello_etime = double_to_me(iface->cnf->hello_params.emission_interval);
  ifp->valtimes.hello = double_to_me(iface->cnf->hello_params.validity_time);
  ifp->valtimes.tc = double_to_me(iface->cnf->tc_params.validity_time);
  ifp->valtimes.mid = double_to_me(iface->cnf->mid_params.validity_time);
  ifp->valtimes.hna = double_to_me(iface->cnf->hna_params.validity_time);


  /*
   *Call possible ifchange functions registered by plugins  
   */
  tmp_ifchgf_list = ifchgf_list;
  while(tmp_ifchgf_list != NULL)
    {
      tmp_ifchgf_list->function(ifp, IFCHG_IF_ADD);
      tmp_ifchgf_list = tmp_ifchgf_list->next;
    }

  return 1;
}



#ifdef linux
/**
 *Check if a interface is wireless
 *Returns 1 if no info can be gathered
 *
 *@param sock socket to use for kernel communication
 *@param ifr a ifreq struct describing the interface
 *
 *@return 1 if interface is wireless(or no info was
 *found) 0 if not.
 */
int
check_wireless_interface(struct ifreq *ifr)
{
  if(ioctl(ioctl_s, SIOCGIWNAME, ifr) >= 0)
    {
      return 1;
    }
  else
    {
      return 0;
    }

}
#else
int check_wireless_interface(struct ifreq *ifr)
{
  return 1;
}
#endif



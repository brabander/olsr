/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2003 Andreas Tønnesen (andreto@ifi.uio.no)
 *
 * This file is part of olsrd-unik.
 *
 * olsrd-unik is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * olsrd-unik is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with olsrd-unik; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "defs.h"
#include "interfaces.h"
#include "ifnet.h"
#include "scheduler.h"

/**
 *Do initialization of various data needed for
 *network interface management.
 *This function also tries to set up the given interfaces.
 *
 *@return the number of interfaces configured
 */
int
ifinit()
{
  struct if_name *tmp_if;


  /* Initial values */
  ifnet = NULL;
  nbinterf = 0;

  /*
   *Initializing addrsock struct to be
   *used on all the sockets
   */
  if(ipversion == AF_INET)
    {
      /* IP version 4 */
      memset(&addrsock, 0, sizeof (addrsock));
      addrsock.sin_family = AF_INET;
      addrsock.sin_port = olsr_udp_port;
      (addrsock.sin_addr).s_addr = INADDR_ANY;
    }
  else
    {
      /* IP version 6 */
      memset(&addrsock6, 0, sizeof (addrsock6));
      addrsock6.sin6_family = AF_INET6;
      addrsock6.sin6_port = olsr_udp_port;
      //(addrsock6.sin6_addr).s_addr = IN6ADDR_ANY_INIT;
    }

  olsr_printf(1, "\n ---- Interface configuration ---- \n\n");
  /* Run trough all interfaces immedeatly */
  for(tmp_if = if_names; tmp_if != NULL; tmp_if = tmp_if->next)
    {
      chk_if_up(tmp_if, 1);	
    }
  
  /* register network interface update function with scheduler */
  olsr_register_scheduler_event(&check_interface_updates, 5.0, 0, NULL);

  return nbinterf;
}



/**
 *Find the local interface with a given address.
 *
 *@param addr the address to check.
 *
 *@return the interface struct representing the interface
 *that matched the address.
 */

struct interface *
if_ifwithaddr(union olsr_ip_addr *addr)
{
  struct interface *ifp;

  for (ifp = ifnet; ifp; ifp = ifp->int_next)
    {
      if(ipversion == AF_INET)
	{
	  /* IPv4 */
	  //printf("Checking: %s == ", inet_ntoa(((struct sockaddr_in *)&ifp->int_addr)->sin_addr));
	  //printf("%s\n", olsr_ip_to_string(addr));

	  if (COMP_IP(&((struct sockaddr_in *)&ifp->int_addr)->sin_addr, addr))
	      return ifp;
	}
      else
	{
	  /* IPv6 */
	  //printf("Checking %s ", olsr_ip_to_string((union olsr_ip_addr *)&ifp->int6_addr.sin6_addr));
	  //printf("== %s\n", olsr_ip_to_string((union olsr_ip_addr *)&((struct sockaddr_in6 *)addr)->sin6_addr));
	  if (COMP_IP(&ifp->int6_addr.sin6_addr, addr))
	    return ifp;
	}
    }
  return NULL;
}



/**
 *Find the interface with a given number.
 *
 *@param nr the number of the interface to find.
 *
 *@return return the interface struct representing the interface
 *that matched the number.
 */
struct interface *
if_ifwithsock(int fd)
{
  struct interface *ifp;
  ifp = ifnet;

  while (ifp) 
    {
      if (ifp->olsr_socket == fd)
	return ifp;
      ifp = ifp->int_next;
    }
  
  return (ifp);
}

#ifndef WIN32
/*
 *From net-tools lib/interface.c
 *
 */

int
get_ipv6_address(char *ifname, struct sockaddr_in6 *saddr6, int scope_in)
{
  char addr6[40], devname[IFNAMSIZ];
  char addr6p[8][5];
  int plen, scope, dad_status, if_idx;
  FILE *f;
  struct sockaddr_in6 tmp_sockaddr6;

  if ((f = fopen(_PATH_PROCNET_IFINET6, "r")) != NULL) 
    {
      while (fscanf(f, "%4s%4s%4s%4s%4s%4s%4s%4s %02x %02x %02x %02x %20s\n",
		    addr6p[0], addr6p[1], addr6p[2], addr6p[3],
		    addr6p[4], addr6p[5], addr6p[6], addr6p[7],
		    &if_idx, &plen, &scope, &dad_status, devname) != EOF) 
	{
	  if (!strcmp(devname, ifname)) 
	    {
	      sprintf(addr6, "%s:%s:%s:%s:%s:%s:%s:%s",
		      addr6p[0], addr6p[1], addr6p[2], addr6p[3],
		      addr6p[4], addr6p[5], addr6p[6], addr6p[7]);
	      if(debug_level > 1)
		{
		  olsr_printf(1, "\tinet6 addr: %s\n", addr6);
		  olsr_printf(3, "\tScope:");
		}
	      if(scope == scope_in)
		{
		  olsr_printf(3, "IPv6 addr:\n");
		  inet_pton(AF_INET6,addr6,&tmp_sockaddr6);
		  memcpy(&saddr6->sin6_addr, &tmp_sockaddr6, sizeof(struct in6_addr));	  
		  fclose(f);
		  return 1;
		}
	    }
	}
      fclose(f);
    }
  
  return 0;
}
#endif





/**
 *Create a new interf_name struct using a given
 *name and insert it into the interface list.
 *
 *@param name the name of the interface.
 *
 *@return nada
 */
void
queue_if(char *name)
{

  struct if_name *interf_n;

  //printf("Adding interface %s\n", name);

  /* check if the inerfaces already exists */
  interf_n = if_names;
  while(interf_n != NULL)
    {
      if(memcmp(interf_n->name, name, strlen(name)) == 0)
	{
	  fprintf(stderr, "Duplicate interfaces defined... not adding %s\n", name);
	  return;
	}
      interf_n = interf_n->next;
    }

  interf_n = olsr_malloc(sizeof(struct if_name), "queue interface");

  /* strlen () does not return length including terminating /0 */
  interf_n->name = olsr_malloc(strlen(name) + 1, "queue interface name");
  interf_n->configured = 0;
  interf_n->interf = NULL;
  interf_n->index = queued_ifs++;

  strcpy(interf_n->name, name);
  interf_n->next = if_names;
  if_names = interf_n;

}




/**
 *Add an ifchange function. These functions are called on all (non-initial)
 *changes in the interface set.
 *
 *@param
 *
 *@return
 */
int
add_ifchgf(int (*f)(struct interface *, int))
{

  struct ifchgf *new_ifchgf;

  new_ifchgf = olsr_malloc(sizeof(struct ifchgf), "Add ifchgfunction");

  new_ifchgf->next = ifchgf_list;
  new_ifchgf->function = f;

  ifchgf_list = new_ifchgf;

  return 1;
}



/*
 * Remove an ifchange function
 */
int
del_ifchgf(int (*f)(struct interface *, int))
{
  struct ifchgf *tmp_ifchgf, *prev;

  tmp_ifchgf = ifchgf_list;
  prev = NULL;

  while(tmp_ifchgf)
    {
      if(tmp_ifchgf->function == f)
	{
	  /* Remove entry */
	  if(prev == NULL)
	    {
	      ifchgf_list = tmp_ifchgf->next;
	      free(tmp_ifchgf);
	    }
	  else
	    {
	      prev->next = tmp_ifchgf->next;
	      free(tmp_ifchgf);
	    }
	  return 1;
	}
      prev = tmp_ifchgf;
      tmp_ifchgf = tmp_ifchgf->next;
    }

  return 0;
}

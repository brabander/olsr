/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2004 Andreas Tønnesen (andreto@ifi.uio.no)
 *
 * This file is part of the olsr.org OLSR daemon.
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
 * $ Id $
 *
 */


#include "defs.h"
#include "olsr.h"
#include "kernel_routes.h"

#ifdef linux
#include "linux/tunnel.h"
#elif defined WIN32
#include "win32/tunnel.h"
#undef strerror
#define strerror(x) StrError(x)
#else
#       error "Unsupported system"
#endif



int
olsr_init_old_table()
{
  int index;

  for(index=0;index<HASHSIZE;index++)
    {
      old_routes[index].next = &old_routes[index];
      old_routes[index].prev = &old_routes[index];
      old_hna[index].next = &old_hna[index];
      old_hna[index].prev = &old_hna[index];
    }

  return 1;
}

/**
 *Checks if there exists a route to a given host
 *in a given hash table.
 *
 *@param dst the host to check for
 *@param table the table to check
 *
 *@return 1 if the host exists in the table, 0 if not
 */
int
olsr_find_up_route(struct rt_entry *dst, struct rt_entry *table)
{ 
  struct rt_entry *destination;
  olsr_u32_t      hash;
 
  hash = olsr_hashing(&dst->rt_dst);

  for(destination = table[hash].next;
      destination != &table[hash];
      destination = destination->next)
    {
      //printf("Checking %s hc: %d ", olsr_ip_to_string(&dst->rt_dst), dst->rt_metric);
      //printf("vs %s hc: %d ... ", olsr_ip_to_string(&destination->rt_dst), destination->rt_metric);      
      if (COMP_IP(&destination->rt_dst, &dst->rt_dst) &&
	  COMP_IP(&destination->rt_router, &dst->rt_router) &&
	  (destination->rt_if->if_nr == dst->rt_if->if_nr))
	{
	  if(destination->rt_metric == dst->rt_metric)
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }
	}
    }

  return 0;

}


/**
 *Create a list containing the entries in in_table
 *that does not exist in from_table
 *
 *@param from_table the table to use
 *@param in_table the routes already added
 *
 *@return a poiter to a linked list of routes to add
 */
struct destination_n *
olsr_build_update_list(struct rt_entry *from_table,struct rt_entry *in_table)
{
  struct destination_n *kernel_route_list = NULL;
  struct destination_n *route_list = NULL;
  struct rt_entry      *destination;
  olsr_u8_t            index;
  
  for(index=0;index<HASHSIZE;index++)
    {
      for(destination = from_table[index].next;
	  destination != &from_table[index];
	  destination = destination->next)
	{
	  if (!olsr_find_up_route(destination, in_table))
	    {
	      
	      route_list = olsr_malloc(sizeof(struct destination_n), "create route tmp list");
	      
	      route_list->destination = destination;
	      
	      route_list->next = kernel_route_list;
	      kernel_route_list = route_list;
	    }
	}   
    }
  
  return (kernel_route_list);
}





/**
 *Deletes all OLSR routes
 *
 *
 *@return 1
 */
int
olsr_delete_all_kernel_routes()
{ 
  struct destination_n *delete_kernel_list=NULL;
  struct destination_n *tmp=NULL;
  union olsr_ip_addr *tmp_addr;
  olsr_u32_t tmp_tnl_addr;

  olsr_printf(1, "Deleting all routes...\n");

  if(use_tunnel)
    {
      /* Delete Internet GW tunnel */
      delete_tunnel_route();
      /* Take down tunnel */
      del_ip_tunnel(&ipt);
      tmp_tnl_addr = 0;
      set_up_gw_tunnel(&tmp_tnl_addr);
    }

  delete_kernel_list = olsr_build_update_list(hna_routes, old_hna);

  tmp = delete_kernel_list;

  olsr_printf(1, "HNA list:\n");
  while(tmp)
    {
      tmp_addr = &tmp->destination->rt_dst;
      olsr_printf(1, "Dest: %s\n", olsr_ip_to_string(tmp_addr));
      tmp = tmp->next;
    }

  olsr_delete_routes_from_kernel(delete_kernel_list);

  delete_kernel_list = olsr_build_update_list(routingtable,old_routes);

  tmp = delete_kernel_list;

  olsr_printf(1, "Route list:\n");
  while(tmp)
    {
      tmp_addr = &tmp->destination->rt_dst;
      olsr_printf(1, "Dest: %s\n", olsr_ip_to_string(tmp_addr));
      tmp = tmp->next;
    }

  olsr_delete_routes_from_kernel(delete_kernel_list);

  return 1;
}


/**
 *Perform all neccessary actions for an update of the 
 *routes in the kernel.
 *
 *@return nada
 */
void
olsr_update_kernel_routes()
{
  struct destination_n *delete_kernel_list = NULL;
  struct destination_n *add_kernel_list = NULL;
  
  olsr_printf(3, "Updating kernel routes...\n");
  delete_kernel_list = olsr_build_update_list(old_routes, routingtable);
  add_kernel_list = olsr_build_update_list(routingtable, old_routes);
  //#warning deletion and addition of routes swapped in 0.4.7 - TEST!
  olsr_add_routes_in_kernel(add_kernel_list);
  olsr_delete_routes_from_kernel(delete_kernel_list);
}



/**
 *Perform all neccessary actions for an update of the 
 *HNA routes in the kernel.
 *
 *@return nada
 */
void
olsr_update_kernel_hna_routes()
{
  struct destination_n *delete_kernel_list = NULL;
  //struct destination_n *delete_kernel_list2;
  struct destination_n *add_kernel_list = NULL;

  olsr_printf(3, "Updating kernel HNA routes...\n");


  delete_kernel_list = olsr_build_update_list(old_hna, hna_routes);
  add_kernel_list = olsr_build_update_list(hna_routes, old_hna);

  olsr_delete_routes_from_kernel(delete_kernel_list);
  olsr_add_routes_in_kernel(add_kernel_list);
}


/**
 *Create a copy of the routing table and
 *clear the current table
 *
 *@param original the table to move from
 *@param the table to move to
 *
 *@return nada
 */
void
olsr_move_route_table(struct rt_entry *original, struct rt_entry *new)
{
  olsr_16_t index;

  for(index=0;index<HASHSIZE;index++)
    {
      if(original[index].next == &original[index])
	{
	  new[index].next = &new[index];
	  new[index].prev = &new[index];
	}
      else
	{
	  /* Copy to old */
	   new[index].next = original[index].next;
	   new[index].next->prev = &new[index];
	   new[index].prev = original[index].prev;
	   new[index].prev->next = &new[index];

	   /* Clear original */
	   original[index].next = &original[index];
	   original[index].prev = &original[index];
	}
    }
}


/**
 *Delete a linked list of routes from the kernel.
 *
 *@param delete_kernel_list the list to delete
 *
 *@return nada
 */
void 
olsr_delete_routes_from_kernel(struct destination_n *delete_kernel_list)
{
  struct destination_n *destination_kernel;
  olsr_16_t error;

  while(delete_kernel_list!=NULL)
    {
      if(ipversion == AF_INET)
	{
	  /* IPv4 */
	  error = olsr_ioctl_del_route(delete_kernel_list->destination);
	}
      else
	{
	  /* IPv6 */
	  error = olsr_ioctl_del_route6(delete_kernel_list->destination);
	}


      if(error < 0)
	{
	  olsr_printf(1, "Delete route:%s\n", strerror(errno));
	  olsr_syslog(OLSR_LOG_ERR, "Delete route:%m");
	}

      destination_kernel=delete_kernel_list;
      delete_kernel_list=delete_kernel_list->next;
      
      free(destination_kernel);
    }


  
}

/**
 *Add a list of routes to the kernel. Adding
 *is done by hopcount to be sure a route
 *to the nexthop is added.
 *
 *@param add_kernel_list the linked list of routes to add
 *
 *@return nada
 */
void 
olsr_add_routes_in_kernel(struct destination_n *add_kernel_list)
{
	struct destination_n *destination_kernel = NULL;
	struct destination_n *previous_node = add_kernel_list;
	olsr_16_t error;
	int metric_counter = 0, first_run = 1;
	//char str[46];

	//printf("Calculating routes\n");

	while(add_kernel_list != NULL)
	  {
	    //searching for all the items with metric equal to n
	    for(destination_kernel = add_kernel_list; destination_kernel != NULL; )
	      {
		if((destination_kernel->destination->rt_metric == metric_counter) &&
		   ((first_run && 
		     COMP_IP(&destination_kernel->destination->rt_dst, &destination_kernel->destination->rt_router)) || !first_run))
		  {
		    /* First add all 1-hop routes that has themselves as GW */

		    if(ipversion == AF_INET)
		      error=olsr_ioctl_add_route(destination_kernel->destination);
		    else
		      error=olsr_ioctl_add_route6(destination_kernel->destination);
		    
		    if(error < 0) //print the error msg
		      {
			olsr_printf(1, "Add route: %s\n",strerror(errno));
			olsr_syslog(OLSR_LOG_ERR, "Add route:%m");
		      }
		    
		    //getting rid of this node and hooking up the broken point
		    if(destination_kernel == add_kernel_list) 
		      {
			destination_kernel = add_kernel_list->next;
			free(add_kernel_list);
			add_kernel_list = destination_kernel;
			previous_node=add_kernel_list;
		      }
		    else 
		      {
			previous_node->next = destination_kernel->next;
			free(destination_kernel);
			destination_kernel = previous_node->next;
		      }
		  }
		else 
		  {
		    previous_node = destination_kernel;
		    destination_kernel = destination_kernel->next;
		  }
		
	      }
	    if(first_run)
	      first_run = 0;
	    else
	      ++metric_counter;
	  }
	
}




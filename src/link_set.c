/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2003 Andreas Tønnesen (andreto@ifi.uio.no)
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
 * $Id: link_set.c,v 1.9 2004/10/18 13:13:37 kattemat Exp $
 *
 */


/*
 * Link sensing database for the OLSR routing daemon
 */

#include "link_set.h"
#include "hysteresis.h"
#include "mid_set.h"
#include "mpr.h"
#include "olsr.h"
#include "scheduler.h"

#include "link_layer.h"



/* Begin:
 * Prototypes for internal functions 
 */

static int
check_link_status(struct hello_message *);

static void
olsr_time_out_hysteresis(void);

static struct link_entry *
add_new_entry(union olsr_ip_addr *, union olsr_ip_addr *, union olsr_ip_addr *, double, double);

static void
olsr_time_out_link_set(void);

static int
get_neighbor_status(union olsr_ip_addr *);


/* End:
 * Prototypes for internal functions 
 */



void
olsr_init_link_set()
{

  /* Timers */
  olsr_init_timer((olsr_u32_t) (NEIGHB_HOLD_TIME*1000), &hold_time_neighbor);

  olsr_register_timeout_function(&olsr_time_out_link_set);
  if(olsr_cnf->use_hysteresis)
    {
      olsr_register_timeout_function(&olsr_time_out_hysteresis);
    }
  link_set = NULL;
}



/**
 * Get the status of a link. The status is based upon different
 * timeouts in the link entry.
 *
 *@param remote address of the remote interface
 *
 *@return the link status of the link
 */
int
lookup_link_status(struct link_entry *entry)
{

  if(entry == NULL || link_set == NULL)
    {
      return UNSPEC_LINK;
    }

  /*
   * Hysteresis
   */
  if(olsr_cnf->use_hysteresis)
    {
      /*
	if L_LOST_LINK_time is not expired, the link is advertised
	with a link type of LOST_LINK.
      */
      if(!TIMED_OUT(&entry->L_LOST_LINK_time))
	return LOST_LINK;
      /*
	otherwise, if L_LOST_LINK_time is expired and L_link_pending
	is set to "true", the link SHOULD NOT be advertised at all;
      */
      if(entry->L_link_pending == 1)
	{
#ifdef DEBUG
	  olsr_printf(3, "HYST[%s]: Setting to HIDE\n", olsr_ip_to_string(&entry->neighbor_iface_addr));
#endif
	  return HIDE_LINK;
	}
      /*
	otherwise, if L_LOST_LINK_time is expired and L_link_pending
	is set to "false", the link is advertised as described
	previously in section 6.
      */
    }

  if(!TIMED_OUT(&entry->SYM_time))
    return SYM_LINK;

  if(!TIMED_OUT(&entry->ASYM_time))
    return ASYM_LINK;

  return LOST_LINK;


}






/**
 *Find the "best" link status to a
 *neighbor
 *
 *@param address the address to check for
 *
 *@return SYM_LINK if a symmetric link exists 0 if not
 */
static int
get_neighbor_status(union olsr_ip_addr *address)
{
  union olsr_ip_addr *main_addr;
  struct addresses   *aliases;
  struct link_entry  *link;
  struct interface   *ifs;

  //printf("GET_NEIGHBOR_STATUS\n");

  /* Find main address */
  if(!(main_addr = mid_lookup_main_addr(address)))
    main_addr = address;

  //printf("\tmain: %s\n", olsr_ip_to_string(main_addr));

  /* Loop trough local interfaces to check all possebilities */
  for(ifs = ifnet; ifs != NULL; ifs = ifs->int_next)
    {
      //printf("\tChecking %s->", olsr_ip_to_string(&ifs->ip_addr));
      //printf("%s : ", olsr_ip_to_string(main_addr)); 
      if((link = lookup_link_entry(main_addr, &ifs->ip_addr)) != NULL)
	{
	  //printf("%d\n", lookup_link_status(link));
	  if(lookup_link_status(link) == SYM_LINK)
	    return SYM_LINK;
	}
      /* Get aliases */
      for(aliases = mid_lookup_aliases(main_addr);
	  aliases != NULL;
	  aliases = aliases->next)
	{
	  //printf("\tChecking %s->", olsr_ip_to_string(&ifs->ip_addr));
	  //printf("%s : ", olsr_ip_to_string(&aliases->address)); 
	  if((link = lookup_link_entry(&aliases->address, &ifs->ip_addr)) != NULL)
	    {
	      //printf("%d\n", lookup_link_status(link));

	      if(lookup_link_status(link) == SYM_LINK)
		return SYM_LINK;
	    }
	}
    }
  
  return 0;
}




/**
 *Get the remote interface address to use as nexthop
 *to reach the remote host.
 *
 *@param address the address of the remote host
 *@return the nexthop address to use. Returns the pointer
 *passed as arg 1 if nothing is found(if no MID is registered).
 */
union olsr_ip_addr *
get_neighbor_nexthop(union olsr_ip_addr *address)
{
  union olsr_ip_addr *main_addr;
  struct addresses   *aliases;
  struct link_entry  *link;
  struct interface   *ifs;

  //printf("GET_NEIGHBOR_NEXTHOP\n");

  /* Find main address */
  if(!(main_addr = mid_lookup_main_addr(address)))
    main_addr = address;

  //printf("\tmain: %s\n", olsr_ip_to_string(main_addr));

  /* Loop trough local interfaces to check all possebilities */
  for(ifs = ifnet; ifs != NULL; ifs = ifs->int_next)
    {
      //printf("\tChecking %s->", olsr_ip_to_string(&ifs->ip_addr));
      //printf("%s : ", olsr_ip_to_string(main_addr)); 
      if((link = lookup_link_entry(main_addr, &ifs->ip_addr)) != NULL)
	{
	  //printf("%d\n", lookup_link_status(link));
	  if(lookup_link_status(link) == SYM_LINK)
	    return main_addr;
	}
      /* Get aliases */
      for(aliases = mid_lookup_aliases(main_addr);
	  aliases != NULL;
	  aliases = aliases->next)
	{
	  //printf("\tChecking %s->", olsr_ip_to_string(&ifs->ip_addr));
	  //printf("%s : ", olsr_ip_to_string(&aliases->address)); 
	  if((link = lookup_link_entry(&aliases->address, &ifs->ip_addr)) != NULL)
	    {
	      //printf("%d\n", lookup_link_status(link));

	      if(lookup_link_status(link) == SYM_LINK)
		return &aliases->address;
	    }
	}
    }
  
  /* This shoud only happen if not MID addresses for the
   * multi-homed remote host are registered yet
   */
  return address;
}




/**
 *Get the interface to use when setting up
 *a route to a neighbor. The interface with
 *the lowest metric is used.
 *
 *As this function is only called by the route calculation
 *functions it is considered that the caller is responsible
 *for making sure the neighbor is symmetric.
 *Due to experiences of route calculaition queryig for interfaces
 *when no links with a valid SYM time is avalibe, the function
 *will return a possible interface with an expired SYM time
 *if no SYM links were discovered.
 *
 *@param address of the neighbor - does not have to
 *be the main address
 *
 *@return a interface struct representing the interface to use
 */
struct interface *
get_interface_link_set(union olsr_ip_addr *remote)
{
  struct link_entry *tmp_link_set;
  union olsr_ip_addr *remote_addr;
  struct interface *if_to_use, *tmp_if, *backup_if;

  if_to_use = NULL;
  backup_if = NULL;

  if(remote == NULL || link_set == NULL)
    {
      olsr_printf(1, "Get interface: not sane request or empty link set!\n");
      return NULL;
    }

  /* Check for main address of address */
  if((remote_addr = mid_lookup_main_addr(remote)) == NULL)
    remote_addr = remote;

  tmp_link_set = link_set;
  
  while(tmp_link_set)
    {
      //printf("Checking %s vs ", olsr_ip_to_string(&tmp_link_set->neighbor_iface_addr));
      //printf("%s\n", olsr_ip_to_string(addr));
      
      if(COMP_IP(remote_addr, &tmp_link_set->neighbor->neighbor_main_addr) ||
	 COMP_IP(remote_addr, &tmp_link_set->neighbor_iface_addr))
	{

	  tmp_if = if_ifwithaddr(&tmp_link_set->local_iface_addr);

	  /* Must be symmetric link! */
	  if(!TIMED_OUT(&tmp_link_set->SYM_time))
	    {
	      if((if_to_use == NULL) || (if_to_use->int_metric > tmp_if->int_metric))
		if_to_use = tmp_if;
	    }
	  /* Backup solution in case the links have timed out */
	  else
	    {
	      if((if_to_use == NULL) && ((backup_if == NULL) || (backup_if->int_metric > tmp_if->int_metric)))
		{
		  backup_if = tmp_if;
		}
	    }
	}
      
      tmp_link_set = tmp_link_set->next;
    }
  
  /* Not found */
  if(if_to_use == NULL)
    return backup_if;
  
  return if_to_use;
}



/**
 *Nothing mysterious here.
 *Adding a new link entry to the link set.
 *
 *@param local the local IP address
 *@param remote the remote IP address
 *@param remote_main teh remote nodes main address
 *@param vtime the validity time of the entry
 *@param htime the HELLO interval of the remote node
 */

static struct link_entry *
add_new_entry(union olsr_ip_addr *local, union olsr_ip_addr *remote, union olsr_ip_addr *remote_main, double vtime, double htime)
{
  struct link_entry *tmp_link_set, *new_link;
  struct neighbor_entry *neighbor;
#ifndef WIN32
  struct interface *local_if;
#endif

  tmp_link_set = link_set;

  while(tmp_link_set)
    {
      if(COMP_IP(remote, &tmp_link_set->neighbor_iface_addr))
	return tmp_link_set;
      tmp_link_set = tmp_link_set->next;
    }

  /*
   * if there exists no link tuple with
   * L_neighbor_iface_addr == Source Address
   */

#ifdef DEBUG
  olsr_printf(3, "Adding %s to link set\n", olsr_ip_to_string(remote));
#endif

  /* a new tuple is created with... */

  new_link = olsr_malloc(sizeof(struct link_entry), "new link entry");

  /*
   * L_local_iface_addr = Address of the interface
   * which received the HELLO message
   */
  //printf("\tLocal IF: %s\n", olsr_ip_to_string(local));
  COPY_IP(&new_link->local_iface_addr, local);
  /* L_neighbor_iface_addr = Source Address */
  COPY_IP(&new_link->neighbor_iface_addr, remote);

  /* L_SYM_time            = current time - 1 (expired) */
  new_link->SYM_time = now;
  /* Subtract 1 */
  new_link->SYM_time.tv_sec -= 1;

  /* L_time = current time + validity time */
  olsr_get_timestamp((olsr_u32_t) vtime*1000, &new_link->time);


  /* HYSTERESIS */
  if(olsr_cnf->use_hysteresis)
    {
      new_link->L_link_pending = 1;
      olsr_get_timestamp((olsr_u32_t) vtime*1000, &new_link->L_LOST_LINK_time);
      olsr_get_timestamp((olsr_u32_t) htime*1500, &new_link->hello_timeout);
      new_link->last_htime = htime;
      new_link->olsr_seqno = 0;
      new_link->L_link_quality = 0;
    }
  /* Add to queue */
  new_link->next = link_set;
  link_set = new_link;


  /*
   * Create the neighbor entry
   */

  /* Neighbor MUST exist! */
  if(NULL == (neighbor = olsr_lookup_neighbor_table(remote_main)))
    {
      neighbor = olsr_insert_neighbor_table(remote_main);
      /* Copy the main address */
      COPY_IP(&neighbor->neighbor_main_addr, remote_main);
#ifdef DEBUG
      olsr_printf(3, "ADDING NEW NEIGHBOR ENTRY %s FROM LINK SET\n", olsr_ip_to_string(remote_main));
#endif
    }

  neighbor->linkcount++;


  new_link->neighbor = neighbor;

  if(!COMP_IP(remote, remote_main))
    {
      /* Add MID alias if not already registered */
      /* This is kind of sketchy... and not specified
       * in the RFC. We can only guess a vtime.
       * We'll go for one that is hopefully long
       * enough in most cases. 20 seconds
       */
      olsr_printf(1, "Adding MID alias main %s ", olsr_ip_to_string(remote_main));
      olsr_printf(1, "-> %s based on HELLO\n\n", olsr_ip_to_string(remote));
      insert_mid_alias(remote_main, remote, 20.0);
    }

  /* Add to link-layer spy list */
#ifndef WIN32
  if(llinfo)
    {
      local_if = if_ifwithaddr(local);
      
      olsr_printf(1, "Adding %s to spylist of interface %s\n", olsr_ip_to_string(remote), local_if->int_name);

      if((local_if != NULL) && (add_spy_node(remote, local_if->int_name)))
	new_link->spy_activated = 1;
    }
#endif

  return link_set;
}


/**
 *Lookup the status of a link.
 *
 *@param int_addr address of the remote interface
 *
 *@return 1 of the link is symmertic 0 if not
 */

int
check_neighbor_link(union olsr_ip_addr *int_addr)
{
  struct link_entry *tmp_link_set;

  tmp_link_set = link_set;

  while(tmp_link_set)
    {
      if(COMP_IP(int_addr, &tmp_link_set->neighbor_iface_addr))
	return lookup_link_status(tmp_link_set);
      tmp_link_set = tmp_link_set->next;
    }
  return UNSPEC_LINK;
}


/**
 *Lookup a link entry
 *
 *@param remote the remote interface address
 *@param local the local interface address
 *
 *@return the link entry if found, NULL if not
 */
struct link_entry *
lookup_link_entry(union olsr_ip_addr *remote, union olsr_ip_addr *local)
{
  struct link_entry *tmp_link_set;

  tmp_link_set = link_set;

  while(tmp_link_set)
    {
      if(COMP_IP(remote, &tmp_link_set->neighbor_iface_addr) &&
	 COMP_IP(local, &tmp_link_set->local_iface_addr))
	return tmp_link_set;
      tmp_link_set = tmp_link_set->next;
    }
  return NULL;

}







/**
 *Update a link entry. This is the "main entrypoint" in
 *the link-sensing. This function is calles from the HELLO
 *parser function.
 *It makes sure a entry is updated or created.
 *
 *@param local the local IP address
 *@param remote the remote IP address
 *@param message the HELLO message
 *@param in_if the interface on which this HELLO was received
 *
 *@return the link_entry struct describing this link entry
 */
struct link_entry *
update_link_entry(union olsr_ip_addr *local, union olsr_ip_addr *remote, struct hello_message *message, struct interface *in_if)
{
  int status;
  struct link_entry *entry;
#ifndef WIN32
  struct interface *local_if;
#endif

  /* Time out entries */
  //timeout_link_set();

  /* Add if not registered */
  entry = add_new_entry(local, remote, &message->source_addr, message->vtime, message->htime);

  /* Update link layer info */
  /* Add to link-layer spy list */
#ifndef WIN32
  if(llinfo && !entry->spy_activated)
    {
      local_if = if_ifwithaddr(local);
      
      olsr_printf(1, "Adding %s to spylist of interface %s\n", olsr_ip_to_string(remote), local_if->int_name);

      if((local_if != NULL) && (add_spy_node(remote, local_if->int_name)))
	entry->spy_activated = 1;
    }
#endif

  /* Update ASYM_time */
  //printf("Vtime is %f\n", message->vtime);
  /* L_ASYM_time = current time + validity time */
  olsr_get_timestamp((olsr_u32_t) (message->vtime*1000), &entry->ASYM_time);


  status = check_link_status(message);

  //printf("Status %d\n", status);

  switch(status)
    {
    case(LOST_LINK):
      /* L_SYM_time = current time - 1 (i.e., expired) */
      entry->SYM_time = now;
      entry->SYM_time.tv_sec -= 1;

      break;
    case(SYM_LINK):
    case(ASYM_LINK):
      /* L_SYM_time = current time + validity time */
      //printf("updating SYM time for %s\n", olsr_ip_to_string(remote));
      olsr_get_timestamp((olsr_u32_t) (message->vtime*1000), &entry->SYM_time);
	//timeradd(&now, &tmp_timer, &entry->SYM_time);

      /* L_time = L_SYM_time + NEIGHB_HOLD_TIME */
      timeradd(&entry->SYM_time, &hold_time_neighbor, &entry->time);

      break;
    default:;
    }

  /* L_time = max(L_time, L_ASYM_time) */
  if(timercmp(&entry->time, &entry->ASYM_time, <))
    entry->time = entry->ASYM_time;


  /*
  printf("Updating link LOCAL: %s ", olsr_ip_to_string(local));
  printf("REMOTE: %s\n", olsr_ip_to_string(remote));
  printf("VTIME: %f ", message->vtime);
  printf("STATUS: %d\n", status);
  */

  /* Update hysteresis values */
  if(olsr_cnf->use_hysteresis)
    olsr_process_hysteresis(entry);

  /* update neighbor status */
  /* Return link status */
  //status = lookup_link_status(entry);
  /* UPDATED ! */
  status = get_neighbor_status(remote);

  /* Update neighbor */
  update_neighbor_status(entry->neighbor, status);
  //update_neighbor_status(entry->neighbor);

  return entry;  
}


/**
 * Fuction that updates all registered pointers to
 * one neighbor entry with another pointer
 * Used by MID updates.
 *
 *@old the pointer to replace
 *@new the pointer to use instead of "old"
 *
 *@return the number of entries updated
 */
int
replace_neighbor_link_set(struct neighbor_entry *old,
			  struct neighbor_entry *new)
{
  struct link_entry *tmp_link_set, *last_link_entry;
  int retval;

  retval = 0;

  if(link_set == NULL)
    return retval;
      
  tmp_link_set = link_set;
  last_link_entry = NULL;

  while(tmp_link_set)
    {

      if(tmp_link_set->neighbor == old)
	{
	  tmp_link_set->neighbor = new;
	  retval++;
	}
      tmp_link_set = tmp_link_set->next;
    }

  return retval;

}


/**
 *Checks the link status to a neighbor by
 *looking in a received HELLO message.
 *
 *@param message the HELLO message to check
 *
 *@return the link status
 */
static int
check_link_status(struct hello_message *message)
{

  struct hello_neighbor  *neighbors;
  struct interface *ifd;

  neighbors = message->neighbors;
  
  while(neighbors!=NULL)
    {  
      //printf("(linkstatus)Checking %s ",olsr_ip_to_string(&neighbors->address));
      //printf("against %s\n",olsr_ip_to_string(&main_addr));

      /* Check all interfaces */	  
      for (ifd = ifnet; ifd ; ifd = ifd->int_next) 
	{
	  if(COMP_IP(&neighbors->address, &ifd->ip_addr))
	    {
	      //printf("ok");
	      return neighbors->link;
	    }
	}

      neighbors = neighbors->next; 
    }


  return UNSPEC_LINK;
}


/**
 *Time out the link set. In other words, the link
 *set is traversed and all non-valid entries are
 *deleted.
 *
 */
static void
olsr_time_out_link_set()
{

  struct link_entry *tmp_link_set, *last_link_entry;

  if(link_set == NULL)
    return;
      
  tmp_link_set = link_set;
  last_link_entry = NULL;

  while(tmp_link_set)
    {

      if(TIMED_OUT(&tmp_link_set->time))
	{
	  if(last_link_entry != NULL)
	    {
	      last_link_entry->next = tmp_link_set->next;

	      /* Delete neighbor entry */
	      if(tmp_link_set->neighbor->linkcount == 1)
		olsr_delete_neighbor_table(&tmp_link_set->neighbor->neighbor_main_addr);
	      else
		tmp_link_set->neighbor->linkcount--;

	      //olsr_delete_neighbor_if_no_link(&tmp_link_set->neighbor->neighbor_main_addr);
	      changes_neighborhood = UP;

	      free(tmp_link_set);
	      tmp_link_set = last_link_entry;
	    }
	  else
	    {
	      link_set = tmp_link_set->next; /* CHANGED */

	      /* Delete neighbor entry */
	      if(tmp_link_set->neighbor->linkcount == 1)
		olsr_delete_neighbor_table(&tmp_link_set->neighbor->neighbor_main_addr);
	      else
		tmp_link_set->neighbor->linkcount--;
	      //olsr_delete_neighbor_if_no_link(&tmp_link_set->neighbor->neighbor_main_addr);

	      changes_neighborhood = UP;

	      free(tmp_link_set);
	      tmp_link_set = link_set;
	      continue;
	    }	    
	}
      
      last_link_entry = tmp_link_set;
      tmp_link_set = tmp_link_set->next;
    }

  return;
}




/**
 *Updates links that we have not received
 *HELLO from in expected time according to 
 *hysteresis.
 *
 *@return nada
 */
static void
olsr_time_out_hysteresis()
{

  struct link_entry *tmp_link_set;
  int status;

  if(link_set == NULL)
    return;

  tmp_link_set = link_set;

  while(tmp_link_set)
    {
      if(TIMED_OUT(&tmp_link_set->hello_timeout))
	{
	  tmp_link_set->L_link_quality = olsr_hyst_calc_instability(tmp_link_set->L_link_quality);
	  olsr_printf(1, "HYST[%s] HELLO timeout %0.3f\n", olsr_ip_to_string(&tmp_link_set->neighbor_iface_addr), tmp_link_set->L_link_quality);
	  /* Update hello_timeout - NO SLACK THIS TIME */
	  olsr_get_timestamp((olsr_u32_t) tmp_link_set->last_htime*1000, &tmp_link_set->hello_timeout);

	  /* Recalculate status */
	  /* Update hysteresis values */
	  olsr_process_hysteresis(tmp_link_set);
	  
	  /* update neighbor status */
	  //status = lookup_link_status(tmp_link_set);
	  /* UPDATED ! */
	  status = get_neighbor_status(&tmp_link_set->neighbor_iface_addr);


	  /* Update neighbor */
	  update_neighbor_status(tmp_link_set->neighbor, status);
	  //update_neighbor_status(tmp_link_set->neighbor);

	  /* Update seqno - not mentioned in the RFC... kind of a hack.. */
	  tmp_link_set->olsr_seqno++;
	}
      tmp_link_set = tmp_link_set->next;
    }

  return;
}


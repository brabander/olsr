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
 */


/*
 * Link sensing database for the OLSR routing daemon
 */

#include "defs.h"
#include "link_set.h"
#include "hysteresis.h"
#include "mid_set.h"
#include "mpr.h"
#include "neighbor_table.h"
#include "olsr.h"
#include "scheduler.h"
#include "lq_route.h"
#include "net_olsr.h"
#include "ipcalc.h"


/* head node for all link sets */
struct list_node link_entry_head;

olsr_bool link_changes; /* is set if changes occur in MPRS set */ 

void
signal_link_changes(olsr_bool val) /* XXX ugly */
{
  link_changes = val;
}

static int
check_link_status(const struct hello_message *message, const struct interface *in_if);

static struct link_entry *
add_link_entry(const union olsr_ip_addr *, const union olsr_ip_addr *, const union olsr_ip_addr *, double, double, const struct interface *);

static int
get_neighbor_status(const union olsr_ip_addr *);

void
olsr_init_link_set(void)
{

  /* Init list head */
  list_head_init(&link_entry_head);
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
lookup_link_status(const struct link_entry *entry)
{

  if(entry == NULL || list_is_empty(&link_entry_head)) {
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

      if(!TIMED_OUT(entry->L_LOST_LINK_time))
	return LOST_LINK;
      /*
	otherwise, if L_LOST_LINK_time is expired and L_link_pending
	is set to "true", the link SHOULD NOT be advertised at all;
      */
      if(entry->L_link_pending == 1)
	{
#ifndef NODEBUG
          struct ipaddr_str buf;
	  OLSR_PRINTF(3, "HYST[%s]: Setting to HIDE\n", olsr_ip_to_string(&buf, &entry->neighbor_iface_addr));
#endif
	  return HIDE_LINK;
	}
      /*
	otherwise, if L_LOST_LINK_time is expired and L_link_pending
	is set to "false", the link is advertised as described
	previously in section 6.
      */
    }

  if (entry->link_sym_timer) {
    return SYM_LINK;
  }

  if(!TIMED_OUT(entry->ASYM_time))
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
get_neighbor_status(const union olsr_ip_addr *address)
{
  const union olsr_ip_addr *main_addr;
  struct interface   *ifs;

  //printf("GET_NEIGHBOR_STATUS\n");

  /* Find main address */
  if(!(main_addr = mid_lookup_main_addr(address)))
    main_addr = address;

  //printf("\tmain: %s\n", olsr_ip_to_string(main_addr));

  /* Loop trough local interfaces to check all possebilities */
  for(ifs = ifnet; ifs != NULL; ifs = ifs->int_next)
    {
      struct mid_address   *aliases;
      struct link_entry  *lnk = lookup_link_entry(main_addr, NULL, ifs);

      //printf("\tChecking %s->", olsr_ip_to_string(&ifs->ip_addr));
      //printf("%s : ", olsr_ip_to_string(main_addr)); 
      if(lnk != NULL)
	{
	  //printf("%d\n", lookup_link_status(link));
	  if(lookup_link_status(lnk) == SYM_LINK)
	    return SYM_LINK;
	}
      /* Get aliases */
      for(aliases = mid_lookup_aliases(main_addr);
	  aliases != NULL;
	  aliases = aliases->next_alias)
	{
	  //printf("\tChecking %s->", olsr_ip_to_string(&ifs->ip_addr));
	  //printf("%s : ", olsr_ip_to_string(&aliases->address)); 
            lnk = lookup_link_entry(&aliases->alias, NULL, ifs);
            if(lnk != NULL)
	    {
	      //printf("%d\n", lookup_link_status(link));

	      if(lookup_link_status(lnk) == SYM_LINK)
		return SYM_LINK;
	    }
	}
    }
  
  return 0;
}

/**
 * Find best link to a neighbor
 */

struct link_entry *
get_best_link_to_neighbor(const union olsr_ip_addr *remote)
{
  const union olsr_ip_addr *main_addr;
  struct link_entry *walker, *good_link, *backup_link;
  int curr_metric = MAX_IF_METRIC;
#ifdef USE_FPM
  fpm curr_lq = itofpm(-1);
#else
  float curr_lq = -1.0;
#endif
  
  // main address lookup

  main_addr = mid_lookup_main_addr(remote);

  // "remote" *already is* the main address

  if (main_addr == NULL)
    main_addr = remote;

  // we haven't selected any links, yet

  good_link = NULL;
  backup_link = NULL;

  // loop through all links that we have

  OLSR_FOR_ALL_LINK_ENTRIES(walker) {

    /* if this is not a link to the neighour in question, skip */
    if (!ipequal(&walker->neighbor->neighbor_main_addr, main_addr))
      continue;

    // handle the non-LQ, RFC-compliant case

    if (olsr_cnf->lq_level == 0)
    {
      struct interface *tmp_if;

      // find the interface for the link - we select the link with the
      // best local interface metric
      tmp_if = walker->if_name ? if_ifwithname(walker->if_name) :
              if_ifwithaddr(&walker->local_iface_addr);

      if(!tmp_if)
	continue;

      // is this interface better than anything we had before?

      if ((tmp_if->int_metric < curr_metric) ||
          // use the requested remote interface address as a tie-breaker
          ((tmp_if->int_metric == curr_metric) && 
           ipequal(&walker->local_iface_addr, remote)))
      {
        // memorize the interface's metric

        curr_metric = tmp_if->int_metric;

        // prefer symmetric links over asymmetric links

        if (lookup_link_status(walker) == SYM_LINK)
          good_link = walker;

        else
          backup_link = walker;
      }
    }

    // handle the LQ, non-RFC compliant case

    else
    {
#ifdef USE_FPM
      fpm tmp_lq;
#else
      float tmp_lq;
#endif

      // calculate the bi-directional link quality - we select the link
      // with the best link quality

#ifdef USE_FPM
      tmp_lq = fpmmul(walker->loss_link_quality, walker->neigh_link_quality);
#else
      tmp_lq = walker->loss_link_quality * walker->neigh_link_quality;
#endif

      // is this link better than anything we had before?
	      
      if((tmp_lq > curr_lq) ||
         // use the requested remote interface address as a tie-breaker
         ((tmp_lq == curr_lq) && ipequal(&walker->local_iface_addr, remote)))
      {
        // memorize the link quality

        curr_lq = tmp_lq;

        // prefer symmetric links over asymmetric links

        if(lookup_link_status(walker) == SYM_LINK)
          good_link = walker;

        else
          backup_link = walker;
      }
    }
  } OLSR_FOR_ALL_LINK_ENTRIES_END(walker);

  // if we haven't found any symmetric links, try to return an
  // asymmetric link

  return good_link ? good_link : backup_link;
}

static void set_loss_link_multiplier(struct link_entry *entry)
{
  struct interface *inter;
  struct olsr_if *cfg_inter;
  struct olsr_lq_mult *mult;
#ifdef USE_FPM
  fpm val = itofpm(-1);
#else
  float val = -1.0;
#endif
  union olsr_ip_addr null_addr;

  // find the interface for the link

  inter = if_ifwithaddr(&entry->local_iface_addr);

  // find the interface configuration for the interface

  for (cfg_inter = olsr_cnf->interfaces; cfg_inter != NULL;
       cfg_inter = cfg_inter->next)
    if (cfg_inter->interf == inter)
      break;

  // create a null address for comparison

  memset(&null_addr, 0, sizeof (union olsr_ip_addr));

  // loop through the multiplier entries

  for (mult = cfg_inter->cnf->lq_mult; mult != NULL; mult = mult->next)
  {
    // use the default multiplier only if there isn't any entry that
    // has a matching IP address

#ifdef USE_FPM
    if ((ipequal(&mult->addr, &null_addr) && val < itofpm(0)) ||
#else
    if ((ipequal(&mult->addr, &null_addr) && val < 0.0) ||
#endif
        ipequal(&mult->addr, &entry->neighbor_iface_addr))
#ifdef USE_FPM
      val = ftofpm(mult->val);
#else
      val = mult->val;
#endif
  }

  // if we have not found an entry, then use the default multiplier

#ifdef USE_FPM
  if (val < itofpm(0))
    val = itofpm(1);
#else
  if (val < 0)
    val = 1.0;
#endif

  // store the multiplier

  entry->loss_link_multiplier = val;
}

/*
 * Delete, unlink and free a link entry.
 */
static void
olsr_delete_link_entry(struct link_entry *link)
{

  /* Delete neighbor entry */
  if (link->neighbor->linkcount == 1) {
    olsr_delete_neighbor_table(&link->neighbor->neighbor_main_addr);
  } else {
    link->neighbor->linkcount--;
  }

  /* Kill running timers */
  olsr_stop_timer(link->link_timer);
  link->link_timer = NULL;
  olsr_stop_timer(link->link_sym_timer);
  link->link_sym_timer = NULL;
  olsr_stop_timer(link->link_hello_timer);
  link->link_hello_timer = NULL;
  olsr_stop_timer(link->link_loss_timer);
  link->link_loss_timer = NULL;

  list_remove(&link->link_list);

  free(link->if_name);
  free(link);

  changes_neighborhood = OLSR_TRUE;
}

void
olsr_delete_link_entry_by_ip(const union olsr_ip_addr *int_addr)
{
  struct link_entry *link;

  if (list_is_empty(&link_entry_head)) {
    return;
  }

  OLSR_FOR_ALL_LINK_ENTRIES(link) {
    if (ipequal(int_addr, &link->local_iface_addr)) {
      olsr_delete_link_entry(link);
    }
  } OLSR_FOR_ALL_LINK_ENTRIES_END(link);
}

/**
 * Callback for the link loss timer.
 */
static void
olsr_expire_link_loss_timer(void *context)
{
  struct link_entry *link;

  link = (struct link_entry *)context;

  /* count the lost packet */
  olsr_update_packet_loss_worker(link, OLSR_TRUE);

  /*
   * memorize that we've counted the packet, so that we do not
   * count it a second time later.
   */
  link->loss_missed_hellos++;

  /* next timeout in 1.0 x htime */
  olsr_change_timer(link->link_loss_timer, link->loss_hello_int * MSEC_PER_SEC,
                    OLSR_LINK_LOSS_JITTER, OLSR_TIMER_PERIODIC);
}

/**
 * Callback for the link SYM timer.
 */
static void
olsr_expire_link_sym_timer(void *context)
{
  struct link_entry *link;

  link = (struct link_entry *)context;
  link->link_sym_timer = NULL; /* be pedandic */

  if (link->prev_status != SYM_LINK) {
    return;
  } 

  link->prev_status = lookup_link_status(link);
  update_neighbor_status(link->neighbor,
                         get_neighbor_status(&link->neighbor_iface_addr));
  changes_neighborhood = OLSR_TRUE;
}

/**
 * Callback for the link_hello timer.
 */
void
olsr_expire_link_hello_timer(void *context)
{
#ifndef NODEBUG
  struct ipaddr_str buf;
#endif
  struct link_entry *link;

  link = (struct link_entry *)context;

  link->L_link_quality = olsr_hyst_calc_instability(link->L_link_quality);

  OLSR_PRINTF(1, "HYST[%s] HELLO timeout %s\n",
              olsr_ip_to_string(&buf, &link->neighbor_iface_addr),
              olsr_etx_to_string(link->L_link_quality));

  /* Update hello_timeout - NO SLACK THIS TIME */
  olsr_change_timer(link->link_hello_timer, link->last_htime * MSEC_PER_SEC,
                    OLSR_LINK_JITTER, OLSR_TIMER_PERIODIC);

  /* Update hysteresis values */
  olsr_process_hysteresis(link);
	  
  /* update neighbor status */
  update_neighbor_status(link->neighbor,
                         get_neighbor_status(&link->neighbor_iface_addr));

  /* Update seqno - not mentioned in the RFC... kind of a hack.. */
  link->olsr_seqno++;
}

/**
 * Callback for the link timer.
 */
static void
olsr_expire_link_entry(void *context)
{
  struct link_entry *link;

  link = (struct link_entry *)context;
  link->link_timer = NULL; /* be pedandic */

  olsr_delete_link_entry(link);
}

/**
 * Set the link expiration timer.
 */
void
olsr_set_link_timer(struct link_entry *link, unsigned int rel_timer)
{
  olsr_set_timer(&link->link_timer, rel_timer, OLSR_LINK_JITTER,
                 OLSR_TIMER_ONESHOT, &olsr_expire_link_entry, link, 0);
}

/**
 *Nothing mysterious here.
 *Adding a new link entry to the link set.
 *
 *@param local the local IP address
 *@param remote the remote IP address
 *@param remote_main the remote nodes main address
 *@param vtime the validity time of the entry
 *@param htime the HELLO interval of the remote node
 *@param local_if the local interface
 */

static struct link_entry *
add_link_entry(const union olsr_ip_addr *local,
               const union olsr_ip_addr *remote,
               const union olsr_ip_addr *remote_main,
               double vtime,
               double htime,
               const struct interface *local_if)
{
  struct link_entry *new_link;
  struct neighbor_entry *neighbor;
  struct link_entry *tmp_link_set = lookup_link_entry(remote, remote_main, local_if);
  if (tmp_link_set) {
    return tmp_link_set;
  }

  /*
   * if there exists no link tuple with
   * L_neighbor_iface_addr == Source Address
   */

#ifdef DEBUG
  {
#ifndef NODEBUG
    struct ipaddr_str localbuf, rembuf;
#endif
    OLSR_PRINTF(1, "Adding %s=>%s to link set\n", olsr_ip_to_string(&localbuf, local), olsr_ip_to_string(&rembuf, remote));
  }
#endif

  /* a new tuple is created with... */

  new_link = olsr_malloc(sizeof(struct link_entry), "new link entry");

  memset(new_link, 0 , sizeof(struct link_entry));
  
  /* copy if_name, if it is defined */
  if (local_if->int_name)
    {
      new_link->if_name = olsr_malloc(strlen(local_if->int_name)+1, "target of if_name in new link entry");
      strcpy(new_link->if_name, local_if->int_name);
    } else 
      new_link->if_name = NULL;

  /* shortcut to interface. XXX refcount */
  new_link->inter = local_if;

  /*
   * L_local_iface_addr = Address of the interface
   * which received the HELLO message
   */
  //printf("\tLocal IF: %s\n", olsr_ip_to_string(local));
  new_link->local_iface_addr = *local;
  /* L_neighbor_iface_addr = Source Address */
  new_link->neighbor_iface_addr = *remote;

  /* L_time = current time + validity time */
  olsr_set_link_timer(new_link, vtime * MSEC_PER_SEC);

  new_link->prev_status = ASYM_LINK;

  /* HYSTERESIS */
  if(olsr_cnf->use_hysteresis)
    {
      new_link->L_link_pending = 1;
      new_link->L_LOST_LINK_time = GET_TIMESTAMP(vtime*1000);
      olsr_update_hysteresis_hello(new_link, htime);
      new_link->last_htime = htime;
      new_link->olsr_seqno = 0;
      new_link->olsr_seqno_valid = OLSR_FALSE;
    }

#ifdef USE_FPM
  new_link->L_link_quality = itofpm(0);
#else
  new_link->L_link_quality = 0.0;
#endif

  if (olsr_cnf->lq_level > 0)
    {
      new_link->loss_hello_int = htime;

      olsr_set_timer(&new_link->link_loss_timer, htime * 1500,
                     OLSR_LINK_LOSS_JITTER, OLSR_TIMER_PERIODIC,
                     &olsr_expire_link_loss_timer, new_link, 0);

      new_link->loss_seqno = 0;
      new_link->loss_seqno_valid = 0;
      new_link->loss_missed_hellos = 0;

      new_link->lost_packets = 0;
      new_link->total_packets = 0;

      new_link->loss_index = 0;

      memset(new_link->loss_bitmap, 0, sizeof (new_link->loss_bitmap));

      set_loss_link_multiplier(new_link);
    }

#ifdef USE_FPM
  new_link->loss_link_quality = itofpm(0);
  new_link->neigh_link_quality = itofpm(0);

  new_link->loss_link_quality2 = itofpm(0);
  new_link->neigh_link_quality2 = itofpm(0);

  new_link->saved_loss_link_quality = itofpm(0);
  new_link->saved_neigh_link_quality = itofpm(0);
#else
  new_link->loss_link_quality = 0.0;
  new_link->neigh_link_quality = 0.0;

  new_link->loss_link_quality2 = 0.0;
  new_link->neigh_link_quality2 = 0.0;

  new_link->saved_loss_link_quality = 0.0;
  new_link->saved_neigh_link_quality = 0.0;
#endif

  /* Add to queue */
  list_add_before(&link_entry_head, &new_link->link_list);

  /*
   * Create the neighbor entry
   */

  /* Neighbor MUST exist! */
  neighbor = olsr_lookup_neighbor_table(remote_main);
  if(neighbor == NULL)
    {
#ifdef DEBUG
#ifndef NODEBUG
      struct ipaddr_str buf;
#endif
      OLSR_PRINTF(3, "ADDING NEW NEIGHBOR ENTRY %s FROM LINK SET\n", olsr_ip_to_string(&buf, remote_main));
#endif
      neighbor = olsr_insert_neighbor_table(remote_main);
    }

  neighbor->linkcount++;
  new_link->neighbor = neighbor;

  return new_link;
}


/**
 * Lookup the status of a link.
 *
 * @param int_addr address of the remote interface
 * @return 1 of the link is symmertic 0 if not
 */
int
check_neighbor_link(const union olsr_ip_addr *int_addr)
{
  struct link_entry *link;

  OLSR_FOR_ALL_LINK_ENTRIES(link) {
    if (ipequal(int_addr, &link->neighbor_iface_addr))
      return lookup_link_status(link);
  } OLSR_FOR_ALL_LINK_ENTRIES_END(link);

  return UNSPEC_LINK;
}


/**
 *Lookup a link entry
 *
 *@param remote the remote interface address
 *@param remote_main the remote nodes main address
 *@param local the local interface address
 *
 *@return the link entry if found, NULL if not
 */
struct link_entry *
lookup_link_entry(const union olsr_ip_addr *remote,
                  const union olsr_ip_addr *remote_main,
                  const struct interface *local)
{
  struct link_entry *link;

  OLSR_FOR_ALL_LINK_ENTRIES(link) {
    if(ipequal(remote, &link->neighbor_iface_addr) &&
       (link->if_name ? !strcmp(link->if_name, local->int_name)
        : ipequal(&local->ip_addr, &link->local_iface_addr)) &&

       /* check the remote-main address only if there is one given */
       (!remote_main || ipequal(remote_main, &link->neighbor->neighbor_main_addr))) {
      return link;
    }
  } OLSR_FOR_ALL_LINK_ENTRIES_END(link);

  return NULL;
}







/**
 *Update a link entry. This is the "main entrypoint" in
 *the link-sensing. This function is called from the HELLO
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
update_link_entry(const union olsr_ip_addr *local, 
		  const union olsr_ip_addr *remote, 
		  const struct hello_message *message, 
		  const struct interface *in_if)
{
  struct link_entry *entry;

  /* Add if not registered */
  entry = add_link_entry(local, remote, &message->source_addr, message->vtime, message->htime, in_if);

  /* Update ASYM_time */
  //printf("Vtime is %f\n", message->vtime);
  /* L_ASYM_time = current time + validity time */
  entry->vtime = message->vtime;
  entry->ASYM_time = GET_TIMESTAMP(message->vtime*1000);
  
  entry->prev_status = check_link_status(message, in_if);
  
  //printf("Status %d\n", status);
  
  switch(entry->prev_status)
    {
    case(LOST_LINK):
      olsr_stop_timer(entry->link_sym_timer);
      entry->link_sym_timer = NULL;
      break;
    case(SYM_LINK):
    case(ASYM_LINK):
      /* L_SYM_time = current time + validity time */
      olsr_set_timer(&entry->link_sym_timer, message->vtime * MSEC_PER_SEC,
                     OLSR_LINK_SYM_JITTER, OLSR_TIMER_ONESHOT,
                     &olsr_expire_link_sym_timer, entry, 0);

      /* L_time = L_SYM_time + NEIGHB_HOLD_TIME */
      olsr_set_link_timer(entry, (message->vtime + NEIGHB_HOLD_TIME) *
                          MSEC_PER_SEC);
      break;
    default:;
    }

  /* L_time = max(L_time, L_ASYM_time) */
  if(entry->link_timer && (entry->link_timer->timer_clock < entry->ASYM_time)) {
    olsr_set_link_timer(entry, TIME_DUE(entry->ASYM_time));
  }


  /*
  printf("Updating link LOCAL: %s ", olsr_ip_to_string(local));
  printf("REMOTE: %s\n", olsr_ip_to_string(remote));
  printf("VTIME: %f ", message->vtime);
  printf("STATUS: %d\n", status);
  */

  /* Update hysteresis values */
  if(olsr_cnf->use_hysteresis)
    olsr_process_hysteresis(entry);

  /* Update neighbor */
  update_neighbor_status(entry->neighbor, get_neighbor_status(remote));

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
replace_neighbor_link_set(const struct neighbor_entry *old,
			  struct neighbor_entry *new)
{
  struct link_entry *link;
  int retval = 0;

  if (list_is_empty(&link_entry_head)) {
    return retval;
  }
      
  OLSR_FOR_ALL_LINK_ENTRIES(link) {

    if (link->neighbor == old) {
      link->neighbor = new;
      retval++;
    }
  } OLSR_FOR_ALL_LINK_ENTRIES_END(link);

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
check_link_status(const struct hello_message *message, const struct interface *in_if)
{
  int ret = UNSPEC_LINK;
  struct hello_neighbor  *neighbors;

  neighbors = message->neighbors;
  
  while(neighbors!=NULL)
    {
      /*
       * Note: If a neigh has 2 cards we can reach, the neigh
       * will send a Hello with the same IP mentined twice
       */
      if(ipequal(&neighbors->address, &in_if->ip_addr))
        {
	  //printf("ok");
	  ret = neighbors->link;
	  if (SYM_LINK == ret) break;
	}

      neighbors = neighbors->next; 
    }


  return ret;
}

void olsr_print_link_set(void)
{
#ifndef NODEBUG
  /* The whole function makes no sense without it. */
  struct link_entry *walker;
  const int addrsize = olsr_cnf->ip_version == AF_INET ? 15 : 39;

  OLSR_PRINTF(0, "\n--- %s ---------------------------------------------------- LINKS\n\n",
              olsr_wallclock_string());
  OLSR_PRINTF(1, "%-*s  %-6s %-6s %-6s %-6s %-6s %s\n", addrsize,
              "IP address", "hyst", "LQ", "lost", "total","NLQ", "ETX");

  OLSR_FOR_ALL_LINK_ENTRIES(walker) {

    struct ipaddr_str buf;
#ifdef USE_FPM
    fpm etx;

    if (walker->loss_link_quality < MIN_LINK_QUALITY || walker->neigh_link_quality < MIN_LINK_QUALITY)
      etx = itofpm(0);
    else
      etx = fpmdiv(itofpm(1), fpmmul(walker->loss_link_quality, walker->neigh_link_quality));
#else
    float etx;

    if (walker->loss_link_quality < MIN_LINK_QUALITY || walker->neigh_link_quality < MIN_LINK_QUALITY)
      etx = 0.0;
    else
      etx = 1.0 / (walker->loss_link_quality * walker->neigh_link_quality);
#endif

    OLSR_PRINTF(1, "%-*s  %s  %s  %-3d    %-3d    %s  %s\n",
                addrsize, olsr_ip_to_string(&buf, &walker->neighbor_iface_addr),
                olsr_etx_to_string(walker->L_link_quality),
                olsr_etx_to_string(walker->loss_link_quality),
		walker->lost_packets,
                walker->total_packets,
		olsr_etx_to_string(walker->neigh_link_quality),
                olsr_etx_to_string(etx));
  } OLSR_FOR_ALL_LINK_ENTRIES_END(walker);
#endif
}

void olsr_update_packet_loss_worker(struct link_entry *entry, olsr_bool lost)
{
  unsigned char mask = 1 << (entry->loss_index & 7);
  const int idx = entry->loss_index >> 3;
#ifdef USE_FPM
  fpm rel_lq, saved_lq;
#else
  double rel_lq, saved_lq;
#endif

  if (!lost)
    {
      // packet not lost

      if ((entry->loss_bitmap[idx] & mask) != 0)
        {
          // but the packet that we replace was lost
          // => decrement packet loss

          entry->loss_bitmap[idx] &= ~mask;
          entry->lost_packets--;
        }
    }

  else
    {
      // packet lost

      if ((entry->loss_bitmap[idx] & mask) == 0)
        {
          // but the packet that we replace was not lost
          // => increment packet loss

          entry->loss_bitmap[idx] |= mask;
          entry->lost_packets++;
        }
    }

  // move to the next packet

  entry->loss_index++;

  // wrap around at the end of the packet loss window

  if (entry->loss_index >= olsr_cnf->lq_wsize)
    entry->loss_index = 0;

  // count the total number of handled packets up to the window size

  if (entry->total_packets < olsr_cnf->lq_wsize)
    entry->total_packets++;

  // the current reference link quality

  saved_lq = entry->saved_loss_link_quality;

#ifdef USE_FPM
  if (saved_lq == itofpm(0))
    saved_lq = itofpm(-1);
#else
  if (saved_lq == 0.0)
    saved_lq = -1.0;
#endif

  // calculate the new link quality
  //
  // start slowly: receive the first packet => link quality = 1 / n
  //               (n = window size)
#ifdef USE_FPM
  entry->loss_link_quality = fpmdiv(
    itofpm(entry->total_packets - entry->lost_packets),
    olsr_cnf->lq_wsize < (2 * 4) ? itofpm(olsr_cnf->lq_wsize):
    fpmidiv(fpmimul(4,fpmadd(fpmmuli(fpmsub(fpmidiv(itofpm(olsr_cnf->lq_wsize),4),
                                            itofpm(1)),(int)entry->total_packets),
                             itofpm(olsr_cnf->lq_wsize))),
            (olsr_32_t)olsr_cnf->lq_wsize));
#else
  entry->loss_link_quality =
    (float)(entry->total_packets - entry->lost_packets) /
    (float)(olsr_cnf->lq_wsize < (2 * 4) ? olsr_cnf->lq_wsize: 
    4 * (((float)olsr_cnf->lq_wsize / 4 - 1) * entry->total_packets + olsr_cnf->lq_wsize) / olsr_cnf->lq_wsize);
#endif
    
  // multiply the calculated link quality with the user-specified multiplier

#ifdef USE_FPM
  entry->loss_link_quality = fpmmul(entry->loss_link_quality, entry->loss_link_multiplier);
#else
  entry->loss_link_quality *= entry->loss_link_multiplier;
#endif

  // if the link quality has changed by more than 10 percent,
  // print the new link quality table

#ifdef USE_FPM
  rel_lq = fpmdiv(entry->loss_link_quality, saved_lq);
#else
  rel_lq = entry->loss_link_quality / saved_lq;
#endif

  if (rel_lq > CEIL_LQDIFF || rel_lq < FLOOR_LQDIFF)
    {
      entry->saved_loss_link_quality = entry->loss_link_quality;

      if (olsr_cnf->lq_dlimit > 0)
      {
        changes_neighborhood = OLSR_TRUE;
        changes_topology = OLSR_TRUE;
      }

      else
        OLSR_PRINTF(3, "Skipping Dijkstra (1)\n");

      // create a new ANSN

      // XXX - we should check whether we actually
      // announce this neighbour

      signal_link_changes(OLSR_TRUE);
    }
}

void olsr_update_packet_loss_hello_int(struct link_entry *entry,
                                       double loss_hello_int)
{
  // called for every LQ HELLO message - update the timeout
  // with the htime value from the message

  entry->loss_hello_int = loss_hello_int;
}

void olsr_update_packet_loss(const union olsr_ip_addr *rem,
                             const struct interface *loc,
                             olsr_u16_t seqno)
{
  struct link_entry *entry;

  // called for every OLSR packet

  entry = lookup_link_entry(rem, NULL, loc);

  // it's the very first LQ HELLO message - we do not yet have a link

  if (entry == NULL)
    return;
    
  // a) have we seen a packet before, i.e. is the sequence number valid?

  // b) heuristically detect a restart (= sequence number reset)
  //    of our neighbor

  if (entry->loss_seqno_valid != 0 && 
      (unsigned short)(seqno - entry->loss_seqno) < 100)
    {
      // loop through all lost packets

      while (entry->loss_seqno != seqno)
        {
          // have we already considered all lost LQ HELLO messages?

          if (entry->loss_missed_hellos == 0)
            olsr_update_packet_loss_worker(entry, OLSR_TRUE);

          // if not, then decrement the number of lost LQ HELLOs

          else
            entry->loss_missed_hellos--;

          entry->loss_seqno++;
        }
    }

  // we have received a packet, otherwise this function would not
  // have been called

  olsr_update_packet_loss_worker(entry, OLSR_FALSE);

  // (re-)initialize

  entry->loss_missed_hellos = 0;
  entry->loss_seqno = seqno + 1;

  // we now have a valid serial number for sure

  entry->loss_seqno_valid = 1;

  // timeout for the first lost packet is 1.5 x htime

  olsr_set_timer(&entry->link_loss_timer, entry->loss_hello_int * 1500,
                 OLSR_LINK_LOSS_JITTER, OLSR_TIMER_PERIODIC,
                 &olsr_expire_link_loss_timer, entry, 0);
}

void olsr_update_dijkstra_link_qualities(void)
{
  struct link_entry *walker;

  OLSR_FOR_ALL_LINK_ENTRIES(walker) {
    walker->loss_link_quality2 = walker->loss_link_quality;
    walker->neigh_link_quality2 = walker->neigh_link_quality;
  } OLSR_FOR_ALL_LINK_ENTRIES_END(walker);
}

#ifdef USE_FPM
fpm
#else
float
#endif
olsr_calc_link_etx(const struct link_entry *link)
{
  return link->loss_link_quality < MIN_LINK_QUALITY ||
         link->neigh_link_quality < MIN_LINK_QUALITY
#ifdef USE_FPM
             ? itofpm(0)
             : fpmdiv(itofpm(1), fpmmul(link->loss_link_quality, link->neigh_link_quality));
#else
             ? 0.0
             : 1.0 / (link->loss_link_quality * link->neigh_link_quality);
#endif
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * End:
 */


/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004-2009, the olsr.org team - see HISTORY file
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
#include "mid_set.h"
#include "neighbor_table.h"
#include "olsr.h"
#include "scheduler.h"
#include "olsr_spf.h"
#include "net_olsr.h"
#include "ipcalc.h"
#include "lq_plugin.h"
#include "common/string.h"
#include "olsr_logging.h"

/* head node for all link sets */
struct list_node link_entry_head;

static struct olsr_cookie_info *link_dead_timer_cookie = NULL;
static struct olsr_cookie_info *link_loss_timer_cookie = NULL;
static struct olsr_cookie_info *link_sym_timer_cookie = NULL;

bool link_changes;                     /* is set if changes occur in MPRS set */

void
signal_link_changes(bool val)
{                               /* XXX ugly */
  link_changes = val;
}

/* Prototypes. */
static int check_link_status(const struct lq_hello_message *message, const struct interface *in_if);
static struct link_entry *add_link_entry(const union olsr_ip_addr *,
                                         const union olsr_ip_addr *,
                                         union olsr_ip_addr *, uint32_t, uint32_t, struct interface *);
static bool get_neighbor_status(const union olsr_ip_addr *);

void
olsr_init_link_set(void)
{
  OLSR_INFO(LOG_LINKS, "Initialize linkset...\n");

  /* Init list head */
  list_head_init(&link_entry_head);

  link_dead_timer_cookie = olsr_alloc_cookie("Link dead", OLSR_COOKIE_TYPE_TIMER);
  link_loss_timer_cookie = olsr_alloc_cookie("Link loss", OLSR_COOKIE_TYPE_TIMER);
  link_sym_timer_cookie = olsr_alloc_cookie("Link SYM", OLSR_COOKIE_TYPE_TIMER);

}


/**
 * Get the status of a link. The status is based upon different
 * timeouts in the link entry.
 *
 * @param remote address of the remote interface
 * @return the link status of the link
 */
int
lookup_link_status(const struct link_entry *entry)
{

  if (entry == NULL || list_is_empty(&link_entry_head)) {
    return UNSPEC_LINK;
  }

  /*
   * Hysteresis
   */
  if (entry->link_sym_timer) {
    return SYM_LINK;
  }

  if (!TIMED_OUT(entry->ASYM_time)) {
    return ASYM_LINK;
  }

  return LOST_LINK;
}


/**
 * Find the "best" link status to a neighbor
 *
 * @param address the address to check for
 * @return true if a symmetric link exists false if not
 */
static bool
get_neighbor_status(const union olsr_ip_addr *address)
{
  const union olsr_ip_addr *main_addr;
  struct interface *ifs;
  struct tc_entry *tc;

  /* Find main address */
  if (!(main_addr = olsr_lookup_main_addr_by_alias(address)))
    main_addr = address;

  /*
   * Locate the hookup point.
   */
  tc = olsr_locate_tc_entry(main_addr);

  /* Loop trough local interfaces to check all possebilities */
  OLSR_FOR_ALL_INTERFACES(ifs) {

    struct mid_entry *aliases;
    struct link_entry *lnk = lookup_link_entry(main_addr, NULL, ifs);

    if (lnk != NULL) {
      if (lookup_link_status(lnk) == SYM_LINK)
        return true;
    }

    /* Walk the aliases */
    OLSR_FOR_ALL_TC_MID_ENTRIES(tc, aliases) {

      lnk = lookup_link_entry(&aliases->mid_alias_addr, NULL, ifs);
      if (lnk && (lookup_link_status(lnk) == SYM_LINK)) {
        return true;
      }
    }
    OLSR_FOR_ALL_TC_MID_ENTRIES_END(tc, aliases);
  }
  OLSR_FOR_ALL_INTERFACES_END(ifs);

  return false;
}

/**
 * Find best link to a neighbor
 */
struct link_entry *
get_best_link_to_neighbor(const union olsr_ip_addr *remote)
{
  const union olsr_ip_addr *main_addr;
  struct link_entry *walker, *good_link, *backup_link;
  olsr_linkcost curr_lcost = LINK_COST_BROKEN;
  olsr_linkcost tmp_lc;

  /* main address lookup */
  main_addr = olsr_lookup_main_addr_by_alias(remote);

  /* "remote" *already is* the main address */
  if (!main_addr) {
    main_addr = remote;
  }

  /* we haven't selected any links, yet */
  good_link = NULL;
  backup_link = NULL;

  /* loop through all links that we have */
  OLSR_FOR_ALL_LINK_ENTRIES(walker) {

    /* if this is not a link to the neighour in question, skip */
    if (olsr_ipcmp(&walker->neighbor->nbr_addr, main_addr) != 0)
      continue;

    /* get the link cost */
    tmp_lc = walker->linkcost;

    /*
     * is this link better than anything we had before ?
     * use the requested remote interface address as a tie-breaker.
     */
    if ((tmp_lc < curr_lcost) || ((tmp_lc == curr_lcost) && olsr_ipcmp(&walker->local_iface_addr, remote) == 0)) {

      /* memorize the link quality */
      curr_lcost = tmp_lc;

      /* prefer symmetric links over asymmetric links */
      if (lookup_link_status(walker) == SYM_LINK) {
        good_link = walker;
      } else {
        backup_link = walker;
      }
    }
  }
  OLSR_FOR_ALL_LINK_ENTRIES_END(walker);

  /*
   * if we haven't found any symmetric links, try to return an asymmetric link.
   */
  return good_link ? good_link : backup_link;
}

static void
set_loss_link_multiplier(struct link_entry *entry)
{
  struct interface *inter;
  struct olsr_if_config *cfg_inter;
  struct olsr_lq_mult *mult;
  uint32_t val = 0;

  /* find the interface for the link */
  inter = if_ifwithaddr(&entry->local_iface_addr);

  /* find the interface configuration for the interface */
  for (cfg_inter = olsr_cnf->if_configs; cfg_inter; cfg_inter = cfg_inter->next) {
    if (cfg_inter->interf == inter) {
      break;
    }
  }

  /* loop through the multiplier entries */
  for (mult = cfg_inter->cnf->lq_mult; mult != NULL; mult = mult->next) {
    /*
     * use the default multiplier only if there isn't any entry that
     * has a matching IP address.
     */
    if ((val == 0 && olsr_ipcmp(&mult->addr, &all_zero) == 0) || olsr_ipcmp(&mult->addr, &entry->neighbor_iface_addr) == 0) {
      val = mult->value;
    }
  }

  /* if we have not found an entry, then use the default multiplier */
  if (val == 0) {
    val = LINK_LOSS_MULTIPLIER;
  }

  /* store the multiplier */
  entry->loss_link_multiplier = val;
}

/*
 * Delete, unlink and free a link entry.
 */
static void
olsr_delete_link_entry(struct link_entry *link)
{

  /*
   * Delete the corresponding tc-edge for that link.
   */
  if (link->link_tc_edge) {
    olsr_delete_tc_edge_entry(link->link_tc_edge);
    link->link_tc_edge = NULL;

    changes_topology = true;
  }

  /*
   * Delete the rt_path for the link-end.
   */
  olsr_delete_routing_table(&link->neighbor_iface_addr, 8 * olsr_cnf->ipsize,
                            &link->neighbor->nbr_addr, OLSR_RT_ORIGIN_LINK);

  /* Delete neighbor entry */
  if (link->neighbor->linkcount == 1) {
    olsr_delete_nbr_entry(link->neighbor);
  } else {
    link->neighbor->linkcount--;

    if (link->is_mprs) {
      link->neighbor->mprs_count --;
    }
  }

  /* Kill running timers */
  olsr_stop_timer(link->link_timer);
  link->link_timer = NULL;
  olsr_stop_timer(link->link_sym_timer);
  link->link_sym_timer = NULL;
  olsr_stop_timer(link->link_loss_timer);
  link->link_loss_timer = NULL;

  list_remove(&link->link_list);

  /* Unlink Interfaces */
  unlock_interface(link->inter);
  link->inter = NULL;

  free(link->if_name);
  olsr_free_link_entry(link);

  changes_neighborhood = true;
}

/**
 * Delete all link entries matching a given interface id
 */
void
olsr_delete_link_entry_by_if(const struct interface *ifp)
{
  struct link_entry *link;
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif

  OLSR_FOR_ALL_LINK_ENTRIES(link) {
    if (ifp == link->inter) {
      OLSR_DEBUG(LOG_LINKS, "Removing link %s of interface %s\n",
          olsr_ip_to_string(&buf, &link->neighbor_iface_addr), ifp->int_name);
      olsr_delete_link_entry(link);
    }
  }
  OLSR_FOR_ALL_LINK_ENTRIES_END(link);
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
  olsr_update_packet_loss_worker(link, true);

  /* next timeout in 1.0 x htime */
  olsr_change_timer(link->link_loss_timer, link->loss_helloint, OLSR_LINK_LOSS_JITTER, OLSR_TIMER_PERIODIC);
}

/**
 * Callback for the link SYM timer.
 */
static void
olsr_expire_link_sym_timer(void *context)
{
  struct link_entry *link;

  link = (struct link_entry *)context;
  link->link_sym_timer = NULL;  /* be pedandic */

  if (link->status != SYM_LINK) {
    return;
  }

  link->status = lookup_link_status(link);
  olsr_update_nbr_status(link->neighbor, get_neighbor_status(&link->neighbor_iface_addr));
  changes_neighborhood = true;
}

/**
 * Callback for the link_hello timer.
 */
void
olsr_expire_link_hello_timer(void *context)
{
  struct link_entry *link;

  link = (struct link_entry *)context;

  /* update neighbor status */
  olsr_update_nbr_status(link->neighbor, get_neighbor_status(&link->neighbor_iface_addr));
}

/**
 * Callback for the link timer.
 */
static void
olsr_expire_link_entry(void *context)
{
  struct link_entry *link;

  link = (struct link_entry *)context;
  link->link_timer = NULL;      /* be pedandic */

  olsr_delete_link_entry(link);
}

/**
 * Set the link expiration timer.
 */
static void
olsr_set_link_timer(struct link_entry *link, unsigned int rel_timer)
{
  olsr_set_timer(&link->link_timer, rel_timer, OLSR_LINK_JITTER,
                 OLSR_TIMER_ONESHOT, &olsr_expire_link_entry, link, link_dead_timer_cookie);
}

/**
 * Nothing mysterious here.
 * Adding a new link entry to the link set.
 *
 * @param local the local IP address
 * @param remote the remote IP address
 * @param remote_main the remote nodes main address
 * @param vtime the validity time of the entry
 * @param htime the HELLO interval of the remote node
 * @param local_if the local interface
 * @return the new (or already existing) link_entry
 */
static struct link_entry *
add_link_entry(const union olsr_ip_addr *local,
               const union olsr_ip_addr *remote,
               union olsr_ip_addr *remote_main, uint32_t vtime, uint32_t htime, struct interface *local_if)
{
  struct link_entry *link;
  struct nbr_entry *neighbor;
#if !defined  REMOVE_LOG_DEBUG
  struct ipaddr_str localbuf, rembuf;
#endif

  link = lookup_link_entry(remote, remote_main, local_if);
  if (link) {
    /*
     * Link exists. Update tc_edge LQ and exit.
     */
    olsr_copylq_link_entry_2_tc_edge_entry(link->link_tc_edge, link);
    changes_neighborhood = olsr_calc_tc_edge_entry_etx(link->link_tc_edge);
    return link;
  }

  /*
   * if there exists no link tuple with
   * L_neighbor_iface_addr == Source Address
   */

  OLSR_DEBUG(LOG_LINKS, "Adding %s=>%s to link set\n", olsr_ip_to_string(&localbuf, local), olsr_ip_to_string(&rembuf, remote));

  /* a new tuple is created with... */
  link = olsr_malloc_link_entry();

  /* copy if_name, if it is defined */
  if (local_if->int_name) {
    size_t name_size = strlen(local_if->int_name) + 1;
    link->if_name = olsr_malloc(name_size, "target of if_name in new link entry");
    strscpy(link->if_name, local_if->int_name, name_size);
  } else
    link->if_name = NULL;

  /* shortcut to interface. */
  link->inter = local_if;
  lock_interface(local_if);

  /*
   * L_local_iface_addr = Address of the interface
   * which received the HELLO message
   */
  link->local_iface_addr = *local;

  /* L_neighbor_iface_addr = Source Address */
  link->neighbor_iface_addr = *remote;

  /* L_time = current time + validity time */
  olsr_set_link_timer(link, vtime);

  link->status = ASYM_LINK;

  link->loss_helloint = htime;

  olsr_set_timer(&link->link_loss_timer, htime + htime / 2,
                 OLSR_LINK_LOSS_JITTER, OLSR_TIMER_PERIODIC, &olsr_expire_link_loss_timer, link, link_loss_timer_cookie);

  set_loss_link_multiplier(link);

  link->linkcost = LINK_COST_BROKEN;

  link->is_mprs = false;

  /* Add to queue */
  list_add_before(&link_entry_head, &link->link_list);


  /*
   * Create the neighbor entry
   */

  /* Neighbor MUST exist! */
  neighbor = olsr_lookup_nbr_entry(remote_main, true);
  if (!neighbor) {
    OLSR_DEBUG(LOG_LINKS, "ADDING NEW NEIGHBOR ENTRY %s FROM LINK SET\n", olsr_ip_to_string(&rembuf, remote_main));
    neighbor = olsr_add_nbr_entry(remote_main);
  }

  neighbor->linkcount++;
  link->neighbor = neighbor;

  /*
   * Now create a tc-edge for that link.
   */
  olsr_change_myself_tc();
  if (link->link_tc_edge == NULL) {
    link->link_tc_edge = olsr_add_tc_edge_entry(tc_myself, remote_main, 0);
  }

  /*
   * Mark the edge local such that it does not get deleted
   * during cleanup functions.
   */
  link->link_tc_edge->is_local = 1;

  /*
   * Add the rt_path for the link-end. This is an optimization
   * that lets us install > 1 hop routes prior to receiving
   * the MID entry for the 1 hop neighbor.
   */
  olsr_insert_routing_table(remote, 8 * olsr_cnf->ipsize, remote_main, OLSR_RT_ORIGIN_LINK);

  changes_neighborhood = true;
  return link;
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
    if (olsr_ipcmp(int_addr, &link->neighbor_iface_addr) == 0) {
      return lookup_link_status(link);
    }
  }
  OLSR_FOR_ALL_LINK_ENTRIES_END(link);

  return UNSPEC_LINK;
}

/**
 * Lookup a link entry
 *
 * @param remote the remote interface address
 * @param remote_main the remote nodes main address
 * @param local the local interface address
 * @return the link entry if found, NULL if not
 */
struct link_entry *
lookup_link_entry(const union olsr_ip_addr *remote, const union olsr_ip_addr *remote_main, const struct interface *local)
{
  struct link_entry *link;

  OLSR_FOR_ALL_LINK_ENTRIES(link) {
    if (olsr_ipcmp(remote, &link->neighbor_iface_addr) == 0 && (link->if_name ? !strcmp(link->if_name, local->int_name)
                                                                : olsr_ipcmp(&local->ip_addr, &link->local_iface_addr) == 0)) {
      /* check the remote-main address only if there is one given */
      if (NULL != remote_main && olsr_ipcmp(remote_main, &link->neighbor->nbr_addr) != 0) {
        /* Neighbor has changed it's main_addr, update */
#if !defined REMOVE_LOG_DEBUG
        struct ipaddr_str oldbuf, newbuf;
#endif
        OLSR_DEBUG(LOG_LINKS, "Neighbor changed main_ip, updating %s -> %s\n",
                   olsr_ip_to_string(&oldbuf, &link->neighbor->nbr_addr), olsr_ip_to_string(&newbuf, remote_main));
        link->neighbor->nbr_addr = *remote_main;
      }
      return link;
    }
  }
  OLSR_FOR_ALL_LINK_ENTRIES_END(link);

  return NULL;
}

/**
 * Update a link entry. This is the "main entrypoint" in
 * the link-sensing. This function is called from the HELLO
 * parser function. It makes sure a entry is updated or created.
 *
 * @param local the local IP address
 * @param remote the remote IP address
 * @param message the HELLO message
 * @param in_if the interface on which this HELLO was received
 * @return the link_entry struct describing this link entry
 */
struct link_entry *
update_link_entry(const union olsr_ip_addr *local,
                  const union olsr_ip_addr *remote, struct lq_hello_message *message, struct interface *in_if)
{
  struct link_entry *entry;

  /* Add if not registered */
  entry = add_link_entry(local, remote, &message->comm.orig, message->comm.vtime, message->htime, in_if);

  /* Update ASYM_time */
  entry->vtime = message->comm.vtime;
  entry->ASYM_time = GET_TIMESTAMP(message->comm.vtime);

  entry->status = check_link_status(message, in_if);

  switch (entry->status) {
  case (LOST_LINK):
    olsr_stop_timer(entry->link_sym_timer);
    entry->link_sym_timer = NULL;
    break;
  case (SYM_LINK):
  case (ASYM_LINK):

    /* L_SYM_time = current time + validity time */
    olsr_set_timer(&entry->link_sym_timer, message->comm.vtime,
                   OLSR_LINK_SYM_JITTER, OLSR_TIMER_ONESHOT, &olsr_expire_link_sym_timer, entry, link_sym_timer_cookie);

    /* L_time = L_SYM_time + NEIGHB_HOLD_TIME */
    olsr_set_link_timer(entry, message->comm.vtime + NEIGHB_HOLD_TIME * MSEC_PER_SEC);
    break;
  default:;
  }

  /* L_time = max(L_time, L_ASYM_time) */
  if (entry->link_timer && (entry->link_timer->timer_clock < entry->ASYM_time)) {
    olsr_set_link_timer(entry, TIME_DUE(entry->ASYM_time));
  }

  /* Update neighbor */
  olsr_update_nbr_status(entry->neighbor, get_neighbor_status(remote));

  return entry;
}


/**
 * Function that updates all registered pointers to
 * one neighbor entry with another pointer
 * Used by MID updates.
 *
 * @old the pointer to replace
 * @new the pointer to use instead of "old"
 * @return the number of entries updated
 */
int
replace_neighbor_link_set(const struct nbr_entry *old, struct nbr_entry *new)
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
  }
  OLSR_FOR_ALL_LINK_ENTRIES_END(link);

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
check_link_status(const struct lq_hello_message *message, const struct interface *in_if)
{
  int ret = UNSPEC_LINK;
  struct lq_hello_neighbor *neighbors;

  neighbors = message->neigh;
  while (neighbors) {

    /*
     * Note: If a neigh has 2 cards we can reach, the neigh
     * will send a Hello with the same IP mentined twice
     */
    if (olsr_ipcmp(&neighbors->addr, &in_if->ip_addr) == 0
        && neighbors->link_type != UNSPEC_LINK) {
      ret = neighbors->link_type;
      if (SYM_LINK == ret) {
        break;
      }
    }
    neighbors = neighbors->next;
  }

  return ret;
}

void
olsr_print_link_set(void)
{
#if !defined REMOVE_LOG_INFO
  /* The whole function makes no sense without it. */
  struct link_entry *walker;
  char totaltxt[256];
  const char *txt;
  int addrsize;
  size_t i, j, length, max, totaltxt_len;
  addrsize = olsr_cnf->ip_version == AF_INET ? INET_ADDRSTRLEN : INET6_ADDRSTRLEN;

  /* generate LQ headline */
  totaltxt[0] = 0;
  totaltxt_len = 0;
  for (i=1; i<olsr_get_linklabel_count(); i++) {
    txt = olsr_get_linklabel(i);
    max = olsr_get_linklabel_maxlength(i);

    length = strlen(txt);

    /* add seperator */
    if (i != 1) {
      totaltxt[totaltxt_len++] = '/';
    }

    /* reserve space for label */
    if (max > length) {
      for (j=0; j<max; j++) {
        totaltxt[totaltxt_len + j] = '-';
      }
    }

    /* copy label */
    strncpy(&totaltxt[totaltxt_len + max/2 - length/2], txt, length);
    totaltxt_len += max;
  }
  totaltxt[totaltxt_len] = 0;

  OLSR_INFO(LOG_LINKS, "\n--- %s ---------------------------------------------------- LINKS\n\n", olsr_wallclock_string());
  OLSR_INFO_NH(LOG_LINKS, "%-*s  %-6s %s %s\n", addrsize, "IP address", "hyst", totaltxt , olsr_get_linklabel(0));

  OLSR_FOR_ALL_LINK_ENTRIES(walker) {
    struct ipaddr_str buf;
    char lqbuffer[LQTEXT_MAXLENGTH];

    /* generate LQ headline */
    totaltxt[0] = 0;
    totaltxt_len = 0;
    for (i=1; i<olsr_get_linklabel_count(); i++) {
      txt = olsr_get_linkdata_text(walker, i, lqbuffer, sizeof(lqbuffer));
      max = olsr_get_linklabel_maxlength(i);

      length = strlen(txt);

      /* add seperator */
      if (i != 1) {
        totaltxt[totaltxt_len++] = '/';
      }

      /* reserve space for label */
      if (max > length) {
        for (j=0; j<max; j++) {
          totaltxt[totaltxt_len + j] = ' ';
        }
      }

      /* copy label */
      strncpy(&totaltxt[totaltxt_len + max/2 - length/2], txt, length);
      totaltxt_len += max;
    }
    totaltxt[totaltxt_len] = 0;
    OLSR_INFO_NH(LOG_LINKS, "%-*s %s %s\n",
                 addrsize, olsr_ip_to_string(&buf, &walker->neighbor_iface_addr),
                 totaltxt, olsr_get_linkcost_text(walker->linkcost, false, lqbuffer, sizeof(lqbuffer)));
  } OLSR_FOR_ALL_LINK_ENTRIES_END(walker);
#endif
}

/*
 * called for every LQ HELLO message.
 * update the timeout with the htime value from the message
 */
void
olsr_update_packet_loss_hello_int(struct link_entry *entry, uint32_t loss_hello_int)
{
  entry->loss_helloint = loss_hello_int;
}

void
olsr_update_packet_loss(struct link_entry *entry)
{
  olsr_update_packet_loss_worker(entry, false);

  /* timeout for the first lost packet is 1.5 x htime */
  olsr_set_timer(&entry->link_loss_timer, entry->loss_helloint + entry->loss_helloint / 2,
                 OLSR_LINK_LOSS_JITTER, OLSR_TIMER_PERIODIC, &olsr_expire_link_loss_timer, entry, link_loss_timer_cookie);
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

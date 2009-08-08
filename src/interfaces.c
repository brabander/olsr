
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

#include "defs.h"
#include "interfaces.h"
#include "ifnet.h"
#include "scheduler.h"
#include "olsr.h"
#include "log.h"
#include "parser.h"
#include "net_olsr.h"
#include "ipcalc.h"
#include "common/string.h"
#include "common/avl.h"
#include "olsr_logging.h"
#include "valgrind/valgrind.h"

#include <signal.h>
#include <unistd.h>
#include <assert.h>

/* The interface list head */
struct list_node interface_head;

/* tree of lost interface IPs */
struct avl_tree interface_lost_tree;

/* Ifchange functions */
struct ifchgf {
  ifchg_cb_func function;
  struct ifchgf *next;
};

static struct ifchgf *ifchgf_list = NULL;


/* Some cookies for stats keeping */
struct olsr_cookie_info *interface_mem_cookie = NULL;
struct olsr_cookie_info *interface_poll_timer_cookie = NULL;
struct olsr_cookie_info *interface_lost_mem_cookie = NULL;

struct olsr_cookie_info *hello_gen_timer_cookie = NULL;
struct olsr_cookie_info *tc_gen_timer_cookie = NULL;
struct olsr_cookie_info *mid_gen_timer_cookie = NULL;
struct olsr_cookie_info *hna_gen_timer_cookie = NULL;
struct olsr_cookie_info *buffer_hold_timer_cookie = NULL;

static void check_interface_updates(void *);

/**
 * Do initialization of various data needed for network interface management.
 * This function also tries to set up the given interfaces.
 *
 * @return if more than zero interfaces were configured
 */
bool
ifinit(void)
{
  struct olsr_if_config *tmp_if;

  /* Initial values */
  list_head_init(&interface_head);
  avl_init(&interface_lost_tree, avl_comp_default);

  /*
   * Get some cookies for getting stats to ease troubleshooting.
   */
  interface_mem_cookie = olsr_alloc_cookie("Interface", OLSR_COOKIE_TYPE_MEMORY);
  olsr_cookie_set_memory_size(interface_mem_cookie, sizeof(struct interface));

  interface_lost_mem_cookie = olsr_alloc_cookie("Interface lost", OLSR_COOKIE_TYPE_MEMORY);
  olsr_cookie_set_memory_size(interface_lost_mem_cookie, sizeof(struct interface_lost));

  interface_poll_timer_cookie = olsr_alloc_cookie("Interface Polling", OLSR_COOKIE_TYPE_TIMER);
  buffer_hold_timer_cookie = olsr_alloc_cookie("Buffer Hold", OLSR_COOKIE_TYPE_TIMER);

  hello_gen_timer_cookie = olsr_alloc_cookie("Hello Generation", OLSR_COOKIE_TYPE_TIMER);
  tc_gen_timer_cookie = olsr_alloc_cookie("TC Generation", OLSR_COOKIE_TYPE_TIMER);
  mid_gen_timer_cookie = olsr_alloc_cookie("MID Generation", OLSR_COOKIE_TYPE_TIMER);
  hna_gen_timer_cookie = olsr_alloc_cookie("HNA Generation", OLSR_COOKIE_TYPE_TIMER);

  OLSR_INFO(LOG_INTERFACE, "\n ---- Interface configuration ---- \n\n");

  /* Run trough all interfaces immediately */
  for (tmp_if = olsr_cnf->if_configs; tmp_if != NULL; tmp_if = tmp_if->next) {
    chk_if_up(tmp_if);
  }

  /* Kick a periodic timer for the network interface update function */
  olsr_start_timer(olsr_cnf->nic_chgs_pollrate, 5,
                   OLSR_TIMER_PERIODIC, &check_interface_updates, NULL, interface_poll_timer_cookie);

  return (!list_is_empty(&interface_head));
}

static void remove_lost_interface_ip(struct interface_lost *lost) {
#if !defined(REMOVE_LOG_DEBUG)
  struct ipaddr_str buf;
#endif

  OLSR_DEBUG(LOG_INTERFACE, "Remove %s from lost interface list\n",
      olsr_ip_to_string(&buf, &lost->ip));
  avl_delete(&interface_lost_tree, &lost->node);
  olsr_cookie_free(interface_lost_mem_cookie, lost);
}

static void add_lost_interface_ip(union olsr_ip_addr *ip, uint32_t hello_timeout) {
  struct interface_lost *lost;
#if !defined(REMOVE_LOG_DEBUG)
  struct ipaddr_str buf;
#endif

  lost = olsr_cookie_malloc(interface_lost_mem_cookie);
  lost->node.key = &lost->ip;
  lost->ip = *ip;
  lost->valid_until = olsr_getTimestamp(hello_timeout * 2);
  avl_insert(&interface_lost_tree, &lost->node, AVL_DUP_NO);

  OLSR_DEBUG(LOG_INTERFACE, "Added %s to lost interface list for %d ms\n",
      olsr_ip_to_string(&buf, ip), hello_timeout*2);
}

static struct interface_lost *get_lost_interface_ip(union olsr_ip_addr *ip) {
  struct avl_node *node;
  assert(ip);
  node = avl_find(&interface_lost_tree, ip);
  if (node) {
    return node_tree2lostif(node);
  }
  return NULL;
}

bool
is_lost_interface_ip(union olsr_ip_addr *ip) {
  assert(ip);
  return get_lost_interface_ip(ip) != NULL;
}

/**
 * Callback function for periodic check of interface parameters.
 */
static void
check_interface_updates(void *foo __attribute__ ((unused)))
{
  struct olsr_if_config *tmp_if;
  struct interface_lost *lost;

  OLSR_DEBUG(LOG_INTERFACE, "Checking for updates in the interface set\n");

  for (tmp_if = olsr_cnf->if_configs; tmp_if != NULL; tmp_if = tmp_if->next) {

    if (!tmp_if->cnf->autodetect_chg) {
      /* Don't check this interface */
      OLSR_DEBUG(LOG_INTERFACE, "Not checking interface %s\n", tmp_if->name);
      continue;
    }

    if (tmp_if->interf) {
      chk_if_changed(tmp_if);
    } else {
      if (chk_if_up(tmp_if) == 1) {
        lost = get_lost_interface_ip(&tmp_if->interf->ip_addr);
        if (lost) {
          remove_lost_interface_ip(lost);
        }
      }
    }
  }

  /* clean up lost interface tree */
  OLSR_FOR_ALL_LOSTIF_ENTRIES(lost) {
    if (olsr_isTimedOut(lost->valid_until)) {
      remove_lost_interface_ip(lost);
    }
  } OLSR_FOR_ALL_LOSTIF_ENTRIES_END(lost)
}

/**
 * Remove and cleanup a physical interface.
 */
void
remove_interface(struct interface **pinterf)
{
  struct interface *ifp = *pinterf;
  struct ipaddr_str buf;

  if (!ifp) {
    return;
  }

  OLSR_INFO(LOG_INTERFACE, "Removing interface %s\n", ifp->int_name);

  olsr_delete_link_entry_by_if(ifp);

  /*
   * Call possible ifchange functions registered by plugins
   */
  run_ifchg_cbs(ifp, IFCHG_IF_REMOVE);

  /* Dequeue */
  list_remove(&ifp->int_node);

  /* Remove output buffer */
  net_remove_buffer(ifp);

  /* Check main addr */
  if (!olsr_cnf->fixed_origaddr && olsr_ipcmp(&olsr_cnf->router_id, &ifp->ip_addr) == 0) {
    if (list_is_empty(&interface_head)) {
      /* No more interfaces */
      memset(&olsr_cnf->router_id, 0, olsr_cnf->ipsize);
      OLSR_INFO(LOG_INTERFACE, "Removed last interface. Cleared main address.\n");
    } else {

      /* Grab the first interface in the list. */
      olsr_cnf->router_id = list2interface(interface_head.next)->ip_addr;
      olsr_ip_to_string(&buf, &olsr_cnf->router_id);
      OLSR_INFO(LOG_INTERFACE, "New main address: %s\n", buf.buf);
    }
  }

  /*
   * Deregister functions for periodic message generation
   */
  olsr_stop_timer(ifp->hello_gen_timer);
  ifp->hello_gen_timer = NULL;
  olsr_stop_timer(ifp->tc_gen_timer);
  ifp->tc_gen_timer = NULL;
  olsr_stop_timer(ifp->mid_gen_timer);
  ifp->mid_gen_timer = NULL;
  olsr_stop_timer(ifp->hna_gen_timer);
  ifp->hna_gen_timer = NULL;

  /*
   * Stop interface pacing.
   */
  olsr_stop_timer(ifp->buffer_hold_timer);
  ifp->buffer_hold_timer = NULL;

  /*
   * remember the IP for some time
   */
  add_lost_interface_ip(&ifp->ip_addr, ifp->hello_validity);

  /*
   * Unlink from config.
   */
  unlock_interface(*pinterf);
  *pinterf = NULL;

  /* Close olsr socket */
  remove_olsr_socket(ifp->olsr_socket, &olsr_input, NULL);
  CLOSESOCKET(ifp->olsr_socket);
  ifp->olsr_socket = -1;

//  free(ifp->int_name);
  unlock_interface(ifp);

  if (list_is_empty(&interface_head) && !olsr_cnf->allow_no_interfaces) {
    OLSR_ERROR(LOG_INTERFACE, "No more active interfaces - exiting.\n");
    olsr_exit(EXIT_FAILURE);
  }
}

void
run_ifchg_cbs(struct interface *ifp, int flag)
{
  struct ifchgf *tmp;
  for (tmp = ifchgf_list; tmp != NULL; tmp = tmp->next) {
    tmp->function(ifp, flag);
  }
}

/**
 * Find the local interface with a given address.
 *
 * @param addr the address to check.
 * @return the interface struct representing the interface
 * that matched the address.
 */
struct interface *
if_ifwithaddr(const union olsr_ip_addr *addr)
{
  struct interface *ifp;
  if (!addr) {
    return NULL;
  }

  if (olsr_cnf->ip_version == AF_INET) {

    /* IPv4 */
    OLSR_FOR_ALL_INTERFACES(ifp) {
      if (ip4cmp(&ifp->int_addr.sin_addr, &addr->v4) == 0) {
        return ifp;
      }
    }
    OLSR_FOR_ALL_INTERFACES_END(ifp);

  } else {

    /* IPv6 */
    OLSR_FOR_ALL_INTERFACES(ifp) {
      if (ip6cmp(&ifp->int6_addr.sin6_addr, &addr->v6) == 0) {
        return ifp;
      }
    }
    OLSR_FOR_ALL_INTERFACES_END(ifp);
  }
  return NULL;
}

/**
 * Find the interface with a given number.
 *
 * @param nr the number of the interface to find.
 * @return return the interface struct representing the interface
 * that matched the number.
 */
struct interface *
if_ifwithsock(int fd)
{
  struct interface *ifp;

  OLSR_FOR_ALL_INTERFACES(ifp) {
    if (ifp->olsr_socket == fd) {
      return ifp;
    }
  }
  OLSR_FOR_ALL_INTERFACES_END(ifp);

  return NULL;
}


/**
 * Find the interface with a given label.
 *
 * @param if_name the label of the interface to find.
 * @return return the interface struct representing the interface
 * that matched the label.
 */
struct interface *
if_ifwithname(const char *if_name)
{
  struct interface *ifp;
  OLSR_FOR_ALL_INTERFACES(ifp) {

    /* good ol' strcmp should be sufficient here */
    if (strcmp(ifp->int_name, if_name) == 0) {
      return ifp;
    }
  }
  OLSR_FOR_ALL_INTERFACES_END(ifp);

  return NULL;
}

#if 0

/**
 * Find the interface with a given interface index.
 *
 * @param iif_index of the interface to find.
 * @return return the interface struct representing the interface
 * that matched the iif_index.
 */
struct interface *
if_ifwithindex(const int if_index)
{
  struct interface *ifp;
  OLSR_FOR_ALL_INTERFACES(ifp) {
    if (ifp->if_index == if_index) {
      return ifp;
    }
  }
  OLSR_FOR_ALL_INTERFACES_END(ifp);

  return NULL;
}
#endif

#if 0

/**
 * Get an interface name for a given interface index
 *
 * @param iif_index of the interface to find.
 * @return "" or interface name.
 */
const char *
if_ifwithindex_name(const int if_index)
{
  const struct interface *const ifp = if_ifwithindex(if_index);
  return ifp == NULL ? "void" : ifp->int_name;
}
#endif

/**
 * Lock an interface.
 */
void
lock_interface(struct interface *ifp)
{
  assert(ifp);

  ifp->refcount++;
}

/**
 * Unlock an interface and free it if the refcount went down to zero.
 */
void
unlock_interface(struct interface *ifp)
{
  /* Node must have a positive refcount balance */
  assert(ifp->refcount);

  if (--ifp->refcount) {
    return;
  }

  /* Node must be dequeued at this point */
  assert(!list_node_on_list(&ifp->int_node));

  /* Free memory */
  free(ifp->int_name);
  olsr_cookie_free(interface_mem_cookie, ifp);
}


/**
 * Add an ifchange function. These functions are called on all (non-initial)
 * changes in the interface set.
 */
void
add_ifchgf(ifchg_cb_func f)
{
  struct ifchgf *tmp = olsr_malloc(sizeof(struct ifchgf), "Add ifchgfunction");

  tmp->function = f;
  tmp->next = ifchgf_list;
  ifchgf_list = tmp;
}

#if 0

/*
 * Remove an ifchange function
 */
int
del_ifchgf(ifchg_cb_func f)
{
  struct ifchgf *tmp, *prev;

  for (tmp = ifchgf_list, prev = NULL; tmp != NULL; prev = tmp, tmp = tmp->next) {
    if (tmp->function == f) {
      /* Remove entry */
      if (prev == NULL) {
        ifchgf_list = tmp->next;
      } else {
        prev->next = tmp->next;
      }
      free(tmp);
      return 1;
    }
  }
  return 0;
}
#endif

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

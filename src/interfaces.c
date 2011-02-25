
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

#include "common/avl_olsr_comp.h"
#include "common/avl.h"
#include "common/string.h"

#include "defs.h"
#include "interfaces.h"
#include "olsr_timer.h"
#include "olsr_socket.h"
#include "olsr.h"
#include "parser.h"
#include "net_olsr.h"
#include "ipcalc.h"
#include "olsr_logging.h"
#include "os_net.h"

#include <signal.h>
#include <unistd.h>
#include <assert.h>

#define BUFSPACE  (127*1024)    /* max. input buffer size to request */

/* The interface list head */
struct list_entity interface_head;

/* tree of lost interface IPs */
struct avl_tree interface_lost_tree;

/* Ifchange functions */
struct ifchgf {
  ifchg_cb_func function;
  struct ifchgf *next;
};

static struct ifchgf *ifchgf_list = NULL;


/* Some cookies for stats keeping */
static struct olsr_memcookie_info *interface_mem_cookie = NULL;
static struct olsr_memcookie_info *interface_lost_mem_cookie = NULL;

static struct olsr_timer_info *interface_poll_timerinfo = NULL;
static struct olsr_timer_info *hello_gen_timerinfo = NULL;

static void check_interface_updates(void *);

/**
 * Do initialization of various data needed for network interface management.
 * This function also tries to set up the given interfaces.
 *
 * @return if more than zero interfaces were configured
 */
bool
init_interfaces(void)
{
  struct olsr_if_config *tmp_if;

  /* Initial values */
  list_init_head(&interface_head);
  avl_init(&interface_lost_tree, avl_comp_default, false, NULL);

  /*
   * Get some cookies for getting stats to ease troubleshooting.
   */
  interface_mem_cookie = olsr_memcookie_add("Interface", sizeof(struct interface));

  interface_lost_mem_cookie = olsr_memcookie_add("Interface lost", sizeof(struct interface_lost));

  interface_poll_timerinfo = olsr_timer_add("Interface Polling", &check_interface_updates, true);
  hello_gen_timerinfo = olsr_timer_add("Hello Generation", &generate_hello, true);

  OLSR_INFO(LOG_INTERFACE, "\n ---- Interface configuration ---- \n\n");

  /* Run trough all interfaces immediately */
  for (tmp_if = olsr_cnf->if_configs; tmp_if != NULL; tmp_if = tmp_if->next) {
    add_interface(tmp_if);
  }

  /* Kick a periodic timer for the network interface update function */
  olsr_timer_start(olsr_cnf->nic_chgs_pollrate, 5,
                   NULL, interface_poll_timerinfo);

  return (!list_is_empty(&interface_head));
}

static void remove_lost_interface_ip(struct interface_lost *lost) {
#if !defined(REMOVE_LOG_DEBUG)
  struct ipaddr_str buf;
#endif

  OLSR_DEBUG(LOG_INTERFACE, "Remove %s from lost interface list\n",
      olsr_ip_to_string(&buf, &lost->ip));
  avl_delete(&interface_lost_tree, &lost->node);
  olsr_memcookie_free(interface_lost_mem_cookie, lost);
}

static void add_lost_interface_ip(union olsr_ip_addr *ip, uint32_t hello_timeout) {
  struct interface_lost *lost;
#if !defined(REMOVE_LOG_DEBUG)
  struct ipaddr_str buf;
#endif

  lost = olsr_memcookie_malloc(interface_lost_mem_cookie);
  lost->node.key = &lost->ip;
  lost->ip = *ip;
  lost->valid_until = olsr_clock_getAbsolute(hello_timeout * 2);
  avl_insert(&interface_lost_tree, &lost->node);

  OLSR_DEBUG(LOG_INTERFACE, "Added %s to lost interface list for %d ms\n",
      olsr_ip_to_string(&buf, ip), hello_timeout*2);
}

static struct interface_lost *get_lost_interface_ip(union olsr_ip_addr *ip) {
  struct interface_lost *lost;
  assert(ip);
  lost = avl_find_element(&interface_lost_tree, ip, lost, node);
  return lost;
}

bool
is_lost_interface_ip(union olsr_ip_addr *ip) {
  assert(ip);
  return get_lost_interface_ip(ip) != NULL;
}

void destroy_interfaces(void) {
  struct interface *iface, *iface_iterator;
  struct interface_lost *lost, *lost_iterator;

  OLSR_FOR_ALL_INTERFACES(iface, iface_iterator) {
    remove_interface(iface);
  }

  OLSR_FOR_ALL_LOSTIF_ENTRIES(lost, lost_iterator) {
    remove_lost_interface_ip(lost);
  }
}

struct interface *
add_interface(struct olsr_if_config *iface) {
  struct interface *ifp;
  int sock_rcv, sock_send;

  ifp = olsr_memcookie_malloc(interface_mem_cookie);
  ifp->int_name = iface->name;

  if ((os_init_interface(ifp, iface))) {
    olsr_memcookie_free(interface_mem_cookie, ifp);
    return NULL;
  }

  sock_rcv = os_getsocket46(olsr_cnf->ip_version, ifp->int_name, olsr_cnf->olsr_port, BUFSPACE, NULL);
  sock_send = os_getsocket46(olsr_cnf->ip_version, ifp->int_name, olsr_cnf->olsr_port, BUFSPACE, &ifp->int_multicast);
  if (sock_rcv < 0 || sock_send < 0) {
    OLSR_ERROR(LOG_INTERFACE, "Could not initialize socket... exiting!\n\n");
    olsr_exit(EXIT_FAILURE);
  }

  set_buffer_timer(ifp);

  /* Register sockets */
  ifp->olsr_socket = olsr_socket_add(sock_rcv, &olsr_input, NULL, OLSR_SOCKET_READ);
  ifp->send_socket = olsr_socket_add(sock_send, &olsr_input, NULL, OLSR_SOCKET_READ);

  os_socket_set_olsr_options(ifp, ifp->olsr_socket->fd, &ifp->int_multicast);
  os_socket_set_olsr_options(ifp, ifp->send_socket->fd, &ifp->int_multicast);

  /*
   *Initialize packet sequencenumber as a random 16bit value
   */
  ifp->olsr_seqnum = random() & 0xFFFF;

  /*
   * Set main address if it's not set
   */
  if (olsr_ipcmp(&all_zero, &olsr_cnf->router_id) == 0) {
#if !defined(REMOVE_LOG_INFO)
    struct ipaddr_str buf;
#endif
    olsr_cnf->router_id = ifp->ip_addr;
    OLSR_INFO(LOG_INTERFACE, "New main address: %s\n", olsr_ip_to_string(&buf, &olsr_cnf->router_id));

    /* initialize representation of this node in tc_set */
    olsr_change_myself_tc();
  }

  /* Set up buffer */
  net_add_buffer(ifp);

  /*
   * Register functions for periodic message generation
   */
  ifp->hello_gen_timer =
    olsr_timer_start(iface->cnf->hello_params.emission_interval,
                     HELLO_JITTER, ifp, hello_gen_timerinfo);
  ifp->hello_interval = iface->cnf->hello_params.emission_interval;
  ifp->hello_validity = iface->cnf->hello_params.validity_time;

  ifp->mode = iface->cnf->mode;

  /*
   * Call possible ifchange functions registered by plugins
   */
  run_ifchg_cbs(ifp, IFCHG_IF_ADD);

  /*
   * The interface is ready, lock it.
   */
  lock_interface(ifp);

  /*
   * Link to config.
   */
  iface->interf = ifp;
  lock_interface(iface->interf);

  /* Queue */
  list_add_before(&interface_head, &ifp->int_node);

  return ifp;
}

/**
 * Callback function for periodic check of interface parameters.
 */
static void
check_interface_updates(void *foo __attribute__ ((unused)))
{
  struct olsr_if_config *tmp_if;
  struct interface_lost *lost, *iterator;

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
      if (add_interface(tmp_if)) {
        lost = get_lost_interface_ip(&tmp_if->interf->ip_addr);
        if (lost) {
          remove_lost_interface_ip(lost);
        }
      }
    }
  }

  /* clean up lost interface tree */
  OLSR_FOR_ALL_LOSTIF_ENTRIES(lost, iterator) {
    if (olsr_clock_isPast(lost->valid_until)) {
      remove_lost_interface_ip(lost);
    }
  }
}

/**
 * Remove and cleanup a physical interface.
 */
void
remove_interface(struct interface *ifp)
{
  if (!ifp) {
    return;
  }

  OLSR_INFO(LOG_INTERFACE, "Removing interface %s\n", ifp->int_name);

  os_cleanup_interface(ifp);

  olsr_delete_link_entry_by_if(ifp);

  /*
   * Call possible ifchange functions registered by plugins
   */
  run_ifchg_cbs(ifp, IFCHG_IF_REMOVE);

  /* Dequeue */
  list_remove(&ifp->int_node);

  /* Remove output buffer */
  net_remove_buffer(ifp);

  /*
   * Deregister functions for periodic message generation
   */
  olsr_timer_stop(ifp->hello_gen_timer);
  ifp->hello_gen_timer = NULL;

  /*
   * Stop interface pacing.
   */
  olsr_timer_stop(ifp->buffer_hold_timer);
  ifp->buffer_hold_timer = NULL;

  /*
   * remember the IP for some time
   */
  add_lost_interface_ip(&ifp->ip_addr, ifp->hello_validity);

  /*
   * Unlink from config.
   */
  unlock_interface(ifp);

  /* Close olsr socket */
  os_close(ifp->olsr_socket->fd);
  os_close(ifp->send_socket->fd);

  olsr_socket_remove(ifp->olsr_socket);
  olsr_socket_remove(ifp->send_socket);

  ifp->int_name = NULL;
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
  struct interface *ifp, *iterator;
  if (!addr) {
    return NULL;
  }

  OLSR_FOR_ALL_INTERFACES(ifp, iterator) {
    if (olsr_ipcmp(&ifp->ip_addr, addr) == 0) {
      return ifp;
    }
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
  struct interface *ifp, *iterator;

  OLSR_FOR_ALL_INTERFACES(ifp, iterator) {
    if (ifp->olsr_socket->fd == fd) {
      return ifp;
    }
    if (ifp->send_socket->fd == fd) {
      return ifp;
    }
  }

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
  struct interface *ifp, *iterator;

  OLSR_FOR_ALL_INTERFACES(ifp, iterator) {
    /* good ol' strcmp should be sufficient here */
    if (strcmp(ifp->int_name, if_name) == 0) {
      return ifp;
    }
  }

  return NULL;
}

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
  struct interface *ifp, *iterator;
  OLSR_FOR_ALL_INTERFACES(ifp, iterator) {
    if (ifp->if_index == if_index) {
      return ifp;
    }
  }

  return NULL;
}

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
  assert(!list_node_added(&ifp->int_node));

  /* Free memory */
  free(ifp->int_name);
  olsr_memcookie_free(interface_mem_cookie, ifp);
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

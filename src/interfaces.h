
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


#ifndef _OLSR_INTERFACE
#define _OLSR_INTERFACE

#include <sys/types.h>
#ifdef _MSC_VER
#include <WS2tcpip.h>
#undef interface
#else
#include <sys/socket.h>
#endif
#include <time.h>

#include "olsr_types.h"
#include "olsr_cfg_data.h"
#include "mantissa.h"
#include "common/list.h"
#include "common/avl.h"

#define _PATH_PROCNET_IFINET6           "/proc/net/if_inet6"


#define IPV6_ADDR_ANY		0x0000U

#define IPV6_ADDR_UNICAST      	0x0001U
#define IPV6_ADDR_MULTICAST    	0x0002U
#define IPV6_ADDR_ANYCAST	0x0004U

#define IPV6_ADDR_LOOPBACK	0x0010U
#define IPV6_ADDR_LINKLOCAL	0x0020U
#define IPV6_ADDR_SITELOCAL	0x0040U

#define IPV6_ADDR_COMPATv4	0x0080U

#define IPV6_ADDR_SCOPE_MASK	0x00f0U

#define IPV6_ADDR_MAPPED	0x1000U
#define IPV6_ADDR_RESERVED	0x2000U


#define MAX_IF_METRIC           100

#define WEIGHT_LOWEST           0       /* No weight            */
#define WEIGHT_LOW              1       /* Low                  */
#define WEIGHT_ETHERNET_1GBP    2       /* Ethernet 1Gb+        */
#define WEIGHT_ETHERNET_1GB     4       /* Ethernet 1Gb         */
#define WEIGHT_ETHERNET_100MB   8       /* Ethernet 100Mb       */
#define WEIGHT_ETHERNET_10MB    16      /* Ethernet 10Mb        */
#define WEIGHT_ETHERNET_DEFAULT 32      /* Ethernet unknown rate */
#define WEIGHT_WLAN_HIGH        64      /* >54Mb WLAN           */
#define WEIGHT_WLAN_54MB        128     /* 54Mb 802.11g         */
#define WEIGHT_WLAN_11MB        256     /* 11Mb 802.11b         */
#define WEIGHT_WLAN_LOW         512     /* <11Mb WLAN           */
#define WEIGHT_WLAN_DEFAULT     1024    /* WLAN unknown rate    */
#define WEIGHT_SERIAL           2048    /* Serial device        */
#define WEIGHT_HIGH             4096    /* High                 */
#define WEIGHT_HIGHEST          8192    /* Really high          */

#if 0
struct if_gen_property {
  uint32_t owner_id;
  void *data;
  struct if_gen_property *next;
};
#endif

struct vtimes {
  uint8_t hello;
  uint8_t tc;
  uint8_t mid;
  uint8_t hna;
};

/*
 * Output buffer structure. This should actually be in net_olsr.h
 * but we have circular references then.
 */
struct olsr_netbuf {
  uint8_t *buff;                       /* Pointer to the allocated buffer */
  int bufsize;                         /* Size of the buffer */
  int maxsize;                         /* Max bytes of payload that can be added */
  int pending;                         /* How much data is currently pending */
  int reserved;                        /* Plugins can reserve space in buffers */
};

/**
 * A struct containing all necessary information about each
 * interface participating in the OLSRD routing
 */
struct interface {
  struct list_node int_node;           /* List of all interfaces */

  enum interface_mode mode;            /* mode of the interface, default is mesh */

  /* IP version 4 */
  struct sockaddr_in int_addr;         /* address */
  struct sockaddr_in int_netmask;      /* netmask */
  struct sockaddr_in int_broadaddr;    /* broadcast address */
  /* IP version 6 */
  struct sockaddr_in6 int6_addr;       /* Address */
  struct sockaddr_in6 int6_multaddr;   /* Multicast */
  /* IP independent */
  union olsr_ip_addr ip_addr;
  int olsr_socket;                     /* The broadcast socket for this interface */
  int int_metric;                      /* metric of interface */
  int int_mtu;                         /* MTU of interface */
  int int_flags;                       /* see below */
  int if_index;                        /* Kernels index of this interface */
  int is_wireless;                     /* wireless interface or not */
  char *int_name;                      /* from kernel if structure */
  uint16_t olsr_seqnum;                /* Olsr message seqno */

  /* Periodic message generation timers */
  struct timer_entry *hello_gen_timer;
  struct timer_entry *hna_gen_timer;
  struct timer_entry *mid_gen_timer;
  struct timer_entry *tc_gen_timer;

  /* Message build related  */
  struct timer_entry *buffer_hold_timer;        /* Timer for message batching */
  struct olsr_netbuf netbuf;           /* the build buffer */
  bool immediate_send_tc;              /* Hello's are sent immediately normally,
                                          this flag prefers to send TC's */

#ifdef linux

/* Struct used to store original redirect/ingress setting */
  struct nic_state {
    char redirect;                     /* The original state of icmp redirect */
    char spoof;                        /* The original state of spoof filter */
  } nic_state;
#endif

  olsr_reltime hello_etime;
  struct vtimes valtimes;
#if 0
  struct if_gen_property *gen_properties;       /* Generic interface properties */
#endif
  int ttl_index;                       /* index in TTL array for fish-eye */

  uint32_t refcount;                   /* Refcount */
};

LISTNODE2STRUCT(list2interface, struct interface, int_node);

/* deletion safe macro for interface list traversal */
#define OLSR_FOR_ALL_INTERFACES(interface) \
{ \
  struct list_node *_interface_node, *_next_interface_node; \
  for (_interface_node = interface_head.next; \
    _interface_node != &interface_head; \
    _interface_node = _next_interface_node) { \
    _next_interface_node = _interface_node->next; \
    interface = list2interface(_interface_node);
#define OLSR_FOR_ALL_INTERFACES_END(interface) }}


struct interface_lost {
  struct avl_node node;
  union olsr_ip_addr ip;
  uint32_t valid_until;
};

AVLNODE2STRUCT(node_tree2lostif, struct interface_lost, node);

#define OLSR_FOR_ALL_LOSTIF_ENTRIES(lostif) \
{ \
  struct avl_node *lostif_tree_node, *next_lostif_tree_node; \
  for (lostif_tree_node = avl_walk_first(&interface_lost_tree); \
    lostif_tree_node; lostif_tree_node = next_lostif_tree_node) { \
    next_lostif_tree_node = avl_walk_next(lostif_tree_node); \
    lostif = node_tree2lostif(lostif_tree_node);
#define OLSR_FOR_ALL_LOSTIF_ENTRIES_END(lostif) }}

#define OLSR_BUFFER_HOLD_JITTER 25      /* percent */
#define OLSR_BUFFER_HOLD_TIME 1000      /* milliseconds */

#define OLSR_DEFAULT_MTU             1500

/* Ifchange actions */

#define IFCHG_IF_ADD           1
#define IFCHG_IF_REMOVE        2
#define IFCHG_IF_UPDATE        3

/* The interface list head */
extern struct list_node EXPORT(interface_head);

typedef int (*ifchg_cb_func) (struct interface *, int);


bool ifinit(void);
bool EXPORT(is_lost_interface_ip)(union olsr_ip_addr *ip);
void remove_interface(struct interface **);
void run_ifchg_cbs(struct interface *, int);
struct interface *if_ifwithsock(int);
struct interface *EXPORT(if_ifwithaddr) (const union olsr_ip_addr *);
struct interface *if_ifwithname(const char *);
#if 0
const char *if_ifwithindex_name(const int if_index);
#endif
#if 0
struct interface *if_ifwithindex(const int if_index);
#endif
void EXPORT(add_ifchgf) (ifchg_cb_func f);
#if 0
int del_ifchgf(ifchg_cb_func f);
#endif
void lock_interface(struct interface *);
void unlock_interface(struct interface *);

extern struct olsr_cookie_info *interface_mem_cookie;
extern struct olsr_cookie_info *interface_poll_timer_cookie;    /* Maybe static */
extern struct olsr_cookie_info *hello_gen_timer_cookie;
extern struct olsr_cookie_info *tc_gen_timer_cookie;
extern struct olsr_cookie_info *mid_gen_timer_cookie;
extern struct olsr_cookie_info *hna_gen_timer_cookie;
extern struct olsr_cookie_info *buffer_hold_timer_cookie;

#endif

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

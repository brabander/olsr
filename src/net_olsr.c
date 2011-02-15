
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

#include "net_olsr.h"
#include "ipcalc.h"
#include "olsr.h"
#include "os_net.h"
#include "link_set.h"
#include "lq_packet.h"
#include "olsr_logging.h"

#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include <errno.h>

static void olsr_add_invalid_address(const union olsr_ip_addr *);
static void olsr_expire_buffer_timer(void *context);

#if 0                           // WIN32
#define perror(x) WinSockPError(x)
void
  WinSockPError(const char *);
#endif

/*
 * Root of the filter set.
 */
static struct avl_tree filter_tree;

/* Packet transform functions */

struct ptf {
  packet_transform_function function;
  struct ptf *next;
};

static struct ptf *ptf_list;

static const char *const deny_ipv4_defaults[] = {
  "0.0.0.0",
  "127.0.0.1",
  NULL
};

static const char *const deny_ipv6_defaults[] = {
  "0::0",
  "0::1",
  NULL
};

/* buffer writeback timer */
struct olsr_timer_info *buffer_hold_timer_info;

/*
 * Converts each invalid IP-address from string to network byte order
 * and adds it to the invalid list.
 */
void
init_net(void)
{
  const char *const *defaults = olsr_cnf->ip_version == AF_INET ? deny_ipv4_defaults : deny_ipv6_defaults;

  /* Init filter tree */
  avl_init(&filter_tree, avl_comp_default, false, NULL);

  if (*defaults) {
    OLSR_INFO(LOG_NETWORKING, "Initializing invalid IP list.\n");
  }

  for (; *defaults != NULL; defaults++) {
    union olsr_ip_addr addr;
    if (inet_pton(olsr_cnf->ip_version, *defaults, &addr) <= 0) {
      OLSR_WARN(LOG_NETWORKING, "Error converting fixed IP %s for deny rule!!\n", *defaults);
      continue;
    }
    olsr_add_invalid_address(&addr);
  }

  buffer_hold_timer_info = olsr_timer_add("Buffer writeback", olsr_expire_buffer_timer, false);
}

/**
 * Wrapper for the timer callback.
 */
static void
olsr_expire_buffer_timer(void *context)
{
  struct interface *ifn;

  ifn = (struct interface *)context;

  /*
   * Clear the pointer to indicate that this timer has
   * been expired and needs to be restarted in case there
   * will be another message queued in the future.
   */
  ifn->buffer_hold_timer = NULL;

  /*
   * Do we have something to emit ?
   */
  if (!net_output_pending(ifn)) {
    return;
  }

  OLSR_DEBUG(LOG_NETWORKING, "Buffer Holdtimer for %s timed out, sending data.\n", ifn->int_name);

  net_output(ifn);
}

/*
 * set_buffer_timer
 *
 * Kick a hold-down timer which defers building of a message.
 * This has the desired effect that olsr messages get bigger.
 */
void
set_buffer_timer(struct interface *ifn)
{

  /*
   * Bail if there is already a timer running.
   */
  if (ifn->buffer_hold_timer) {
    return;
  }

  /*
   * This is the first message since the last time this interface has
   * been drained. Flush the buffer in second or so.
   */
  ifn->buffer_hold_timer =
    olsr_timer_start(OLSR_BUFFER_HOLD_TIME, OLSR_BUFFER_HOLD_JITTER,
                     ifn, buffer_hold_timer_info);
}

void
deinit_netfilters(void)
{
  struct filter_entry *filter, *iterator;
  OLSR_FOR_ALL_FILTERS(filter, iterator) {
    avl_delete(&filter_tree, &filter->filter_node);
    free(filter);
  }
}

/**
 * Create an outputbuffer for the given interface. This
 * function will allocate the needed storage according
 * to the MTU of the interface.
 *
 * @param ifp the interface to create a buffer for
 *
 * @return 0 on success, negative if a buffer already existed
 *  for the given interface
 */
int
net_add_buffer(struct interface *ifp)
{
  /* Can the interfaces MTU actually change? If not, we can elimiate
   * the "bufsize" field in "struct olsr_netbuf".
   */
  if (ifp->netbuf.bufsize != ifp->int_mtu && ifp->netbuf.buff != NULL) {
    free(ifp->netbuf.buff);
    ifp->netbuf.buff = NULL;
  }

  if (ifp->netbuf.buff == NULL) {
    ifp->netbuf.buff = olsr_malloc(ifp->int_mtu, "add_netbuff");
  }

  /* Fill struct */
  ifp->netbuf.bufsize = ifp->int_mtu;
  ifp->netbuf.maxsize = ifp->int_mtu - OLSR_HEADERSIZE;

  ifp->netbuf.pending = 0;
  ifp->netbuf.reserved = 0;

  return 0;
}

/**
 * Remove a outputbuffer. Frees the allocated memory.
 *
 * @param ifp the interface corresponding to the buffer
 * to remove
 *
 * @return 0 on success, negative if no buffer is found
 */
void
net_remove_buffer(struct interface *ifp)
{
  /* Flush pending data */
  if (ifp->netbuf.pending != 0) {
    net_output(ifp);
  }
  free(ifp->netbuf.buff);
  ifp->netbuf.buff = NULL;
}

#if 0

/**
 * Reserve space in a outputbuffer. This should only be needed
 * in very special cases. This will decrease the reported size
 * of the buffer so that there is always <i>size</i> bytes
 * of data available in the buffer. To add data in the reserved
 * area one must use the net_outbuffer_push_reserved function.
 *
 * @param ifp the interface corresponding to the buffer
 * to reserve space on
 * @param size the number of bytes to reserve
 *
 * @return 0 on success, negative if there was not enough
 *  bytes to reserve
 */
int
net_reserve_bufspace(struct interface *ifp, int size)
{
  if (size > ifp->netbuf.maxsize) {
    return -1;
  }
  ifp->netbuf.reserved = size;
  ifp->netbuf.maxsize -= size;

  return 0;
}
#endif

/**
 * Add data to a buffer.
 *
 * @param ifp the interface corresponding to the buffer
 * @param data a pointer to the data to add
 * @param size the number of byte to copy from data
 *
 * @return -1 if no buffer was found, 0 if there was not
 *  enough room in buffer or the number of bytes added on
 *  success
 */
int
net_outbuffer_push(struct interface *ifp, const void *data, const uint16_t size)
{
  if (ifp->netbuf.pending + size > ifp->netbuf.maxsize) {
    return 0;
  }
  memcpy(&ifp->netbuf.buff[ifp->netbuf.pending + OLSR_HEADERSIZE], data, size);
  ifp->netbuf.pending += size;

  return size;
}

#if 0

/**
 * Add data to the reserved part of a buffer
 *
 * @param ifp the interface corresponding to the buffer
 * @param data a pointer to the data to add
 * @param size the number of byte to copy from data
 *
 * @return -1 if no buffer was found, 0 if there was not
 *  enough room in buffer or the number of bytes added on
 *  success
 */
int
net_outbuffer_push_reserved(struct interface *ifp, const void *data, const uint16_t size)
{
  if (ifp->netbuf.pending + size > ifp->netbuf.maxsize + ifp->netbuf.reserved) {
    return 0;
  }
  memcpy(&ifp->netbuf.buff[ifp->netbuf.pending + OLSR_HEADERSIZE], data, size);
  ifp->netbuf.pending += size;

  return size;
}
#endif

/**
 * Add a packet transform function. Theese are functions
 * called just prior to sending data in a buffer.
 *
 * @param f the function pointer
 *
 * @returns 1
 */
void
add_ptf(packet_transform_function f)
{
  struct ptf *new_ptf = olsr_malloc(sizeof(struct ptf), "Add PTF");

  new_ptf->function = f;
  new_ptf->next = ptf_list;
  ptf_list = new_ptf;
}

#if 0

/**
 * Remove a packet transform function
 *
 * @param f the function pointer
 *
 * @returns 1 if a functionpointer was removed
 *  0 if not
 */
int
del_ptf(packet_transform_function f)
{
  struct ptf *prev, *tmp_ptf;
  for (prev = NULL, tmp_ptf = ptf_list; tmp_ptf != NULL; prev = tmp_ptf, tmp_ptf = tmp_ptf->next) {
    if (tmp_ptf->function == f) {
      /* Remove entry */
      if (prev == NULL) {
        ptf_list = tmp_ptf->next;
      } else {
        prev->next = tmp_ptf->next;
      }
      free(tmp_ptf);
      return 1;
    }
  }
  return 0;
}
#endif

/**
 *Sends a packet on a given interface.
 *
 *@param ifp the interface to send on.
 *
 *@return negative on error
 */
int
net_output(struct interface *ifp)
{
  struct ptf *tmp_ptf;
  struct olsr_packet *outmsg;
  int retval;

  if (ifp->netbuf.pending == 0) {
    return 0;
  }

  ifp->netbuf.pending += OLSR_HEADERSIZE;

  retval = ifp->netbuf.pending;

  outmsg = (struct olsr_packet *)ifp->netbuf.buff;
  /* Add the Packet seqno */
  outmsg->seqno = htons(ifp->olsr_seqnum++);
  /* Set the packetlength */
  outmsg->size = htons(ifp->netbuf.pending);

  /*
   * Call possible packet transform functions registered by plugins
   */
  for (tmp_ptf = ptf_list; tmp_ptf != NULL; tmp_ptf = tmp_ptf->next) {
    tmp_ptf->function(ifp->netbuf.buff, &ifp->netbuf.pending);
  }

  if (os_sendto(ifp->send_socket->fd, ifp->netbuf.buff, ifp->netbuf.pending,
      MSG_DONTROUTE, &ifp->int_multicast) < 0) {
#if !defined REMOVE_LOG_WARN
    const int save_errno = errno;
    struct ipaddr_str buf;
#endif
    OLSR_WARN(LOG_NETWORKING, "sending %d bytes (IPv%d) to %s:%d on interface %s/%d: %s (%d)\n",
            ifp->netbuf.pending, olsr_cnf->ip_version == AF_INET ? 4 : 6,
            olsr_sockaddr_to_string(&buf, &ifp->int_multicast), ntohs(ifp->int_multicast.v4.sin_port),
            ifp->int_name, ifp->if_index,
            strerror(save_errno), save_errno);
    retval = -1;
  }

  ifp->netbuf.pending = 0;
  return retval;
}

/*
 * Adds the given IP-address to the invalid list.
 */
static void
olsr_add_invalid_address(const union olsr_ip_addr *addr)
{
#if !defined REMOVE_LOG_INFO
  struct ipaddr_str buf;
#endif
  struct filter_entry *filter;

  /*
   * Check first if the address already exists.
   */
  if (!olsr_validate_address(addr)) {
    return;
  }

  filter = olsr_malloc(sizeof(struct filter_entry), "Add filter address");

  filter->filter_addr = *addr;
  filter->filter_node.key = &filter->filter_addr;
  avl_insert(&filter_tree, &filter->filter_node);

  OLSR_INFO(LOG_NETWORKING, "Added %s to filter set\n", olsr_ip_to_string(&buf, &filter->filter_addr));
}


bool
olsr_validate_address(const union olsr_ip_addr *addr)
{
  if (avl_find(&filter_tree, addr)) {
#if !defined REMOVE_LOG_DEBUG
    struct ipaddr_str buf;
#endif
    OLSR_DEBUG(LOG_NETWORKING, "Validation of address %s failed!\n", olsr_ip_to_string(&buf, addr));
    return false;
  }
  return true;
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

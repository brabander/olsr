
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



#ifndef _NET_OLSR
#define _NET_OLSR

#include "olsr_types.h"
#include "interfaces.h"
#include "process_routes.h"

#include <arpa/inet.h>
#include <net/if.h>

typedef int (*packet_transform_function) (uint8_t *, int *);

/*
 * Used for filtering addresses.
 */
struct filter_entry {
  struct avl_node filter_node;
  union olsr_ip_addr filter_addr;
};

AVLNODE2STRUCT(filter_tree2filter, filter_entry, filter_node);
#define OLSR_FOR_ALL_FILTERS(filter) OLSR_FOR_ALL_AVL_ENTRIES(&filter_tree, filter_tree2filter, filter)
#define OLSR_FOR_ALL_FILTERS_END() OLSR_FOR_ALL_AVL_ENTRIES_END()

void init_net(void);

void deinit_netfilters(void);

int net_add_buffer(struct interface *);

void net_remove_buffer(struct interface *);

/**
 * Report the number of bytes currently available in the buffer
 * (not including possible reserved bytes)
 *
 * @param ifp the interface corresponding to the buffer
 *
 * @return the number of bytes available in the buffer or
 */
static INLINE int
net_outbuffer_bytes_left(const struct interface *ifp)
{
  return ifp->netbuf.maxsize - ifp->netbuf.pending;
}

/**
 * Returns the number of bytes pending in the buffer. That
 * is the number of bytes added but not sent.
 *
 * @param ifp the interface corresponding to the buffer
 *
 * @return the number of bytes currently pending
 */
static INLINE uint16_t
net_output_pending(const struct interface *ifp)
{
  return ifp->netbuf.pending;
}

#if 0
int net_reserve_bufspace(struct interface *, int);
#endif

int EXPORT(net_outbuffer_push) (struct interface *, const void *, const uint16_t);

#if 0
int net_outbuffer_push_reserved(struct interface *, const void *, const uint16_t);
#endif

int EXPORT(net_output) (struct interface *);

void EXPORT(add_ptf) (packet_transform_function);

#if 0
int del_ptf(packet_transform_function);
#endif

bool olsr_validate_address(const union olsr_ip_addr *);

#endif

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */


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
 * This file defines the OS dependent network related functions
 * that MUST be available to olsrd.
 * The implementations of the functions should be found in
 * <OS>/net.c (e.g. linux/net.c)
 */


#ifndef _OLSR_NET_OS_H
#define _OLSR_NET_OS_H

#include <assert.h>
#include <sys/time.h>

#include "olsr_types.h"
#include "interfaces.h"

/* OS dependent functions socket functions */
ssize_t olsr_sendto(int, const void *, size_t, int, const union olsr_sockaddr *);
ssize_t olsr_recvfrom(int, void *, size_t, int, union olsr_sockaddr *, socklen_t *);
int olsr_select(int, fd_set *, fd_set *, fd_set *, struct timeval *);

int getsocket4(int, struct interface *, bool, uint16_t);
int getsocket6(int, struct interface *, bool, uint16_t);

/* OS dependent interface functions */
int os_init_interface(struct interface *, struct olsr_if_config *);

int chk_if_changed(struct olsr_if_config *);

#ifdef WIN32
void CallSignalHandler(void);
void ListInterfaces(void);
#endif

int disable_redirects(const char *, struct interface *, int);

int disable_redirects_global(int);

int deactivate_spoof(const char *, struct interface *, int);

int restore_settings(int);

int enable_ip_forwarding(int);


void os_set_olsr_socketoptions(int socket);

int get_ipv6_address(char *, struct sockaddr_in6 *, int);

bool is_if_link_up(char *);

int join_mcast(struct interface *, int);

/* helper function for getting a socket */
static inline int
getsocket46(int family, int bufferSize, struct interface *interf,
    bool bind_to_unicast, uint16_t port) {
  assert (family == AF_INET || family == AF_INET6);

  if (family == AF_INET) {
    return getsocket4(bufferSize, interf, bind_to_unicast, port);
  }
  else {
    return getsocket6(bufferSize, interf, bind_to_unicast, port);
  }
}
#endif

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

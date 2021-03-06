
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
ssize_t EXPORT(os_sendto)(int, const void *, size_t, int, const union olsr_sockaddr *);
ssize_t EXPORT(os_recvfrom)(int, void *, size_t, int, union olsr_sockaddr *, socklen_t *);
int os_select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
int EXPORT(os_close)(int);

int EXPORT(os_getsocket4)(const char *if_name, uint16_t port, int bufspace, union olsr_sockaddr *bindto);
int EXPORT(os_getsocket6)(const char *if_name, uint16_t port, int bufspace, union olsr_sockaddr *bindto);

int EXPORT(os_socket_set_nonblocking) (int fd);

/* OS dependent interface functions */
int os_init_interface(struct interface *, struct olsr_if_config *);
void os_cleanup_interface(struct interface *);

int chk_if_changed(struct olsr_if_config *);

bool EXPORT(os_is_interface_up)(const char * dev);
int EXPORT(os_interface_set_state)(const char *dev, bool up);

#ifdef WIN32
void ListInterfaces(void);
#endif

void os_socket_set_olsr_options(struct interface *ifs, int socket, union olsr_sockaddr *);

int get_ipv6_address(char *, struct sockaddr_in6 *, int);

/* helper function for getting a socket */
static INLINE int
os_getsocket46(int family, const char *if_name, uint16_t port, int bufspace, union olsr_sockaddr *bindto) {
  assert (family == AF_INET || family == AF_INET6);

  if (family == AF_INET) {
    return os_getsocket4(if_name, port, bufspace, bindto);
  }
  else {
    return os_getsocket6(if_name, port, bufspace, bindto);
  }
}
#endif

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

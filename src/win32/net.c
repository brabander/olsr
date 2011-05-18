
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

#if defined WINCE
#include <sys/types.h>          // for time_t
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#undef interface

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "defs.h"
#include "os_net.h"
#include "os_system.h"
#include "net_olsr.h"
#include "ipcalc.h"
#include "olsr_logging.h"
#include "win32/compat.h"
#include "os_system.h"
#if defined WINCE
#define WIDE_STRING(s) L##s
#else
#define WIDE_STRING(s) s
#endif

void WinSockPError(const char *Str);
void PError(const char *);

static void DisableIcmpRedirects(void);

/**
 * Argument preprocessor
 * @param argc
 * @param argv
 */
void
os_arg(int *argc __attribute__ ((unused)), char **argv __attribute__ ((unused))) {
  return;
}

/**
 * Wrapper for exit call
 * @param ret return value
 */
void  __attribute__((noreturn))
os_exit(int ret) {
  exit(ret);
}

/**
 * Wrapper for closing sockets
 * @param
 * @param sock
 * @return
 */
int
os_close(int sock) {
  return os_close(sock);
}

int
os_socket_set_nonblocking(int fd)
{
  /* make the fd non-blocking */
  unsigned long flags = 1;
  if (ioctlsocket(fd, FIONBIO, &flags) != 0) {
    OLSR_WARN(LOG_NETWORKING, "Cannot set the socket flags: %s", win32_strerror(WSAGetLastError()));
    return -1;
  }
  return 0;
}

int
os_getsocket4(const char *if_name __attribute__ ((unused)), uint16_t port, int bufspace, union olsr_sockaddr *bindto)
{
  struct sockaddr_in Addr;
  int On = 1;
  unsigned long Len;
  int Sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (Sock < 0) {
    OLSR_ERROR(LOG_NETWORKING, "Cannot open socket for OLSR PDUs (%s)\n", strerror(errno));
    olsr_exit(EXIT_FAILURE);
  }

  if (setsockopt(Sock, SOL_SOCKET, SO_BROADCAST, (char *)&On, sizeof(On)) < 0) {
    OLSR_ERROR(LOG_NETWORKING, "Cannot set socket for OLSR PDUs to broadcast mode (%s)\n", strerror(errno));
    os_close(Sock);
    olsr_exit(EXIT_FAILURE);
  }

  if (setsockopt(Sock, SOL_SOCKET, SO_REUSEADDR, (char *)&On, sizeof(On)) < 0) {
    OLSR_ERROR(LOG_NETWORKING, "Cannot set socket for OLSR PDUs to broadcast mode (%s)\n", strerror(errno));
    os_close(Sock);
    olsr_exit(EXIT_FAILURE);
  }

  while (bufspace > 8192) {
    if (setsockopt(Sock, SOL_SOCKET, SO_RCVBUF, (char *)&bufspace, sizeof(bufspace)) == 0)
      break;

    bufspace -= 1024;
  }

  if (bufspace <= 8192)
    OLSR_WARN(LOG_NETWORKING, "Cannot set IPv4 socket receive buffer.\n");

  if (bindto == NULL) {
    memset(&Addr, 0, sizeof(Addr));
    Addr.sin_family = AF_INET;
    Addr.sin_port = htons(port);
    Addr.sin_addr.s_addr = INADDR_ANY;

    bindto = (union olsr_sockaddr *)&Addr;
  }

  if (bind(Sock, &bindto->std, sizeof(Addr)) < 0) {
    OLSR_ERROR(LOG_NETWORKING, "Could not bind socket for OLSR PDUs to device (%s)\n", strerror(errno));
    os_close(Sock);
    olsr_exit(EXIT_FAILURE);
  }

  if (WSAIoctl(Sock, FIONBIO, &On, sizeof(On), NULL, 0, &Len, NULL, NULL) < 0) {
    OLSR_ERROR(LOG_NETWORKING, "WSAIoctl");
    os_close(Sock);
    olsr_exit(EXIT_FAILURE);
  }

  return Sock;
}

int
os_getsocket6(const char *if_name __attribute__ ((unused)), uint16_t port, int bufspace, union olsr_sockaddr *bindto)
{
  struct sockaddr_in6 Addr6;
  int On = 1;
  int Sock = socket(AF_INET6, SOCK_DGRAM, 0);
  if (Sock < 0) {
    OLSR_ERROR(LOG_NETWORKING, "Cannot open socket for OLSR PDUs (%s)\n", strerror(errno));
    olsr_exit(EXIT_FAILURE);
  }

  if (setsockopt(Sock, SOL_SOCKET, SO_BROADCAST, (char *)&On, sizeof(On)) < 0) {
    OLSR_ERROR(LOG_NETWORKING, "Cannot set socket for OLSR PDUs to broadcast mode (%s)\n", strerror(errno));
    os_close(Sock);
    olsr_exit(EXIT_FAILURE);
  }

  if (setsockopt(Sock, SOL_SOCKET, SO_REUSEADDR, (char *)&On, sizeof(On)) < 0) {
    OLSR_ERROR(LOG_NETWORKING, "Cannot set socket for OLSR PDUs to broadcast mode (%s)\n", strerror(errno));
    os_close(Sock);
    olsr_exit(EXIT_FAILURE);
  }

  while (bufspace > 8192) {
    if (setsockopt(Sock, SOL_SOCKET, SO_RCVBUF, (char *)&bufspace, sizeof(bufspace)) == 0)
      break;

    bufspace -= 1024;
  }

  if (bufspace <= 8192)
    OLSR_WARN(LOG_NETWORKING, "Cannot set IPv6 socket receive buffer.\n");

  if (bindto == NULL) {
    memset(&Addr6, 0, sizeof(Addr6));
    Addr6.sin6_family = AF_INET6;
    Addr6.sin6_port = htons(port);
    bindto = (union olsr_sockaddr *)&Addr6;
  }

  if (bind(Sock, &bindto->std, sizeof(Addr6)) < 0) {
    OLSR_ERROR(LOG_NETWORKING, "Could not bind socket for OLSR PDUs to device (%s)\n", strerror(errno));
    os_close(Sock);
    olsr_exit(EXIT_FAILURE);
  }

  return Sock;
}

static int
join_mcast(struct interface *Nic, int Sock, union olsr_sockaddr *mcast)
{
  /* See linux/in6.h */
  struct ipaddr_str buf;
  struct ipv6_mreq McastReq;

  McastReq.ipv6mr_multiaddr = mcast->v6.sin6_addr;
  McastReq.ipv6mr_interface = Nic->if_index;

  OLSR_DEBUG(LOG_NETWORKING, "Interface %s joining multicast %s...", Nic->int_name,
             olsr_ip_to_string(&buf, (union olsr_ip_addr *)&Nic->int_multicast.v6.sin6_addr));
  /* Send multicast */
  if (setsockopt(Sock, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, (char *)&McastReq, sizeof(struct ipv6_mreq))
      < 0) {
    OLSR_WARN(LOG_NETWORKING, "Join multicast: %s\n", strerror(errno));
    return -1;
  }

  /* Old libc fix */
#ifdef IPV6_JOIN_GROUP
  /* Join reciever group */
  if (setsockopt(Sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, (char *)&McastReq, sizeof(struct ipv6_mreq))
      < 0)
#else
  /* Join reciever group */
  if (setsockopt(Sock, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, (char *)&McastReq, sizeof(struct ipv6_mreq))
      < 0)
#endif
  {
    OLSR_WARN(LOG_NETWORKING, "Join multicast send: %s\n", strerror(errno));
    return -1;
  }


  if (setsockopt(Sock, IPPROTO_IPV6, IPV6_MULTICAST_IF, (char *)&McastReq.ipv6mr_interface, sizeof(McastReq.ipv6mr_interface))
      < 0) {
    OLSR_WARN(LOG_NETWORKING, "Join multicast if: %s\n", strerror(errno));
    return -1;
  }

  return 0;
}

void
os_socket_set_olsr_options(struct interface *ifs,
    int sock, union olsr_sockaddr *mcast) {
  join_mcast(ifs, sock, mcast);
}

static int
SetEnableRedirKey(unsigned long New)
{
#if !defined WINCE
  HKEY Key;
  unsigned long Type;
  unsigned long Len;
  unsigned long Old;

  if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                   "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters", 0, KEY_READ | KEY_WRITE, &Key) != ERROR_SUCCESS)
    return -1;

  Len = sizeof(Old);

  if (RegQueryValueEx(Key, "EnableICMPRedirect", NULL, &Type, (unsigned char *)&Old, &Len) != ERROR_SUCCESS || Type != REG_DWORD)
    Old = 1;

  if (RegSetValueEx(Key, "EnableICMPRedirect", 0, REG_DWORD, (unsigned char *)&New, sizeof(New))) {
    RegCloseKey(Key);
    return -1;
  }

  RegCloseKey(Key);
  return Old;
#else
  return 0;
#endif
}

static void
DisableIcmpRedirects(void)
{
  int Res;

  Res = SetEnableRedirKey(0);

  if (Res != 1)
    return;

  OLSR_ERROR(LOG_NETWORKING, "\n*** IMPORTANT *** IMPORTANT *** IMPORTANT *** IMPORTANT *** IMPORTANT ***\n\n");

#if 0
  if (Res < 0) {
    OLSR_ERROR(LOG_NETWORKING, "Cannot disable ICMP redirect processing in the registry.\n");
    OLSR_ERROR(LOG_NETWORKING, "Please disable it manually. Continuing in 3 seconds...\n");
    Sleep(3000);

    return;
  }
#endif

  OLSR_ERROR(LOG_NETWORKING, "I have disabled ICMP redirect processing in the registry for you.\n");
  OLSR_ERROR(LOG_NETWORKING, "REBOOT NOW, so that these changes take effect. Exiting...\n\n");

  exit(0);
}

static OVERLAPPED RouterOver;

/* enable IP forwarding */
void os_init(void)
{
  HMODULE Lib;
  unsigned int __stdcall(*EnableRouterFunc) (HANDLE * Hand, OVERLAPPED * Over);
  HANDLE Hand;

  Lib = LoadLibrary(WIDE_STRING("iphlpapi.dll"));

  if (Lib == NULL)
    return;

  EnableRouterFunc = (unsigned int __stdcall(*)(HANDLE *, OVERLAPPED *))
    GetProcAddress(Lib, WIDE_STRING("EnableRouter"));

  if (EnableRouterFunc == NULL)
    return;

  memset(&RouterOver, 0, sizeof(OVERLAPPED));

  RouterOver.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

  if (RouterOver.hEvent == NULL) {
    OLSR_WARN(LOG_NETWORKING, "CreateEvent()");
    return;
  }

  if (EnableRouterFunc(&Hand, &RouterOver) != ERROR_IO_PENDING) {
    OLSR_WARN(LOG_NETWORKING, "EnableRouter()");
    return;
  }

  OLSR_DEBUG(LOG_NETWORKING, "Routing enabled.\n");

  DisableIcmpRedirects();

  return;
}

/**
 * disable IP forwarding
 */
void os_cleanup(void) {
  HMODULE Lib;
  unsigned int __stdcall(*UnenableRouterFunc) (OVERLAPPED * Over, unsigned int *Count);
  unsigned int Count;

  Lib = LoadLibrary(WIDE_STRING("iphlpapi.dll"));

  if (Lib == NULL)
    return;

  UnenableRouterFunc = (unsigned int __stdcall(*)(OVERLAPPED *, unsigned int *))
    GetProcAddress(Lib, WIDE_STRING("UnenableRouter"));

  if (UnenableRouterFunc == NULL)
    return;

  if (UnenableRouterFunc(&RouterOver, &Count) != NO_ERROR) {
    OLSR_WARN(LOG_NETWORKING, "UnenableRouter()");
    return;
  }

  OLSR_DEBUG(LOG_NETWORKING, "Routing disabled, count = %u.\n", Count);
}

/**
 * Wrapper for sendto(2)
 */

ssize_t
os_sendto(int s, const void *buf, size_t len, int flags, const union olsr_sockaddr *sock)
{
  return sendto(s, buf, len, flags, &sock->std, sizeof(*sock));
}


/**
 * Wrapper for recvfrom(2)
 */

ssize_t
os_recvfrom(int s, void *buf, size_t len, int flags __attribute__ ((unused)), union olsr_sockaddr *sock, socklen_t * fromlen)
{
  return recvfrom(s, buf, len, 0, &sock->std, fromlen);
}

/**
 * Wrapper for select(2)
 */

int
os_select(int nfds, fd_set * readfds, fd_set * writefds, fd_set * exceptfds, struct timeval *timeout)
{
  return select(nfds, readfds, writefds, exceptfds, timeout);
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

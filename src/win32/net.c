
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
#include "net_olsr.h"
#include "ipcalc.h"
#include "olsr_logging.h"
#include "win32/compat.h"
#if defined WINCE
#define WIDE_STRING(s) L##s
#else
#define WIDE_STRING(s) s
#endif

void WinSockPError(const char *Str);
void PError(const char *);

static void DisableIcmpRedirects(void);

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
os_getsocket4(int bufspace, struct interface *ifp, bool bind_to_unicast, uint16_t port)
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
    CLOSESOCKET(Sock);
    olsr_exit(EXIT_FAILURE);
  }

  if (setsockopt(Sock, SOL_SOCKET, SO_REUSEADDR, (char *)&On, sizeof(On)) < 0) {
    OLSR_ERROR(LOG_NETWORKING, "Cannot set socket for OLSR PDUs to broadcast mode (%s)\n", strerror(errno));
    CLOSESOCKET(Sock);
    olsr_exit(EXIT_FAILURE);
  }

  while (bufspace > 8192) {
    if (setsockopt(Sock, SOL_SOCKET, SO_RCVBUF, (char *)&bufspace, sizeof(bufspace)) == 0)
      break;

    bufspace -= 1024;
  }

  if (bufspace <= 8192)
    OLSR_WARN(LOG_NETWORKING, "Cannot set IPv4 socket receive buffer.\n");

  memset(&Addr, 0, sizeof(Addr));
  Addr.sin_family = AF_INET;
  Addr.sin_port = htons(port);

  if(bind_to_unicast) {
    Addr.sin_addr.s_addr = ifp->int_src.v4.sin_addr.s_addr;
  }
  else {
    Addr.sin_addr.s_addr = INADDR_ANY;
  }

  if (bind(Sock, (struct sockaddr *)&Addr, sizeof(Addr)) < 0) {
    OLSR_ERROR(LOG_NETWORKING, "Could not bind socket for OLSR PDUs to device (%s)\n", strerror(errno));
    CLOSESOCKET(Sock);
    olsr_exit(EXIT_FAILURE);
  }

  if (WSAIoctl(Sock, FIONBIO, &On, sizeof(On), NULL, 0, &Len, NULL, NULL) < 0) {
    OLSR_ERROR(LOG_NETWORKING, "WSAIoctl");
    CLOSESOCKET(Sock);
    olsr_exit(EXIT_FAILURE);
  }

  return Sock;
}

int
os_getsocket6(int bufspace, struct interface *ifp, bool bind_to_unicast, uint16_t port)
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
    CLOSESOCKET(Sock);
    olsr_exit(EXIT_FAILURE);
  }

  if (setsockopt(Sock, SOL_SOCKET, SO_REUSEADDR, (char *)&On, sizeof(On)) < 0) {
    OLSR_ERROR(LOG_NETWORKING, "Cannot set socket for OLSR PDUs to broadcast mode (%s)\n", strerror(errno));
    CLOSESOCKET(Sock);
    olsr_exit(EXIT_FAILURE);
  }

  while (bufspace > 8192) {
    if (setsockopt(Sock, SOL_SOCKET, SO_RCVBUF, (char *)&bufspace, sizeof(bufspace)) == 0)
      break;

    bufspace -= 1024;
  }

  if (bufspace <= 8192)
    OLSR_WARN(LOG_NETWORKING, "Cannot set IPv6 socket receive buffer.\n");

  memset(&Addr6, 0, sizeof(Addr6));
  Addr6.sin6_family = AF_INET6;
  Addr6.sin6_port = htons(port);

  if(bind_to_unicast) {
    memcpy(&Addr6.sin6_addr, &ifp->int_src.v6.sin6_addr, sizeof(struct in6_addr));
  }

  if (bind(Sock, (struct sockaddr *)&Addr6, sizeof(Addr6)) < 0) {
    OLSR_ERROR(LOG_NETWORKING, "Could not bind socket for OLSR PDUs to device (%s)\n", strerror(errno));
    CLOSESOCKET(Sock);
    olsr_exit(EXIT_FAILURE);
  }

  return Sock;
}

void
os_socket_set_olsr_options(int sock __attribute__ ((unused))) {
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

void os_init_global_ifoptions(void)
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

static int
disable_ip_forwarding(void)
{
  HMODULE Lib;
  unsigned int __stdcall(*UnenableRouterFunc) (OVERLAPPED * Over, unsigned int *Count);
  unsigned int Count;

  Lib = LoadLibrary(WIDE_STRING("iphlpapi.dll"));

  if (Lib == NULL)
    return 0;

  UnenableRouterFunc = (unsigned int __stdcall(*)(OVERLAPPED *, unsigned int *))
    GetProcAddress(Lib, WIDE_STRING("UnenableRouter"));

  if (UnenableRouterFunc == NULL)
    return 0;

  if (UnenableRouterFunc(&RouterOver, &Count) != NO_ERROR) {
    OLSR_WARN(LOG_NETWORKING, "UnenableRouter()");
    return -1;
  }

  OLSR_DEBUG(LOG_NETWORKING, "Routing disabled, count = %u.\n", Count);

  return 0;
}

int os_cleanup_global_ifoptions(void) {
  disable_ip_forwarding();

  return 0;
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

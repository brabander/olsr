/*
 * The olsr.org Optimized Link-State Routing daemon (olsrd)
 * Copyright (c) 2004, Thomas Lopatic (thomas@lopatic.de)
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
 * $Id: net.c,v 1.14 2005/02/14 16:55:37 kattemat Exp $
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#undef interface

#include <stdio.h>
#include <stdlib.h>
#include "../defs.h"
#include "../net_os.h"


void WinSockPError(char *Str);
void PError(char *);

int olsr_printf(int, char *, ...);

void DisableIcmpRedirects(void);
int disable_ip_forwarding(int Ver);

int getsocket(struct sockaddr *Addr, int BuffSize, char *Int)
{
  int Sock;
  int On = 1;
  unsigned long Len;

  Sock = socket(AF_INET, SOCK_DGRAM, 0);

  if (Sock < 0)
  {
    WinSockPError("getsocket/socket()");
    return -1;
  }

  if (setsockopt(Sock, SOL_SOCKET, SO_BROADCAST,
                 (char *)&On, sizeof (On)) < 0)
  {
    WinSockPError("getsocket/setsockopt(SO_BROADCAST)");
    closesocket(Sock);
    return -1;
  }

  while (BuffSize > 8192)
  {
    if (setsockopt(Sock, SOL_SOCKET, SO_RCVBUF, (char *)&BuffSize,
                   sizeof (BuffSize)) == 0)
      break;

    BuffSize -= 1024;
  }

  if (BuffSize <= 8192) 
    fprintf(stderr, "Cannot set IPv4 socket receive buffer.\n");

  if (bind(Sock, Addr, sizeof (struct sockaddr_in)) < 0)
  {
    WinSockPError("getsocket/bind()");
    closesocket(Sock);
    return -1;
  }

  if (WSAIoctl(Sock, FIONBIO, &On, sizeof (On), NULL, 0, &Len, NULL, NULL) < 0)
  {
    WinSockPError("WSAIoctl");
    closesocket(Sock);
    return -1;
  }

  return Sock;
}

int getsocket6(struct sockaddr_in6 *Addr, int BuffSize, char *Int)
{
  int Sock;
  int On = 1;

  Sock = socket(AF_INET6, SOCK_DGRAM, 0);

  if (Sock < 0)
  {
    WinSockPError("getsocket6/socket()");
    return -1;
  }

  if (setsockopt(Sock, SOL_SOCKET, SO_BROADCAST,
                 (char *)&On, sizeof (On)) < 0)
  {
    WinSockPError("getsocket6/setsockopt(SO_BROADCAST)");
    closesocket(Sock);
    return -1;
  }

  while (BuffSize > 8192)
  {
    if (setsockopt(Sock, SOL_SOCKET, SO_RCVBUF, (char *)&BuffSize,
                   sizeof (BuffSize)) == 0)
      break;

    BuffSize -= 1024;
  }

  if (BuffSize <= 8192) 
    fprintf(stderr, "Cannot set IPv6 socket receive buffer.\n");

  if (bind(Sock, (struct sockaddr *)Addr, sizeof (struct sockaddr_in6)) < 0)
  {
    WinSockPError("getsocket6/bind()");
    closesocket(Sock);
    return -1;
  }

  return Sock;
}

static OVERLAPPED RouterOver;

int enable_ip_forwarding(int Ver)
{
  HMODULE Lib;
  unsigned int __stdcall (*EnableRouter)(HANDLE *Hand, OVERLAPPED *Over);
  HANDLE Hand;

  Ver = Ver;
  
  Lib = LoadLibrary("iphlpapi.dll");

  if (Lib == NULL)
    return 0;

  EnableRouter = (unsigned int _stdcall (*)(HANDLE *, OVERLAPPED *))
    GetProcAddress(Lib, "EnableRouter");

  if (EnableRouter == NULL)
    return 0;

  memset(&RouterOver, 0, sizeof (OVERLAPPED));

  RouterOver.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

  if (RouterOver.hEvent == NULL)
  {
    PError("CreateEvent()");
    return -1;
  }
  
  if (EnableRouter(&Hand, &RouterOver) != ERROR_IO_PENDING)
  {
    PError("EnableRouter()");
    return -1;
  }

  olsr_printf(3, "Routing enabled.\n");

  return 0;
}

int disable_ip_forwarding(int Ver)
{
  HMODULE Lib;
  unsigned int  __stdcall (*UnenableRouter)(OVERLAPPED *Over,
                                            unsigned int *Count);
  unsigned int Count;

  Ver = Ver;
  
  Lib = LoadLibrary("iphlpapi.dll");

  if (Lib == NULL)
    return 0;

  UnenableRouter = (unsigned int _stdcall (*)(OVERLAPPED *, unsigned int *))
    GetProcAddress(Lib, "UnenableRouter");

  if (UnenableRouter == NULL)
    return 0;

  if (UnenableRouter(&RouterOver, &Count) != NO_ERROR)
  {
    PError("UnenableRouter()");
    return -1;
  }

  olsr_printf(3, "Routing disabled, count = %u.\n", Count);

  return 0;
}

int restore_settings(int Ver)
{
  disable_ip_forwarding(Ver);

  return 0;
}

static int SetEnableRedirKey(unsigned long New)
{
  HKEY Key;
  unsigned long Type;
  unsigned long Len;
  unsigned long Old;

  if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                   "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters",
                   0, KEY_READ | KEY_WRITE, &Key) != ERROR_SUCCESS)
    return -1;

  Len = sizeof (Old);

  if (RegQueryValueEx(Key, "EnableICMPRedirect", NULL, &Type,
                      (unsigned char *)&Old, &Len) != ERROR_SUCCESS ||
      Type != REG_DWORD)
    Old = 1;

  if (RegSetValueEx(Key, "EnableICMPRedirect", 0, REG_DWORD,
                    (unsigned char *)&New, sizeof (New)))
  {
    RegCloseKey(Key);
    return -1;
  }

  RegCloseKey(Key);
  return Old;
}

void DisableIcmpRedirects(void)
{
  int Res;

  Res = SetEnableRedirKey(0);

  if (Res != 1)
    return;

  fprintf(stderr, "\n*** IMPORTANT *** IMPORTANT *** IMPORTANT *** IMPORTANT *** IMPORTANT ***\n\n");

#if 0
  if (Res < 0)
  {
    fprintf(stderr, "Cannot disable ICMP redirect processing in the registry.\n");
    fprintf(stderr, "Please disable it manually. Continuing in 3 seconds...\n");
    Sleep(3000);

    return;
  }
#endif

  fprintf(stderr, "I have disabled ICMP redirect processing in the registry for you.\n");
  fprintf(stderr, "REBOOT NOW, so that these changes take effect. Exiting...\n\n");

  exit(0);
}


/**
 * Wrapper for sendto(2)
 */

ssize_t
olsr_sendto(int s, 
	    const void *buf, 
	    size_t len, 
	    int flags, 
	    const struct sockaddr *to, 
	    socklen_t tolen)
{
  return sendto(s, buf, len, flags, to, tolen);
}


/**
 * Wrapper for recvfrom(2)
 */

ssize_t  
olsr_recvfrom(int  s, 
	      void *buf, 
	      size_t len, 
	      int flags, 
	      struct sockaddr *from,
	      socklen_t *fromlen)
{
  return recvfrom(s, 
		  buf, 
		  len, 
		  0, 
		  from, 
		  fromlen);
}


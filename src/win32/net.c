/*
 * $Id: net.c,v 1.2 2004/09/15 11:18:42 tlopatic Exp $
 * Copyright (C) 2004 Thomas Lopatic (thomas@lopatic.de)
 *
 * Derived from its Linux counterpart.
 * Copyright (C) 2003 Andreas Tønnesen (andreto@ifi.uio.no)
 *
 * This file is part of olsr.org.
 *
 * olsr.org is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * olsr.org is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with olsr.org; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#undef interface

void WinSockPError(char *Str);
void PError(char *);

int olsr_printf(int, char *, ...);

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

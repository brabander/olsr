/*
 * $Id: tunnel.c,v 1.2 2004/09/15 11:18:42 tlopatic Exp $
 * Copyright (C) 2004 Thomas Lopatic (thomas@lopatic.de)
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

#include "../olsr_protocol.h"
#include "tunnel.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#undef interface

#include "../olsr.h"

void PError(char *Str);

struct ip_tunnel_parm ipt;

static unsigned int TunnelIndex;
static int StopThread;
static HANDLE TunnelHandle;
static HANDLE EventHandle;

static char Subkey[] = "System\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces\\{EECD2AB6-C2FC-4826-B92E-CAA53B29D67C}";
static unsigned char Guid[] =
{
  0xb6, 0x2a, 0xcd, 0xee, 0xfc, 0xc2, 0x26, 0x48,
  0xb9, 0x2e, 0xca, 0xa5, 0x3b, 0x29, 0xd6, 0x7c
};

static unsigned long __stdcall TunnelMonitor(void *Para)
{
  char GetBuff[1000];
  struct IpInIpGetTunnelTable *Get;
  int Idx;
  struct IpInIpTunnelEntry *Ent;
  struct IpInIpProcessNotification Not;
  unsigned long Bytes;
  OVERLAPPED Over;

  memset(&Over, 0, sizeof (OVERLAPPED));
  Over.hEvent = EventHandle;

  if ((!DeviceIoControl(TunnelHandle, IOCTL_IPINIP_GET_TUNNEL_TABLE,
                       NULL, 0, GetBuff, sizeof (GetBuff), &Bytes, &Over) &&
       GetLastError() != ERROR_IO_PENDING) ||
      !GetOverlappedResult(TunnelHandle, &Over, &Bytes, TRUE))
  {
    PError("DeviceIoControl(GET_TUNNEL_TABLE)/GetOverlappedResult()");
    return 1;
  }
  
  Get = (struct IpInIpGetTunnelTable *)GetBuff;

  for (Idx = 0; Idx < Get->Num; Idx++)
  {
    Ent = &Get->Entries[Idx];

    olsr_printf(3, "Tunnel %08x: %08x, %08x, %08x, %d\n",
           Ent->Index, Ent->LocalAddr, Ent->RemoteAddr, Ent->Unknown1,
           Ent->TimeToLive);
  }

  for (;;)
  {
    memset(&Over, 0, sizeof (OVERLAPPED));
    Over.hEvent = EventHandle;

    if (!DeviceIoControl(TunnelHandle, IOCTL_IPINIP_PROCESS_NOTIFICATION,
                         &Not, sizeof (Not), &Not, sizeof (Not), &Bytes,
                         &Over) &&
        GetLastError() != ERROR_IO_PENDING)
    {
      PError("DeviceIoControl(PROCESS_NOTIFICATION)");
      return 1;
    }

    if (WaitForSingleObject(EventHandle, INFINITE) != WAIT_OBJECT_0)
    {
      PError("WaitForSingleObject()");
      return 1;
    }

    if (StopThread != 0)
    {
      olsr_printf(5, "Leaving monitor thread.\n");
      return 0;
    }

    if (!GetOverlappedResult(TunnelHandle, &Over, &Bytes, FALSE))
    {
      PError("GetOverlappedResult()");
      return 1;
    }

    if (Not.Down)
    {
      printf("IP-in-IP tunnel went down. (");

      switch (Not.Reason)
      {
      case 0:
        printf("ICMP time exceeded");
        break;

      case 1:
        printf("ICMP destination unreachable");
        break;

      default:
        printf("no known reason");
        break;
      }

      printf(")");
    }

    else
      printf("IP-in-IP tunnel went up.\n");
  }

  return 0;
}

int InitRegistry(void)
{
  HKEY Key;
  unsigned long Res;
  unsigned long Zero = 0;

  olsr_printf(5, "InitRegistry()\n");

  Res = RegCreateKeyEx(HKEY_LOCAL_MACHINE, Subkey, 0, NULL,
                       REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL,
                       &Key, NULL);

  if (Res != ERROR_SUCCESS)
  {
    fprintf(stderr, "Cannot create HKLM\\%s.", Subkey);
    PError("RegCreateKeyEx()");
    return -1;
  }

  Res = RegSetValueEx(Key, "DefaultGateway", 0, REG_MULTI_SZ, "\0", 2);

  if (Res != ERROR_SUCCESS)
  {
    PError("RegSetValueEx()");

    Res = RegDeleteKey(HKEY_LOCAL_MACHINE, Subkey);

    if (Res != ERROR_SUCCESS)
      PError("RegDeleteKey()");

    return -1;
  }

  Res = RegSetValueEx(Key, "EnableDHCP", 0, REG_DWORD, (char *)&Zero, 4);

  if (Res != ERROR_SUCCESS)
  {
    PError("RegSetValueEx()");

    Res = RegDeleteKey(HKEY_LOCAL_MACHINE, Subkey);

    if (Res != ERROR_SUCCESS)
      PError("RegDeleteKey()");

    return -1;
  }

  Res = RegSetValueEx(Key, "IPAddress", 0, REG_MULTI_SZ, "0.0.0.0\0", 9);

  if (Res != ERROR_SUCCESS)
  {
    PError("RegSetValueEx()");

    Res = RegDeleteKey(HKEY_LOCAL_MACHINE, Subkey);

    if (Res != ERROR_SUCCESS)
      PError("RegDeleteKey()");

    return -1;
  }

  Res = RegSetValueEx(Key, "NTEContextList", 0, REG_MULTI_SZ, "\0", 2);

  if (Res != ERROR_SUCCESS)
  {
    PError("RegSetValueEx()");

    Res = RegDeleteKey(HKEY_LOCAL_MACHINE, Subkey);

    if (Res != ERROR_SUCCESS)
      PError("RegDeleteKey()");

    return -1;
  }

  Res = RegSetValueEx(Key, "SubnetMask", 0, REG_MULTI_SZ, "0.0.0.0\0", 9);

  if (Res != ERROR_SUCCESS)
  {
    PError("RegSetValueEx()");

    Res = RegDeleteKey(HKEY_LOCAL_MACHINE, Subkey);

    if (Res != ERROR_SUCCESS)
      PError("RegDeleteKey()");

    return -1;
  }

  Res = RegSetValueEx(Key, "UseZeroBroadcast", 0, REG_DWORD, (char *)&Zero, 4);

  if (Res != ERROR_SUCCESS)
  {
    PError("RegSetValueEx()");

    Res = RegDeleteKey(HKEY_LOCAL_MACHINE, Subkey);

    if (Res != ERROR_SUCCESS)
      PError("RegDeleteKey()");

    return -1;
  }

  return 0;
}

void CleanRegistry(void)
{
  unsigned long Res;

  olsr_printf(5, "CleanRegistry()\n");

  Res = RegDeleteKey(HKEY_LOCAL_MACHINE, Subkey);

  if (Res != ERROR_SUCCESS)
    PError("RegDeleteKey()");
}

// XXX - to be extended

void set_up_source_tnl(union olsr_ip_addr *Local, union olsr_ip_addr *Remote,
                       int Inter)
{
  unsigned long Bytes;
  OVERLAPPED Over;
  struct IpInIpAddTunnelInterface Add;
  struct IpInIpSetTunnelInfo Set;
  struct IpInIpDeleteTunnelInterface Del;
  unsigned long ThreadId;
  HANDLE Thread;

  olsr_printf(5, "set_up_source_tnl(), interface index = %08x\n", Inter);

  if (InitRegistry() < 0)
    return;

  EventHandle = CreateEvent(NULL, FALSE, FALSE, NULL);

  if (EventHandle == NULL)
  {
    PError("CreateEvent()");
    return;
  }

  TunnelHandle = CreateFile("\\\\.\\IPINIP", GENERIC_READ | GENERIC_WRITE,
                            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                            OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);

  if (TunnelHandle == INVALID_HANDLE_VALUE)
  {
    fprintf(stderr,
            "Cannot open tunnel device - is the IP-in-IP service running?\n");
    CloseHandle(EventHandle);
    CleanRegistry();
    return;
  }

  olsr_printf(5, "Tunnel device opened.\n");

  memcpy(&Add.Guid, Guid, 16);

  memset(&Over, 0, sizeof (OVERLAPPED));
  Over.hEvent = EventHandle;

  if ((!DeviceIoControl(TunnelHandle, IOCTL_IPINIP_ADD_TUNNEL_INTERFACE, &Add,
                        sizeof (Add), &Add, sizeof (Add), &Bytes, &Over) &&
       GetLastError() != ERROR_IO_PENDING) ||
      !GetOverlappedResult(TunnelHandle, &Over, &Bytes, TRUE))
  {
    PError("DeviceIoControl(ADD_TUNNEL_INTERFACE)/GetOverlappedResult()");
    CloseHandle(TunnelHandle);
    CloseHandle(EventHandle);
    CleanRegistry();
    return;
  }

  TunnelIndex = Add.Index;

  olsr_printf(5, "Tunnel interface added, index = %08x.\n", TunnelIndex);

  if (TunnelIndex == 0)
  {
    fprintf(stderr, "Tunnel interface already in use.\n");
    CloseHandle(TunnelHandle);
    CloseHandle(EventHandle);
    CleanRegistry();
    return;
  }

  Set.Index = TunnelIndex;
  Set.LocalAddr = Local->v4;
  Set.RemoteAddr = Remote->v4;
  Set.TimeToLive = 64;

  olsr_printf(5, "Local endpoint = %08x, remote endpoint = %08x.\n",
              Set.LocalAddr, Set.RemoteAddr);

  memset(&Over, 0, sizeof (OVERLAPPED));
  Over.hEvent = EventHandle;

  if ((!DeviceIoControl(TunnelHandle, IOCTL_IPINIP_SET_TUNNEL_INFO, &Set,
                        sizeof (Set), &Set, sizeof (Set), &Bytes, &Over) &&
       GetLastError() != ERROR_IO_PENDING) ||
      !GetOverlappedResult(TunnelHandle, &Over, &Bytes, TRUE))
  {
    PError("DeviceIoControl(SET_TUNNEL_INFO)/GetOverlappedResult()");

    Del.Index = TunnelIndex;

    memset(&Over, 0, sizeof (OVERLAPPED));
    Over.hEvent = EventHandle;

    if ((!DeviceIoControl(TunnelHandle, IOCTL_IPINIP_DELETE_TUNNEL_INTERFACE,
                          &Del, sizeof (Del), NULL, 0, &Bytes, &Over) &&
         GetLastError() != ERROR_IO_PENDING) ||
        !GetOverlappedResult(TunnelHandle, &Over, &Bytes, TRUE))
      PError("DeviceIoControl(DELETE_TUNNEL_INTERFACE)/GetOverlappedResult()");

    CloseHandle(TunnelHandle);
    CloseHandle(EventHandle);
    CleanRegistry();
    return;
  }

  StopThread = 0;

  Thread = CreateThread(NULL, 0, TunnelMonitor, NULL, 0, &ThreadId);

  if (Thread == NULL)
  {
    PError("CreateThread()");

    Del.Index = TunnelIndex;

    memset(&Over, 0, sizeof (OVERLAPPED));
    Over.hEvent = EventHandle;

    if ((!DeviceIoControl(TunnelHandle, IOCTL_IPINIP_DELETE_TUNNEL_INTERFACE,
                          &Del, sizeof (Del), NULL, 0, &Bytes, &Over) &&
         GetLastError() != ERROR_IO_PENDING) ||
        !GetOverlappedResult(TunnelHandle, &Over, &Bytes, TRUE))
      PError("DeviceIoControl(DELETE_TUNNEL_INTERFACE)/GetOverlappedResult()");

    CloseHandle(TunnelHandle);
    CloseHandle(EventHandle);
    CleanRegistry();
    return;
  }
}

// XXX - to be extended

int del_ip_tunnel(struct ip_tunnel_parm *Para)
{
  struct IpInIpDeleteTunnelInterface Del;
  unsigned long Bytes;
  OVERLAPPED Over;

  olsr_printf(5, "del_ip_tunnel()\n");

  StopThread = 1;

  if (!SetEvent(EventHandle))
    PError("SetEvent()");

  Del.Index = TunnelIndex;

  memset(&Over, 0, sizeof (OVERLAPPED));
  Over.hEvent = EventHandle;

  if ((!DeviceIoControl(TunnelHandle, IOCTL_IPINIP_DELETE_TUNNEL_INTERFACE,
                       &Del, sizeof (Del), NULL, 0, &Bytes, &Over) &&
       GetLastError() != ERROR_IO_PENDING) ||
      !GetOverlappedResult(TunnelHandle, &Over, &Bytes, TRUE))
    PError("DeviceIoControl(DELETE_TUNNEL_INTERFACE)");

  CloseHandle(TunnelHandle);
  CloseHandle(EventHandle);
  CleanRegistry();
  return 1;
}

// XXX - to be implemented

int set_up_gw_tunnel(union olsr_ip_addr *Addr)
{
  return 1;
}

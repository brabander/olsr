/* 
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2004 Thomas Lopatic (thomas@lopatic.de)
 *
 * Derived from its Linux counterpart.
 * Copyright (C) 2003 Andreas Tønnesen (andreto@ifi.uio.no)
 *
 * This file is part of the olsr.org OLSR daemon.
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
 * $Id: ifnet.c,v 1.5 2004/10/19 13:55:51 tlopatic Exp $
 *
 */

#include "../interfaces.h"
#include "../olsr.h"
#include "../net.h"
#include "../parser.h"
#include "../socket_parser.h"
#include "../defs.h"
#include "../net_os.h"
#include "../ifnet.h"
#include "../generate_msg.h"
#include "../scheduler.h"
#include "../mantissa.h"

#include <iphlpapi.h>
#include <iprtrmib.h>

void WinSockPError(char *);
char *StrError(unsigned int ErrNo);
int inet_pton(int af, char *src, void *dst);

#define MAX_INTERFACES 25

int __stdcall SignalHandler(unsigned long Signal);

static unsigned long __stdcall SignalHandlerWrapper(void *Dummy)
{
  SignalHandler(0);
  return 0;
}

static void CallSignalHandler(void)
{
  unsigned long ThreadId;

  CreateThread(NULL, 0, SignalHandlerWrapper, NULL, 0, &ThreadId);
}

static void MiniIndexToIntName(char *String, int MiniIndex)
{
  char *HexDigits = "0123456789abcdef";

  String[0] = 'i';
  String[1] = 'f';

  String[2] = HexDigits[(MiniIndex >> 4) & 15];
  String[3] = HexDigits[MiniIndex & 15];

  String[4] = 0;
}

static int IntNameToMiniIndex(int *MiniIndex, char *String)
{
  char *HexDigits = "0123456789abcdef";
  int i, k;
  char ch;

  if ((String[0] != 'i' && String[0] != 'I') ||
      (String[1] != 'f' && String[1] != 'F'))
    return -1;

  *MiniIndex = 0;

  for (i = 2; i < 4; i++)
  {
    ch = String[i];

    if (ch >= 'A' && ch <= 'F')
      ch += 32;

    for (k = 0; k < 16 && ch != HexDigits[k]; k++);

    if (k == 16)
      return -1;

    *MiniIndex = (*MiniIndex << 4) | k;
  }

  return 0;
}

static int MiniIndexToGuid(char *Guid, int MiniIndex)
{
  IP_ADAPTER_INFO AdInfo[MAX_INTERFACES], *Walker;
  unsigned long AdInfoLen;
  unsigned long Res;
  
  if (olsr_cnf->ip_version == AF_INET6)
  {
    fprintf(stderr, "IPv6 not supported by MiniIndexToGuid()!\n");
    return -1;
  }

  AdInfoLen = sizeof (AdInfo);

  Res = GetAdaptersInfo(AdInfo, &AdInfoLen);

  if (Res != NO_ERROR)
  {
    fprintf(stderr, "GetAdaptersInfo() = %08lx, %s", GetLastError(),
            StrError(Res));
    return -1;
  }

  for (Walker = AdInfo; Walker != NULL; Walker = Walker->Next)
  {
    olsr_printf(5, "Index = %08x\n", Walker->Index);

    if ((Walker->Index & 255) == MiniIndex)
      break;
  }

  if (Walker != NULL)
  {
    olsr_printf(5, "Found interface.\n");

    strcpy(Guid, Walker->AdapterName);
    return 0;
  }

  olsr_printf(5, "Cannot map mini index %02x to an adapter GUID.\n",
              MiniIndex);
  return -1;
}

static int AddrToIndex(int *Index, unsigned int Addr)
{
  unsigned int IntAddr;
  IP_ADAPTER_INFO AdInfo[MAX_INTERFACES], *Walker;
  unsigned long AdInfoLen;
  IP_ADDR_STRING *Walker2;
  unsigned long Res;
  
  olsr_printf(5, "AddrToIndex(%08x)\n", Addr);

  if (olsr_cnf->ip_version == AF_INET6)
  {
    fprintf(stderr, "IPv6 not supported by AddrToIndex()!\n");
    return -1;
  }

  AdInfoLen = sizeof (AdInfo);

  Res = GetAdaptersInfo(AdInfo, &AdInfoLen);

  if (Res != NO_ERROR)
  {
    fprintf(stderr, "GetAdaptersInfo() = %08lx, %s", Res, StrError(Res));
    return -1;
  }

  for (Walker = AdInfo; Walker != NULL; Walker = Walker->Next)
  {
    olsr_printf(5, "Index = %08x\n", Walker->Index);

    for (Walker2 = &Walker->IpAddressList; Walker2 != NULL;
         Walker2 = Walker2->Next)
    {
      inet_pton(AF_INET, Walker2->IpAddress.String, &IntAddr);

      olsr_printf(5, "\tIP address = %08x\n", IntAddr);

      if (Addr == IntAddr)
      {
        olsr_printf(5, "Found interface.\n");
        *Index = Walker->Index;
        return 0;
      }
    }
  }

  olsr_printf(5, "Cannot map IP address %08x to an adapter index.\n", Addr);
  return -1;
}

#if !defined OID_802_11_CONFIGURATION
#define OID_802_11_CONFIGURATION 0x0d010211
#endif

#if !defined IOCTL_NDIS_QUERY_GLOBAL_STATS
#define IOCTL_NDIS_QUERY_GLOBAL_STATS 0x00170002
#endif

static int IsWireless(char *IntName)
{
  int MiniIndex;
  char DevName[43];
  HANDLE DevHand;
  unsigned int ErrNo;
  unsigned int Oid;
  unsigned char OutBuff[100];
  unsigned long OutBytes;

  if (IntNameToMiniIndex(&MiniIndex, IntName) < 0)
    return -1;

  DevName[0] = '\\';
  DevName[1] = '\\';
  DevName[2] = '.';
  DevName[3] = '\\';

  if (MiniIndexToGuid(DevName + 4, MiniIndex) < 0)
    return -1;

  olsr_printf(5, "Checking whether interface %s is wireless.\n", DevName);

  DevHand = CreateFile(DevName, GENERIC_READ,
                       FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL, NULL);

  if (DevHand == INVALID_HANDLE_VALUE)
  {
    ErrNo = GetLastError();

    olsr_printf(5, "CreateFile() = %08lx, %s\n", ErrNo, StrError(ErrNo));
    return -1;
  }

  Oid = OID_802_11_CONFIGURATION;

  if (!DeviceIoControl(DevHand, IOCTL_NDIS_QUERY_GLOBAL_STATS,
                       &Oid, sizeof (Oid),
                       OutBuff, sizeof (OutBuff),
                       &OutBytes, NULL))
  {
    ErrNo = GetLastError();

    CloseHandle(DevHand);

    if (ErrNo == ERROR_GEN_FAILURE)
    {
      olsr_printf(5, "OID not supported. Device probably not wireless.\n");
      return 0;
    }

    olsr_printf(5, "DeviceIoControl() = %08lx, %s\n", ErrNo, StrError(ErrNo));
    return -1;
  }

  CloseHandle(DevHand);
  return 1;
}

void ListInterfaces(void)
{
  IP_ADAPTER_INFO AdInfo[MAX_INTERFACES], *Walker;
  unsigned long AdInfoLen;
  char IntName[5];
  IP_ADDR_STRING *Walker2;
  unsigned long Res;
  int IsWlan;
  
  if (olsr_cnf->ip_version == AF_INET6)
  {
    fprintf(stderr, "IPv6 not supported by ListInterfaces()!\n");
    return;
  }

  AdInfoLen = sizeof (AdInfo);

  Res = GetAdaptersInfo(AdInfo, &AdInfoLen);

  if (Res == ERROR_NO_DATA)
  {
    printf("No interfaces detected.\n");
    return;
  }
  
  if (Res != NO_ERROR)
  {
    fprintf(stderr, "GetAdaptersInfo() = %08lx, %s", Res, StrError(Res));
    return;
  }

  for (Walker = AdInfo; Walker != NULL; Walker = Walker->Next)
  {
    olsr_printf(5, "Index = %08x\n", Walker->Index);

    MiniIndexToIntName(IntName, Walker->Index);

    printf("%s: ", IntName);

    IsWlan = IsWireless(IntName);

    if (IsWlan < 0)
      printf("?");

    else if (IsWlan == 0)
      printf("-");

    else
      printf("+");

    for (Walker2 = &Walker->IpAddressList; Walker2 != NULL;
         Walker2 = Walker2->Next)
      printf(" %s", Walker2->IpAddress.String);

    printf("\n");
  }
}

int InterfaceEntry(MIB_IFROW *IntPara, int *Index, struct if_name *IntName)
{
  int MiniIndex;
  unsigned char Buff[MAX_INTERFACES * sizeof (MIB_IFROW) + 4];
  MIB_IFTABLE *IfTable;
  unsigned long BuffLen;
  unsigned long Res;
  int TabIdx;

  if (olsr_cnf->ip_version == AF_INET6)
  {
    fprintf(stderr, "IPv6 not supported by AdapterInfo()!\n");
    return -1;
  }

  if (IntNameToMiniIndex(&MiniIndex, IntName->name) < 0)
  {
    fprintf(stderr, "No such interface: %s!\n", IntName->name);
    return -1;
  }

  IfTable = (MIB_IFTABLE *)Buff;

  BuffLen = sizeof (Buff);

  Res = GetIfTable(IfTable, &BuffLen, FALSE);

  if (Res != NO_ERROR)
  {
    fprintf(stderr, "GetIfTable() = %08lx, %s", Res, StrError(Res));
    return -1;
  }

  for (TabIdx = 0; TabIdx < IfTable->dwNumEntries; TabIdx++)
  {
    olsr_printf(5, "Index = %08x\n", IfTable->table[TabIdx].dwIndex);

    if ((IfTable->table[TabIdx].dwIndex & 255) == MiniIndex)
      break;
  }

  if (TabIdx == IfTable->dwNumEntries)
  {
    fprintf(stderr, "No such interface: %s!\n", IntName->name);
    return -1;
  }
    
  *Index = IfTable->table[TabIdx].dwIndex;

  memcpy(IntPara, &IfTable->table[TabIdx], sizeof (MIB_IFROW));
  return 0;
}

int InterfaceInfo(INTERFACE_INFO *IntPara, int *Index, struct if_name *IntName)
{
  int MiniIndex;
  int Sock;
  INTERFACE_INFO IntInfo[25];
  long Num;
  int WsIdx;
  int CandIndex;

  if (IntNameToMiniIndex(&MiniIndex, IntName->name) < 0)
  {
    fprintf(stderr, "No such interface: %s!\n", IntName->name);
    return -1;
  }

  Sock = socket(olsr_cnf->ip_version, SOCK_STREAM, IPPROTO_TCP);

  if (Sock < 0)
  {
    WinSockPError("socket()");
    return -1;
  }

  if (WSAIoctl(Sock, SIO_GET_INTERFACE_LIST, NULL, 0,
               IntInfo, sizeof (IntInfo), &Num, NULL, NULL) < 0)
  {
    WinSockPError("WSAIoctl(SIO_GET_INTERFACE_LIST)");
    closesocket(Sock);
    return -1;
  }

  closesocket(Sock);

  Num /= sizeof (INTERFACE_INFO);

  olsr_printf(5, "%s:\n", IntName->name);

  for (WsIdx = 0; WsIdx < Num; WsIdx++)
  {
    if (AddrToIndex(&CandIndex,
                    IntInfo[WsIdx].iiAddress.AddressIn.sin_addr.s_addr) < 0)
      continue;

    if ((CandIndex & 255) == MiniIndex)
      break;
  }

  if (WsIdx == Num)
  {
    fprintf(stderr, "No such interface: %s!\n", IntName->name);
    return -1;
  }
    
  *Index = CandIndex;

  olsr_printf(5, "\tIndex: %08x\n", *Index);

  olsr_printf(5, "\tFlags: %08x\n", IntInfo[WsIdx].iiFlags);

  if ((IntInfo[WsIdx].iiFlags & IFF_UP) == 0)
  {
    olsr_printf(1, "\tInterface not up - skipping it...\n");
    return -1;
  }

  if (olsr_cnf->ip_version == AF_INET &&
      (IntInfo[WsIdx].iiFlags & IFF_BROADCAST) == 0)
  {
    olsr_printf(1, "\tNo broadcast - skipping it...\n");
    return -1;
  }

  if ((IntInfo[WsIdx].iiFlags & IFF_LOOPBACK) != 0)
  {
    olsr_printf(1, "\tThis is a loopback interface - skipping it...\n");
    return -1;
  }

  // Windows seems to always return 255.255.255.255 as broadcast
  // address, so I've tried using (address | ~netmask).

  {
    struct sockaddr_in *sin_a, *sin_n, *sin_b;
    unsigned int a, n, b;

    sin_a = (struct sockaddr_in *)&IntInfo[WsIdx].iiAddress;
    sin_n = (struct sockaddr_in *)&IntInfo[WsIdx].iiNetmask;
    sin_b = (struct sockaddr_in *)&IntInfo[WsIdx].iiBroadcastAddress;

    a = sin_a->sin_addr.s_addr;
    n = sin_n->sin_addr.s_addr;
    b = sin_b->sin_addr.s_addr =
      sin_a->sin_addr.s_addr | ~sin_n->sin_addr.s_addr;
  }

  memcpy(IntPara, &IntInfo[WsIdx], sizeof (INTERFACE_INFO));
  return 0;
}

void RemoveInterface(struct if_name *IntName)
{
  struct interface *Int, *Prev;
  struct ifchgf *Walker;

  olsr_printf(1, "Removing interface %s.\n", IntName->name);
  
  Int = IntName->interf;

  for (Walker = ifchgf_list; Walker != NULL; Walker = Walker->next)
    Walker->function(Int, IFCHG_IF_REMOVE);

  if (Int == ifnet)
    ifnet = Int->int_next;

  else
  {
    for (Prev = ifnet; Prev->int_next != Int; Prev = Prev->int_next);

    Prev->int_next = Int->int_next;
  }

  if(COMP_IP(&main_addr, &Int->ip_addr))
  {
    if(ifnet == NULL)
    {
      memset(&main_addr, 0, ipsize);
      olsr_printf(1, "Removed last interface. Cleared main address.\n");
    }

    else
    {
      COPY_IP(&main_addr, &ifnet->ip_addr);
      olsr_printf(1, "New main address: %s.\n", olsr_ip_to_string(&main_addr));
    }
  }

  nbinterf--;
  
  olsr_remove_scheduler_event(&generate_hello, Int,
                              IntName->cnf->hello_params.emission_interval,
                              0, NULL);

  olsr_remove_scheduler_event(&generate_tc, Int,
                              IntName->cnf->tc_params.emission_interval,
                              0, NULL);

  olsr_remove_scheduler_event(&generate_mid, Int,
                              IntName->cnf->mid_params.emission_interval,
                              0, NULL);

  olsr_remove_scheduler_event(&generate_hna, Int,
                              IntName->cnf->hna_params.emission_interval,
                              0, NULL);

  net_remove_buffer(Int);

  IntName->configured = 0;
  IntName->interf = NULL;

  closesocket(Int->olsr_socket);
  remove_olsr_socket(Int->olsr_socket, &olsr_input);

  free(Int->int_name);
  free(Int);

  if(nbinterf == 0 && !olsr_cnf->allow_no_interfaces)
  {
    olsr_printf(1, "No more active interfaces - exiting.\n");
    exit_value = EXIT_FAILURE;
    CallSignalHandler();
  }
}

int chk_if_changed(struct if_name *IntName)
{
  struct interface *Int;
  INTERFACE_INFO IntInfo;
  MIB_IFROW IntRow;
  int Index;
  int Res;
  union olsr_ip_addr OldVal, NewVal;
  struct ifchgf *Walker;
  int IsWlan;

  if (olsr_cnf->ip_version == AF_INET6)
  {
    fprintf(stderr, "IPv6 not supported by chk_if_changed()!\n");
    return 0;
  }

#ifdef DEBUG
  olsr_printf(3, "Checking if %s is set down or changed\n", IntName->name);
#endif

  Int = IntName->interf;

  if (InterfaceInfo(&IntInfo, &Index, IntName) < 0 ||
      InterfaceEntry(&IntRow, &Index, IntName))
  {
    RemoveInterface(IntName);
    return 1;
  }

  Res = 0;

  IsWlan = IsWireless(IntName->name);

  if (IsWlan < 0)
    IsWlan = 1;

  if (Int->is_wireless != IsWlan)
  {
    olsr_printf(1, "\tLAN/WLAN change: %d -> %d.\n", Int->is_wireless, IsWlan);

    Int->is_wireless = IsWlan;
    Int->int_metric = IsWlan;

    Res = 1;
  }

  if (Int->int_mtu != IntRow.dwMtu)
  {
    olsr_printf(1, "\tMTU change: %d -> %d.\n", Int->int_mtu, IntRow.dwMtu);

    Int->int_mtu = IntRow.dwMtu;

    net_remove_buffer(Int);
    net_add_buffer(Int);

    Res = 1;
  }

  OldVal.v4 = ((struct sockaddr_in *)&Int->int_addr)->sin_addr.s_addr;
  NewVal.v4 = ((struct sockaddr_in *)&IntInfo.iiAddress)->sin_addr.s_addr;

#ifdef DEBUG
  olsr_printf(3, "\tAddress: %s\n", olsr_ip_to_string(&NewVal));
#endif

  if (NewVal.v4 != OldVal.v4)
  {
    olsr_printf(1, "\tAddress change.\n");
    olsr_printf(1, "\tOld: %s\n", olsr_ip_to_string(&OldVal));
    olsr_printf(1, "\tNew: %s\n", olsr_ip_to_string(&NewVal));

    Int->ip_addr.v4 = NewVal.v4;

    memcpy(&Int->int_addr, &IntInfo.iiAddress, sizeof (struct sockaddr_in));

    if (main_addr.v4 == OldVal.v4)
    {
      olsr_printf(1, "\tMain address change.\n");

      main_addr.v4 = NewVal.v4;
    }

    Res = 1;
  }

  else
    olsr_printf(3, "\tNo address change.\n");

  OldVal.v4 = ((struct sockaddr_in *)&Int->int_netmask)->sin_addr.s_addr;
  NewVal.v4 = ((struct sockaddr_in *)&IntInfo.iiNetmask)->sin_addr.s_addr;

#ifdef DEBUG
  olsr_printf(3, "\tNetmask: %s\n", olsr_ip_to_string(&NewVal));
#endif

  if(NewVal.v4 != OldVal.v4)
  {
    olsr_printf(1, "\tNetmask change.\n");
    olsr_printf(1, "\tOld: %s\n", olsr_ip_to_string(&OldVal));
    olsr_printf(1, "\tNew: %s\n", olsr_ip_to_string(&NewVal));

    memcpy(&Int->int_netmask, &IntInfo.iiNetmask, sizeof (struct sockaddr_in));

    Res = 1;
  }

  else
    olsr_printf(3, "\tNo netmask change.\n");

  OldVal.v4 = ((struct sockaddr_in *)&Int->int_broadaddr)->sin_addr.s_addr;
  NewVal.v4 =
    ((struct sockaddr_in *)&IntInfo.iiBroadcastAddress)->sin_addr.s_addr;

#ifdef DEBUG
  olsr_printf(3, "\tBroadcast address: %s\n", olsr_ip_to_string(&NewVal));
#endif

  if(NewVal.v4 != OldVal.v4)
  {
    olsr_printf(1, "\tBroadcast address change.\n");
    olsr_printf(1, "\tOld: %s\n", olsr_ip_to_string(&OldVal));
    olsr_printf(1, "\tNew: %s\n", olsr_ip_to_string(&NewVal));

    memcpy(&Int->int_broadaddr, &IntInfo.iiBroadcastAddress,
           sizeof (struct sockaddr_in));

    Res = 1;
  }

  else
    olsr_printf(3, "\tNo broadcast address change.\n");

  if (Res != 0)
    for (Walker = ifchgf_list; Walker != NULL; Walker = Walker->next)
      Walker->function(Int, IFCHG_IF_UPDATE);

  return Res;
}

int chk_if_up(struct if_name *IntName, int DebugLevel)
{
  struct interface *New;
  union olsr_ip_addr NullAddr;
  INTERFACE_INFO IntInfo;
  MIB_IFROW IntRow;
  int Index;
  unsigned int AddrSockAddr;
  struct ifchgf *Walker;
  int IsWlan;
  
  if (olsr_cnf->ip_version == AF_INET6)
  {
    fprintf(stderr, "IPv6 not supported by chk_if_up()!\n");
    return 0;
  }

  if (InterfaceInfo(&IntInfo, &Index, IntName) < 0 ||
      InterfaceEntry(&IntRow, &Index, IntName) < 0)
    return 0;

  New = olsr_malloc(sizeof (struct interface), "Interface 1");
      
  memcpy(&New->int_addr, &IntInfo.iiAddress, sizeof (struct sockaddr_in));

  memcpy(&New->int_netmask, &IntInfo.iiNetmask, sizeof (struct sockaddr_in));

  memcpy(&New->int_broadaddr, &IntInfo.iiBroadcastAddress,
         sizeof (struct sockaddr_in));

  if (IntName->cnf->ipv4_broadcast.v4 != 0)
    ((struct sockaddr_in *)&New->int_broadaddr)->sin_addr.s_addr =
      IntName->cnf->ipv4_broadcast.v4;

  New->int_flags = IntInfo.iiFlags;

  New->int_mtu = IntRow.dwMtu;

  New->int_name = olsr_malloc(strlen (IntName->name) + 1, "Interface 2");
  strcpy(New->int_name, IntName->name);

  New->if_nr = IntName->index;

  IsWlan = IsWireless(IntName->name);

  if (IsWlan < 0)
    IsWlan = 1;

  New->is_wireless = IsWlan;
  New->int_metric = IsWlan;

  New->olsr_seqnum = random() & 0xffff;
    
  olsr_printf(1, "\tInterface %s set up for use with index %d\n\n",
              IntName->name, New->if_nr);
      
  olsr_printf(1, "\tMTU: %d\n", New->int_mtu);
  olsr_printf(1, "\tAddress: %s\n", sockaddr_to_string(&New->int_addr));
  olsr_printf(1, "\tNetmask: %s\n", sockaddr_to_string(&New->int_netmask));
  olsr_printf(1, "\tBroadcast address: %s\n",
              sockaddr_to_string(&New->int_broadaddr));

  New->ip_addr.v4 =
    ((struct sockaddr_in *)&New->int_addr)->sin_addr.s_addr;
      
  New->if_index = Index;

  olsr_printf(3, "\tKernel index: %08x\n", New->if_index);

  AddrSockAddr = addrsock.sin_addr.s_addr;
  addrsock.sin_addr.s_addr = New->ip_addr.v4;

  New->olsr_socket = getsocket((struct sockaddr *)&addrsock,
                               bufspace, New->int_name);
      
  addrsock.sin_addr.s_addr = AddrSockAddr;

  if (New->olsr_socket < 0)
  {
    fprintf(stderr, "Could not initialize socket... exiting!\n\n");
    exit(1);
  }

  add_olsr_socket(New->olsr_socket, &olsr_input);

  New->int_next = ifnet;
  ifnet = New;

  IntName->interf = New;
  IntName->configured = 1;

  nbinterf++;

  memset(&NullAddr, 0, ipsize);
  
  if(COMP_IP(&NullAddr, &main_addr))
  {
    COPY_IP(&main_addr, &New->ip_addr);

    olsr_printf(1, "New main address: %s\n", olsr_ip_to_string(&main_addr));
  }

  net_add_buffer(New);

  olsr_register_scheduler_event(&generate_hello, New,
                                IntName->cnf->hello_params.emission_interval,
                                0, NULL);

  olsr_register_scheduler_event(&generate_tc, New,
                                IntName->cnf->tc_params.emission_interval,
                                0, NULL);

  olsr_register_scheduler_event(&generate_mid, New,
                                IntName->cnf->mid_params.emission_interval,
                                0, NULL);

  olsr_register_scheduler_event(&generate_hna, New,
                                IntName->cnf->hna_params.emission_interval,
                                0, NULL);

  /* XXX */

  if(max_jitter == 0 ||
     IntName->cnf->hello_params.emission_interval / 4 < max_jitter)
    max_jitter = IntName->cnf->hello_params.emission_interval / 4;

  if(max_tc_vtime < IntName->cnf->tc_params.emission_interval)
    max_tc_vtime = IntName->cnf->tc_params.emission_interval;

  New->hello_etime =
    double_to_me(IntName->cnf->hello_params.emission_interval);

  New->valtimes.hello = double_to_me(IntName->cnf->hello_params.validity_time);
  New->valtimes.tc = double_to_me(IntName->cnf->tc_params.validity_time);
  New->valtimes.mid = double_to_me(IntName->cnf->mid_params.validity_time);
  New->valtimes.hna = double_to_me(IntName->cnf->hna_params.validity_time);

  for (Walker = ifchgf_list; Walker != NULL; Walker = Walker->next)
    Walker->function(New, IFCHG_IF_ADD);

  return 1;
}

void check_interface_updates(void *dummy)
{
  struct if_name *tmp_if;

#ifdef DEBUG
  olsr_printf(3, "Checking for updates in the interface set\n");
#endif

  for(tmp_if = if_names; tmp_if != NULL; tmp_if = tmp_if->next)
  {
    if(tmp_if->configured)    
      chk_if_changed(tmp_if);

    else
      chk_if_up(tmp_if, 3);
  }
}

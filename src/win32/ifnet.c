
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

#include <winsock2.h>

#include "olsr.h"
#include "olsr_cfg.h"
#include "interfaces.h"
#include "parser.h"
#include "defs.h"
#include "os_net.h"
#include "olsr_timer.h"
#include "olsr_socket.h"
#include "olsr_clock.h"
#include "lq_packet.h"
#include "net_olsr.h"
#include "common/string.h"
#include "olsr_logging.h"
#include "mid_set.h"
#include "hna_set.h"
#include "olsr_logging.h"
#include "link_set.h"

#include <iphlpapi.h>
#include <iprtrmib.h>

#include <arpa/inet.h>

struct MibIpInterfaceRow {
  USHORT Family;
  ULONG64 InterfaceLuid;
  ULONG InterfaceIndex;
  ULONG MaxReassemblySize;
  ULONG64 InterfaceIdentifier;
  ULONG MinRouterAdvertisementInterval;
  ULONG MaxRouterAdvertisementInterval;
  BOOLEAN AdvertisingEnabled;
  BOOLEAN ForwardingEnabled;
  BOOLEAN WeakHostSend;
  BOOLEAN WeakHostReceive;
  BOOLEAN UseAutomaticMetric;
  BOOLEAN UseNeighborUnreachabilityDetection;
  BOOLEAN ManagedAddressConfigurationSupported;
  BOOLEAN OtherStatefulConfigurationSupported;
  BOOLEAN AdvertiseDefaultRoute;
  INT RouterDiscoveryBehavior;
  ULONG DadTransmits;
  ULONG BaseReachableTime;
  ULONG RetransmitTime;
  ULONG PathMtuDiscoveryTimeout;
  INT LinkLocalAddressBehavior;
  ULONG LinkLocalAddressTimeout;
  ULONG ZoneIndices[16];
  ULONG SitePrefixLength;
  ULONG Metric;
  ULONG NlMtu;
  BOOLEAN Connected;
  BOOLEAN SupportsWakeUpPatterns;
  BOOLEAN SupportsNeighborDiscovery;
  BOOLEAN SupportsRouterDiscovery;
  ULONG ReachableTime;
  BYTE TransmitOffload;
  BYTE ReceiveOffload;
  BOOLEAN DisableDefaultRoutes;
};

typedef DWORD(__stdcall * GETIPINTERFACEENTRY)
  (struct MibIpInterfaceRow * Row);

typedef DWORD(__stdcall * GETADAPTERSADDRESSES)
  (ULONG Family, DWORD Flags, PVOID Reserved, PIP_ADAPTER_ADDRESSES pAdapterAddresses, PULONG pOutBufLen);

struct InterfaceInfo {
  unsigned int Index;
  int Mtu;
  int Metric;
  unsigned int Addr;
  unsigned int Mask;
  unsigned int Broad;
  char Guid[39];
};

void WinSockPError(char *);
char *win32_strerror(unsigned int ErrNo);
int GetIntInfo(struct InterfaceInfo *Info, char *Name);

#define MAX_INTERFACES 100

static void
MiniIndexToIntName(char *String, int MiniIndex)
{
  const char *HexDigits = "0123456789abcdef";

  String[0] = 'i';
  String[1] = 'f';

  String[2] = HexDigits[(MiniIndex >> 4) & 15];
  String[3] = HexDigits[MiniIndex & 15];

  String[4] = 0;
}

static int
IntNameToMiniIndex(int *MiniIndex, char *String)
{
  const char *HexDigits = "0123456789abcdef";
  int i, k;
  char ch;

  if ((String[0] != 'i' && String[0] != 'I') || (String[1] != 'f' && String[1] != 'F'))
    return -1;

  *MiniIndex = 0;

  for (i = 2; i < 4; i++) {
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

static int
FriendlyNameToMiniIndex(int *MiniIndex, char *String)
{
  unsigned long BuffLen;
  unsigned long Res;
  IP_ADAPTER_ADDRESSES AdAddr[MAX_INTERFACES], *WalkerAddr;
  char FriendlyName[MAX_INTERFACE_NAME_LEN];
  HMODULE h;
  GETADAPTERSADDRESSES pfGetAdaptersAddresses;

  h = LoadLibrary("iphlpapi.dll");

  if (h == NULL) {
    OLSR_WARN(LOG_NETWORKING, "LoadLibrary() = %08lx", GetLastError());
    return -1;
  }

  pfGetAdaptersAddresses = (GETADAPTERSADDRESSES) GetProcAddress(h, "GetAdaptersAddresses");

  if (pfGetAdaptersAddresses == NULL) {
    OLSR_WARN(LOG_NETWORKING, "Unable to use adapter friendly name (GetProcAddress() = %08lx)\n", GetLastError());
    return -1;
  }

  BuffLen = sizeof(AdAddr);

  Res = pfGetAdaptersAddresses(AF_INET, 0, NULL, AdAddr, &BuffLen);

  if (Res != NO_ERROR) {
    OLSR_WARN(LOG_NETWORKING, "GetAdaptersAddresses() = %08lx", GetLastError());
    return -1;
  }

  for (WalkerAddr = AdAddr; WalkerAddr != NULL; WalkerAddr = WalkerAddr->Next) {
    OLSR_DEBUG(LOG_NETWORKING, "Index = %08x - ", (int)WalkerAddr->IfIndex);

    wcstombs(FriendlyName, WalkerAddr->FriendlyName, MAX_INTERFACE_NAME_LEN);

    OLSR_DEBUG(LOG_NETWORKING, "Friendly name = %s\n", FriendlyName);

    if (strncmp(FriendlyName, String, MAX_INTERFACE_NAME_LEN) == 0)
      break;
  }

  if (WalkerAddr == NULL) {
    OLSR_WARN(LOG_NETWORKING, "No such interface: %s!\n", String);
    return -1;
  }

  *MiniIndex = WalkerAddr->IfIndex & 255;

  return 0;
}

int
GetIntInfo(struct InterfaceInfo *Info, char *Name)
{
  int MiniIndex;
  unsigned char Buff[MAX_INTERFACES * sizeof(MIB_IFROW) + 4];
  MIB_IFTABLE *IfTable;
  unsigned long BuffLen;
  unsigned long Res;
  int TabIdx;
  IP_ADAPTER_INFO AdInfo[MAX_INTERFACES], *Walker;
  HMODULE Lib;
  struct MibIpInterfaceRow Row;
  GETIPINTERFACEENTRY InterfaceEntry;

  if (olsr_cnf->ip_version == AF_INET6) {
    OLSR_WARN(LOG_NETWORKING, "IPv6 not supported by GetIntInfo()!\n");
    return -1;
  }

  if ((Name[0] != 'i' && Name[0] != 'I') || (Name[1] != 'f' && Name[1] != 'F')) {
    if (FriendlyNameToMiniIndex(&MiniIndex, Name) < 0) {
      OLSR_WARN(LOG_NETWORKING, "No such interface: %s!\n", Name);
      return -1;
    }
  }

  else {
    if (IntNameToMiniIndex(&MiniIndex, Name) < 0) {
      OLSR_WARN(LOG_NETWORKING, "No such interface: %s!\n", Name);
      return -1;
    }
  }

  IfTable = (MIB_IFTABLE *) Buff;

  BuffLen = sizeof(Buff);

  Res = GetIfTable(IfTable, &BuffLen, FALSE);

  if (Res != NO_ERROR) {
    OLSR_WARN(LOG_NETWORKING, "GetIfTable() = %08lx, %s", Res, win32_strerror(Res));
    return -1;
  }

  for (TabIdx = 0; TabIdx < (int)IfTable->dwNumEntries; TabIdx++) {
    OLSR_DEBUG(LOG_NETWORKING, "Index = %08x\n", (int)IfTable->table[TabIdx].dwIndex);

    if ((int)(IfTable->table[TabIdx].dwIndex & 255) == MiniIndex)
      break;
  }

  if (TabIdx == (int)IfTable->dwNumEntries) {
    OLSR_WARN(LOG_NETWORKING, "No such interface: %s!\n", Name);
    return -1;
  }

  Info->Index = IfTable->table[TabIdx].dwIndex;
  Info->Mtu = (int)IfTable->table[TabIdx].dwMtu;

  Info->Mtu -= olsr_cnf->ip_version == AF_INET6 ? UDP_IPV6_HDRSIZE : UDP_IPV4_HDRSIZE;

  Lib = LoadLibrary("iphlpapi.dll");

  if (Lib == NULL) {
    OLSR_WARN(LOG_NETWORKING, "Cannot load iphlpapi.dll: %08lx\n", GetLastError());
    return -1;
  }

  InterfaceEntry = (GETIPINTERFACEENTRY) GetProcAddress(Lib, "GetIpInterfaceEntry");

  if (InterfaceEntry == NULL) {
    OLSR_DEBUG(LOG_NETWORKING, "Not running on Vista - setting interface metric to 0.\n");

    Info->Metric = 0;
  }

  else {
    memset(&Row, 0, sizeof(struct MibIpInterfaceRow));

    Row.Family = AF_INET;
    Row.InterfaceIndex = Info->Index;

    Res = InterfaceEntry(&Row);

    if (Res != NO_ERROR) {
      OLSR_WARN(LOG_NETWORKING, "GetIpInterfaceEntry() = %08lx", Res);
      FreeLibrary(Lib);
      return -1;
    }

    Info->Metric = Row.Metric;

    OLSR_DEBUG(LOG_NETWORKING, "Running on Vista - interface metric is %d.\n", Info->Metric);
  }

  FreeLibrary(Lib);

  BuffLen = sizeof(AdInfo);

  Res = GetAdaptersInfo(AdInfo, &BuffLen);

  if (Res != NO_ERROR) {
    OLSR_WARN(LOG_NETWORKING, "GetAdaptersInfo() = %08lx, %s", GetLastError(), win32_strerror(Res));
    return -1;
  }

  for (Walker = AdInfo; Walker != NULL; Walker = Walker->Next) {
    OLSR_DEBUG(LOG_NETWORKING, "Index = %08x\n", (int)Walker->Index);

    if ((int)(Walker->Index & 255) == MiniIndex)
      break;
  }

  if (Walker == NULL) {
    OLSR_WARN(LOG_NETWORKING, "No such interface: %s!\n", Name);
    return -1;
  }

  inet_pton(AF_INET, Walker->IpAddressList.IpAddress.String, &Info->Addr);
  inet_pton(AF_INET, Walker->IpAddressList.IpMask.String, &Info->Mask);

  Info->Broad = Info->Addr | ~Info->Mask;

  strscpy(Info->Guid, Walker->AdapterName, sizeof(Info->Guid));

  if ((IfTable->table[TabIdx].dwOperStatus != MIB_IF_OPER_STATUS_CONNECTED &&
       IfTable->table[TabIdx].dwOperStatus != MIB_IF_OPER_STATUS_OPERATIONAL) || Info->Addr == 0) {
    OLSR_WARN(LOG_NETWORKING, "Interface %s not up!\n", Name);
    return -1;
  }

  return 0;
}

#if !defined OID_802_11_CONFIGURATION
#define OID_802_11_CONFIGURATION 0x0d010211
#endif

#if !defined IOCTL_NDIS_QUERY_GLOBAL_STATS
#define IOCTL_NDIS_QUERY_GLOBAL_STATS 0x00170002
#endif

void
ListInterfaces(void)
{
  IP_ADAPTER_INFO AdInfo[MAX_INTERFACES], *Walker;
  unsigned long AdInfoLen;
  char IntName[5];
  unsigned long Res;

  if (olsr_cnf->ip_version == AF_INET6) {
    OLSR_WARN(LOG_NETWORKING, "IPv6 not supported by ListInterfaces()!\n");
    return;
  }

  AdInfoLen = sizeof(AdInfo);

  Res = GetAdaptersInfo(AdInfo, &AdInfoLen);

  if (Res == ERROR_NO_DATA) {
    OLSR_INFO(LOG_NETWORKING, "No interfaces detected.\n");
    return;
  }

  if (Res != NO_ERROR) {
    OLSR_WARN(LOG_NETWORKING, "GetAdaptersInfo() = %08lx, %s", Res, win32_strerror(Res));
    return;
  }
  // TODO: change to new logging API ?
  for (Walker = AdInfo; Walker != NULL; Walker = Walker->Next) {
    OLSR_DEBUG(LOG_NETWORKING, "Index = %08x\n", (int)Walker->Index);

    MiniIndexToIntName(IntName, Walker->Index);
#if 0
    printf("%s: ", IntName);

    IsWlan = IsWireless(IntName);

    if (IsWlan < 0)
      printf("?");

    else if (IsWlan == 0)
      printf("-");

    else
      printf("+");

    for (Walker2 = &Walker->IpAddressList; Walker2 != NULL; Walker2 = Walker2->Next)
      printf(" %s", Walker2->IpAddress.String);

    printf("\n");
#endif
  }
}

int
chk_if_changed(struct olsr_if_config *olsr_if)
{
  struct ipaddr_str buf;
  struct interface *Int;
  struct InterfaceInfo Info;
  int Res;
  union olsr_ip_addr OldVal, NewVal;
  struct sockaddr_in *AddrIn;

  if (olsr_cnf->ip_version == AF_INET6) {
    OLSR_WARN(LOG_NETWORKING, "IPv6 not supported by chk_if_changed()!\n");
    return 0;
  }

  Int = olsr_if->interf;

  if (GetIntInfo(&Info, olsr_if->name) < 0) {
    remove_interface(olsr_if->interf);
    return 1;
  }

  Res = 0;

  if (Int->int_mtu != Info.Mtu) {
    OLSR_INFO(LOG_NETWORKING, "\tMTU change: %d -> %d.\n", (int)Int->int_mtu, Info.Mtu);

    Int->int_mtu = Info.Mtu;

    net_remove_buffer(Int);
    net_add_buffer(Int);

    Res = 1;
  }

  OldVal.v4 = Int->int_src.v4.sin_addr;
  NewVal.v4.s_addr = Info.Addr;

  OLSR_INFO(LOG_NETWORKING, "\tAddress: %s\n", olsr_ip_to_string(&buf, &NewVal));

  if (NewVal.v4.s_addr != OldVal.v4.s_addr) {
    OLSR_DEBUG(LOG_NETWORKING, "\tAddress change.\n");
    OLSR_DEBUG(LOG_NETWORKING, "\tOld: %s\n", olsr_ip_to_string(&buf, &OldVal));
    OLSR_DEBUG(LOG_NETWORKING, "\tNew: %s\n", olsr_ip_to_string(&buf, &NewVal));

    Int->ip_addr.v4 = NewVal.v4;

    AddrIn = &Int->int_src.v4;

    AddrIn->sin_family = AF_INET;
    AddrIn->sin_port = 0;
    AddrIn->sin_addr = NewVal.v4;

    Res = 1;
  }

  else
    OLSR_DEBUG(LOG_NETWORKING, "\tNo address change.\n");

  OldVal.v4 = Int->int_multicast.v4.sin_addr;
  NewVal.v4.s_addr = Info.Broad;

  OLSR_INFO(LOG_NETWORKING, "\tBroadcast address: %s\n", olsr_ip_to_string(&buf, &NewVal));

  if (NewVal.v4.s_addr != OldVal.v4.s_addr) {
    OLSR_DEBUG(LOG_NETWORKING, "\tBroadcast address change.\n");
    OLSR_DEBUG(LOG_NETWORKING, "\tOld: %s\n", olsr_ip_to_string(&buf, &OldVal));
    OLSR_DEBUG(LOG_NETWORKING, "\tNew: %s\n", olsr_ip_to_string(&buf, &NewVal));

    AddrIn = &Int->int_multicast.v4;

    AddrIn->sin_family = AF_INET;
    AddrIn->sin_port = 0;
    AddrIn->sin_addr = NewVal.v4;

    Res = 1;
  }

  else
    OLSR_DEBUG(LOG_NETWORKING, "\tNo broadcast address change.\n");

  if (Res != 0)
    run_ifchg_cbs(Int, IFCHG_IF_UPDATE);

  return Res;
}

int
os_init_interface(struct interface *ifp, struct olsr_if_config *IntConf)
{
  struct ipaddr_str buf;
  struct InterfaceInfo if_info;
  struct sockaddr_in *if_addr;
  size_t name_size;

  if (olsr_cnf->ip_version == AF_INET6) {
    OLSR_ERROR(LOG_NETWORKING, "IPv6 not supported by win32!\n");
    olsr_exit(1);
  }

  if (GetIntInfo(&if_info, IntConf->name) < 0)
    return 0;

#if 0
  ifp->gen_properties = NULL;
#endif
  if_addr = &ifp->int_src.v4;

  if_addr->sin_family = AF_INET;
  if_addr->sin_port = 0;
  if_addr->sin_addr.s_addr = if_info.Addr;

  if_addr = &ifp->int_multicast.v4;

  if_addr->sin_family = AF_INET;
  if_addr->sin_port = 0;
  if_addr->sin_addr.s_addr = if_info.Broad;

  if (IntConf->cnf->ipv4_broadcast.v4.s_addr != 0)
    if_addr->sin_addr = IntConf->cnf->ipv4_broadcast.v4;

  ifp->int_mtu = if_info.Mtu;

  name_size = strlen(IntConf->name) + 1;
  ifp->int_name = olsr_malloc(name_size, "Interface 2");
  strscpy(ifp->int_name, IntConf->name, name_size);

  ifp->olsr_seqnum = rand() & 0xffff;

  OLSR_INFO(LOG_NETWORKING, "\tInterface %s set up for use with index %d\n\n", IntConf->name, ifp->if_index);

  OLSR_INFO(LOG_NETWORKING, "\tMTU: %d\n", ifp->int_mtu);
  OLSR_INFO(LOG_NETWORKING, "\tAddress: %s\n", ip4_to_string(&buf, ifp->int_src.v4.sin_addr));
  OLSR_INFO(LOG_NETWORKING, "\tBroadcast address: %s\n", ip4_to_string(&buf, ifp->int_multicast.v4.sin_addr));

  ifp->ip_addr.v4 = ifp->int_src.v4.sin_addr;

  ifp->if_index = if_info.Index;

  OLSR_INFO(LOG_NETWORKING, "\tKernel index: %08x\n", ifp->if_index);
  return 0;
}

void
os_cleanup_interface(struct interface *ifp  __attribute__ ((unused))) {
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */


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

#include "interfaces.h"
#include "olsr.h"
#include "parser.h"
#include "defs.h"
#include "net_os.h"
#include "ifnet.h"
#include "generate_msg.h"
#include "scheduler.h"
#include "olsr_time.h"
#include "lq_packet.h"
#include "net_olsr.h"
#include "common/string.h"
#include "olsr_logging.h"

#include <iphlpapi.h>
#include <iprtrmib.h>

#include <arpa/inet.h>

#define BUFSPACE  (127*1024)    /* max. input buffer size to request */


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
char *StrError(unsigned int ErrNo);
int GetIntInfo(struct InterfaceInfo *Info, char *Name);

#define MAX_INTERFACES 100

int __stdcall SignalHandler(unsigned long Signal);

static unsigned long __stdcall
SignalHandlerWrapper(void *Dummy __attribute__ ((unused)))
{
  SignalHandler(0);
  return 0;
}

void
CallSignalHandler(void)
{
  unsigned long ThreadId;              /* Win9x compat */

  CreateThread(NULL, 0, SignalHandlerWrapper, NULL, 0, &ThreadId);
}

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
    OLSR_WARN(LOG_NETWORKING, "GetIfTable() = %08lx, %s", Res, StrError(Res));
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
    OLSR_WARN(LOG_NETWORKING, "GetAdaptersInfo() = %08lx, %s", GetLastError(), StrError(Res));
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

static int
IsWireless(char *IntName)
{
#if !defined WINCE
  struct InterfaceInfo Info;
  char DevName[43];
  HANDLE DevHand;
  unsigned int ErrNo;
  unsigned int Oid;
  unsigned char OutBuff[100];
  unsigned long OutBytes;

  if (GetIntInfo(&Info, IntName) < 0)
    return -1;

  DevName[0] = '\\';
  DevName[1] = '\\';
  DevName[2] = '.';
  DevName[3] = '\\';

  strscpy(DevName + 4, Info.Guid, sizeof(DevName) - 4);

  OLSR_INFO(LOG_NETWORKING, "Checking whether interface %s is wireless.\n", DevName);

  DevHand = CreateFile(DevName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

  if (DevHand == INVALID_HANDLE_VALUE) {
    ErrNo = GetLastError();

    OLSR_WARN(LOG_NETWORKING, "CreateFile() = %08x, %s\n", ErrNo, StrError(ErrNo));
    return -1;
  }

  Oid = OID_802_11_CONFIGURATION;

  if (!DeviceIoControl(DevHand, IOCTL_NDIS_QUERY_GLOBAL_STATS, &Oid, sizeof(Oid), OutBuff, sizeof(OutBuff), &OutBytes, NULL)) {
    ErrNo = GetLastError();

    CloseHandle(DevHand);

    if (ErrNo == ERROR_GEN_FAILURE || ErrNo == ERROR_INVALID_PARAMETER) {
      OLSR_INFO(LOG_NETWORKING, "OID not supported. Device probably not wireless.\n");
      return 0;
    }

    OLSR_WARN(LOG_NETWORKING, "DeviceIoControl() = %08x, %s\n", ErrNo, StrError(ErrNo));
    return -1;
  }

  CloseHandle(DevHand);
#endif
  return 1;
}

void
ListInterfaces(void)
{
  IP_ADAPTER_INFO AdInfo[MAX_INTERFACES], *Walker;
  unsigned long AdInfoLen;
  char IntName[5];
  IP_ADDR_STRING *Walker2;
  unsigned long Res;
  int IsWlan;

  if (olsr_cnf->ip_version == AF_INET6) {
    OLSR_WARN(LOG_NETWORKING, "IPv6 not supported by ListInterfaces()!\n");
    return;
  }

  AdInfoLen = sizeof(AdInfo);

  Res = GetAdaptersInfo(AdInfo, &AdInfoLen);

  if (Res == ERROR_NO_DATA) {
    LOG_INFO(LOG_NETWORKING, "No interfaces detected.\n");
    return;
  }

  if (Res != NO_ERROR) {
    OLSR_WARN(LOG_NETWORKING, "GetAdaptersInfo() = %08lx, %s", Res, StrError(Res));
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
chk_if_changed(struct olsr_if_config *IntConf)
{
  struct ipaddr_str buf;
  struct interface *Int;
  struct InterfaceInfo Info;
  int Res;
  int IsWlan;
  union olsr_ip_addr OldVal, NewVal;
  struct sockaddr_in *AddrIn;

  if (olsr_cnf->ip_version == AF_INET6) {
    OLSR_WARN(LOG_NETWORKING, "IPv6 not supported by chk_if_changed()!\n");
    return 0;
  }

  Int = IntConf->interf;

  if (GetIntInfo(&Info, IntConf->name) < 0) {
    remove_interface(&IntConf->interf);
    return 1;
  }

  Res = 0;

  IsWlan = IsWireless(IntConf->name);

  if (IsWlan < 0)
    IsWlan = 1;

  if (Int->is_wireless != IsWlan) {
    OLSR_INFO(LOG_NETWORKING, "\tLAN/WLAN change: %d -> %d.\n", Int->is_wireless, IsWlan);

    Int->is_wireless = IsWlan;

    if (IntConf->cnf->weight.fixed)
      Int->int_metric = IntConf->cnf->weight.value;

    else
      Int->int_metric = Info.Metric;

    Res = 1;
  }

  if (Int->int_mtu != Info.Mtu) {
    OLSR_INFO(LOG_NETWORKING, "\tMTU change: %d -> %d.\n", (int)Int->int_mtu, Info.Mtu);

    Int->int_mtu = Info.Mtu;

    net_remove_buffer(Int);
    net_add_buffer(Int);

    Res = 1;
  }

  OldVal.v4 = Int->int_addr.sin_addr;
  NewVal.v4.s_addr = Info.Addr;

  OLSR_INFO(LOG_NETWORKING, "\tAddress: %s\n", olsr_ip_to_string(&buf, &NewVal));

  if (NewVal.v4.s_addr != OldVal.v4.s_addr) {
    OLSR_DEBUG(LOG_NETWORKING, "\tAddress change.\n");
    OLSR_DEBUG(LOG_NETWORKING, "\tOld: %s\n", olsr_ip_to_string(&buf, &OldVal));
    OLSR_DEBUG(LOG_NETWORKING, "\tNew: %s\n", olsr_ip_to_string(&buf, &NewVal));

    Int->ip_addr.v4 = NewVal.v4;

    AddrIn = &Int->int_addr;

    AddrIn->sin_family = AF_INET;
    AddrIn->sin_port = 0;
    AddrIn->sin_addr = NewVal.v4;

    if (!olsr_cnf->fixed_origaddr && olsr_cnf->router_id.v4.s_addr == OldVal.v4.s_addr) {
      OLSR_INFO(LOG_NETWORKING, "\tMain address change.\n");

      olsr_cnf->router_id.v4 = NewVal.v4;
    }

    Res = 1;
  }

  else
    OLSR_DEBUG(LOG_NETWORKING, "\tNo address change.\n");

  OldVal.v4 = ((struct sockaddr_in *)&Int->int_netmask)->sin_addr;
  NewVal.v4.s_addr = Info.Mask;

  OLSR_INFO(LOG_NETWORKING, "\tNetmask: %s\n", olsr_ip_to_string(&buf, &NewVal));

  if (NewVal.v4.s_addr != OldVal.v4.s_addr) {
    OLSR_DEBUG(LOG_NETWORKING, "\tNetmask change.\n");
    OLSR_DEBUG(LOG_NETWORKING, "\tOld: %s\n", olsr_ip_to_string(&buf, &OldVal));
    OLSR_DEBUG(LOG_NETWORKING, "\tNew: %s\n", olsr_ip_to_string(&buf, &NewVal));

    AddrIn = (struct sockaddr_in *)&Int->int_netmask;

    AddrIn->sin_family = AF_INET;
    AddrIn->sin_port = 0;
    AddrIn->sin_addr = NewVal.v4;

    Res = 1;
  }

  else
    OLSR_DEBUG(LOG_NETWORKING, "\tNo netmask change.\n");

  OldVal.v4 = Int->int_broadaddr.sin_addr;
  NewVal.v4.s_addr = Info.Broad;

  OLSR_INFO(LOG_NETWORKING, "\tBroadcast address: %s\n", olsr_ip_to_string(&buf, &NewVal));

  if (NewVal.v4.s_addr != OldVal.v4.s_addr) {
    OLSR_DEBUG(LOG_NETWORKING, "\tBroadcast address change.\n");
    OLSR_DEBUG(LOG_NETWORKING, "\tOld: %s\n", olsr_ip_to_string(&buf, &OldVal));
    OLSR_DEBUG(LOG_NETWORKING, "\tNew: %s\n", olsr_ip_to_string(&buf, &NewVal));

    AddrIn = &Int->int_broadaddr;

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
chk_if_up(struct olsr_if_config *IntConf)
{
  struct ipaddr_str buf;
  struct InterfaceInfo Info;
  struct interface *New;
  union olsr_ip_addr NullAddr;
  int IsWlan;
  struct sockaddr_in *AddrIn;
  size_t name_size;

  if (olsr_cnf->ip_version == AF_INET6) {
    OLSR_WARN(LOG_NETWORKING, "IPv6 not supported by chk_if_up()!\n");
    return 0;
  }

  if (GetIntInfo(&Info, IntConf->name) < 0)
    return 0;

  New = olsr_cookie_malloc(interface_mem_cookie);

  New->immediate_send_tc = (olsr_cnf->tc_params.emission_interval < IntConf->cnf->hello_params.emission_interval);
#if 0
  New->gen_properties = NULL;
#endif
  AddrIn = &New->int_addr;

  AddrIn->sin_family = AF_INET;
  AddrIn->sin_port = 0;
  AddrIn->sin_addr.s_addr = Info.Addr;

  AddrIn = (struct sockaddr_in *)&New->int_netmask;

  AddrIn->sin_family = AF_INET;
  AddrIn->sin_port = 0;
  AddrIn->sin_addr.s_addr = Info.Mask;

  AddrIn = &New->int_broadaddr;

  AddrIn->sin_family = AF_INET;
  AddrIn->sin_port = 0;
  AddrIn->sin_addr.s_addr = Info.Broad;

  if (IntConf->cnf->ipv4_broadcast.v4.s_addr != 0)
    AddrIn->sin_addr = IntConf->cnf->ipv4_broadcast.v4;

  New->int_flags = 0;

  New->is_hcif = false;

  New->int_mtu = Info.Mtu;

  name_size = strlen(IntConf->name) + 1;
  New->int_name = olsr_malloc(name_size, "Interface 2");
  strscpy(New->int_name, IntConf->name, name_size);

  IsWlan = IsWireless(IntConf->name);

  if (IsWlan < 0)
    IsWlan = 1;

  New->is_wireless = IsWlan;

  if (IntConf->cnf->weight.fixed)
    New->int_metric = IntConf->cnf->weight.value;

  else
    New->int_metric = Info.Metric;

  New->olsr_seqnum = rand() & 0xffff;

  New->ttl_index = -32;         /* For the first 32 TC's, fish-eye is disabled */

  OLSR_INFO(LOG_NETWORKING, "\tInterface %s set up for use with index %d\n\n", IntConf->name, New->if_index);

  OLSR_INFO(LOG_NETWORKING, "\tMTU: %d\n", New->int_mtu);
  OLSR_INFO(LOG_NETWORKING, "\tAddress: %s\n", ip4_to_string(&buf, New->int_addr.sin_addr));
  OLSR_INFO(LOG_NETWORKING, "\tNetmask: %s\n", ip4_to_string(&buf, ((struct sockaddr_in *)&New->int_netmask)->sin_addr));
  OLSR_INFO(LOG_NETWORKING, "\tBroadcast address: %s\n", ip4_to_string(&buf, New->int_broadaddr.sin_addr));

  New->ip_addr.v4 = New->int_addr.sin_addr;

  New->if_index = Info.Index;

  OLSR_INFO(LOG_NETWORKING, "\tKernel index: %08x\n", New->if_index);

  New->olsr_socket = getsocket(BUFSPACE, New->int_name);

  if (New->olsr_socket < 0) {
    OLSR_ERROR(LOG_NETWORKING, "Could not initialize socket... exiting!\n\n");
    olsr_exit(1);
  }

  add_olsr_socket(New->olsr_socket, &olsr_input, NULL, NULL, SP_PR_READ);

  /* Queue */
  list_node_init(&New->int_node);
  list_add_before(&interface_head, &New->int_node);

  IntConf->interf = New;
  lock_interface(IntConf->interf);

  memset(&NullAddr, 0, olsr_cnf->ipsize);

  if (!olsr_cnf->fixed_origaddr && olsr_ipcmp(&NullAddr, &olsr_cnf->router_id) == 0) {
    olsr_cnf->router_id = New->ip_addr;
    OLSR_INFO(LOG_NETWORKING, "New main address: %s\n", olsr_ip_to_string(&buf, &olsr_cnf->router_id));
  }

  net_add_buffer(New);

  /*
   * Register functions for periodic message generation
   */
  New->hello_gen_timer =
    olsr_start_timer(IntConf->cnf->hello_params.emission_interval,
                     HELLO_JITTER, OLSR_TIMER_PERIODIC, &olsr_output_lq_hello, New, hello_gen_timer_cookie->ci_id);
  New->tc_gen_timer =
    olsr_start_timer(olsr_cnf->tc_params.emission_interval,
                     TC_JITTER, OLSR_TIMER_PERIODIC, &olsr_output_lq_tc, New, tc_gen_timer_cookie->ci_id);
  New->mid_gen_timer =
    olsr_start_timer(olsr_cnf->mid_params.emission_interval,
                     MID_JITTER, OLSR_TIMER_PERIODIC, &generate_mid, New, mid_gen_timer_cookie->ci_id);
  New->hna_gen_timer =
    olsr_start_timer(olsr_cnf->hna_params.emission_interval,
                     HNA_JITTER, OLSR_TIMER_PERIODIC, &generate_hna, New, hna_gen_timer_cookie->ci_id);

  New->hello_etime = (uint32_t) (IntConf->cnf->hello_params.emission_interval);
  New->hello_valtime = reltime_to_me(IntConf->cnf->hello_params.validity_time);

  New->mode = IntConf->cnf->mode;

  /*
   * Call possible ifchange functions registered by plugins
   */
  run_ifchg_cbs(New, IFCHG_IF_ADD);

  lock_interface(New);

  return 1;
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */


/*
OLSR MDNS plugin.
Written by Saverio Proto <zioproto@gmail.com> and Claudio Pisa <clauz@ninux.org>.

    This file is part of OLSR MDNS PLUGIN.

    The OLSR MDNS PLUGIN is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    The OLSR MDNS PLUGIN is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Foobar.  If not, see <http://www.gnu.org/licenses/>.


 */


#ifndef _BMF_NETWORKINTERFACES_H
#define _BMF_NETWORKINTERFACES_H

/* System includes */
#include <netinet/in.h>         /* struct in_addr */

/* OLSR includes */
#include "olsr_types.h"         /* olsr_ip_addr */
#include "plugin.h"             /* union set_plugin_parameter_addon */

/* Plugin includes */
#include "Packet.h"             /* IFHWADDRLEN */
#include "mdns.h"

/* Size of buffer in which packets are received */
#define BMF_BUFFER_SIZE 2048

struct TBmfInterface {
  /* File descriptor of raw packet socket, used for capturing multicast packets */
  int capturingSkfd;

  /* File descriptor of UDP (datagram) socket for encapsulated multicast packets.
   * Only used for OLSR-enabled interfaces; set to -1 if interface is not OLSR-enabled. */
  int encapsulatingSkfd;

  /* File descriptor of UDP packet socket, used for listening to encapsulation packets.
   * Used only when PlParam "BmfMechanism" is set to "UnicastPromiscuous". */
  int listeningSkfd;

  unsigned char macAddr[IFHWADDRLEN];

  char ifName[IFNAMSIZ];

  /* OLSRs idea of this network interface. NULL if this interface is not
   * OLSR-enabled. */
  struct interface *olsrIntf;

  /* IP address of this network interface */
  union olsr_ip_addr intAddr;

  /* Broadcast address of this network interface */
  union olsr_ip_addr broadAddr;

#define FRAGMENT_HISTORY_SIZE 10
  struct TFragmentHistory {
    u_int16_t ipId;
    u_int8_t ipProto;
    struct in_addr ipSrc;
    struct in_addr ipDst;
  } fragmentHistory[FRAGMENT_HISTORY_SIZE];

  int nextFragmentHistoryEntry;

  /* Number of received and transmitted BMF packets on this interface */
  u_int32_t nBmfPacketsRx;
  u_int32_t nBmfPacketsRxDup;
  u_int32_t nBmfPacketsTx;

  /* Next element in list */
  struct TBmfInterface *next;
};

extern struct TBmfInterface *BmfInterfaces;

extern int my_MDNS_TTL;

extern int HighestSkfd;
extern fd_set InputSet;

extern int EtherTunTapFd;

extern char EtherTunTapIfName[];

/* 10.255.255.253 in host byte order */
#define ETHERTUNTAPDEFAULTIP 0x0AFFFFFD

extern u_int32_t EtherTunTapIp;
extern u_int32_t EtherTunTapIpMask;
extern u_int32_t EtherTunTapIpBroadcast;


enum TBmfMechanism { BM_BROADCAST = 0, BM_UNICAST_PROMISCUOUS };
extern enum TBmfMechanism BmfMechanism;

int SetBmfInterfaceName(const char *ifname, void *data, set_plugin_parameter_addon addon);
int SetBmfInterfaceIp(const char *ip, void *data, set_plugin_parameter_addon addon);
int SetCapturePacketsOnOlsrInterfaces(const char *enable, void *data, set_plugin_parameter_addon addon);
int SetBmfMechanism(const char *mechanism, void *data, set_plugin_parameter_addon addon);
int DeactivateSpoofFilter(void);
void RestoreSpoofFilter(void);

#define MAX_UNICAST_NEIGHBORS 10
struct TBestNeighbors {
  struct link_entry *links[MAX_UNICAST_NEIGHBORS];
};

void FindNeighbors(struct TBestNeighbors *neighbors,
                   struct link_entry **bestNeighbor,
                   struct TBmfInterface *intf,
                   union olsr_ip_addr *source,
                   union olsr_ip_addr *forwardedBy, union olsr_ip_addr *forwardedTo, int *nPossibleNeighbors);

int CreateBmfNetworkInterfaces(struct interface *skipThisIntf);
void AddInterface(struct interface *newIntf);
void CloseBmfNetworkInterfaces(void);
int AddNonOlsrBmfIf(const char *ifName, void *data, set_plugin_parameter_addon addon);
int set_MDNS_TTL(const char *MDNS_TTL, void *data, set_plugin_parameter_addon addon);
int IsNonOlsrBmfIf(const char *ifName);
void CheckAndUpdateLocalBroadcast(unsigned char *ipPacket, union olsr_ip_addr *broadAddr);
void AddMulticastRoute(void);
void DeleteMulticastRoute(void);

#endif /* _BMF_NETWORKINTERFACES_H */

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

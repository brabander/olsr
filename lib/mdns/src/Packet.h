
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


#ifndef _MDNS_PACKET_H
#define _MDNS_PACKET_H


/* System includes */
#include <net/if.h>             /* IFNAMSIZ, IFHWADDRLEN */
#include <sys/types.h>          /* u_int8_t, u_int16_t */

/* BMF-encapsulated packets are Ethernet-IP-UDP packets, which start
 * with a 8-bytes BMF header (struct TEncapHeader), followed by the
 * encapsulated Ethernet-IP packet itself */

struct TEncapHeader {
  /* Use a standard Type-Length-Value (TLV) element */
  u_int8_t type;
  u_int8_t len;
  u_int16_t reserved;                  /* Always 0 */
  u_int32_t crc32;
} __attribute__ ((__packed__));

#define ENCAP_HDR_LEN ((int)sizeof(struct TEncapHeader))
#define BMF_ENCAP_TYPE 1
#define BMF_ENCAP_LEN 6

struct TSaveTtl {
  u_int8_t ttl;
  u_int16_t check;
} __attribute__ ((__packed__));

int IsIpFragment(unsigned char *ipPacket);
u_int16_t GetIpTotalLength(unsigned char *ipPacket);
unsigned int GetIpHeaderLength(unsigned char *ipPacket);
u_int8_t GetTtl(unsigned char *ipPacket);
void SaveTtlAndChecksum(unsigned char *ipPacket, struct TSaveTtl *sttl);
void RestoreTtlAndChecksum(unsigned char *ipPacket, struct TSaveTtl *sttl);
void DecreaseTtlAndUpdateHeaderChecksum(unsigned char *ipPacket);
struct ip *GetIpHeader(unsigned char *encapsulationUdpData);
unsigned char *GetIpPacket(unsigned char *encapsulationUdpData);

#endif /* _MDNS_PACKET_H */

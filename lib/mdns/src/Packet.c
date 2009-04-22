
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

#include "Packet.h"

/* System includes */
#include <stddef.h>             /* NULL */
#include <assert.h>             /* assert() */
#include <string.h>             /* memcpy() */
#include <sys/types.h>          /* u_int8_t, u_int16_t, u_int32_t */
#include <netinet/in.h>         /* ntohs(), htons() */
#include <netinet/ip.h>         /* struct iphdr */

/* -------------------------------------------------------------------------
 * Function   : IsIpFragment
 * Description: Check if an IP packet is an IP fragment
 * Input      : ipPacket - the IP packet
 * Output     : none
 * Return     : true (1) or false (0)
 * Data Used  : none
 * ------------------------------------------------------------------------- */
int
IsIpFragment(unsigned char *ipPacket)
{
  struct ip *iph;

  assert(ipPacket != NULL);

  iph = (struct ip *)ipPacket;
  if ((ntohs(iph->ip_off) & IP_OFFMASK) != 0) {
    return 1;
  }
  return 0;
}                               /* IsIpFragment */

/* -------------------------------------------------------------------------
 * Function   : GetIpTotalLength
 * Description: Retrieve the total length of the IP packet (in bytes) of
 *              an IP packet
 * Input      : ipPacket - the IP packet
 * Output     : none
 * Return     : IP packet length
 * Data Used  : none
 * ------------------------------------------------------------------------- */
u_int16_t
GetIpTotalLength(unsigned char *ipPacket)
{
  struct iphdr *iph;

  assert(ipPacket != NULL);

  iph = (struct iphdr *)ipPacket;
  return ntohs(iph->tot_len);
}                               /* GetIpTotalLength */

/* -------------------------------------------------------------------------
 * Function   : GetIpHeaderLength
 * Description: Retrieve the IP header length (in bytes) of an IP packet
 * Input      : ipPacket - the IP packet
 * Output     : none
 * Return     : IP header length
 * Data Used  : none
 * ------------------------------------------------------------------------- */
unsigned int
GetIpHeaderLength(unsigned char *ipPacket)
{
  struct iphdr *iph;

  assert(ipPacket != NULL);

  iph = (struct iphdr *)ipPacket;
  return iph->ihl << 2;
}                               /* GetIpHeaderLength */

/* -------------------------------------------------------------------------
 * Function   : GetTtl
 * Description: Retrieve the TTL (Time To Live) value from the IP header of
 *              an IP packet
 * Input      : ipPacket - the IP packet
 * Output     : none
 * Return     : TTL value
 * Data Used  : none
 * ------------------------------------------------------------------------- */
u_int8_t
GetTtl(unsigned char *ipPacket)
{
  struct iphdr *iph;

  assert(ipPacket != NULL);

  iph = (struct iphdr *)ipPacket;
  return iph->ttl;
}                               /* GetTtl */

/* -------------------------------------------------------------------------
 * Function   : SaveTtlAndChecksum
 * Description: Save the TTL (Time To Live) value and IP checksum as found in
 *              the IP header of an IP packet
 * Input      : ipPacket - the IP packet
 * Output     : sttl - the TTL and checksum values
 * Return     : none
 * Data Used  : none
 * ------------------------------------------------------------------------- */
void
SaveTtlAndChecksum(unsigned char *ipPacket, struct TSaveTtl *sttl)
{
  struct iphdr *iph;

  assert(ipPacket != NULL && sttl != NULL);

  iph = (struct iphdr *)ipPacket;
  sttl->ttl = iph->ttl;
  sttl->check = ntohs(iph->check);
}                               /* SaveTtlAndChecksum */

/* -------------------------------------------------------------------------
 * Function   : RestoreTtlAndChecksum
 * Description: Restore the TTL (Time To Live) value and IP checksum in
 *              the IP header of an IP packet
 * Input      : ipPacket - the IP packet
 *              sttl - the TTL and checksum values
 * Output     : none
 * Return     : none
 * Data Used  : none
 * ------------------------------------------------------------------------- */
void
RestoreTtlAndChecksum(unsigned char *ipPacket, struct TSaveTtl *sttl)
{
  struct iphdr *iph;

  assert(ipPacket != NULL && sttl != NULL);

  iph = (struct iphdr *)ipPacket;
  iph->ttl = sttl->ttl;
  iph->check = htons(sttl->check);
}                               /* RestoreTtlAndChecksum */

/* -------------------------------------------------------------------------
 * Function   : DecreaseTtlAndUpdateHeaderChecksum
 * Description: For an IP packet, decrement the TTL value and update the IP header
 *              checksum accordingly.
 * Input      : ipPacket - the IP packet
 * Output     : none
 * Return     : none
 * Data Used  : none
 * Notes      : See also RFC1141
 * ------------------------------------------------------------------------- */
void
DecreaseTtlAndUpdateHeaderChecksum(unsigned char *ipPacket)
{
  struct iphdr *iph;
  u_int32_t sum;

  assert(ipPacket != NULL);

  iph = (struct iphdr *)ipPacket;

  iph->ttl--;                   /* decrement ttl */
  sum = ntohs(iph->check) + 0x100;      /* increment checksum high byte */
  iph->check = htons(sum + (sum >> 16));        /* add carry */
}                               /* DecreaseTtlAndUpdateHeaderChecksum */

/* -------------------------------------------------------------------------
 * Function   : GetIpHeader
 * Description: Retrieve the IP header from BMF encapsulation UDP data
 * Input      : encapsulationUdpData - the encapsulation UDP data
 * Output     : none
 * Return     : IP header
 * Data Used  : none
 * ------------------------------------------------------------------------- */
struct ip *
GetIpHeader(unsigned char *encapsulationUdpData)
{
  return (struct ip *)(encapsulationUdpData + ENCAP_HDR_LEN);
}                               /* GetIpHeader */

/* -------------------------------------------------------------------------
 * Function   : GetIpPacket
 * Description: Retrieve the IP packet from BMF encapsulation UDP data
 * Input      : encapsulationUdpData - the encapsulation UDP data
 * Output     : none
 * Return     : The IP packet
 * Data Used  : none
 * ------------------------------------------------------------------------- */
unsigned char *
GetIpPacket(unsigned char *encapsulationUdpData)
{
  return encapsulationUdpData + ENCAP_HDR_LEN;
}                               /* GetIpPacket */




/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

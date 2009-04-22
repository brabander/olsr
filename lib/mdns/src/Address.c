
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


#include "Address.h"

/* System includes */
#include <stddef.h>             /* NULL */
#include <string.h>             /* strcmp */
#include <assert.h>             /* assert() */
#include <netinet/ip.h>         /* struct ip */
#include <netinet/udp.h>        /* struct udphdr */

/* OLSRD includes */
#include "defs.h"               /* ipequal */
#include "olsr_protocol.h"      /* OLSRPORT */

/* Plugin includes */
#include "mdns.h"               /* BMF_ENCAP_PORT */
#include "NetworkInterfaces.h"  /* TBmfInterface */

/* Whether or not to flood local broadcast packets (e.g. packets with IP
 * destination 192.168.1.255). May be overruled by setting the plugin
 * parameter "DoLocalBroadcast" to "no" */
//int EnableLocalBroadcast = 1;

/* -------------------------------------------------------------------------
 * Function   : IsMulticast
 * Description: Check if an IP address is a multicast address
 * Input      : ipAddress
 * Output     : none
 * Return     : true (1) or false (0)
 * Data Used  : none
 * ------------------------------------------------------------------------- */
int
IsMulticast(union olsr_ip_addr *ipAddress)
{
  assert(ipAddress != NULL);

  return (ntohl(ipAddress->v4.s_addr) & 0xF0000000) == 0xE0000000;
}

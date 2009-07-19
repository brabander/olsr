
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


#ifndef _MDNS_MDNS_H
#define _MDNS_MDNS_H


#include "plugin.h"             /* union set_plugin_parameter_addon */
#include "duplicate_set.h"
#include "parser.h"

#define MESSAGE_TYPE 132
#define PARSER_TYPE		MESSAGE_TYPE
#define EMISSION_INTERVAL       10      /* seconds */
#define EMISSION_JITTER         25      /* percent */
#define MDNS_VALID_TIME          1800   /* seconds */

/* BMF plugin data */
#define PLUGIN_NAME "OLSRD MDNS plugin"
#define PLUGIN_NAME_SHORT "OLSRD MDNS"
#define PLUGIN_VERSION "1.0.0 (" __DATE__ " " __TIME__ ")"
#define PLUGIN_COPYRIGHT "  (C) Ninux.org"
#define PLUGIN_AUTHOR "  Saverio Proto (zioproto@gmail.com)"
#define MOD_DESC PLUGIN_NAME " " PLUGIN_VERSION "\n" PLUGIN_COPYRIGHT "\n" PLUGIN_AUTHOR
#define PLUGIN_INTERFACE_VERSION 5

/* UDP-Port on which multicast packets are encapsulated */
//#define BMF_ENCAP_PORT 50698

/* Forward declaration of OLSR interface type */
struct interface;

//extern int FanOutLimit;
//extern int BroadcastRetransmitCount;

void DoMDNS(int sd, void *x, unsigned int y);
void BmfPError(const char *format, ...) __attribute__ ((format(printf, 1, 2)));
union olsr_ip_addr *MainAddressOf(union olsr_ip_addr *ip);
//int InterfaceChange(struct interface* interf, int action);
//int SetFanOutLimit(const char* value, void* data, set_plugin_parameter_addon addon);
//int InitBmf(struct interface* skipThisIntf);
//void CloseBmf(void);
int InitMDNS(struct interface *skipThisIntf);
void CloseMDNS(void);

void olsr_mdns_gen(unsigned char *packet, int len);

/* Parser function to register with the scheduler */
void olsr_parser(union olsr_message *, struct interface *, union olsr_ip_addr *, enum duplicate_status);

#endif /* _MDNS_MDNS_H */

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */


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


#ifndef _MDNS_ADDRESS_H
#define _MDNS_ADDRESS_H

#include "olsr_types.h"         /* olsr_ip_addr */
#include "plugin.h"             /* union set_plugin_parameter_addon */
#include "interfaces.h"         /* struct interface */

struct TBmfInterface;

int IsMulticast(union olsr_ip_addr *ipAddress);

#endif /* _MDNS_ADDRESS_H */

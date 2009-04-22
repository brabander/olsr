
/*
 * OLSRd Quagga plugin
 *
 * Copyright (C) 2006-2008 Immo 'FaUl' Wehrenberg <immo@chaostreff-dortmund.de>
 * Copyright (C) 2007-2008 Vasilis Tsiligiannis <acinonyxs@yahoo.gr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation or - at your option - under
 * the terms of the GNU General Public Licence version 2 but can be
 * linked to any BSD-Licenced Software with public available sourcecode
 *
 */

/* -------------------------------------------------------------------------
 * File               : quagga.h
 * Description        : header file for quagga.c
 * ------------------------------------------------------------------------- */


#include "routing_table.h"      /* rt_entry */
#include "process_routes.h"     /* export_route_function */
#include "olsr_types.h"         /* olsr_ip_addr */

#include <stdint.h>
#include <stdlib.h>

#define HAVE_SOCKLEN_T

/* Zebra port */
#ifndef ZEBRA_PORT
#define ZEBRA_PORT 2600
#endif

/* Zebra version */
#ifdef ZEBRA_HEADER_MARKER
#ifndef ZSERV_VERSION
#define ZSERV_VERSION 1
#endif
#endif

/* Zebra socket */
#define ZEBRA_SOCKET "/var/run/quagga/zserv.api"

/* Zebra packet size */
#define ZEBRA_MAX_PACKET_SIZ          4096

/* Zebra message types */
#define ZEBRA_IPV4_ROUTE_ADD		7
#define ZEBRA_IPV4_ROUTE_DELETE		8
#define ZEBRA_REDISTRIBUTE_ADD		11
#define ZEBRA_REDISTRIBUTE_DELETE	12
#define ZEBRA_MESSAGE_MAX		23

/* Zebra route types */
#define ZEBRA_ROUTE_OLSR		11
#define ZEBRA_ROUTE_MAX			13

/* Zebra flags */
#define ZEBRA_FLAG_SELECTED		0x10

/* Zebra nexthop flags */
#define ZEBRA_NEXTHOP_IFINDEX		1
#define ZEBRA_NEXTHOP_IPV4		3

/* Zebra message flags */
#define ZAPI_MESSAGE_NEXTHOP  0x01
#define ZAPI_MESSAGE_IFINDEX  0x02
#define ZAPI_MESSAGE_DISTANCE 0x04
#define ZAPI_MESSAGE_METRIC   0x08

/* Buffer size */
#define BUFSIZE 1024

/* Quagga plugin flags */
#define STATUS_CONNECTED 1
#define OPTION_EXPORT 1


struct zebra_route {
  unsigned char type;
  unsigned char flags;
  unsigned char message;
  unsigned char prefixlen;
  union olsr_ip_addr prefix;
  unsigned char nexthop_num;
  union olsr_ip_addr *nexthop;
  unsigned char ifindex_num;
  uint32_t *ifindex;
  uint32_t metric;
  uint8_t distance;
};

extern export_route_function orig_add_route_function;
extern export_route_function orig_del_route_function;

void init_zebra(void);
void zebra_cleanup(void);
void zebra_parse(void *);
int zebra_redistribute(unsigned char);
int zebra_disable_redistribute(unsigned char);
int zebra_add_route(const struct rt_entry *);
int zebra_del_route(const struct rt_entry *);
int zebra_add_route_hook(const struct rt_entry *, int);
int zebra_del_route_hook(const struct rt_entry *, int);
void zebra_olsr_localpref(void);
void zebra_olsr_distance(unsigned char);
void zebra_export_routes(unsigned char);

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

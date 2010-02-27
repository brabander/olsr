
/***************************************************************************
 projekt              : olsrd-quagga
 file                 : quagga.h
 usage                : header for quagga.c
 copyright            : (C) 2006 by Immo 'FaUl' Wehrenberg
 e-mail               : immo@chaostreff-dortmund.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License version 2 as     *
 *   published by the Free Software Foundation.                            *
 *                                                                         *
 ***************************************************************************/

#include "routing_table.h"

#include <stdint.h>
#include <stdlib.h>

#define HAVE_SOCKLEN_T

#ifndef ZEBRA_PORT
#define ZEBRA_PORT 2600
#endif

#ifndef ZEBRA_HEADER_MARKER
#define ZEBRA_HEADER_MARKER 255
#endif

#ifndef ZSERV_VERSION
#define ZSERV_VERSION 1
#endif

struct ipv4_route {
  unsigned char type;
  unsigned char flags;
  unsigned char message;
  unsigned char prefixlen;
  union olsr_ip_addr prefix;
  unsigned char nh_count;
  union olsr_ip_addr *nexthop;
  unsigned char ind_num;
  uint32_t *index;
  uint32_t metric;
  uint8_t distance;
};

void init_zebra(void);
void zebra_cleanup(void);
unsigned char zebra_send_command(unsigned char *);
#if 0
int zebra_add_v4_route(const struct ipv4_route r);
int zebra_delete_v4_route(const struct ipv4_route r);
void zebra_check(void *);
int zebra_parse_packet(unsigned char *, ssize_t);
int zebra_redistribute(unsigned char);
int zebra_disable_redistribute(unsigned char);
int add_hna4_route(struct ipv4_route);
int delete_hna4_route(struct ipv4_route);
#endif
void *my_realloc(void *, size_t, const char *);
int zebra_add_route(const struct rt_entry *);
int zebra_del_route(const struct rt_entry *);
void zebra_olsr_localpref(void);
void zebra_olsr_distance(unsigned char);
void zebra_export_routes(unsigned char);

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2004 Andreas Tønnesen (andreto@ifi.uio.no)
 *
 * This file is part of the olsr.org OLSR daemon.
 *
 * olsr.org is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * olsr.org is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with olsr.org; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */


#ifndef _OLSR_MSG_PARSER
#define _OLSR_MSG_PARSER

#include "olsr_protocol.h"
#include "packet.h"

#define PROMISCUOUS 0xffffffff

struct parse_function_entry
{
  int type; /* If set to PROMISCUOUS all messages will be received */
  int caller_forwarding; /* If set to 0 this entry is not registered as forwarding packets */
  void (*function)(union olsr_message *, struct interface *, union olsr_ip_addr *);
  struct parse_function_entry *next;
};


struct parse_function_entry *parse_functions;

void
olsr_init_parser();

void 
olsr_input(int);

void
olsr_parser_add_function(void (*)(union olsr_message *, struct interface *, union olsr_ip_addr *), int, int);

int
olsr_parser_remove_function(void (*)(union olsr_message *, struct interface *, union olsr_ip_addr *), int, int);

void
parse_packet(struct olsr *, int, struct interface *, union olsr_ip_addr *);

#endif

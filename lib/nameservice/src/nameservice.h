/*
 * Copyright (c) 2005, Bruno Randolf <bruno.randolf@4g-systems.biz>
 * Copyright (c) 2004, Andreas Tønnesen(andreto-at-olsr.org)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met:
 *
 * * Redistributions of source code must retain the above copyright notice, 
 *   this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice, 
 *   this list of conditions and the following disclaimer in the documentation 
 *   and/or other materials provided with the distribution.
 * * Neither the name of the UniK olsr daemon nor the names of its contributors 
 *   may be used to endorse or promote products derived from this software 
 *   without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED 
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* $Id: nameservice.h,v 1.2 2005/03/01 20:16:56 tlopatic Exp $ */
 
/*
 * Dynamic linked library for UniK OLSRd
 */

#ifndef _NAMESERVICE_PLUGIN
#define _NAMESERVICE_PLUGIN

#include "olsrd_plugin.h"


#define PLUGIN_NAME	"OLSRD nameservice plugin"
#define PLUGIN_VERSION	"0.1"
#define PLUGIN_AUTHOR	"Bruno Randolf"


#define MESSAGE_TYPE		129
#define PARSER_TYPE		MESSAGE_TYPE
#define EMISSION_INTERVAL	30
#define NAME_VALID_TIME		90.0

#define MAX_NAME 255
char* my_name;
olsr_u8_t my_name_len;

#define MAX_FILE 255
char* my_filename;


/* Database entry */
struct name_entry
{
  union olsr_ip_addr originator;  /* IP address of the node this entry describes */
  struct timeval     timer;       /* Validity time */
  char* name;
  struct name_entry    *next;       /* Next element in line */
  struct name_entry    *prev;       /* Previous elemnt in line */
};


/* Timeout function to register with the sceduler */
void
olsr_timeout(void);

/* Parser function to register with the sceduler */
void
olsr_parser(union olsr_message *, struct interface *, union olsr_ip_addr *);

/* Event function to register with the sceduler */
void
olsr_event(void *);

int
get_namemsg(struct namemsg *);

int
read_namemsg(struct namemsg *, struct name_entry *);

void
update_name_entry(union olsr_ip_addr *, struct namemsg *, double);

void
write_name_table(void);

#endif


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
 
#ifndef _CONFIGFILE_H
#define _CONFIGFILE_H

#include <arpa/inet.h>


#define OLSRD_CONF_FILE_NAME "olsrd.conf"
#define OLSRD_GLOBAL_CONF_FILE "/etc/" OLSRD_CONF_FILE_NAME
#define CONFIG_MAX_LINESIZE FILENAME_MAX + 100

#define MAX_KEYWORDS 4


extern int nonwlan_multiplier;

extern olsr_u8_t tos;

char ipv6_mult_site[50];             /* IPv6 multicast group site-local */
char ipv6_mult_global[50];             /* IPv6 multicast group global */

int
read_config_file(char *);

/* So we don't have to include main.h */
extern void
queue_if(char *);

#endif

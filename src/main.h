/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2004 Andreas Tønnesen (andreto@ifi.uio.no)
 *
 * This file is part of olsr.org.
 *
 * UniK olsrd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * UniK olsrd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with olsr.org; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _OLSR_MAIN
#define _OLSR_MAIN


#include "defs.h"

#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <arpa/inet.h>

struct sockaddr_in6 null_addr6;      /* Address used as Originator Address IPv6 */

char	packet[MAXMESSAGESIZE+1];
char	fwd_packet[MAXMESSAGESIZE+1];
int     outputsize = 0;         /* current size of the output buffer */
int     fwdsize = 0;         /* current size of the forward buffer */

int     precedence;
int     tos_bits;

int     option_i = 0;
int	bufspace = 127*1024;	/* max. input buffer size to request */

union olsr_packet *msg = (union olsr_packet *)packet;
union olsr_packet *fwdmsg = (union olsr_packet *)fwd_packet;

/* ID of the timer thread */
pthread_t main_thread;

/* Broadcast 255.255.255.255 */
int bcast_set;
struct sockaddr_in bcastaddr;

int del_gws;

olsr_u8_t tos;

int minsize;


extern pthread_mutex_t mutex; /* Mutex for thread */

void
olsr_shutdown(int);

void
print_usage();

void
set_default_values();

#endif

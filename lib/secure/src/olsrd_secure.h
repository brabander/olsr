/*
 * Secure OLSR plugin
 * Copyright (C) 2004 Andreas Tønnesen (andreto@ifi.uio.no)
 *
 * This file is part of the secure OLSR plugin(solsrp) for UniK olsrd.
 *
 * Solsrp is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * solsrp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with olsrd-unik; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*
 * Dynamic linked library example for UniK OLSRd
 */

#ifndef _OLSRD_PLUGIN_TEST
#define _OLSRD_PLUGIN_TEST

#include "olsrd_plugin.h"

#define KEYFILE "/root/.olsr/olsrd_secure_key"

/* Schemes */
#define ONE_CHECKSUM          1

/* Algorithm definitions */
#define SHA1_INCLUDING_KEY   1

#define	MAXMESSAGESIZE 512

#define SIGNATURE_SIZE 20
#define KEYLENGTH      16

#define UPPER_DIFF 3
#define LOWER_DIFF -3

char aes_key[16];
/* Seconds of slack allowed */
#define SLACK 3

/* Timestamp node */
struct stamp
{
  union olsr_ip_addr addr;
  /* Timestamp difference */
  int diff;
  olsr_u32_t challenge;
  olsr_u8_t validated;
  struct timeval valtime; /* Validity time */
  struct timeval conftime; /* Reconfiguration time */
  struct stamp *prev;
  struct stamp *next;
};

/* Seconds to cache a valid timestamp entry */
#define TIMESTAMP_HOLD_TIME 30
/* Seconds to cache a not verified timestamp entry */
#define EXCHANGE_HOLD_TIME 5

struct stamp timestamps[HASHSIZE];

char checksum_cache[512 + KEYLENGTH];

/* Input interface */
struct interface *olsr_in_if;

/* Timeout function to register with the sceduler */
void
olsr_timeout();


/* Event function to register with the sceduler */
void
olsr_event();

int
send_challenge(union olsr_ip_addr *);

int
ifchange(struct interface *, int);

int
send_cres(union olsr_ip_addr *, union olsr_ip_addr *, olsr_u32_t, struct stamp *);

int
send_rres(union olsr_ip_addr *, union olsr_ip_addr *, olsr_u32_t);

int
parse_challenge(char *);

int
parse_cres(char *);

int
parse_rres(char *);

int
check_auth(char *, int *);

void
ipc_action(int);

int
ipc_send(char *, int);

int
add_signature(char *, int*);

int
validate_packet(char *, int*);

void
packet_parser(int);

void
timeout_timestamps();

int
check_timestamp(union olsr_ip_addr *, time_t);

struct stamp *
lookup_timestamp_entry(union olsr_ip_addr *);

int
read_key_from_file(char *);

#endif

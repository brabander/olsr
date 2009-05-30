
/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004-2009, the olsr.org team - see HISTORY file
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of olsr.org, olsrd nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Visit http://www.olsr.org for more information.
 *
 * If you find this software useful feel free to make a donation
 * to the project. For more information see the website or contact
 * the copyright holders.
 *
 */


/*
 * Dynamic linked library for the olsr.org olsr daemon
 */

#include "olsrd_secure.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef linux
#include <linux/in_route.h>
#endif
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include "defs.h"
#include "ipcalc.h"
#include "olsr.h"
#include "parser.h"
#include "scheduler.h"
#include "net_olsr.h"
#include "common/string.h"
#include "olsr_logging.h"

#ifdef USE_OPENSSL

/* OpenSSL stuff */
#include <openssl/sha.h>

#define CHECKSUM SHA1
#define SCHEME   SHA1_INCLUDING_KEY

#else

/* Homebrewn checksuming */
#include "md5.h"

static void
MD5_checksum(const uint8_t * data, const uint16_t data_len, uint8_t * hashbuf)
{
  MD5_CTX context;

  MD5Init(&context);
  MD5Update(&context, data, data_len);
  MD5Final(hashbuf, &context);
}

#define CHECKSUM MD5_checksum
#define SCHEME   MD5_INCLUDING_KEY

#endif

#ifdef OS
#undef OS
#endif

#ifdef WIN32
#undef EWOULDBLOCK
#define EWOULDBLOCK WSAEWOULDBLOCK
#define OS "Windows"
#endif
#ifdef linux
#define OS "GNU/Linux"
#endif
#ifdef __FreeBSD__
#define OS "FreeBSD"
#endif

#ifndef OS
#define OS "Undefined"
#endif

static struct timeval now;

/* Timestamp node */
struct stamp {
  union olsr_ip_addr addr;
  /* Timestamp difference */
  int diff;
  uint32_t challenge;
  uint8_t validated;
  uint32_t valtime;                    /* Validity time */
  uint32_t conftime;                   /* Reconfiguration time */
  struct stamp *prev;
  struct stamp *next;
};

/* Seconds to cache a valid timestamp entry */
#define TIMESTAMP_HOLD_TIME 30

/* Seconds to cache a not verified timestamp entry */
#define EXCHANGE_HOLD_TIME 5

static struct stamp timestamps[HASHSIZE];


char keyfile[FILENAME_MAX + 1];
char aes_key[16];

/* Event function to register with the sceduler */
#if 0
static void olsr_event(void);
#endif
static int send_challenge(struct interface *olsr_if_config, const union olsr_ip_addr *);
static int send_cres(struct interface *olsr_if_config, union olsr_ip_addr *, union olsr_ip_addr *, uint32_t, struct stamp *);
static int send_rres(struct interface *olsr_if_config, union olsr_ip_addr *, union olsr_ip_addr *, uint32_t);
static int parse_challenge(struct interface *olsr_if_config, char *);
static int parse_cres(struct interface *olsr_if_config, char *);
static int parse_rres(char *);
static int check_auth(struct interface *olsr_if_config, char *, int *);
#if 0
static int ipc_send(char *, int);
#endif
static int add_signature(uint8_t *, int *);
static int validate_packet(struct interface *olsr_if_config, const char *, int *);
static char *secure_preprocessor(char *packet, struct interface *olsr_if_config, union olsr_ip_addr *from_addr, int *length);
static void timeout_timestamps(void *);
static int check_timestamp(struct interface *olsr_if_config, const union olsr_ip_addr *, time_t);
static struct stamp *lookup_timestamp_entry(const union olsr_ip_addr *);
static int read_key_from_file(const char *);

static struct olsr_cookie_info *timeout_timestamps_timer_cookie;

/**
 *Do initialization here
 *
 *This function is called by the my_init
 *function in uolsrd_plugin.c
 */
int
secure_plugin_init(void)
{
  int i;


  /* Initialize the timestamp database */
  for (i = 0; i < HASHSIZE; i++) {
    timestamps[i].next = &timestamps[i];
    timestamps[i].prev = &timestamps[i];
  }
  OLSR_INFO(LOG_PLUGINS, "Timestamp database initialized\n");

  if (!strlen(keyfile))
    strscpy(keyfile, KEYFILE, sizeof(keyfile));

  i = read_key_from_file(keyfile);

  if (i < 0) {
    OLSR_ERROR(LOG_PLUGINS, "[ENC]Could not read key from file %s!\nExitting!\n\n", keyfile);
    olsr_exit(1);
  }
  if (i == 0) {
    OLSR_ERROR(LOG_PLUGINS, "[ENC]There was a problem reading key from file %s. Is the key long enough?\nExitting!\n\n", keyfile);
    olsr_exit(1);
  }

  /* Register the packet transform function */
  add_ptf(&add_signature);

  olsr_preprocessor_add_function(&secure_preprocessor);

  /* create the cookie */
  timeout_timestamps_timer_cookie = olsr_alloc_cookie("Secure: Timeout Timestamps", OLSR_COOKIE_TYPE_TIMER);

  /* Register timeout - poll every 2 seconds */
  olsr_start_timer(2 * MSEC_PER_SEC, 0, OLSR_TIMER_PERIODIC, &timeout_timestamps, NULL, timeout_timestamps_timer_cookie);

  return 1;
}

int
plugin_ipc_init(void)
{
  return 1;
}

/*
 * destructor - called at unload
 */
void
secure_plugin_exit(void)
{
  olsr_preprocessor_remove_function(&secure_preprocessor);
}


#if 0

/**
 *Scheduled event
 */
static void
olsr_event(void)
{

}
#endif

#if 0
static int
ipc_send(char *data __attribute__ ((unused)), int size __attribute__ ((unused)))
{
  return 1;
}
#endif

static char *
secure_preprocessor(char *packet, struct interface *olsr_if_config, union olsr_ip_addr *from_addr
                    __attribute__ ((unused)), int *length)
{
  struct olsr *olsr = (struct olsr *)packet;
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif
  /*
   * Check for challenge/response messages
   */
  check_auth(olsr_if_config, packet, length);

  /*
   * Check signature
   */

  if (!validate_packet(olsr_if_config, packet, length)) {
    OLSR_DEBUG(LOG_PLUGINS, "[ENC]Rejecting packet from %s\n", olsr_ip_to_string(&buf, from_addr));
    return NULL;
  }

  OLSR_DEBUG(LOG_PLUGINS, "[ENC]Packet from %s OK size %d\n", olsr_ip_to_string(&buf, from_addr), *length);

  /* Fix OLSR packet header */
  olsr->olsr_packlen = htons(*length);
  return packet;
}



/**
 * Check a incoming OLSR packet for
 * challenge/responses.
 * They need not be verified as they
 * are signed in the message.
 *
 */
static int
check_auth(struct interface *olsr_if_config, char *pck, int *size __attribute__ ((unused)))
{

  OLSR_DEBUG(LOG_PLUGINS, "[ENC]Checking packet for challenge response message...\n");

  switch (pck[4]) {
  case (TYPE_CHALLENGE):
    parse_challenge(olsr_if_config, &pck[4]);
    break;

  case (TYPE_CRESPONSE):
    parse_cres(olsr_if_config, &pck[4]);
    break;

  case (TYPE_RRESPONSE):
    parse_rres(&pck[4]);
    break;

  default:
    return 0;
  }

  return 1;
}



/**
 * Packet transform function
 * Build a SHA-1/MD5 hash of the original message
 * + the signature message(-digest) + key
 *
 * Then add the signature message to the packet and
 * increase the size
 */
int
add_signature(uint8_t * pck, int *size)
{
  struct s_olsrmsg *msg;
#ifdef DEBUG
#endif

  OLSR_DEBUG(LOG_PLUGINS, "[ENC]Adding signature for packet size %d\n", *size);

  msg = (struct s_olsrmsg *)&pck[*size];
  /* Update size */
  ((struct olsr *)pck)->olsr_packlen = htons(*size + sizeof(struct s_olsrmsg));

  /* Fill packet header */
  msg->olsr_msgtype = MESSAGE_TYPE;
  msg->olsr_vtime = 0;
  msg->olsr_msgsize = htons(sizeof(struct s_olsrmsg));
  memcpy(&msg->originator, &olsr_cnf->router_id, olsr_cnf->ipsize);
  msg->ttl = 1;
  msg->hopcnt = 0;
  msg->seqno = htons(get_msg_seqno());

  /* Fill subheader */
  msg->sig.type = ONE_CHECKSUM;
  msg->sig.algorithm = SCHEME;
  memset(&msg->sig.reserved, 0, 2);

  /* Add timestamp */
  msg->sig.timestamp = htonl(now.tv_sec);
  OLSR_DEBUG(LOG_PLUGINS, "[ENC]timestamp: %lld\n", (long long)now.tv_sec);

  /* Set the new size */
  *size += sizeof(struct s_olsrmsg);

  {
    uint8_t checksum_cache[512 + KEYLENGTH];
    /* Create packet + key cache */
    /* First the OLSR packet + signature message - digest */
    memcpy(checksum_cache, pck, *size - SIGNATURE_SIZE);
    /* Then the key */
    memcpy(&checksum_cache[*size - SIGNATURE_SIZE], aes_key, KEYLENGTH);

    /* Create the hash */
    CHECKSUM(checksum_cache, (*size - SIGNATURE_SIZE) + KEYLENGTH, &pck[*size - SIGNATURE_SIZE]);
  }

#if 0
  {
    unsigned int i;
    int j;
    const uint8_t *sigmsg;
    OLSR_PRINTF(1, "Signature message:\n");

    j = 0;
    sigmsg = (uint8_t *) msg;

    for (i = 0; i < sizeof(struct s_olsrmsg); i++) {
      OLSR_PRINTF(1, "  %3i", sigmsg[i]);
      j++;
      if (j == 4) {
        OLSR_PRINTF(1, "\n");
        j = 0;
      }
    }
  }
#endif

  OLSR_DEBUG(LOG_PLUGINS, "[ENC] Message signed\n");

  return 1;
}



static int
validate_packet(struct interface *olsr_if_config, const char *pck, int *size)
{
  int packetsize;
  uint8_t sha1_hash[SIGNATURE_SIZE];
  const struct s_olsrmsg *sig;
  time_t rec_time;

  /* Find size - signature message */
  packetsize = *size - sizeof(struct s_olsrmsg);

  if (packetsize < 4)
    return 0;

  sig = (const struct s_olsrmsg *)&pck[packetsize];

  //OLSR_PRINTF(1, "Size: %d\n", packetsize);

#if 0
  {
    unsigned int i;
    int j;
    const uint8_t *sigmsg;
    OLSR_PRINTF(1, "Input message:\n");

    j = 0;
    sigmsg = (const uint8_t *)sig;

    for (i = 0; i < sizeof(struct s_olsrmsg); i++) {
      OLSR_PRINTF(1, "  %3i", sigmsg[i]);
      j++;
      if (j == 4) {
        OLSR_PRINTF(1, "\n");
        j = 0;
      }
    }
  }
#endif

  /* Sanity check first */
  if ((sig->olsr_msgtype != MESSAGE_TYPE) ||
      (sig->olsr_vtime != 0) || (sig->olsr_msgsize != ntohs(sizeof(struct s_olsrmsg))) || (sig->ttl != 1) || (sig->hopcnt != 0)) {
    OLSR_DEBUG(LOG_PLUGINS, "[ENC]Packet not sane!\n");
    return 0;
  }

  /* Check scheme and type */
  switch (sig->sig.type) {
  case (ONE_CHECKSUM):
    switch (sig->sig.algorithm) {
    case (SCHEME):
      goto one_checksum_SHA;    /* Ahhh... fix this */
      break;

    }
    break;

  default:
    OLSR_DEBUG(LOG_PLUGINS, "[ENC]Unsupported sceme: %d enc: %d!\n", sig->sig.type, sig->sig.algorithm);
    return 0;
  }
  //OLSR_PRINTF(1, "Packet sane...\n");

one_checksum_SHA:

  {
    uint8_t checksum_cache[512 + KEYLENGTH];
    /* Create packet + key cache */
    /* First the OLSR packet + signature message - digest */
    memcpy(checksum_cache, pck, *size - SIGNATURE_SIZE);
    /* Then the key */
    memcpy(&checksum_cache[*size - SIGNATURE_SIZE], aes_key, KEYLENGTH);

    /* generate SHA-1 */
    CHECKSUM(checksum_cache, *size - SIGNATURE_SIZE + KEYLENGTH, sha1_hash);
  }

#if 0
  OLSR_PRINTF(1, "Recevied hash:\n");

  sigmsg = (const uint8_t *)sig->sig.signature;

  for (i = 0; i < SIGNATURE_SIZE; i++) {
    OLSR_PRINTF(1, " %3i", sigmsg[i]);
  }
  OLSR_PRINTF(1, "\n");

  OLSR_PRINTF(1, "Calculated hash:\n");

  sigmsg = sha1_hash;

  for (i = 0; i < SIGNATURE_SIZE; i++) {
    OLSR_PRINTF(1, " %3i", sigmsg[i]);
  }
  OLSR_PRINTF(1, "\n");
#endif

  if (memcmp(sha1_hash, sig->sig.signature, SIGNATURE_SIZE) != 0) {
    OLSR_DEBUG(LOG_PLUGINS, "[ENC]Signature missmatch\n");
    return 0;
  }

  /* Check timestamp */
  rec_time = ntohl(sig->sig.timestamp);

  if (!check_timestamp(olsr_if_config, (const union olsr_ip_addr *)&sig->originator, rec_time)) {
#if !defined REMOVE_LOG_DEBUG
    struct ipaddr_str buf;
#endif
    OLSR_DEBUG(LOG_PLUGINS, "[ENC]Timestamp missmatch in packet from %s!\n",
               olsr_ip_to_string(&buf, (const union olsr_ip_addr *)&sig->originator));
    return 0;
  }

  OLSR_DEBUG(LOG_PLUGINS, "[ENC]Received timestamp %lld diff: %lld\n", (long long)rec_time,
             (long long)now.tv_sec - (long long)rec_time);

  /* Remove signature message */
  *size = packetsize;
  return 1;
}


int
check_timestamp(struct interface *olsr_if_config, const union olsr_ip_addr *originator, time_t tstamp)
{
  struct stamp *entry;
  int diff;

  entry = lookup_timestamp_entry(originator);

  if (!entry) {
    /* Initiate timestamp negotiation */

    send_challenge(olsr_if_config, originator);

    return 0;
  }

  if (!entry->validated) {
    OLSR_DEBUG(LOG_PLUGINS, "[ENC]Message from non-validated host!\n");
    return 0;
  }

  diff = entry->diff - (now.tv_sec - tstamp);

  OLSR_DEBUG(LOG_PLUGINS, "[ENC]Timestamp slack: %d\n", diff);

  if ((diff > UPPER_DIFF) || (diff < LOWER_DIFF)) {
    OLSR_DEBUG(LOG_PLUGINS, "[ENC]Timestamp scew detected!!\n");
    return 0;
  }

  /* ok - update diff */
  entry->diff = ((now.tv_sec - tstamp) + entry->diff) ? ((now.tv_sec - tstamp) + entry->diff) / 2 : 0;

  OLSR_DEBUG(LOG_PLUGINS, "[ENC]Diff set to : %d\n", entry->diff);

  /* update validtime */

  entry->valtime = GET_TIMESTAMP(TIMESTAMP_HOLD_TIME * 1000);

  return 1;
}


/**
 * Create and send a timestamp
 * challenge message to new_host
 *
 * The host is registered in the timestamps
 * repository with valid=0
 */

int
send_challenge(struct interface *olsr_if_config, const union olsr_ip_addr *new_host)
{
  struct challengemsg cmsg;
  struct stamp *entry;
  uint32_t challenge, hash;
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif

  OLSR_DEBUG(LOG_PLUGINS, "[ENC]Building CHALLENGE message\n");

  /* Set the size including OLSR packet size */


  challenge = rand() << 16;
  challenge |= rand();

  /* Fill challengemessage */
  cmsg.olsr_msgtype = TYPE_CHALLENGE;
  cmsg.olsr_vtime = 0;
  cmsg.olsr_msgsize = htons(sizeof(struct challengemsg));
  memcpy(&cmsg.originator, &olsr_cnf->router_id, olsr_cnf->ipsize);
  cmsg.ttl = 1;
  cmsg.hopcnt = 0;
  cmsg.seqno = htons(get_msg_seqno());

  /* Fill subheader */
  memcpy(&cmsg.destination, new_host, olsr_cnf->ipsize);
  cmsg.challenge = htonl(challenge);

  OLSR_DEBUG(LOG_PLUGINS, "[ENC]Size: %lu\n", (unsigned long)sizeof(struct challengemsg));

  {
    uint8_t checksum_cache[512 + KEYLENGTH];
    /* Create packet + key cache */
    /* First the OLSR packet + signature message - digest */
    memcpy(checksum_cache, &cmsg, sizeof(struct challengemsg) - SIGNATURE_SIZE);
    /* Then the key */
    memcpy(&checksum_cache[sizeof(struct challengemsg) - SIGNATURE_SIZE], aes_key, KEYLENGTH);

    /* Create the hash */
    CHECKSUM(checksum_cache, (sizeof(struct challengemsg) - SIGNATURE_SIZE) + KEYLENGTH, cmsg.signature);
  }
  OLSR_DEBUG(LOG_PLUGINS, "[ENC]Sending timestamp request to %s challenge 0x%x\n", olsr_ip_to_string(&buf, new_host), challenge);

  /* Add to buffer */
  net_outbuffer_push(olsr_if_config, &cmsg, sizeof(struct challengemsg));

  /* Send the request */
  net_output(olsr_if_config);

  /* Create new entry */
  entry = malloc(sizeof(struct stamp));

  entry->diff = 0;
  entry->validated = 0;
  entry->challenge = challenge;

  memcpy(&entry->addr, new_host, olsr_cnf->ipsize);

  /* update validtime - not validated */
  entry->conftime = GET_TIMESTAMP(EXCHANGE_HOLD_TIME * 1000);

  hash = olsr_ip_hashing(new_host);

  /* Queue */
  timestamps[hash].next->prev = entry;
  entry->next = timestamps[hash].next;
  timestamps[hash].next = entry;
  entry->prev = &timestamps[hash];


  return 1;

}

int
parse_cres(struct interface *olsr_if_config, char *in_msg)
{
  struct c_respmsg *msg;
  uint8_t sha1_hash[SIGNATURE_SIZE];
  struct stamp *entry;
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif

  msg = (struct c_respmsg *)in_msg;

  OLSR_DEBUG(LOG_PLUGINS, "[ENC]Challenge-response message received\n");
  OLSR_DEBUG(LOG_PLUGINS, "[ENC]To: %s\n", olsr_ip_to_string(&buf, (union olsr_ip_addr *)&msg->destination));

  if (if_ifwithaddr((union olsr_ip_addr *)&msg->destination) == NULL) {
    OLSR_DEBUG(LOG_PLUGINS, "[ENC]Not for us...\n");
    return 0;
  }

  OLSR_DEBUG(LOG_PLUGINS, "[ENC]Challenge: 0x%lx\n", (unsigned long)ntohl(msg->challenge));     /* ntohl() returns a unsignedlong onwin32 */

  /* Check signature */

  {
    uint8_t checksum_cache[512 + KEYLENGTH];
    /* Create packet + key cache */
    /* First the OLSR packet + signature message - digest */
    memcpy(checksum_cache, msg, sizeof(struct c_respmsg) - SIGNATURE_SIZE);
    /* Then the key */
    memcpy(&checksum_cache[sizeof(struct c_respmsg) - SIGNATURE_SIZE], aes_key, KEYLENGTH);

    /* Create the hash */
    CHECKSUM(checksum_cache, (sizeof(struct c_respmsg) - SIGNATURE_SIZE) + KEYLENGTH, sha1_hash);
  }

  if (memcmp(sha1_hash, &msg->signature, SIGNATURE_SIZE) != 0) {
    OLSR_DEBUG(LOG_PLUGINS, "[ENC]Signature missmatch in challenge-response!\n");
    return 0;
  }

  OLSR_DEBUG(LOG_PLUGINS, "[ENC]Signature verified\n");


  /* Now to check the digest from the emitted challenge */
  if ((entry = lookup_timestamp_entry((const union olsr_ip_addr *)&msg->originator)) == NULL) {
    OLSR_DEBUG(LOG_PLUGINS, "[ENC]Received challenge-response from non-registered node %s!\n",
               olsr_ip_to_string(&buf, (union olsr_ip_addr *)&msg->originator));
    return 0;
  }

  /* Generate the digest */
  OLSR_DEBUG(LOG_PLUGINS, "[ENC]Entry-challenge 0x%x\n", entry->challenge);

  {
    uint8_t checksum_cache[512 + KEYLENGTH];
    /* First the challenge received */
    memcpy(checksum_cache, &entry->challenge, 4);
    /* Then the local IP */
    memcpy(&checksum_cache[sizeof(uint32_t)], &msg->originator, olsr_cnf->ipsize);

    /* Create the hash */
    CHECKSUM(checksum_cache, sizeof(uint32_t) + olsr_cnf->ipsize, sha1_hash);
  }

  if (memcmp(msg->res_sig, sha1_hash, SIGNATURE_SIZE) != 0) {
    OLSR_DEBUG(LOG_PLUGINS, "[ENC]Error in challenge signature from %s!\n",
               olsr_ip_to_string(&buf, (union olsr_ip_addr *)&msg->originator));

    return 0;
  }

  OLSR_DEBUG(LOG_PLUGINS, "[ENC]Challenge-response signature ok\n");

  /* Update entry! */


  entry->challenge = 0;
  entry->validated = 1;
  entry->diff = now.tv_sec - msg->timestamp;

  /* update validtime - validated entry */
  entry->valtime = GET_TIMESTAMP(TIMESTAMP_HOLD_TIME * 1000);

  OLSR_DEBUG(LOG_PLUGINS, "[ENC]%s registered with diff %d!\n",
             olsr_ip_to_string(&buf, (union olsr_ip_addr *)&msg->originator), entry->diff);

  /* Send response-response */
  send_rres(olsr_if_config, (union olsr_ip_addr *)&msg->originator, (union olsr_ip_addr *)&msg->destination, ntohl(msg->challenge));

  return 1;
}


int
parse_rres(char *in_msg)
{
  struct r_respmsg *msg;
  uint8_t sha1_hash[SIGNATURE_SIZE];
  struct stamp *entry;
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif

  msg = (struct r_respmsg *)in_msg;

  OLSR_DEBUG(LOG_PLUGINS, "[ENC]Response-response message received\n");
  OLSR_DEBUG(LOG_PLUGINS, "[ENC]To: %s\n", olsr_ip_to_string(&buf, (union olsr_ip_addr *)&msg->destination));

  if (if_ifwithaddr((union olsr_ip_addr *)&msg->destination) == NULL) {
    OLSR_DEBUG(LOG_PLUGINS, "[ENC]Not for us...\n");
    return 0;
  }

  /* Check signature */

  {
    uint8_t checksum_cache[512 + KEYLENGTH];
    /* Create packet + key cache */
    /* First the OLSR packet + signature message - digest */
    memcpy(checksum_cache, msg, sizeof(struct r_respmsg) - SIGNATURE_SIZE);
    /* Then the key */
    memcpy(&checksum_cache[sizeof(struct r_respmsg) - SIGNATURE_SIZE], aes_key, KEYLENGTH);

    /* Create the hash */
    CHECKSUM(checksum_cache, (sizeof(struct r_respmsg) - SIGNATURE_SIZE) + KEYLENGTH, sha1_hash);
  }

  if (memcmp(sha1_hash, &msg->signature, SIGNATURE_SIZE) != 0) {
    OLSR_DEBUG(LOG_PLUGINS, "[ENC]Signature missmatch in response-response!\n");
    return 0;
  }

  OLSR_DEBUG(LOG_PLUGINS, "[ENC]Signature verified\n");


  /* Now to check the digest from the emitted challenge */
  if ((entry = lookup_timestamp_entry((const union olsr_ip_addr *)&msg->originator)) == NULL) {
    OLSR_DEBUG(LOG_PLUGINS, "[ENC]Received response-response from non-registered node %s!\n",
               olsr_ip_to_string(&buf, (union olsr_ip_addr *)&msg->originator));
    return 0;
  }

  /* Generate the digest */
  OLSR_DEBUG(LOG_PLUGINS, "[ENC]Entry-challenge 0x%x\n", entry->challenge);

  {
    uint8_t checksum_cache[512 + KEYLENGTH];
    /* First the challenge received */
    memcpy(checksum_cache, &entry->challenge, 4);
    /* Then the local IP */
    memcpy(&checksum_cache[sizeof(uint32_t)], &msg->originator, olsr_cnf->ipsize);

    /* Create the hash */
    CHECKSUM(checksum_cache, sizeof(uint32_t) + olsr_cnf->ipsize, sha1_hash);
  }

  if (memcmp(msg->res_sig, sha1_hash, SIGNATURE_SIZE) != 0) {
    OLSR_DEBUG(LOG_PLUGINS, "[ENC]Error in response signature from %s!\n",
               olsr_ip_to_string(&buf, (union olsr_ip_addr *)&msg->originator));

    return 0;
  }

  OLSR_DEBUG(LOG_PLUGINS, "[ENC]Challenge-response signature ok\n");

  /* Update entry! */


  entry->challenge = 0;
  entry->validated = 1;
  entry->diff = now.tv_sec - msg->timestamp;

  /* update validtime - validated entry */
  entry->valtime = GET_TIMESTAMP(TIMESTAMP_HOLD_TIME * 1000);

  OLSR_DEBUG(LOG_PLUGINS, "[ENC]%s registered with diff %d!\n",
             olsr_ip_to_string(&buf, (union olsr_ip_addr *)&msg->originator), entry->diff);

  return 1;
}


int
parse_challenge(struct interface *olsr_if_config, char *in_msg)
{
  struct challengemsg *msg;
  uint8_t sha1_hash[SIGNATURE_SIZE];
  struct stamp *entry;
  uint32_t hash;
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif

  msg = (struct challengemsg *)in_msg;

  OLSR_DEBUG(LOG_PLUGINS, "[ENC]Challenge message received\n");
  OLSR_DEBUG(LOG_PLUGINS, "[ENC]To: %s\n", olsr_ip_to_string(&buf, (union olsr_ip_addr *)&msg->destination));

  if (if_ifwithaddr((union olsr_ip_addr *)&msg->destination) == NULL) {
    OLSR_DEBUG(LOG_PLUGINS, "[ENC]Not for us...\n");
    return 0;
  }

  /* Create entry if not registered */
  if ((entry = lookup_timestamp_entry((const union olsr_ip_addr *)&msg->originator)) == NULL) {
    entry = malloc(sizeof(struct stamp));
    memcpy(&entry->addr, &msg->originator, olsr_cnf->ipsize);

    hash = olsr_ip_hashing((union olsr_ip_addr *)&msg->originator);

    /* Queue */
    timestamps[hash].next->prev = entry;
    entry->next = timestamps[hash].next;
    timestamps[hash].next = entry;
    entry->prev = &timestamps[hash];
  } else {
    /* Check configuration timeout */
    if (!TIMED_OUT(entry->conftime)) {
      /* If registered - do not accept! */
      OLSR_DEBUG(LOG_PLUGINS, "[ENC]Challenge from registered node...dropping!\n");
      return 0;
    } else {
      OLSR_DEBUG(LOG_PLUGINS, "[ENC]Challenge from registered node...accepted!\n");
    }
  }

  OLSR_DEBUG(LOG_PLUGINS, "[ENC]Challenge: 0x%lx\n", (unsigned long)ntohl(msg->challenge));     /* ntohl() returns a unsignedlong onwin32 */

  /* Check signature */

  {
    uint8_t checksum_cache[512 + KEYLENGTH];
    /* Create packet + key cache */
    /* First the OLSR packet + signature message - digest */
    memcpy(checksum_cache, msg, sizeof(struct challengemsg) - SIGNATURE_SIZE);
    /* Then the key */
    memcpy(&checksum_cache[sizeof(struct challengemsg) - SIGNATURE_SIZE], aes_key, KEYLENGTH);

    /* Create the hash */
    CHECKSUM(checksum_cache, (sizeof(struct challengemsg) - SIGNATURE_SIZE) + KEYLENGTH, sha1_hash);
  }
  if (memcmp(sha1_hash, &msg->signature, SIGNATURE_SIZE) != 0) {
    OLSR_DEBUG(LOG_PLUGINS, "[ENC]Signature missmatch in challenge!\n");
    return 0;
  }

  OLSR_DEBUG(LOG_PLUGINS, "[ENC]Signature verified\n");


  entry->diff = 0;
  entry->validated = 0;

  /* update validtime - not validated */
  entry->conftime = GET_TIMESTAMP(EXCHANGE_HOLD_TIME * 1000);

  /* Build and send response */

  send_cres(olsr_if_config, (union olsr_ip_addr *)&msg->originator,
            (union olsr_ip_addr *)&msg->destination, ntohl(msg->challenge), entry);

  return 1;
}





/**
 * Build and transmit a challenge response
 * message.
 *
 */
int
send_cres(struct interface *olsr_if_config, union olsr_ip_addr *to, union olsr_ip_addr *from, uint32_t chal_in, struct stamp *entry)
{
  struct c_respmsg crmsg;
  uint32_t challenge;
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif

  OLSR_DEBUG(LOG_PLUGINS, "[ENC]Building CRESPONSE message\n");

  challenge = rand() << 16;
  challenge |= rand();

  entry->challenge = challenge;

  OLSR_DEBUG(LOG_PLUGINS, "[ENC]Challenge-response: 0x%x\n", challenge);

  /* Fill challengemessage */
  crmsg.olsr_msgtype = TYPE_CRESPONSE;
  crmsg.olsr_vtime = 0;
  crmsg.olsr_msgsize = htons(sizeof(struct c_respmsg));
  memcpy(&crmsg.originator, &olsr_cnf->router_id, olsr_cnf->ipsize);
  crmsg.ttl = 1;
  crmsg.hopcnt = 0;
  crmsg.seqno = htons(get_msg_seqno());

  /* set timestamp */
  crmsg.timestamp = now.tv_sec;
  OLSR_DEBUG(LOG_PLUGINS, "[ENC]Timestamp %lld\n", (long long)crmsg.timestamp);

  /* Fill subheader */
  memcpy(&crmsg.destination, to, olsr_cnf->ipsize);
  crmsg.challenge = htonl(challenge);

  /* Create digest of received challenge + IP */

  {
    uint8_t checksum_cache[512 + KEYLENGTH];
    /* Create packet + key cache */
    /* First the challenge received */
    memcpy(checksum_cache, &chal_in, 4);
    /* Then the local IP */
    memcpy(&checksum_cache[sizeof(uint32_t)], from, olsr_cnf->ipsize);

    /* Create the hash */
    CHECKSUM(checksum_cache, sizeof(uint32_t) + olsr_cnf->ipsize, crmsg.res_sig);
  }

  /* Now create the digest of the message and the key */

  {
    uint8_t checksum_cache[512 + KEYLENGTH];
    /* Create packet + key cache */
    /* First the OLSR packet + signature message - digest */
    memcpy(checksum_cache, &crmsg, sizeof(struct c_respmsg) - SIGNATURE_SIZE);
    /* Then the key */
    memcpy(&checksum_cache[sizeof(struct c_respmsg) - SIGNATURE_SIZE], aes_key, KEYLENGTH);

    /* Create the hash */
    CHECKSUM(checksum_cache, (sizeof(struct c_respmsg) - SIGNATURE_SIZE) + KEYLENGTH, crmsg.signature);
  }

  OLSR_DEBUG(LOG_PLUGINS, "[ENC]Sending challenge response to %s challenge 0x%x\n", olsr_ip_to_string(&buf, to), challenge);

  /* Add to buffer */
  net_outbuffer_push(olsr_if_config, &crmsg, sizeof(struct c_respmsg));
  /* Send the request */
  net_output(olsr_if_config);

  return 1;
}






/**
 * Build and transmit a response response
 * message.
 *
 */
static int
send_rres(struct interface *olsr_if_config, union olsr_ip_addr *to, union olsr_ip_addr *from, uint32_t chal_in)
{
  struct r_respmsg rrmsg;
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif

  OLSR_DEBUG(LOG_PLUGINS, "[ENC]Building RRESPONSE message\n");


  /* Fill challengemessage */
  rrmsg.olsr_msgtype = TYPE_RRESPONSE;
  rrmsg.olsr_vtime = 0;
  rrmsg.olsr_msgsize = htons(sizeof(struct r_respmsg));
  memcpy(&rrmsg.originator, &olsr_cnf->router_id, olsr_cnf->ipsize);
  rrmsg.ttl = 1;
  rrmsg.hopcnt = 0;
  rrmsg.seqno = htons(get_msg_seqno());

  /* set timestamp */
  rrmsg.timestamp = now.tv_sec;
  OLSR_DEBUG(LOG_PLUGINS, "[ENC]Timestamp %lld\n", (long long)rrmsg.timestamp);

  /* Fill subheader */
  memcpy(&rrmsg.destination, to, olsr_cnf->ipsize);

  /* Create digest of received challenge + IP */

  {
    uint8_t checksum_cache[512 + KEYLENGTH];
    /* Create packet + key cache */
    /* First the challenge received */
    memcpy(checksum_cache, &chal_in, 4);
    /* Then the local IP */
    memcpy(&checksum_cache[sizeof(uint32_t)], from, olsr_cnf->ipsize);

    /* Create the hash */
    CHECKSUM(checksum_cache, sizeof(uint32_t) + olsr_cnf->ipsize, rrmsg.res_sig);
  }

  /* Now create the digest of the message and the key */

  {
    uint8_t checksum_cache[512 + KEYLENGTH];
    /* Create packet + key cache */
    /* First the OLSR packet + signature message - digest */
    memcpy(checksum_cache, &rrmsg, sizeof(struct r_respmsg) - SIGNATURE_SIZE);
    /* Then the key */
    memcpy(&checksum_cache[sizeof(struct r_respmsg) - SIGNATURE_SIZE], aes_key, KEYLENGTH);

    /* Create the hash */
    CHECKSUM(checksum_cache, (sizeof(struct r_respmsg) - SIGNATURE_SIZE) + KEYLENGTH, rrmsg.signature);
  }

  OLSR_DEBUG(LOG_PLUGINS, "[ENC]Sending response response to %s\n", olsr_ip_to_string(&buf, to));

  /* add to buffer */
  net_outbuffer_push(olsr_if_config, &rrmsg, sizeof(struct r_respmsg));

  /* Send the request */
  net_output(olsr_if_config);

  return 1;
}



static struct stamp *
lookup_timestamp_entry(const union olsr_ip_addr *adr)
{
  uint32_t hash;
  struct stamp *entry;
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif

  hash = olsr_ip_hashing(adr);

  for (entry = timestamps[hash].next; entry != &timestamps[hash]; entry = entry->next) {
    if (memcmp(&entry->addr, adr, olsr_cnf->ipsize) == 0) {
      OLSR_DEBUG(LOG_PLUGINS, "[ENC]Match for %s\n", olsr_ip_to_string(&buf, adr));
      return entry;
    }
  }

  OLSR_DEBUG(LOG_PLUGINS, "[ENC]No match for %s\n", olsr_ip_to_string(&buf, adr));

  return NULL;
}



/**
 *Find timed out entries and delete them
 *
 *@return nada
 */
void
timeout_timestamps(void *foo __attribute__ ((unused)))
{
  struct stamp *tmp_list;
  struct stamp *entry_to_delete;
  int idx;

  /* Update our local timestamp */
  gettimeofday(&now, NULL);

  for (idx = 0; idx < HASHSIZE; idx++) {
    tmp_list = timestamps[idx].next;
    /*Traverse MID list */
    while (tmp_list != &timestamps[idx]) {
      /*Check if the entry is timed out */
      if ((TIMED_OUT(tmp_list->valtime)) && (TIMED_OUT(tmp_list->conftime))) {
#if !defined REMOVE_LOG_DEBUG
        struct ipaddr_str buf;
#endif
        entry_to_delete = tmp_list;
        tmp_list = tmp_list->next;

        OLSR_DEBUG(LOG_PLUGINS, "[ENC]timestamp info for %s timed out.. deleting it\n",
                   olsr_ip_to_string(&buf, &entry_to_delete->addr));

        /*Delete it */
        entry_to_delete->next->prev = entry_to_delete->prev;
        entry_to_delete->prev->next = entry_to_delete->next;

        free(entry_to_delete);
      } else
        tmp_list = tmp_list->next;
    }
  }

  return;
}



static int
read_key_from_file(const char *file)
{
  FILE *kf;
  size_t keylen;

  keylen = 16;
  kf = fopen(file, "r");

  OLSR_DEBUG(LOG_PLUGINS, "[ENC]Reading key from file \"%s\"\n", file);

  if (kf == NULL) {
    OLSR_WARN(LOG_PLUGINS, "[ENC]Could not open keyfile %s!\nError: %s\n", file, strerror(errno));
    return -1;
  }

  if (fread(aes_key, 1, keylen, kf) != keylen) {
    OLSR_WARN(LOG_PLUGINS, "[ENC]Could not read key from keyfile %s!\nError: %s\n", file, strerror(errno));
    fclose(kf);
    return 0;
  }


  fclose(kf);
  return 1;
}


/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

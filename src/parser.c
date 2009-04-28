
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

#include "parser.h"
#include "ipcalc.h"
#include "defs.h"
#include "process_package.h"
#include "mantissa.h"
#include "duplicate_set.h"
#include "mid_set.h"
#include "olsr.h"
#include "net_os.h"
#include "log.h"
#include "net_olsr.h"

#include <errno.h>
#include <stdlib.h>

#ifdef WIN32
#undef EWOULDBLOCK
#define EWOULDBLOCK WSAEWOULDBLOCK
#endif

static void parse_packet(struct olsr *, int, struct interface *, union olsr_ip_addr *);


/* Sven-Ola: On very slow devices used in huge networks
 * the amount of lq_tc messages is so high, that the
 * recv() loop never ends. This is a small hack to end
 * the loop in this cases
 */

static struct parse_function_entry *parse_functions;
static struct preprocessor_function_entry *preprocessor_functions;
static struct packetparser_function_entry *packetparser_functions;

/**
 *Initialize the parser.
 *
 *@return nada
 */
void
olsr_init_parser(void)
{
  OLSR_INFO(LOG_PACKET_PARSING, "Initializing parser...\n");

  /* Initialize the packet functions */
  olsr_init_package_process();
}

void
olsr_deinit_parser(void)
{
  OLSR_INFO(LOG_PACKET_PARSING, "Deinitializing parser...\n");
  olsr_deinit_package_process();
}

void
olsr_parser_add_function(parse_function * function, uint32_t type)
{
  struct parse_function_entry *new_entry;

  new_entry = olsr_malloc(sizeof(*new_entry), "Register parse function");

  new_entry->function = function;
  new_entry->type = type;

  /* Queue */
  new_entry->next = parse_functions;
  parse_functions = new_entry;

  OLSR_INFO(LOG_PACKET_PARSING, "Register parse function: Added function for type %u\n", type);
}

int
olsr_parser_remove_function(parse_function * function, uint32_t type)
{
  struct parse_function_entry *entry, *prev;

  for (entry = parse_functions, prev = NULL; entry != NULL; prev = entry, entry = entry->next) {
    if ((entry->function == function) && (entry->type == type)) {
      if (entry == parse_functions) {
        parse_functions = entry->next;
      } else {
        prev->next = entry->next;
      }
      free(entry);
      return 1;
    }
  }
  return 0;
}

void
olsr_preprocessor_add_function(preprocessor_function * function)
{
  struct preprocessor_function_entry *new_entry;

  new_entry = olsr_malloc(sizeof(*new_entry), "Register preprocessor function");
  new_entry->function = function;

  /* Queue */
  new_entry->next = preprocessor_functions;
  preprocessor_functions = new_entry;

  OLSR_INFO(LOG_PACKET_PARSING, "Registered preprocessor function\n");
}

int
olsr_preprocessor_remove_function(preprocessor_function * function)
{
  struct preprocessor_function_entry *entry, *prev;

  for (entry = preprocessor_functions, prev = NULL; entry != NULL; prev = entry, entry = entry->next) {
    if (entry->function == function) {
      if (entry == preprocessor_functions) {
        preprocessor_functions = entry->next;
      } else {
        prev->next = entry->next;
      }
      free(entry);
      return 1;
    }
  }
  return 0;
}

void
olsr_packetparser_add_function(packetparser_function * function)
{
  struct packetparser_function_entry *new_entry;

  new_entry = olsr_malloc(sizeof(struct packetparser_function_entry), "Register packetparser function");

  new_entry->function = function;

  /* Queue */
  new_entry->next = packetparser_functions;
  packetparser_functions = new_entry;

  OLSR_INFO(LOG_PACKET_PARSING, "Registered packetparser  function\n");
}

int
olsr_packetparser_remove_function(packetparser_function * function)
{
  struct packetparser_function_entry *entry, *prev;

  for (entry = packetparser_functions, prev = NULL; entry != NULL; prev = entry, entry = entry->next) {
    if (entry->function == function) {
      if (entry == packetparser_functions) {
        packetparser_functions = entry->next;
      } else {
        prev->next = entry->next;
      }
      free(entry);
      return 1;
    }
  }
  return 0;
}

/**
 * Shared code to parse the message headers and validate the message originator.
 */
const unsigned char *
olsr_parse_msg_hdr(const union olsr_message *msg, struct olsrmsg_hdr *msg_hdr)
{
  const unsigned char *curr = (const void *)msg;
  if (!msg) {
    return NULL;
  }

  pkt_get_u8(&curr, &msg_hdr->type);
  pkt_get_reltime(&curr, &msg_hdr->vtime);
  pkt_get_u16(&curr, &msg_hdr->size);
  pkt_get_ipaddress(&curr, &msg_hdr->originator);
  pkt_get_u8(&curr, &msg_hdr->ttl);
  pkt_get_u8(&curr, &msg_hdr->hopcnt);
  pkt_get_u16(&curr, &msg_hdr->seqno);

  if (!olsr_validate_address(&msg_hdr->originator)) {
    return NULL;
  }
  return curr;
}

/**
 *Process a newly received OLSR packet. Checks the type
 *and to the neccessary convertions and call the
 *corresponding functions to handle the information.
 *@param from the sockaddr struct describing the sender
 *@param olsr the olsr struct containing the message
 *@param size the size of the message
 *@return nada
 */
static void
parse_packet(struct olsr *olsr, int size, struct interface *in_if, union olsr_ip_addr *from_addr)
{
  union olsr_message *m = (union olsr_message *)olsr->olsr_msg;
  int msgsize;
  struct parse_function_entry *entry;
  struct packetparser_function_entry *packetparser;
  int count = size - ((char *)m - (char *)olsr);
#if !defined(REMOVE_LOG_INFO) || !defined(REMOVE_LOG_WARN)
  struct ipaddr_str buf;
#endif

  if (count < MIN_PACKET_SIZE(olsr_cnf->ip_version)) {
    return;
  }
  if (ntohs(olsr->olsr_packlen) != (size_t) size) {
    OLSR_WARN(LOG_PACKET_PARSING, "Size error detected in received packet from %s.\nRecieved %d, in packet %d\n",
              olsr_ip_to_string(&buf, from_addr), size, ntohs(olsr->olsr_packlen));
    return;
  }
  // translate sequence number to host order
  olsr->olsr_seqno = ntohs(olsr->olsr_seqno);

  // call packetparser
  for (packetparser = packetparser_functions; packetparser != NULL; packetparser = packetparser->next) {
    packetparser->function(olsr, in_if, from_addr);
  }

  msgsize = ntohs(olsr_cnf->ip_version == AF_INET ? m->v4.olsr_msgsize : m->v6.olsr_msgsize);

  for (; count > 0; m = (union olsr_message *)((char *)m + msgsize)) {
    bool forward = true;

    if (count < MIN_PACKET_SIZE(olsr_cnf->ip_version)) {
      break;
    }
    msgsize = ntohs(olsr_cnf->ip_version == AF_INET ? m->v4.olsr_msgsize : m->v6.olsr_msgsize);

    count -= msgsize;

    /* Check size of message */
    if (count < 0) {
      OLSR_WARN(LOG_PACKET_PARSING, "packet length error in  packet received from %s!", olsr_ip_to_string(&buf, from_addr));
      break;
    }

    /*RFC 3626 section 3.4:
     *  2    If the time to live of the message is less than or equal to
     *  '0' (zero), or if the message was sent by the receiving node
     *  (i.e., the Originator Address of the message is the main
     *  address of the receiving node): the message MUST silently be
     *  dropped.
     */

    /* Should be the same for IPv4 and IPv6 */
    if (olsr_ipcmp((union olsr_ip_addr *)&m->v4.originator, &olsr_cnf->router_id) == 0
        || !olsr_validate_address((union olsr_ip_addr *)&m->v4.originator)) {
      OLSR_INFO(LOG_PACKET_PARSING, "Not processing message originating from %s!\n",
                olsr_ip_to_string(&buf, (union olsr_ip_addr *)&m->v4.originator));
      continue;
    }

    for (entry = parse_functions; entry != NULL; entry = entry->next) {
      /* Should be the same for IPv4 and IPv6 */
      /* Promiscuous or exact match */
      if ((entry->type == PROMISCUOUS) || (entry->type == m->v4.olsr_msgtype)) {
        if (!entry->function(m, in_if, from_addr))
          forward = false;
      }
    }

    if (forward) {
      olsr_forward_message(m, in_if, from_addr);
    }
  }                             /* for olsr_msg */
}

/**
 *Processing OLSR data from socket. Reading data, setting
 *wich interface recieved the message, Sends IPC(if used)
 *and passes the packet on to parse_packet().
 *
 *@param fd the filedescriptor that data should be read from.
 *@return nada
 */
void
olsr_input(int fd, void *data __attribute__ ((unused)), unsigned int flags __attribute__ ((unused)))
{
  unsigned int cpu_overload_exit = 0;
#ifndef REMOVE_LOG_DEBUG
  char addrbuf[128];
#endif
#ifndef REMOVE_LOG_WARN
  struct ipaddr_str buf;
#endif

  for (;;) {
    struct interface *olsr_in_if;
    union olsr_ip_addr from_addr;
    struct preprocessor_function_entry *entry;
    char *packet;
    /* sockaddr_in6 is bigger than sockaddr !!!! */
    struct sockaddr_storage from;
    socklen_t fromlen;
    int cc;
    char inbuf[MAXMESSAGESIZE + 1];

    if (32 < ++cpu_overload_exit) {
      OLSR_WARN(LOG_PACKET_PARSING, "CPU overload detected, ending olsr_input() loop\n");
      break;
    }

    fromlen = sizeof(from);
    cc = olsr_recvfrom(fd, inbuf, sizeof(inbuf), 0, (struct sockaddr *)&from, &fromlen);

    if (cc <= 0) {
      if (cc < 0 && errno != EWOULDBLOCK) {
        OLSR_WARN(LOG_PACKET_PARSING, "error recvfrom: %s", strerror(errno));
      }
      break;
    }

    OLSR_DEBUG(LOG_PACKET_PARSING, "Recieved a packet from %s\n",
               sockaddr_to_string(addrbuf, sizeof(addrbuf), (struct sockaddr *)&from, fromlen));

    if (olsr_cnf->ip_version == AF_INET) {
      /* IPv4 sender address */
      if (fromlen != sizeof(struct sockaddr_in)) {
        break;
      }
      from_addr.v4 = ((struct sockaddr_in *)&from)->sin_addr;
    } else {
      /* IPv6 sender address */
      if (fromlen != sizeof(struct sockaddr_in6)) {
        break;
      }
      from_addr.v6 = ((struct sockaddr_in6 *)&from)->sin6_addr;
    }

    /* are we talking to ourselves? */
    if (if_ifwithaddr(&from_addr) != NULL) {
      return;
    }
    olsr_in_if = if_ifwithsock(fd);
    if (olsr_in_if == NULL) {
      OLSR_WARN(LOG_PACKET_PARSING, "Could not find input interface for message from %s size %d\n",
                olsr_ip_to_string(&buf, &from_addr), cc);
      return;
    }
    // call preprocessors
    packet = &inbuf[0];
    for (entry = preprocessor_functions; entry != NULL; entry = entry->next) {
      packet = entry->function(packet, olsr_in_if, &from_addr, &cc);
      // discard package ?
      if (packet == NULL) {
        return;
      }
    }

    /*
     * &from - sender
     * &inbuf.olsr
     * cc - bytes read
     */
    parse_packet((struct olsr *)packet, cc, olsr_in_if, &from_addr);
  }
}

/**
 *Processing OLSR data from socket. Reading data, setting
 *wich interface recieved the message, Sends IPC(if used)
 *and passes the packet on to parse_packet().
 *
 *@param fd the filedescriptor that data should be read from.
 *@return nada
 */
void
olsr_input_hostemu(int fd, void *data __attribute__ ((unused)), unsigned int flags __attribute__ ((unused)))
{
  /* sockaddr_in6 is bigger than sockaddr !!!! */
  struct sockaddr_storage from;
  socklen_t fromlen;
  struct interface *olsr_in_if;
  union olsr_ip_addr from_addr;
  uint16_t pcklen;
  struct preprocessor_function_entry *entry;
  char *packet;
  char inbuf[MAXMESSAGESIZE + 1];
#ifndef REMOVE_LOG_WARN
  struct ipaddr_str buf;
#endif

  /* Host emulator receives IP address first to emulate
     direct link */

  int cc = recv(fd, from_addr.v6.s6_addr, olsr_cnf->ipsize, 0);
  if (cc != (int)olsr_cnf->ipsize) {
    OLSR_WARN(LOG_NETWORKING, "Error receiving host-client IP hook(%d) %s!\n", cc, strerror(errno));
    memcpy(&from_addr, &((struct olsr *)inbuf)->olsr_msg->originator, olsr_cnf->ipsize);
  }

  /* are we talking to ourselves? */
  if (if_ifwithaddr(&from_addr) != NULL) {
    return;
  }

  /* Extract size */
  cc = recv(fd, (void *)&pcklen, 2, MSG_PEEK);  /* Win needs a cast */
  if (cc != 2) {
    if (cc <= 0) {
      OLSR_ERROR(LOG_NETWORKING, "Lost olsr_switch connection - exit!\n");
      olsr_exit(EXIT_FAILURE);
    }
    OLSR_WARN(LOG_NETWORKING, "[hust-emu] error extracting size(%d) %s!\n", cc, strerror(errno));
    return;
  }
  pcklen = ntohs(pcklen);

  fromlen = sizeof(from);
  cc = olsr_recvfrom(fd, inbuf, pcklen, 0, (struct sockaddr *)&from, &fromlen);
  if (cc <= 0) {
    if (cc < 0 && errno != EWOULDBLOCK) {
      OLSR_WARN(LOG_NETWORKING, "error recvfrom: %s", strerror(errno));
    }
    return;
  }

  if (cc != pcklen) {
    OLSR_WARN(LOG_NETWORKING, "Could not read whole packet(size %d, read %d)\n", pcklen, cc);
    return;
  }

  olsr_in_if = if_ifwithsock(fd);
  if (olsr_in_if == NULL) {
    OLSR_WARN(LOG_NETWORKING, "Could not find input interface for message from %s size %d\n",
              olsr_ip_to_string(&buf, &from_addr), cc);
    return;
  }
  // call preprocessors
  packet = &inbuf[0];
  for (entry = preprocessor_functions; entry != NULL; entry = entry->next) {
    packet = entry->function(packet, olsr_in_if, &from_addr, &cc);
    // discard package ?
    if (packet == NULL) {
      return;
    }
  }

  /*
   * &from - sender
   * &inbuf.olsr
   * cc - bytes read
   */
  parse_packet((struct olsr *)inbuf, cc, olsr_in_if, &from_addr);
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

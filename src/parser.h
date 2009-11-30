
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


#ifndef _OLSR_MSG_PARSER
#define _OLSR_MSG_PARSER

#include "duplicate_set.h"
#include "olsr_protocol.h"
#include "lq_packet.h"

#define PROMISCUOUS 0xffffffff

#define MIN_MESSAGE_SIZE()	((int)(8 + olsr_cnf->ipsize))

/* Function returns false if the message should not be forwarded */
typedef void parse_function(struct olsr_message *, struct interface *, union olsr_ip_addr *, enum duplicate_status);

struct parse_function_entry {
  uint32_t type;                       /* If set to PROMISCUOUS all messages will be received */
  parse_function *function;
  struct parse_function_entry *next;
};

typedef uint8_t *preprocessor_function(uint8_t *packet, struct interface *, union olsr_ip_addr *, int *length);

struct preprocessor_function_entry {
  preprocessor_function *function;
  struct preprocessor_function_entry *next;
};

typedef void packetparser_function(struct olsr_packet *pkt, uint8_t *binary, struct interface *in_if, union olsr_ip_addr *from_addr);

struct packetparser_function_entry {
  packetparser_function *function;
  struct packetparser_function_entry *next;
};

void
olsr_init_parser(void);

void
olsr_deinit_parser(void);

void
olsr_input(int, void *, unsigned int);

void
EXPORT(olsr_parser_add_function) (parse_function, uint32_t);

int
EXPORT(olsr_parser_remove_function) (parse_function);

void
EXPORT(olsr_preprocessor_add_function) (preprocessor_function);

int
EXPORT(olsr_preprocessor_remove_function) (preprocessor_function);

void
EXPORT(olsr_packetparser_add_function) (packetparser_function * function);

int
EXPORT(olsr_packetparser_remove_function) (packetparser_function * function);
#endif

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */


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


#ifndef _OLSR_CLOCK
#define _OLSR_CLOCK

#include "defs.h"
#include "olsr_types.h"

/* Some defs for juggling with timers */
#define MSEC_PER_SEC 1000
#define USEC_PER_SEC 1000000
#define NSEC_PER_USEC 1000
#define USEC_PER_MSEC 1000

struct millitxt_buf {
  char buf[16];
};

void olsr_clock_init(void);
void olsr_clock_update(void);

int32_t EXPORT(olsr_clock_getRelative) (uint32_t absolute);
bool EXPORT(olsr_clock_isPast) (uint32_t s);
uint32_t EXPORT(olsr_clock_getNow)(void);

/**
 * Returns a timestamp s seconds in the future
 * @param s milliseconds until timestamp
 * @return absolute time when event will happen
 */
static inline uint32_t
olsr_clock_getAbsolute(uint32_t relative)
{
  return olsr_clock_getNow() + relative;
}

uint32_t EXPORT(olsr_clock_decode_olsrv1) (const uint8_t);
uint8_t EXPORT(olsr_clock_encode_olsrv1) (const uint32_t);

char *EXPORT(olsr_clock_to_string)(struct millitxt_buf *buffer, uint32_t t);
uint32_t EXPORT(olsr_clock_parse_string)(char *txt);

#endif

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

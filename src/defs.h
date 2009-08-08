
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

#ifndef _OLSR_DEFS
#define _OLSR_DEFS

#include "olsr_types.h"

#include <stdio.h>
#include <errno.h>

/* Export symbol for use in plugins. See ../olsrd-exports.sh */
#define EXPORT(sym) sym

extern const char EXPORT(olsrd_version)[];
extern const char EXPORT(build_date)[];
extern const char EXPORT(build_host)[];

#define	MAXMESSAGESIZE		1500    /* max broadcast size */
#define UDP_IPV4_HDRSIZE        28
#define UDP_IPV6_HDRSIZE        62

#if __GNUC__
extern FILE *EXPORT(debug_handle);
#endif

#define ARRAYSIZE(x)	(sizeof(x)/sizeof(*(x)))
#ifndef MAX
#define MAX(x,y)	((x) > (y) ? (x) : (y))
#endif
#ifndef MIN
#define MIN(x,y)	((x) < (y) ? (x) : (y))
#endif

/* we actually want the below #define. But to easily check for "errors" because of
 * too large inline functions, we want to have just "inline" there.
 */
#ifndef INLINE
#ifdef __GNUC__
#define INLINE inline __attribute__((always_inline))
#else
#define INLINE inline
#endif
#endif

#define ROUND_UP_TO_POWER_OF_2(val, pow2) (((val) + (pow2) - 1) & ~((pow2) - 1))

#ifdef WIN32
#define CLOSESOCKET(fd)  do { closesocket(fd); (fd) = -1; } while (0)
#else
#define CLOSESOCKET(fd)  do { close(fd); (fd) = -1; } while (0)
#endif

enum app_state {
  STATE_INIT,
  STATE_RUNNING,
  STATE_SHUTDOWN,
#ifndef WIN32
  STATE_RECONFIGURE,
#endif
};

extern volatile enum app_state app_state;

#endif

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

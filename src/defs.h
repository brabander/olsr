
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

#ifdef WIN32
#define IF_NAMESIZE 32
#endif

/* Export symbol for use in plugins. See ../olsrd-exports.sh */
#ifndef EXPORT
#  define EXPORT(x) x
#endif

extern const char EXPORT(olsrd_version)[];
extern const char EXPORT(build_date)[];
extern const char EXPORT(build_host)[];

#define	MAXMESSAGESIZE		1500    /* max broadcast size */
#define UDP_IPV4_HDRSIZE        28
#define UDP_IPV6_HDRSIZE        62

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

/*
 * On ARM, the compiler spits out additional warnings if called
 * with -Wcast-align if you cast e.g. char* -> int*. While this
 * is fine, most of that warnings are un-critical. Also the ARM
 * CPU will throw BUS_ERROR if alignment does not fit. For this,
 * we add an additional cast to (void *) to prevent the warning.
 */
#define ARM_NOWARN_ALIGN(x) ((void *)(x))
#define ARM_CONST_NOWARN_ALIGN const void *

#define ROUND_UP_TO_POWER_OF_2(val, pow2) (((val) + (pow2) - 1) & ~((pow2) - 1))

enum app_state {
  STATE_INIT,
  STATE_RUNNING,
  STATE_SHUTDOWN,
#ifndef WIN32
  STATE_RECONFIGURE,
#endif
};

extern enum app_state app_state;

#endif

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

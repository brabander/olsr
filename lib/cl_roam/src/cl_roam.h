
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

#ifndef _OLSRD_PLUGIN_TEST
#define _OLSRD_PLUGIN_TEST

#include "plugin.h"
#include "olsr.h"

#define INET_NET       0
#define INET_PREFIX    0

#define PROCENTRY_ROUTE "/proc/net/route"


#define CLIENT_ROAM_MESSAGE   134
#define MAX_MESSAGE           134



/*
 * Client Roam Plugin message
 */

enum cl_roam_status_types {
  CL_ROAM_PINGABLE,            /* 0 */
  CL_ROAM_NOT_PINGABLE,            /* 1 */
};



struct cl_roam_msg
{
  union olsr_ip_addr   client_ip;
  union olsr_ip_addr   node_ip;
  u_int64_t mac;
  uint16_t last_seen;
};




int check_gw(union olsr_ip_addr *net, union olsr_ip_addr *mask);

/* Event function to register with the scheduler */
void olsr_event1(void *foo);
void olsr_event2(void *foo);

#endif

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

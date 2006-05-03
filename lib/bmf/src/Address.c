/*
 * OLSR Basic Multicast Forwarding (BMF) plugin.
 * Copyright (c) 2005, 2006, Thales Communications, Huizen, The Netherlands.
 * Written by Erik Tromp.
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
 * * Neither the name of Thales, BMF nor the names of its 
 *   contributors may be used to endorse or promote products derived 
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED 
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* $Id: Address.c,v 1.1 2006/05/03 08:59:04 kattemat Exp $ */

#include "Address.h"

/* System includes */
#include <assert.h> /* assert() */

/* OLSRD includes */
#include "defs.h" /* COMP_IP */

/* Plugin includes */
#include "Bmf.h" /* BMF_ENCAP_PORT */

int IsMulticast(union olsr_ip_addr* ipAddress)
{
  assert(ipAddress != NULL);

  return (ntohl(ipAddress->v4) & 0xF0000000) == 0xE0000000;
}

int IsLocalBroadcast(union olsr_ip_addr* destIp, struct interface* ifFrom)
{
  struct sockaddr_in* sin;
  
  assert(destIp != NULL);

  /* Protect ourselves against bogus input */
  if (ifFrom == NULL) return 0;

  /* Cast down to correct sockaddr subtype */
  sin = (struct sockaddr_in*)&(ifFrom->int_broadaddr);

  /* Just in case OLSR does not have int_broadaddr filled in for this
   * interface. */
  if (sin == NULL) return 0;

  return COMP_IP(&(sin->sin_addr.s_addr), destIp);
}

int IsOlsrOrBmfPacket(unsigned char* buffer, ssize_t len)
{
  u_int16_t port;

  assert(buffer != NULL);

  /* Consider OLSR and BMF packets not to be local broadcast
   * OLSR packets are UDP - port 698
   * BMF packets are UDP - port 50505 */

  memcpy(&port, buffer + 0x24, 2);
  port = ntohs(port);

  if (len > 0x25 &&
      buffer[0x17] == 0x11 && /* UDP */
      (port == OLSRPORT || port == BMF_ENCAP_PORT))
  {
    return 1;
  }
  return 0;
}

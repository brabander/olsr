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

/* $Id: Packet.c,v 1.1 2006/05/03 08:59:04 kattemat Exp $ */

#include "Packet.h"

/* System includes */
#include <assert.h> /* assert() */
#include <sys/types.h> /* u_int32_t */
#include <netinet/in.h> /* ntohs(), htons() */
#include <linux/ip.h>

/* Retrieve the TTL (Time To Live) value from the IP header of the
 * passed ethernet packet */
int GetIpTtl(unsigned char* buffer)
{
  struct iphdr* iph;

  assert(buffer != NULL);

  iph = (struct iphdr*) (buffer + IP_HDR_OFFSET);
  return iph->ttl;
}

void SaveTtlAndChecksum(unsigned char* buffer, struct TSaveTtl* sttl)
{
  struct iphdr* iph;

  assert(buffer != NULL && sttl != NULL);

  iph = (struct iphdr*) (buffer + IP_HDR_OFFSET);
  sttl->ttl = iph->ttl;
  sttl->check = ntohs(iph->check);
}

void RestoreTtlAndChecksum(unsigned char* buffer, struct TSaveTtl* sttl)
{
  struct iphdr* iph;

  assert(buffer != NULL && sttl != NULL);

  iph = (struct iphdr*) (buffer + IP_HDR_OFFSET);
  iph->ttl = sttl->ttl;
  iph->check = htons(sttl->check);
}

/* For an IP packet, decrement the TTL value and update the IP header
 * checksum accordingly. See also RFC1141. */
void PacketDecreaseTtlAndUpdateHeaderChecksum(unsigned char* buffer)
{
  struct iphdr* iph;
  u_int32_t sum;

  assert(buffer != NULL);

  iph = (struct iphdr*) (buffer + IP_HDR_OFFSET);

  iph->ttl--; /* decrement ttl */
  sum = ntohs(iph->check) + 0x100; /* increment checksum high byte */
  iph->check = htons(sum + (sum>>16)); /* add carry */
}

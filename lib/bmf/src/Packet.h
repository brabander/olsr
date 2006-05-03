#ifndef _BMF_PACKET_H
#define _BMF_PACKET_H

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

/* $Id: Packet.h,v 1.1 2006/05/03 08:59:04 kattemat Exp $ */

/* System includes */
#include <net/if.h> /* IFNAMSIZ, IFHWADDRLEN */
#include <sys/types.h> /* u_int8_t, u_int16_t */

/* Offsets and sizes into IP-ethernet packets */
#define IPV4_ADDR_SIZE 4
#define ETH_TYPE_OFFSET (2*IFHWADDRLEN)
#define ETH_TYPE_LEN 2
#define IP_HDR_OFFSET (ETH_TYPE_OFFSET + ETH_TYPE_LEN)
#define IPV4_OFFSET_SRCIP 12
#define IPV4_OFFSET_DSTIP (IPV4_OFFSET_SRCIP + IPV4_ADDR_SIZE)

#define IPV4_TYPE 0x0800

struct TSaveTtl
{
  u_int8_t ttl;
  u_int16_t check;
};

int GetIpTtl(unsigned char* buffer);
void SaveTtlAndChecksum(unsigned char* buffer, struct TSaveTtl* sttl);
void RestoreTtlAndChecksum(unsigned char* buffer, struct TSaveTtl* sttl);
void PacketDecreaseTtlAndUpdateHeaderChecksum(unsigned char* buffer);

#endif /* _BMF_PACKET_H */

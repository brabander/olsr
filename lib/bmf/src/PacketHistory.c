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

/* $Id: PacketHistory.c,v 1.1 2006/05/03 08:59:04 kattemat Exp $ */

#include "PacketHistory.h"

/* System includes */
#include <assert.h> /* assert() */
#include <sys/types.h> /* u_int16_t, u_int32_t */
#include <string.h> /* memset */

/* OLSRD includes */
#include "olsr.h" /* olsr_printf */

/* Plugin includes */
#include "Packet.h"

static u_int32_t PacketHistory[HISTORY_TABLE_SIZE];

/* Calculate 16-bits CRC according to CRC-CCITT specification, modified
 * to leave out some parts of the packet. */
static u_int16_t CalcCrcCcitt(unsigned char* buffer, ssize_t len)
{
  /* Initial value of 0xFFFF should be 0x1D0F according to
   * www.joegeluso.com/software/articles/ccitt.htm */
  u_int16_t crc = 0xFFFF; 
  int i;

  assert(buffer != NULL);

  for (i = 0; i < len; i++)
  {
    /* Skip IP header checksum; we want to avoid as much as possible
     * calculating a checksum over data containing a checksum */
    if (i >= 12 && i < 14) continue;

    crc  = (unsigned char)(crc >> 8) | (crc << 8);
    crc ^= buffer[i];
    crc ^= (unsigned char)(crc & 0xff) >> 4;
    crc ^= (crc << 8) << 4;
    crc ^= ((crc & 0xff) << 4) << 1;
  }
  return crc;

#if 0
  /* Alternative, simpler and perhaps just as good: add source IP address,
   * destination IP address and IP identification, in 16-bit */
  return
    ((buffer[0x0E] << 8) + buffer[0x0F]) + ((buffer[0x10] << 8) + buffer[0x11]) +
    ((buffer[0x12] << 8) + buffer[0x13]) + ((buffer[0x14] << 8) + buffer[0x15]) +
    ((buffer[0x06] << 8) + buffer[0x07]);
#endif
}

void InitPacketHistory()
{
  memset(PacketHistory, 0, sizeof(PacketHistory));
}

/* Record the fact that this packet was seen recently */
void MarkRecentPacket(unsigned char* buffer, ssize_t len)
{
  u_int16_t crc;
  u_int32_t index;
  uint offset;

  assert(buffer != NULL);

  /* Start CRC calculation at ethertype; skip source and destination MAC 
   * addresses */
  crc = CalcCrcCcitt(buffer + ETH_TYPE_OFFSET, len - ETH_TYPE_OFFSET);

  index = crc / NPACKETS_PER_ENTRY;
  assert(index < HISTORY_TABLE_SIZE);

  offset = (crc % NPACKETS_PER_ENTRY) * NBITS_PER_PACKET;
  assert(offset <= NBITS_IN_UINT32 - NBITS_PER_PACKET);

  /* Mark "seen recently" */
  PacketHistory[index] = PacketHistory[index] | (0x3u << offset);
}

/* Check if this packet was seen recently */
int CheckMarkRecentPacket(unsigned char* buffer, ssize_t len)
{
  u_int16_t crc;
  u_int32_t index;
  uint offset;
  u_int32_t bitMask;
  int result;

  assert(buffer != NULL);

  /* Start CRC calculation at ethertype; skip source and destination MAC 
   * addresses */
  crc = CalcCrcCcitt(buffer + ETH_TYPE_OFFSET, len - ETH_TYPE_OFFSET);

  index = crc / NPACKETS_PER_ENTRY;
  assert(index < HISTORY_TABLE_SIZE);

  offset =  (crc % NPACKETS_PER_ENTRY) * NBITS_PER_PACKET;
  assert(offset <= NBITS_IN_UINT32 - NBITS_PER_PACKET);

  bitMask = 0x1u << offset;
  result = ((PacketHistory[index] & bitMask) == bitMask);
  
  /* Always mark "seen recently" */
  PacketHistory[index] = PacketHistory[index] | (0x3u << offset);

  return result;
}
  
void PrunePacketHistory(void* useless)
{
  uint i;
  for (i = 0; i < HISTORY_TABLE_SIZE; i++)
  {
    if (PacketHistory[i] > 0)
    {
      uint j;
      for (j = 0; j < NPACKETS_PER_ENTRY; j++)
      {
        uint offset = j * NBITS_PER_PACKET;

        u_int32_t bitMask = 0x3u << offset;
        u_int32_t bitsSeenRecenty = 0x3u << offset;
        u_int32_t bitsTimingOut = 0x1u << offset;

        /* 10 should never occur */
        assert ((PacketHistory[i] & bitMask) != (0x2u << offset));
        
        if ((PacketHistory[i] & bitMask) == bitsSeenRecenty)
        {
          /* 11 -> 01 */
          PacketHistory[i] &= ~bitMask | bitsTimingOut;
        }
        else if ((PacketHistory[i] & bitMask) == bitsTimingOut)
        {
          /* 01 -> 00 */
          PacketHistory[i] &= ~bitMask;
        }
      } /* for (j = ...) */
    } /* if (PacketHistory[i] > 0) */
  } /* for (i = ...) */
}

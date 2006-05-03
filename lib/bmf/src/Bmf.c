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

/* $Id: Bmf.c,v 1.1 2006/05/03 08:59:04 kattemat Exp $ */

#define _MULTI_THREADED

#include "Bmf.h"

/* System includes */
#include <stdio.h> /* NULL */
#include <sys/types.h> /* ssize_t */
#include <string.h> /* strerror() */
#include <errno.h> /* errno */
#include <assert.h> /* assert() */
#include <linux/if_packet.h> /* struct sockaddr_ll, PACKET_MULTICAST */
#include <pthread.h> /* pthread_create() */
#include <signal.h> /* SIGINT */

/* OLSRD includes */
#include "defs.h" /* olsr_cnf */
#include "olsr.h" /* olsr_printf */
#include "scheduler.h" /* olsr_register_scheduler_event */
#include "mid_set.h" /* mid_lookup_main_addr() */
#include "mpr_selector_set.h" /* olsr_lookup_mprs_set() */

/* Plugin includes */
#include "NetworkInterfaces.h" /* TBmfInterface, CreateBmfNetworkInterfaces(), CloseBmfNetworkInterfaces() */
#include "Address.h" /* IsMulticast(), IsLocalBroadcast() */
#include "Packet.h" /* ETH_TYPE_OFFSET, IFHWADDRLEN etc. */
#include "PacketHistory.h" /* InitPacketHistory() */
#include "DropList.h" /* DropMac() */

static pthread_t BmfThread;
static int BmfThreadRunning = 0;


static void BmfPacketCaptured(struct TBmfInterface* intf, unsigned char* buffer, ssize_t len)
{
  struct interface* ifFrom;
  unsigned char* srcMac;
  union olsr_ip_addr srcIp;
  union olsr_ip_addr destIp;
  union olsr_ip_addr* originator;
  struct sockaddr_in sin;
  struct TBmfInterface* nextFwIntf;

  /* Only forward IPv4 packets */
  u_int16_t type;
  memcpy(&type, buffer + ETH_TYPE_OFFSET, 2);
  if (ntohs(type) != IPV4_TYPE)
  {
    return;
  }

  /* Lookup the OLSR interface on which this packet is received */
  ifFrom = intf->olsrIntf;

  /* Only forward multicast or local broadcast packets */
  COPY_IP(&destIp, buffer + IP_HDR_OFFSET + IPV4_OFFSET_DSTIP);
  if (! IsMulticast(&destIp) && ! IsLocalBroadcast(&destIp, ifFrom))
  {
    return;
  }
  
  /* Discard OLSR packets (UDP port 698) and BMF encapsulated packets
   * (UDP port 50505) */
  if (IsOlsrOrBmfPacket(buffer, len))
  {
    return;
  }

  /* Apply drop list for testing purposes. */
  srcMac = buffer + IFHWADDRLEN;
  if (IsInDropList(srcMac))
  {
    return;
  }

  /* Lookup main address of source */
	COPY_IP(&srcIp, buffer + IP_HDR_OFFSET + IPV4_OFFSET_SRCIP);
  originator = mid_lookup_main_addr(&srcIp);
  if (originator == NULL)
  {
    originator = &srcIp;
  }

  olsr_printf(
    9,
    "MC pkt to %s received from originator %s (%s) via \"%s\"\n",
    olsr_ip_to_string(&destIp),
    olsr_ip_to_string(originator),
    olsr_ip_to_string(&srcIp),
    intf->ifName);

  /* Check if I am MPR for that originator */
  if (ifFrom != NULL && olsr_lookup_mprs_set(originator) == NULL)
  {
    olsr_printf(
      9,
      "--> Discarding pkt: I am not selected as MPR by that originator\n");
    return;
  }

  /* If this packet is captured on a non-OLSR interface, decrease
   * the TTL and re-calculate the IP header checksum */
  if (ifFrom == NULL)
  {
    PacketDecreaseTtlAndUpdateHeaderChecksum(buffer);
  }

  /* If the TTL is <= 0, do not forward this packet */
  if (GetIpTtl(buffer) <= 0)
  {
    return;
  }

  /* Check if this packet was seen recently */
  if (CheckMarkRecentPacket(buffer, len))
  {
    olsr_printf(
      9,
      "--> Discarding pkt: was a duplicate\n");
    return;
  }

  /* Encapsulate and forward packet on all OLSR interfaces */
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = htons(BMF_ENCAP_PORT);

  nextFwIntf = BmfInterfaces;
  while (nextFwIntf != NULL)
  {
    struct TBmfInterface* fwIntf = nextFwIntf;
    nextFwIntf = fwIntf->next;

    if (fwIntf->olsrIntf != NULL)
    {
      int nBytesWritten;

      /* Change source MAC address to that of myself */
      memcpy(buffer + IFHWADDRLEN, fwIntf->macAddr, IFHWADDRLEN);

      /* Destination address is local broadcast */
      sin.sin_addr.s_addr = ((struct sockaddr_in*)&fwIntf->olsrIntf->int_broadaddr)->sin_addr.s_addr;

      nBytesWritten = sendto(
        fwIntf->encapsulatingSkfd,
        buffer,
        len,
        MSG_DONTROUTE,
        (struct sockaddr*) &sin,
        sizeof(sin));                   

      if (nBytesWritten != len)
      {
        olsr_printf(
          1,
          "%s: sendto() error forwarding MC pkt for %s to \"%s\": %s\n",
          PLUGIN_NAME,
          olsr_ip_to_string(&destIp),
          fwIntf->olsrIntf->int_name,
          strerror(errno));
      }
      else
      {
        olsr_printf(
          9,
          "Successfully encapsulated one MC pkt for %s to \"%s\"\n",
          olsr_ip_to_string(&destIp),
          fwIntf->olsrIntf->int_name);
      } /* if (nBytesWritten != len) */
    } /* if (fwIntf->olsrIntf != NULL) */
  } /* while (nextFwIntf != NULL) */
}

static void BmfEncapsulatedPacketReceived(
  struct TBmfInterface* intf, 
  struct in_addr srcIp,
  unsigned char* buffer,
  ssize_t len)
{
  union olsr_ip_addr fromAddr;
  struct interface* ifFrom;
  union olsr_ip_addr* forwarder;
  int nBytesToWrite;
  unsigned char* bufferToWrite;
  int nBytesWritten;
  int iAmMpr;
  struct sockaddr_in sin;
  struct TBmfInterface* nextFwIntf;

  COPY_IP(&fromAddr, &srcIp.s_addr);

  /* Are we talking to ourselves? */
  if (if_ifwithaddr(&fromAddr) != NULL)
  {
    return;
  }

  /* Lookup the OLSR interface on which this packet is received */
  ifFrom = intf->olsrIntf;

  /* Encapsulated packet received on non-OLSR interface? Then discard */
  if (ifFrom == NULL)
  {
    return;
  }

  /* Apply drop list? No, not needed: encapsulated packets are routed,
   * so filtering should be done by adding a rule to the iptables FORWARD
   * chain, e.g.:
   * iptables -A FORWARD -m mac --mac-source 00:0C:29:28:0E:CC -j DROP */

  /* Lookup main address of forwarding node */
  forwarder = mid_lookup_main_addr(&fromAddr);
  if (forwarder == NULL)
  {
    forwarder = &fromAddr;
    olsr_printf(
      9,
      "Encapsulated MC pkt received; forwarder to me by %s on \"%s\"\n",
      olsr_ip_to_string(forwarder),
      intf->ifName);
  }
  else
  {
    olsr_printf(
      9,
      "Encapsulated MC pkt received; forwarder to me by %s (thru %s) on \"%s\"\n",
      olsr_ip_to_string(forwarder),
      olsr_ip_to_string(&fromAddr),
      intf->ifName);
  }

  /* Check if this packet was seen recently */
  if (CheckMarkRecentPacket(buffer, len))
  {
    olsr_printf(
      9,
      "--> Discarding encapsulated pkt: was a duplicate\n");
    return;
  }

  /* Unpack encapsulated packet and send a copy to myself via the EtherTunTap device */
  nBytesToWrite = len;
  bufferToWrite = buffer;
  if (TunOrTap == TT_TUN)
  {
    nBytesToWrite -= IP_HDR_OFFSET;
    bufferToWrite += IP_HDR_OFFSET;
  }
  nBytesWritten = write(EtherTunTapFd, bufferToWrite, nBytesToWrite);
  if (nBytesWritten != nBytesToWrite)
  {
    olsr_printf(
      1,
      "%s: write() error sending encapsulated MC pkt to \"%s\": %s\n",
      PLUGIN_NAME,
      EtherTunTapIfName,
      strerror(errno));
  }
  else
  {
    olsr_printf(
      9,
      "Successfully unpacked and sent encapsulated MC pkt to \"%s\"\n",
      EtherTunTapIfName);
  }

  /* Check if I am MPR for the forwarder */
  iAmMpr = (olsr_lookup_mprs_set(forwarder) != NULL);
  if (! iAmMpr)
  {
    olsr_printf(
      9,
      "--> Not forwarding encapsulated pkt: I am not selected as MPR by that forwarder\n");
  }

  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = htons(BMF_ENCAP_PORT);

  nextFwIntf = BmfInterfaces;
  while (nextFwIntf != NULL)
  {
    struct TBmfInterface* fwIntf = nextFwIntf;
    nextFwIntf = fwIntf->next;

    /* On non-OLSR interfaces: unpack encapsulated packet, decrease TTL
     * and send */
    if (fwIntf->olsrIntf == NULL)
    {
      struct TSaveTtl sttl;

      /* Change source MAC address to that of sending interface */
      memcpy(buffer + IFHWADDRLEN, fwIntf->macAddr, IFHWADDRLEN);

      /* Save IP header checksum and the TTL-value of the packet, then 
       * decrease the TTL by 1 before writing */
      SaveTtlAndChecksum(buffer, &sttl);
      PacketDecreaseTtlAndUpdateHeaderChecksum(buffer);

      /* If the TTL is <= 0, do not forward this packet */
      if (GetIpTtl(buffer) <= 0)
      {
        return;
      }

      nBytesWritten = write(fwIntf->capturingSkfd, buffer, len);
      if (nBytesWritten != len)
      {
        olsr_printf(
          1,
          "%s: write() error sending unpacked encapsulated MC pkt to \"%s\": %s\n",
          PLUGIN_NAME,
          fwIntf->ifName,
          strerror(errno));
      }
      else
      {
        olsr_printf(
          9,
          "Successfully unpacked and sent one encapsulated MC pkt to \"%s\"\n",
          fwIntf->ifName);
      }

      /* Restore the IP header checksum and the TTL-value of the packet */
      RestoreTtlAndChecksum(buffer, &sttl);
    } /* if (fwIntf->olsrIntf == NULL) */

    /* On OLSR interfaces, forward the packet if this node is selected as MPR by the 
     * forwarding node */
    else if (iAmMpr)
    {
      /* Change source MAC address to that of sending interface */
      memcpy(buffer + IFHWADDRLEN, fwIntf->macAddr, IFHWADDRLEN);

      /* Destination address is local broadcast */
      sin.sin_addr.s_addr = ((struct sockaddr_in*)&fwIntf->olsrIntf->int_broadaddr)->sin_addr.s_addr;

      nBytesWritten = sendto(
        fwIntf->encapsulatingSkfd,
        buffer,
        len,
        MSG_DONTROUTE,
        (struct sockaddr*) &sin,
        sizeof(sin));                   

      if (nBytesWritten != len)
      {
        olsr_printf(
          1,
          "%s: sendto() error forwarding encapsulated MC pkt to \"%s\": %s\n",
          PLUGIN_NAME,
          fwIntf->olsrIntf->int_name,
          strerror(errno));
      }
      else
      {
        olsr_printf(
          9,
          "Successfully forwarded one encapsulated MC pkt to \"%s\"\n",
          fwIntf->olsrIntf->int_name);
      }
    } /* else if (iAmMpr) */
  } /* while (nextFwIntf != NULL) */
}

static void DoBmf(void* useless)
{
#define BUFFER_MAX 2048
  struct TBmfInterface* intf;
  int nFdBitsSet;

  /* Compose set of socket file descriptors. 
   * Keep the highest descriptor seen. */
  int highestSkfd = -1;
  fd_set input_set;
  FD_ZERO(&input_set);

  intf = BmfInterfaces;
  while (intf != NULL)
  {
    FD_SET(intf->capturingSkfd, &input_set);
    if (intf->capturingSkfd > highestSkfd)
    {
      highestSkfd = intf->capturingSkfd;
    }

    if (intf->encapsulatingSkfd >= 0)
    {
      FD_SET(intf->encapsulatingSkfd, &input_set);
      if (intf->encapsulatingSkfd > highestSkfd)
      {
        highestSkfd = intf->encapsulatingSkfd;
      }
    }

    intf = intf->next;    
  }

  assert(highestSkfd >= 0);

  /* Wait (blocking) for packets received on any of the sockets */
  nFdBitsSet = select(highestSkfd + 1, &input_set, NULL, NULL, NULL);
  if (nFdBitsSet < 0)
  {
    if (errno != EINTR)
    {
      olsr_printf(1, "%s: select() error: %s\n", PLUGIN_NAME, strerror(errno));
    }
    return;
  }
    
  if (nFdBitsSet == 0)
  {
    /* No packets waiting. This is unexpected; normally we would excpect select(...)
     * to return only if at least one packet was received (so nFdBitsSet > 0), or
     * if this thread received a signal (so nFdBitsSet < 0). */
    return;
  }

  while (nFdBitsSet > 0)
  {
    intf = BmfInterfaces;
    while (intf != NULL)
    {
      int skfd = intf->capturingSkfd;
      if (FD_ISSET(skfd, &input_set))
      {
        unsigned char buffer[BUFFER_MAX];
        struct sockaddr_ll pktAddr;
        socklen_t addrLen;
        int nBytes;

        /* A packet was captured */

        nFdBitsSet--;

        memset(&pktAddr, 0, sizeof(struct sockaddr_ll));
        addrLen = sizeof(pktAddr);

        nBytes = recvfrom(skfd, buffer, BUFFER_MAX, 0, (struct sockaddr*)&pktAddr, &addrLen);
        if (nBytes < 0)
        {
          olsr_printf(1, "%s: recvfrom() error: %s\n", PLUGIN_NAME, strerror(errno));
        }

        /* Don't let BMF crash by sending too short packets. IP packets are always
         * at least 14 (Ethernet header) + 20 (IP header) = 34 bytes long. */
        if (nBytes >= 34)
        {
          if (pktAddr.sll_pkttype == PACKET_OUTGOING)
          {
            union olsr_ip_addr destIp;
            COPY_IP(&destIp, buffer + IP_HDR_OFFSET + IPV4_OFFSET_DSTIP);
            if (IsMulticast(&destIp) || IsLocalBroadcast(&destIp, intf->olsrIntf))
            {
              if (! IsOlsrOrBmfPacket(buffer, nBytes))
              {
                MarkRecentPacket(buffer, nBytes);
              }
            }
          }
          else if (pktAddr.sll_pkttype == PACKET_MULTICAST ||
                   pktAddr.sll_pkttype == PACKET_BROADCAST)
          {
            BmfPacketCaptured(intf, buffer, nBytes);
          }
        } /* if (nBytes >= 34) */
      } /* if (FD_ISSET...) */

      skfd = intf->encapsulatingSkfd;
      if (skfd >= 0 && (FD_ISSET(skfd, &input_set)))
      {
        unsigned char buffer[BUFFER_MAX];
        struct sockaddr_in addr;
        socklen_t addrLen = sizeof(addr);
        int nBytes;

        /* An encapsulated packet was received */

        nFdBitsSet--;

        memset(&addr, 0, sizeof(addr));

        nBytes = recvfrom(skfd, buffer, BUFFER_MAX, 0, (struct sockaddr*)&addr, &addrLen);
        if (nBytes < 0)
        {
          olsr_printf(1, "%s: recvfrom() error: %s\n", PLUGIN_NAME, strerror(errno));
        }
        if (nBytes > 0)
        {
          BmfEncapsulatedPacketReceived(intf, addr.sin_addr, buffer, nBytes);
        }
      } /* if (skfd >= 0 && (FD_ISSET...) */

      intf = intf->next;    
    } /* while (intf != NULL) */
  } /* while (nFdBitsSet > 0) */
}

static void BmfSignalHandler(int signo)
{
  /* Dummy handler function */
  return;
}

/* Thread entry function. Another thread can gracefully stop this thread by writing a
 * '0' into global variable 'BmfThreadRunning' followed by sending a SIGALRM signal. */
static void* BmfRun(void* useless)
{
  sigset_t blockedSigs;
  sigfillset(&blockedSigs);
  sigdelset(&blockedSigs, SIGALRM);
  if (pthread_sigmask(SIG_BLOCK, &blockedSigs, NULL) < 0)
  {
    olsr_printf(1, "%s: pthread_sigmask() error: %s\n", PLUGIN_NAME, strerror(errno));
  }

  /* Set up the signal handler for the process: use SIGALRM to terminate
   * the BMF thread. Only if a signal handler is specified, does a blocking
   * system call return with errno set to EINTR; if a signal hander is not
   * specified, any system call in which the thread may be waiting will not
   * return. Note that the BMF thread is usually blocked in the select()
   * function (see DoBmf()). */
  if (signal(SIGALRM, BmfSignalHandler) == SIG_ERR)
  {
    olsr_printf(1, "%s: signal() error: %s\n", PLUGIN_NAME, strerror(errno));
  }

  /* Call the thread function until flagged to exit */
  while (BmfThreadRunning != 0)
  {
    DoBmf(useless);
  }
  
  return NULL;
}

/* Initialize the BMF plugin */
int InitBmf()
{
  /* Check validity */
  if (olsr_cnf->ip_version != AF_INET)
  {
    fprintf(stderr, PLUGIN_NAME ": This plugin only supports IPv4!\n");
    return 0;
  }

  /* Clear the packet history */
  InitPacketHistory();

  if (CreateBmfNetworkInterfaces() < 0)
  {
    fprintf(stderr, PLUGIN_NAME ": could not initialize network interfaces!\n");
    return 0;
  }
  
  /* Run the multicast packet processing thread */
  BmfThreadRunning = 1;
  pthread_create(&BmfThread, NULL, BmfRun, NULL);

  /* Register the duplicate registration pruning process */
  olsr_register_scheduler_event(&PrunePacketHistory, NULL, 3.0, 2.0, NULL);

  return 1;
}

/* Close the BMF plugin */
void CloseBmf()
{
  /* Signal BmfThread to exit */
  BmfThreadRunning = 0;
  if (pthread_kill(BmfThread, SIGALRM) < 0)
  /* Strangely enough, all running threads receive the SIGALRM signal. But only the
   * BMF thread is affected by this signal, having specified a handler for this
   * signal in its thread entry function BmfRun(...). */
  {
    olsr_printf(1, "%s: pthread_kill() error: %s\n", PLUGIN_NAME, strerror(errno));
  }

  /* Wait for BmfThread to acknowledge */
  if (pthread_join(BmfThread, NULL) < 0)
  {
    olsr_printf(1, "%s: pthread_join() error: %s\n", PLUGIN_NAME, strerror(errno));
  }

  /* Time to clean up */
  CloseBmfNetworkInterfaces();
}

int RegisterBmfParameter(char* key, char* value)
{
  if (strcmp(key, "Drop") == 0)
  {
    return DropMac(value);
  }
  else if (strcmp(key, "NonOlsrIf") == 0)
  {
    return AddNonOlsrBmfIf(value);
  }

  return 0;
}

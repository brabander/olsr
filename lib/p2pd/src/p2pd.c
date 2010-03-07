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


#include "p2pd.h"

/* System includes */
#include <stddef.h>             /* NULL */
#include <sys/types.h>          /* ssize_t */
#include <string.h>             /* strerror() */
#include <stdarg.h>             /* va_list, va_start, va_end */
#include <errno.h>              /* errno */
#include <assert.h>             /* assert() */
#include <unistd.h>
#include <fcntl.h>
#include <linux/if_ether.h>     /* ETH_P_IP */
#include <linux/if_packet.h>    /* struct sockaddr_ll, PACKET_MULTICAST */
//#include <pthread.h> /* pthread_t, pthread_create() */
#include <signal.h>             /* sigset_t, sigfillset(), sigdelset(), SIGINT */
#include <netinet/ip.h>         /* struct ip */
#include <netinet/udp.h>        /* struct udphdr */
#include <unistd.h>             /* close() */

#include <netinet/in.h>
#include <netinet/ip6.h>

#include <time.h>

/* OLSRD includes */
#include "plugin_util.h"        /* set_plugin_int */
#include "defs.h"               /* olsr_cnf, //OLSR_PRINTF */
#include "ipcalc.h"
#include "olsr.h"               /* //OLSR_PRINTF */
#include "mid_set.h"            /* mid_lookup_main_addr() */
#include "link_set.h"           /* get_best_link_to_neighbor() */
#include "net_olsr.h"           /* ipequal */
#include "log.h"                /* Teco: syslog */
#include "parser.h"

/* plugin includes */
#include "NetworkInterfaces.h"  /* NonOlsrInterface, CreateBmfNetworkInterfaces(), CloseBmfNetworkInterfaces() */
//#include "Address.h"            /* IsMulticast() */
#include "Packet.h"             /* ENCAP_HDR_LEN, BMF_ENCAP_TYPE, BMF_ENCAP_LEN etc. */
#include "dllist.h"

int P2pdTtl=0;
int P2pdDuplicateTimeout           = P2PD_VALID_TIME;

/* List of UDP destination address and port information */
struct UdpDestPort *                 UdpDestPortList = NULL;

/* List of filter entries to check for duplicate messages
 */
struct node *                        dupFilterHead = NULL;
struct node *                        dupFilterTail = NULL;

clockid_t clockid = CLOCK_MONOTONIC;

bool is_broadcast(const struct sockaddr_in addr);
bool is_multicast(const struct sockaddr_in addr);

/* -------------------------------------------------------------------------
 * Function   : PacketReceivedFromOLSR
 * Description: Handle a received packet from a OLSR message
 * Input      : ipPacket into an unsigned char and the lenght of the packet
 * Output     : none
 * Return     : none
 * Data Used  : BmfInterfaces
 * ------------------------------------------------------------------------- */
static void
PacketReceivedFromOLSR(unsigned char *encapsulationUdpData, int len)
{
  struct ip *ipHeader;        /* IP header inside the encapsulated IP packet */
  struct ip6_hdr *ip6Header;  /* IP header inside the encapsulated IP packet */
  struct NonOlsrInterface *walker;
  int stripped_len = 0;
  ipHeader = (struct ip *)encapsulationUdpData;
  ip6Header = (struct ip6_hdr *)encapsulationUdpData;
  //OLSR_DEBUG(LOG_PLUGINS, "P2PD PLUGIN got packet from OLSR message\n");

  /* Check with each network interface what needs to be done on it */
  for (walker = nonOlsrInterfaces; walker != NULL; walker = walker->next) {
    /* To a non-OLSR interface: unpack the encapsulated IP packet and forward it */
    if (walker->olsrIntf == NULL) {
      int nBytesWritten;
      struct sockaddr_ll dest;

      memset(&dest, 0, sizeof(dest));
      dest.sll_family = AF_PACKET;
      if ((encapsulationUdpData[0] & 0xf0) == 0x40) {
        dest.sll_protocol = htons(ETH_P_IP);
        stripped_len = ntohs(ipHeader->ip_len);
      }
      
      if ((encapsulationUdpData[0] & 0xf0) == 0x60) {
        dest.sll_protocol = htons(ETH_P_IPV6);
        stripped_len = 40 + ntohs(ip6Header->ip6_plen); //IPv6 Header size (40) + payload_len 
      }
      
      // Sven-Ola: Don't know how to handle the "stripped_len is uninitialized" condition, maybe exit(1) is better...?
      if (0 == stripped_len)
        return;
      
      //TODO: if packet is not IP die here
      
      if (stripped_len > len) {
      }
      
      dest.sll_ifindex = if_nametoindex(walker->ifName);
      dest.sll_halen = IFHWADDRLEN;

      if (olsr_cnf->ip_version == AF_INET) {
        /* Use all-ones as destination MAC address. When the IP destination is
         * a multicast address, the destination MAC address should normally also
         * be a multicast address. E.g., when the destination IP is 224.0.0.1,
         * the destination MAC should be 01:00:5e:00:00:01. However, it does not
         * seem to matter when the destination MAC address is set to all-ones
         * in that case. */

        if (IsMulticastv4(ipHeader)) {
          dest.sll_addr[0] = 0x01;
          dest.sll_addr[1] = 0x00;
          dest.sll_addr[2] = 0x5E;
          dest.sll_addr[3] = (ipHeader->ip_dst.s_addr >> 16) & 0xFF;
          dest.sll_addr[4] = (ipHeader->ip_dst.s_addr >> 8) & 0xFF;
          dest.sll_addr[5] = ipHeader->ip_dst.s_addr & 0xFF;
        } else /* if (IsBroadcast(ipHeader)) */ {
          memset(dest.sll_addr, 0xFF, IFHWADDRLEN);
        }
      } else /*(olsr_cnf->ip_version == AF_INET6) */ {
        if (IsMulticastv6(ip6Header)) {
          dest.sll_addr[0] = 0x33;
          dest.sll_addr[1] = 0x33;
          dest.sll_addr[2] = ip6Header->ip6_dst.s6_addr[12];
          dest.sll_addr[3] = ip6Header->ip6_dst.s6_addr[13];
          dest.sll_addr[4] = ip6Header->ip6_dst.s6_addr[14];
          dest.sll_addr[5] = ip6Header->ip6_dst.s6_addr[15];
        }
      }

      nBytesWritten = sendto(walker->capturingSkfd,
                             encapsulationUdpData,
                             stripped_len,
                             0,
                             (struct sockaddr *)&dest,
                             sizeof(dest));
      if (nBytesWritten != stripped_len) {
        P2pdPError("sendto() error forwarding unpacked encapsulated pkt on \"%s\"", walker->ifName);
      } else {

        //OLSR_PRINTF(
        //  2,
        //  "%s: --> unpacked and forwarded on \"%s\"\n",
        //  PLUGIN_NAME_SHORT,
        //  walker->ifName);
      }
    }                           /* if (walker->olsrIntf == NULL) */
  }
}                               /* PacketReceivedFromOLSR */

/* Highest-numbered open socket file descriptor. To be used as first
 * parameter in calls to select(...). */
int HighestSkfd = -1;

/* Set of socket file descriptors */
fd_set InputSet;

bool
p2pd_message_seen(struct node **head, struct node **tail, union olsr_message *m)
{
  struct node * curr;
  time_t now;

  now = time(NULL);
  
  // Check whether any entries have aged
  curr = *head;
  while (curr) {
    struct DupFilterEntry *filter;
    struct node * next = curr->next; // Save the current pointer since curr may be destroyed
    
    filter = (struct DupFilterEntry*)curr->data;
    
    if ((filter->creationtime + P2pdDuplicateTimeout) < now)
      remove_node(head, tail, curr, true);
      
    // Skip to the next element
    curr = next;
  }
  
  // Now check whether there are any duplicates
  for (curr = *head; curr; curr = curr->next) {
    struct DupFilterEntry *filter = (struct DupFilterEntry*)curr->data;
    
    if (olsr_cnf->ip_version == AF_INET) {
      if (filter->address.v4.s_addr  == m->v4.originator &&
          filter->msgtype            == m->v4.olsr_msgtype &&
          filter->seqno              == m->v4.seqno) {
          return true;
      }
    } else /* if (olsr_cnf->ip_version == AF_INET6) */ {
      if (memcmp(filter->address.v6.s6_addr,
                 m->v6.originator.s6_addr,
                 sizeof(m->v6.originator.s6_addr)) == 0 &&
          filter->msgtype            == m->v6.olsr_msgtype &&
          filter->seqno              == m->v6.seqno) {
          return true;
      }
    }
  }
  
  return false;
}

void
p2pd_store_message(struct node **head, struct node **tail, union olsr_message *m)
{
  time_t now;

  // Store a message into the database
  struct DupFilterEntry *new_dup = calloc(1, sizeof(struct DupFilterEntry));
  if (new_dup == NULL) {
    olsr_printf(1, "P2PD: Out of memory\n");
    return;
  }

  now = time(NULL);
  
  new_dup->creationtime = now;
  if (olsr_cnf->ip_version == AF_INET) {
    new_dup->address.v4.s_addr = m->v4.originator;
    new_dup->msgtype = m->v4.olsr_msgtype;
    new_dup->seqno = m->v4.seqno;
  } else /* if (olsr_cnf->ip_version == AF_INET6) */ {
    memcpy(new_dup->address.v6.s6_addr,
           m->v6.originator.s6_addr,
           sizeof(m->v6.originator.s6_addr));
    new_dup->msgtype = m->v6.olsr_msgtype;
    new_dup->seqno = m->v6.seqno;
  }
  
  // Add the element to the head of the list
  append_node(head, tail, new_dup);
}

bool
p2pd_is_duplicate_message(union olsr_message *msg)
{
  if(p2pd_message_seen(&dupFilterHead, &dupFilterTail, msg)) {
    return true;
  }

  p2pd_store_message(&dupFilterHead, &dupFilterTail, msg);
  
  return false;
}

bool
olsr_parser(union olsr_message *m, struct interface *in_if __attribute__ ((unused)), union olsr_ip_addr *ipaddr __attribute__ ((unused)))
{
  union olsr_ip_addr originator;
  int size;
  uint32_t vtime;

  //OLSR_DEBUG(LOG_PLUGINS, "P2PD PLUGIN: Received msg in parser\n");
  
	/* Fetch the originator of the messsage */
  if (olsr_cnf->ip_version == AF_INET) {
    memcpy(&originator, &m->v4.originator, olsr_cnf->ipsize);
    vtime = me_to_reltime(m->v4.olsr_vtime);
    size = ntohs(m->v4.olsr_msgsize);
  } else {
    memcpy(&originator, &m->v6.originator, olsr_cnf->ipsize);
    vtime = me_to_reltime(m->v6.olsr_vtime);
    size = ntohs(m->v6.olsr_msgsize);
  }

  /* Check if message originated from this node.
   *         If so - back off */
  if (ipequal(&originator, &olsr_cnf->main_addr))
    return false;          /* Don't forward either */

  /* Check for duplicates for processing */
  if (p2pd_is_duplicate_message(m))
    return true;  /* Don't process but allow to be forwarded */

  if (olsr_cnf->ip_version == AF_INET) {
    PacketReceivedFromOLSR((unsigned char *)&m->v4.message, size - 12);
  } else {
    PacketReceivedFromOLSR((unsigned char *)&m->v6.message, size - 12 - 96);
  }

	return true;
}

//Sends a packet in the OLSR network
void
olsr_p2pd_gen(unsigned char *packet, int len)
{
  /* send buffer: huge */
  char buffer[10240];
  int aligned_size;
  union olsr_message *message = (union olsr_message *)buffer;
  struct interface *ifn;
  
  aligned_size=len;

  if ((aligned_size % 4) != 0) {
    aligned_size = (aligned_size - (aligned_size % 4)) + 4;
  }

  /* fill message */
  if (olsr_cnf->ip_version == AF_INET) {
    /* IPv4 */
    message->v4.olsr_msgtype  = P2PD_MESSAGE_TYPE;
    message->v4.olsr_vtime    = reltime_to_me(P2PD_VALID_TIME * MSEC_PER_SEC);
    memcpy(&message->v4.originator, &olsr_cnf->main_addr, olsr_cnf->ipsize);
    message->v4.ttl           = P2pdTtl ? P2pdTtl : MAX_TTL;
    message->v4.hopcnt        = 0;
    message->v4.seqno         = htons(get_msg_seqno());
    message->v4.olsr_msgsize  = htons(aligned_size + 12);
    memset(&message->v4.message, 0, aligned_size);
    memcpy(&message->v4.message, packet, len);
    aligned_size = aligned_size + 12;
  } else /* if (olsr_cnf->ip_version == AF_INET6) */ {
    /* IPv6 */
    message->v6.olsr_msgtype  = P2PD_MESSAGE_TYPE;
    message->v6.olsr_vtime    = reltime_to_me(P2PD_VALID_TIME * MSEC_PER_SEC);
    memcpy(&message->v6.originator, &olsr_cnf->main_addr, olsr_cnf->ipsize);
    message->v6.ttl           = P2pdTtl ? P2pdTtl : MAX_TTL;
    message->v6.hopcnt        = 0;
    message->v6.seqno         = htons(get_msg_seqno());
    message->v6.olsr_msgsize  = htons(aligned_size + 12 + 96);
    memset(&message->v6.message, 0, aligned_size);
    memcpy(&message->v6.message, packet, len);
    aligned_size = aligned_size + 12 + 96;
  }

  /* looping trough interfaces */
  for (ifn = ifnet; ifn; ifn = ifn->int_next) {
    //OLSR_PRINTF(1, "P2PD PLUGIN: Generating packet - [%s]\n", ifn->int_name);

    if (net_outbuffer_push(ifn, message, aligned_size) != aligned_size) {
      /* send data and try again */
      net_output(ifn);
      if (net_outbuffer_push(ifn, message, aligned_size) != aligned_size) {
        //OLSR_PRINTF(1, "P2PD PLUGIN: could not send on interface: %s\n", ifn->int_name);
      }
    }
  }
}

/* -------------------------------------------------------------------------
 * Function   : P2pdPError
 * Description: Prints an error message at OLSR debug level 1.
 *              First the plug-in name is printed. Then (if format is not NULL
 *              and *format is not empty) the arguments are printed, followed
 *              by a colon and a blank. Then the message and a new-line.
 * Input      : format, arguments
 * Output     : none
 * Return     : none
 * Data Used  : none
 * ------------------------------------------------------------------------- */
void
P2pdPError(const char *format, ...)
{
#define MAX_STR_DESC 255
  char strDesc[MAX_STR_DESC];

#if !defined REMOVE_LOG_DEBUG
  char *stringErr = strerror(errno);
#endif

  /* Rely on short-circuit boolean evaluation */
  if (format == NULL || *format == '\0') {
    //OLSR_DEBUG(LOG_PLUGINS, "%s: %s\n", PLUGIN_NAME, stringErr);
  } else {
    va_list arglist;

    va_start(arglist, format);
    vsnprintf(strDesc, MAX_STR_DESC, format, arglist);
    va_end(arglist);

    strDesc[MAX_STR_DESC - 1] = '\0';   /* Ensures null termination */
    
#if !defined REMOVE_LOG_DEBUG
    OLSR_DEBUG(LOG_PLUGINS, "%s: %s\n", strDesc, stringErr);
#endif
  }
}                               /* P2pdPError */

/* -------------------------------------------------------------------------
 * Function   : MainAddressOf
 * Description: Lookup the main address of a node
 * Input      : ip - IP address of the node
 * Output     : none
 * Return     : The main IP address of the node
 * Data Used  : none
 * ------------------------------------------------------------------------- */
union olsr_ip_addr *
MainAddressOf(union olsr_ip_addr *ip)
{
  union olsr_ip_addr *result;

  /* TODO: mid_lookup_main_addr() is not thread-safe! */
  result = mid_lookup_main_addr(ip);
  if (result == NULL) {
    result = ip;
  }
  return result;
}                               /* MainAddressOf */


/* -------------------------------------------------------------------------
 * Function   : InUdpDestPortList
 * Description: Check whether the specified address and port is in the list of
 *              configured UDP destination/port entries
 * Input      : ip_version  - IP version to use for this check
 *              addr        - address to check for in the list
 *              port        - port to check for in the list
 * Output     : none
 * Return     : true if destination/port combination was found, false otherwise
 * Data Used  : UdpDestPortList
 * ------------------------------------------------------------------------- */
bool
InUdpDestPortList(int ip_version, union olsr_ip_addr *addr, uint16_t port)
{
  struct UdpDestPort *walker;
  
  for (walker = UdpDestPortList; walker; walker = walker->next) {
    if (walker->ip_version == ip_version) {
      if (ip_version == AF_INET) {
        if (addr->v4.s_addr == walker->address.v4.s_addr &&
            walker->port == port)
          return true;  // Found so we can stop here
      } else /* ip_version == AF_INET6 */ {
        if (memcmp(addr->v6.s6_addr,
                   walker->address.v6.s6_addr,
                   sizeof(addr->v6.s6_addr) == 0) &&
            walker->port == port)
          return true;  // Found so we can stop here
      }
    }
  }
  return false;
}

/* -------------------------------------------------------------------------
 * Function   : P2pdPacketCaptured
 * Description: Handle a captured IP packet
 * Input      : encapsulationUdpData - space for the encapsulation header, 
 * 							followed by the captured IP packet
 * 							nBytes - The number of bytes in the data packet 
 * Output     : none
 * Return     : none
 * Data Used  : P2pdInterfaces
 * Notes      : The IP packet is assumed to be captured on a socket of family
 *              PF_PACKET and type SOCK_DGRAM (cooked).
 * ------------------------------------------------------------------------- */
static void
P2pdPacketCaptured(unsigned char *encapsulationUdpData, int nBytes)
{
  union olsr_ip_addr src;      /* Source IP address in captured packet */
  union olsr_ip_addr dst;      /* Destination IP address in captured packet */
  union olsr_ip_addr *origIp;  /* Main OLSR address of source of captured packet */
  struct ip *ipHeader;         /* The IP header inside the captured IP packet */
  struct ip6_hdr *ipHeader6;   /* The IP header inside the captured IP packet */
  struct udphdr *udpHeader;
  u_int16_t destPort;

  if ((encapsulationUdpData[0] & 0xf0) == 0x40) {       //IPV4

    ipHeader = (struct ip *)encapsulationUdpData;

    dst.v4 = ipHeader->ip_dst;

    if (ipHeader->ip_p != SOL_UDP) {
      /* Not UDP */
      //OLSR_PRINTF(1,"NON UDP PACKET\n");
      return;                   /* for */
    }

    // If we're dealing with a fragment we bail out here since there's no valid
    // UDP header in this message
    if (IsIpv4Fragment(ipHeader)) {
      return;
    }
    
    udpHeader = (struct udphdr *)(encapsulationUdpData +
                                  GetIpHeaderLength(encapsulationUdpData));
    destPort = ntohs(udpHeader->dest);
    
    if (!InUdpDestPortList(AF_INET, &dst, destPort))
       return;
  }                            //END IPV4
  else if ((encapsulationUdpData[0] & 0xf0) == 0x60) {  //IPv6

    ipHeader6 = (struct ip6_hdr *)encapsulationUdpData;
    
    memcpy(&dst.v6, &ipHeader6->ip6_dst, sizeof(struct in6_addr));
    
    if (ipHeader6->ip6_dst.s6_addr[0] == 0xff)  //Multicast
    {
      //Continue
    } else {
      return;                   //not multicast
    }
    if (ipHeader6->ip6_nxt != SOL_UDP) {
      /* Not UDP */
      //OLSR_PRINTF(1,"NON UDP PACKET\n");
      return;                   /* for */
    }
    
    // Check whether this is a IPv6 fragment
    if (IsIpv6Fragment(ipHeader6)) {
      return;
    }
    
    udpHeader = (struct udphdr *)(encapsulationUdpData + 40);
    destPort = ntohs(udpHeader->dest);

    if (!InUdpDestPortList(AF_INET6, &dst, destPort))
      return;
  }                             //END IPV6
  else {
    return;                     //Is not IP packet
  }

  /* Lookup main address of source in the MID table of OLSR */
  origIp = MainAddressOf(&src);

  // send the packet to OLSR forward mechanism
  olsr_p2pd_gen(encapsulationUdpData, nBytes);
}                               /* P2pdPacketCaptured */


/* -------------------------------------------------------------------------
 * Function   : DoP2pd
 * Description: This function is registered with the OLSR scheduler and called when something is captured
 * Input      : none
 * Output     : none
 * Return     : none
 * Data Used  :
 * ------------------------------------------------------------------------- */
void
DoP2pd(int skfd, void *data __attribute__ ((unused)), unsigned int flags __attribute__ ((unused)))
{
  unsigned char rxBuffer[P2PD_BUFFER_SIZE];
  if (skfd >= 0) {
    struct sockaddr_ll pktAddr;
    socklen_t addrLen = sizeof(pktAddr);
    int nBytes;
    unsigned char *ipPacket;

    /* Receive the captured Ethernet frame, leaving space for the BMF
     * encapsulation header */
    ipPacket = GetIpPacket(rxBuffer);
    nBytes = recvfrom(skfd, ipPacket, P2PD_BUFFER_SIZE,  //TODO: understand how to change this
                      0, (struct sockaddr *)&pktAddr, &addrLen);
    if (nBytes < 0) {

      return;                   /* for */
    }

    /* if (nBytes < 0) */
    /* Check if the number of received bytes is large enough for an IP
     * packet which contains at least a minimum-size IP header.
     * Note: There is an apparent bug in the packet socket implementation in
     * combination with VLAN interfaces. On a VLAN interface, the value returned
     * by 'recvfrom' may (but need not) be 4 (bytes) larger than the value
     * returned on a non-VLAN interface, for the same ethernet frame. */
    if (nBytes < (int)sizeof(struct ip)) {
      ////OLSR_PRINTF(
      //              1,
      //              "%s: captured frame too short (%d bytes) on \"%s\"\n",
      //              PLUGIN_NAME,
      //              nBytes,
      //              walker->ifName);

      return;                   /* for */
    }

    if (pktAddr.sll_pkttype == PACKET_OUTGOING ||
        pktAddr.sll_pkttype == PACKET_MULTICAST ||
        pktAddr.sll_pkttype == PACKET_BROADCAST) {
      /* A multicast or broadcast packet was captured */

      P2pdPacketCaptured(ipPacket, nBytes);

    }                           /* if (pktAddr.sll_pkttype == ...) */
  }                             /* if (skfd >= 0 && (FD_ISSET...)) */
}                               /* DoP2pd */

/* -------------------------------------------------------------------------
 * Function   : InitP2pd
 * Description: Initialize the P2pd plugin
 * Input      : skipThisInterface - pointer to interface to skip
 * Output     : none
 * Return     : Always 0
 * Data Used  : none
 * ------------------------------------------------------------------------- */
int
InitP2pd(struct interface *skipThisIntf)
{
  //Tells OLSR to launch olsr_parser when the packets for this plugin arrive
  //olsr_parser_add_function(&olsr_parser, PARSER_TYPE,1);
  olsr_parser_add_function(&olsr_parser, PARSER_TYPE);
  
  //Creates captures sockets and register them to the OLSR scheduler
  CreateNonOlsrNetworkInterfaces(skipThisIntf);

  return 0;
}                               /* InitP2pd */

/* -------------------------------------------------------------------------
 * Function   : CloseP2pd
 * Description: Close the P2pd plugin and clean up
 * Input      : none
 * Output     : none
 * Return     : none
 * Data Used  :
 * ------------------------------------------------------------------------- */
void
CloseP2pd(void)
{
  CloseNonOlsrNetworkInterfaces();
}

/* -------------------------------------------------------------------------
 * Function   : SetP2pdTtl
 * Description: Set the TTL for message from this plugin
 * Input      : value - parameter value to evaluate
 * Output     : none
 * Return     : Always 0
 * Data Used  : P2pdTtl
 * ------------------------------------------------------------------------- */
int
SetP2pdTtl(const char *value, void *data __attribute__ ((unused)), set_plugin_parameter_addon addon __attribute__ ((unused)))
{
  assert(value != NULL);
  P2pdTtl = atoi(value);

  return 0;
}

bool is_broadcast(const struct sockaddr_in addr)
{
  if (addr.sin_addr.s_addr == 0xFFFFFFFF)
    return true;
    
  return false;
}

bool is_multicast(const struct sockaddr_in addr)
{
  if ((htonl(addr.sin_addr.s_addr) & 0xE0000000) == 0xE0000000)
    return true;

  return false;
}

/* -------------------------------------------------------------------------
 * Function   : AddUdpDestPort
 * Description: Set the UDP destination/port combination as an entry in the
 *              UdpDestPortList
 * Input      : value - parameter value to evaluate
 * Output     : none
 * Return     : -1 on error condition, 0 if all is ok
 * Data Used  : UdpDestPortList
 * ------------------------------------------------------------------------- */
int
AddUdpDestPort(const char *value, void *data __attribute__ ((unused)), set_plugin_parameter_addon addon __attribute__ ((unused)))
{
  char destAddr[INET6_ADDRSTRLEN];
  uint16_t destPort;
  int num;
  struct UdpDestPort *    new;
  struct sockaddr_in      addr4;
  struct sockaddr_in6     addr6;
  int                     ip_version	= AF_INET;
  int                     res;
  
  assert(value != NULL);
  
  // Retrieve the data from the argument string passed
  memset(destAddr, 0, sizeof(destAddr));
  num = sscanf(value, "%45s %hd", destAddr, &destPort);
  if (num != 2) {
    olsr_printf(1, "Invalid argument for \"UdpDestPort\"");
    return -1;
  }
	
  // Check whether we're dealing with an IPv4 or IPv6 address
  // When the string contains a ':' we can assume we're dealing with IPv6
  if (strchr(destAddr, (int)':')) {
    ip_version = AF_INET6;
  }

  // Check whether the specified address was either IPv4 multicast,
  // IPv4 broadcast or IPv6 multicast.

  switch (ip_version) {
  case AF_INET:
    res = inet_pton(AF_INET, destAddr, &addr4.sin_addr);
    if (!is_broadcast(addr4) && !is_multicast(addr4)) {
      olsr_printf(1, "WARNING: IPv4 address must be multicast or broadcast... ");
    }
    break;
  case AF_INET6:
    res = inet_pton(AF_INET6, destAddr, &addr6.sin6_addr);
    if (addr6.sin6_addr.s6_addr[0] != 0xFF) {
      olsr_printf(1, "WARNING: IPv6 address must be multicast... ");
      return -1;
    }
    break;
  }
  // Determine whether it is a valid IP address
  if (res == 0) {
    olsr_printf(1, "Invalid address specified for \"UdpDestPort\"");
    return -1;
  }
	
  // Create a new entry and link it into the chain
  new = calloc(1, sizeof(struct UdpDestPort));
  if (new == NULL) {
    olsr_printf(1, "P2PD: Out of memory");
    return -1;
  }
	
  new->ip_version = ip_version;
  switch (ip_version) {
  case AF_INET:
    new->address.v4.s_addr = addr4.sin_addr.s_addr;
    break;
  case AF_INET6:
    memcpy(&new->address.v6.s6_addr, &addr6.sin6_addr.s6_addr, sizeof(sizeof(addr6.sin6_addr.s6_addr)));
    break;
  }
  new->port = destPort;
  new->next = UdpDestPortList;
  UdpDestPortList = new;
	
  // And then we're done
  return 0;
}


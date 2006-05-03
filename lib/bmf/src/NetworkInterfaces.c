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

/* $Id: NetworkInterfaces.c,v 1.1 2006/05/03 08:59:04 kattemat Exp $ */

#include "NetworkInterfaces.h"

/* System includes */
#include <syslog.h> /* syslog() */
#include <string.h> /* strerror() */
#include <errno.h> /* errno */
#include <unistd.h> /* close() */
#include <sys/ioctl.h> /* ioctl() */
#include <fcntl.h> /* fcntl() */
#include <assert.h> /* assert() */
#include <net/if.h> /* if_indextoname() */
#include <netinet/in.h> /* htons() */
#include <linux/if_ether.h> /* ETH_P_ALL */
#include <linux/if_packet.h> /* packet_mreq, PACKET_MR_PROMISC, PACKET_ADD_MEMBERSHIP */
#include <linux/if_tun.h> /* IFF_TAP */

/* OLSRD includes */
#include "olsr.h" /* olsr_printf */
#include "defs.h" /* olsr_cnf */

/* Plugin includes */
#include "Packet.h" /* IFHWADDRLEN */
#include "Bmf.h" /* PLUGIN_NAME */

/* List of network interfaces used by BMF plugin */
struct TBmfInterface* BmfInterfaces = NULL;

/* File descriptor of EtherTunTap device */
int EtherTunTapFd = -1;

/* Network interface name of EtherTunTap device. If the name starts with "tun", an
 * IP tunnel interface will be used. Otherwise, an EtherTap device will be used. */
const char* EtherTunTapIfName = "tun0"; /* "tap0"; */

/* If the network interface name starts with "tun", an IP tunnel interface will be
 * used, and this variable will be set to TUN. Otherwise, an EtherTap device will
 * be used, and this variable will be set to TAP. */
enum TTunOrTap TunOrTap;

/* Create raw packet socket for capturing multicast IP traffic. Returns
 * the socket descriptor, or -1 if an error occurred. */
static int CreateCaptureSocket(int ifIndex)
{
  struct packet_mreq mreq;
  struct ifreq req;
  struct sockaddr_ll iface_addr;

  /* Open raw packet socket */
  int skfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
  if (skfd < 0)
  {
    olsr_printf(1, "%s: socket(PF_PACKET) error: %s\n", PLUGIN_NAME, strerror(errno));
    return -1;
  }

  /* Set interface to promiscuous mode */
  memset(&mreq, 0, sizeof(struct packet_mreq));
  mreq.mr_ifindex = ifIndex;
  mreq.mr_type = PACKET_MR_PROMISC;
  if (setsockopt(skfd, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
  {
    olsr_printf(1, "%s: setsockopt(PACKET_MR_PROMISC) error: %s\n", PLUGIN_NAME, strerror(errno));
    close(skfd);
    return -1;
  }

  /* Get hardware (MAC) address */
  memset(&req, 0, sizeof(struct ifreq));
  if (if_indextoname(ifIndex, req.ifr_name) == NULL ||
      ioctl(skfd, SIOCGIFHWADDR, &req) < 0)
  {
    olsr_printf(1, "%s: error retrieving MAC address: %s\n", PLUGIN_NAME, strerror(errno));
    close(skfd);
    return -1;
  }
   
  /* Bind the socket to the specified interface */
  memset(&iface_addr, 0, sizeof(iface_addr));
  iface_addr.sll_protocol = htons(ETH_P_ALL);
  iface_addr.sll_ifindex = ifIndex;
  iface_addr.sll_family = AF_PACKET;
  memcpy(iface_addr.sll_addr, req.ifr_hwaddr.sa_data, IFHWADDRLEN);
  iface_addr.sll_halen = IFHWADDRLEN;
    
  if (bind(skfd, (struct sockaddr*)&iface_addr, sizeof(iface_addr)) < 0)
  {
    olsr_printf(1, "%s: bind() error: %s\n", PLUGIN_NAME, strerror(errno));
    close(skfd);
    return -1;
  }

  /* Set socket to blocking operation */
  if (fcntl(skfd, F_SETFL, fcntl(skfd, F_GETFL, 0) & ~O_NONBLOCK) < 0)
  {
    olsr_printf(1, "%s: fcntl() error: %s\n", PLUGIN_NAME, strerror(errno));
    close(skfd);
    return -1;
  }

  return skfd;
}

/* Create UDP (datagram) over IP socket to send encapsulated multicast packets over.
 * Returns the socket descriptor, or -1 if an error occurred. */
static int CreateEncapsulateSocket(int ifIndex)
{
  int on = 1;
  char ifName[IFNAMSIZ];
  struct sockaddr_in sin;

  /* Open UDP-IP socket */
  int skfd = socket(PF_INET, SOCK_DGRAM, 0);
  if (skfd < 0)
  {
    olsr_printf(1, "%s: socket(PF_INET) error: %s\n", PLUGIN_NAME, strerror(errno));
    return -1;
  }

  /* Enable sending to broadcast addresses */
  if (setsockopt(skfd, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) < 0)
  {
    olsr_printf(1, "%s: setsockopt() error: %s\n", PLUGIN_NAME, strerror(errno));
    close(skfd);
    return -1;
  }
	
  /* Bind to the specific network interfaces indicated by ifIndex */
  if (if_indextoname(ifIndex, ifName) == NULL ||
      setsockopt(skfd, SOL_SOCKET, SO_BINDTODEVICE, ifName, strlen(ifName) + 1) < 0)
  {
    olsr_printf(1, "%s: setsockopt() error: %s\n", PLUGIN_NAME, strerror(errno));
    close(skfd);
    return -1;
  }
    
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = htons(BMF_ENCAP_PORT);
  sin.sin_addr.s_addr = htonl(INADDR_ANY);
      
  if (bind(skfd, (struct sockaddr*)&sin, sizeof(sin)) < 0) 
  {
    olsr_printf(1, "%s: bind() error: %s\n", PLUGIN_NAME, strerror(errno));
    close(skfd);
    return -1;
  }

  /* Set socket to blocking operation */
  if (fcntl(skfd, F_SETFL, fcntl(skfd, F_GETFL, 0) & ~O_NONBLOCK) < 0)
  {
    olsr_printf(1, "%s: fcntl() error: %s\n", PLUGIN_NAME, strerror(errno));
    close(skfd);
    return -1;
  }

  return skfd;
}

/* To save the state of the IP spoof filter for the EtherTunTap device */
static char EthTapSpoofState = '1';

static int DeactivateSpoofFilter(const char* ifName)
{
  FILE* procSpoof;
  char procFile[FILENAME_MAX];

  assert(ifName != NULL);

  /* Generate the procfile name */
  sprintf(procFile, "/proc/sys/net/ipv4/conf/%s/rp_filter", ifName);

  procSpoof = fopen(procFile, "r");
  if (procSpoof == NULL)
  {
    fprintf(
      stderr,
      "WARNING! Could not open the %s file to check/disable the IP spoof filter!\n"
      "Are you using the procfile filesystem?\n"
      "Does your system support IPv4?\n"
      "I will continue (in 3 sec) - but you should manually ensure that IP spoof\n"
      "filtering is disabled!\n\n",
      procFile);
      
    sleep(3);
    return 0;
  }

  EthTapSpoofState = fgetc(procSpoof);
  fclose(procSpoof);

  procSpoof = fopen(procFile, "w");
  if (procSpoof == NULL)
  {
    fprintf(stderr, "Could not open %s for writing!\n", procFile);
    fprintf(
      stderr,
      "I will continue (in 3 sec) - but you should manually ensure that IP"
      " spoof filtering is disabled!\n\n");
    sleep(3);
    return 0;
  }

  syslog(LOG_INFO, "Writing \"0\" to %s", procFile);
  fputs("0", procSpoof);

  fclose(procSpoof);

  return 1;
}

static void RestoreSpoofFilter(const char* ifName)
{
  FILE* procSpoof;
  char procFile[FILENAME_MAX];

  assert(ifName != NULL);

  /* Generate the procfile name */
  sprintf(procFile, "/proc/sys/net/ipv4/conf/%s/rp_filter", ifName);

  procSpoof = fopen(procFile, "w");
  if (procSpoof == NULL)
  {
    fprintf(stderr, "Could not open %s for writing!\nSettings not restored!\n", procFile);
  }
  else
  {
    syslog(LOG_INFO, "Resetting %s to %c\n", procFile, EthTapSpoofState);

    fputc(EthTapSpoofState, procSpoof);
    fclose(procSpoof);
  }
}

/* Creates and brings up an EtherTunTap device e.g. "tun0" or "tap0"
 * (as specified in const char* EtherTunTapIfName) */
static int CreateLocalEtherTunTap(void)
{
  struct ifreq ifreq;
  int etfd = open("/dev/net/tun", O_RDWR);
  int skfd;
  int ioctlres = 0;

  if (etfd < 0)
  {
    olsr_printf(1, "%s: open() error: %s\n", PLUGIN_NAME, strerror(errno));
    return -1;
  }

  memset(&ifreq, 0, sizeof(ifreq));

  /* Specify either the IFF_TAP flag for Ethernet frames, or the IFF_TUN flag for IP.
   * Specify IFF_NO_PI for not receiving extra meta packet information. */
  if (strncmp(EtherTunTapIfName, "tun", 3) == 0)
  {
    ifreq.ifr_flags = IFF_TUN;
    TunOrTap = TT_TUN;
  }
  else
  {
    ifreq.ifr_flags = IFF_TAP;
    TunOrTap = TT_TAP;
  }
  ifreq.ifr_flags |= IFF_NO_PI;

  strcpy(ifreq.ifr_name, EtherTunTapIfName);
  if (ioctl(etfd, TUNSETIFF, (void *)&ifreq) < 0)
  {
    olsr_printf(1, "%s: ioctl() error: %s\n", PLUGIN_NAME, strerror(errno));
    close(etfd);
    return -1;
  }

  memset(&ifreq, 0, sizeof(ifreq));
  strcpy(ifreq.ifr_name, EtherTunTapIfName);
  ifreq.ifr_addr.sa_family = AF_INET;
  skfd = socket(PF_INET, SOCK_DGRAM, 0);
  if (skfd >= 0)
  {
    if (ioctl(skfd, SIOCGIFADDR, &ifreq) < 0)
    {
      /* EtherTunTap interface does not yet have an IP address.
       * Give it a dummy IP address "1.2.3.4". */
      struct sockaddr_in *inaddr = (struct sockaddr_in *)&ifreq.ifr_addr;
      inet_aton("1.2.3.4", &inaddr->sin_addr);
      ioctlres = ioctl(skfd, SIOCSIFADDR, &ifreq);

      if (ioctlres >= 0)
      {
        /* Bring EtherTunTap interface up (if not already) */
        ioctlres = ioctl(skfd, SIOCGIFFLAGS, &ifreq);
        if (ioctlres >= 0)
        {
          ifreq.ifr_flags |= (IFF_UP | IFF_RUNNING);
          ioctlres = ioctl(skfd, SIOCSIFFLAGS, &ifreq);
        }
      } /* if (ioctlres >= 0) */
    } /* if (ioctl...) */
  } /* if (skfd >= 0) */
  if (skfd < 0 || ioctlres < 0)
  {
    olsr_printf(
      1,
      "%s: Error bringing up EtherTunTap interface: %s\n",
      PLUGIN_NAME,
      strerror(errno));

    close(etfd);
    if (skfd >= 0)
    {
      close(skfd);
    }
    return -1;
  } /* if (skfd < 0 || ioctlres < 0) */

  /* Set the multicast flag on the interface. TODO: Maybe also set
   * IFF_ALLMULTI. */
  memset(&ifreq, 0, sizeof(ifreq));
  strcpy(ifreq.ifr_name, EtherTunTapIfName);
  ioctlres = ioctl(skfd, SIOCGIFFLAGS, &ifreq);
  if (ioctlres >= 0)
  {
    ifreq.ifr_flags |= IFF_MULTICAST;
    ioctlres = ioctl(skfd, SIOCSIFFLAGS, &ifreq);
  }
  if (ioctlres < 0)
  {
    olsr_printf(
      1,
      "%s: Error setting the multicast flag on EtherTunTap interface: %s\n",
      PLUGIN_NAME,
      strerror(errno));
  }
  close(skfd);
  
  /* Deactivate IP spoof filter for EtherTunTap device */
  DeactivateSpoofFilter(ifreq.ifr_name);

  return etfd;
}

static int IsNullMacAddress(char* mac)
{
  int i;

  assert(mac != NULL);

  for (i = 0; i < IFHWADDRLEN; i++)
  {
    if (mac[i] != 0) return 0;
  }
  return 1;
}

int CreateBmfNetworkInterfaces(void)
{
  int skfd;
  struct ifconf ifc;
  int numreqs = 30;
  struct ifreq* ifr;
  int n;

  EtherTunTapFd = CreateLocalEtherTunTap();
  if (EtherTunTapFd < 0)
  {
    olsr_printf(1, "%s: error creating local EtherTunTap\n", PLUGIN_NAME);
    return -1;    
  }

  skfd = socket(PF_INET, SOCK_DGRAM, 0);
  if (skfd < 0)
  {
    olsr_printf(
      1,
      "%s: No inet socket available: %s\n",
      PLUGIN_NAME,
      strerror(errno));
    return -1;
  }

  /* Retrieve the network interface configuration list */
  ifc.ifc_buf = NULL;
  for (;;)
  {
    ifc.ifc_len = sizeof(struct ifreq) * numreqs;
    ifc.ifc_buf = realloc(ifc.ifc_buf, ifc.ifc_len);

    if (ioctl(skfd, SIOCGIFCONF, &ifc) < 0)
    {
      olsr_printf(1, "%s: SIOCGIFCONF error: %s\n", PLUGIN_NAME, strerror(errno));

      close(skfd);
      free(ifc.ifc_buf);
      return -1;
    }
    if ((unsigned)ifc.ifc_len == sizeof(struct ifreq) * numreqs)
    {
      /* Assume it overflowed; double the space and try again */
      numreqs *= 2;
      assert(numreqs < 1024);
      continue; /* for (;;) */
    }
    break; /* for (;;) */
  } /* for (;;) */

  /* For each item in the interface configuration list... */
  ifr = ifc.ifc_req;
  for (n = ifc.ifc_len / sizeof(struct ifreq); --n >= 0; ifr++)
  {
    struct interface* olsrIntf;
    struct ifreq ifrAddr;
    int capturingSkfd;
    int encapsulatingSkfd = -1;
    struct TBmfInterface* newBmfInterface;

    /* ...find the OLSR interface structure, if any */
    union olsr_ip_addr ipAddr;
    COPY_IP(&ipAddr, &((struct sockaddr_in*)&ifr->ifr_addr)->sin_addr.s_addr);
    olsrIntf = if_ifwithaddr(&ipAddr);

    if (olsrIntf == NULL && ! IsNonOlsrBmfIf(ifr->ifr_name))
    {
      /* Interface is neither OLSR interface, nor specified as non-OLSR BMF
       * interface in the BMF plugin parameter list */
      continue; /* for (n = ...) */
    }

    /* Retrieve the MAC address */
    memset(&ifrAddr, 0, sizeof(struct ifreq));
    strcpy(ifrAddr.ifr_name, ifr->ifr_name); 
    if (ioctl(skfd, SIOCGIFHWADDR, &ifrAddr) < 0)
    {
      olsr_printf(
        1,
        "%s: SIOCGIFHWADDR error for device \"%s\": %s\n",
        PLUGIN_NAME,
        ifrAddr.ifr_name,
        strerror(errno));
      continue; /* for (n = ...) */
    }

    if (IsNullMacAddress(ifrAddr.ifr_hwaddr.sa_data))
    {
      continue; /* for (n = ...) */
    }

    /* Create socket for capturing and sending multicast packets */
    capturingSkfd = CreateCaptureSocket(if_nametoindex(ifr->ifr_name));
    if (capturingSkfd < 0)
    {
      continue; /* for (n = ...) */
    }

    if (olsrIntf != NULL)
    {
      /* Create socket for encapsulating and forwarding multicast packets */
      encapsulatingSkfd = CreateEncapsulateSocket(olsrIntf->if_index);
      if (encapsulatingSkfd < 0)
      {
        close(capturingSkfd);
        continue; /* for (n = ...) */
      }
    }

    newBmfInterface = malloc(sizeof(struct TBmfInterface));
    if (newBmfInterface == NULL)
    {
      close(capturingSkfd);
      close(encapsulatingSkfd);
      continue; /* for (n = ...) */
    }

    newBmfInterface->capturingSkfd = capturingSkfd;
    newBmfInterface->encapsulatingSkfd = encapsulatingSkfd;
    memcpy(newBmfInterface->macAddr, ifrAddr.ifr_hwaddr.sa_data, IFHWADDRLEN);
    memcpy(newBmfInterface->ifName, ifr->ifr_name, IFNAMSIZ);
    newBmfInterface->olsrIntf = olsrIntf;
    newBmfInterface->next = BmfInterfaces;
    BmfInterfaces = newBmfInterface;
  } /* for (n = ...) */
  
  if (BmfInterfaces == NULL)
  {
    olsr_printf(1, "%s: could not initialize any network interface\n", PLUGIN_NAME);
    return -1;
  }

  close(skfd);
  free(ifc.ifc_buf);
  return 0;
}

/* Closes every socket on each network interface used by BMF:
 * -- the local EtherTunTap interface (e.g. "tun0" or "tap0")
 * -- for each OLSR-enabled interface:
 *    - the socket used for capturing multicast packets
 *    - the socket used for encapsulating packets
 * Also restores the network state to the situation before OLSR was
 * started */
void CloseBmfNetworkInterfaces()
{
  int nClosed = 0;
  
  /* Close all opened sockets */
  struct TBmfInterface* nextBmfIf = BmfInterfaces;
  while (nextBmfIf != NULL)
  {
    struct TBmfInterface* bmfIf = nextBmfIf;
    nextBmfIf = bmfIf->next;

    close(bmfIf->capturingSkfd);
    nClosed++;
    if (bmfIf->encapsulatingSkfd >= 0) 
    {
      close(bmfIf->encapsulatingSkfd);
      nClosed++;
    }

    free(bmfIf);
  }
  
  /* Restore IP spoof filter for EtherTunTap device */
  RestoreSpoofFilter(EtherTunTapIfName);

  close(EtherTunTapFd);
  nClosed++;

  olsr_printf(1, "%s: closed %d sockets\n", PLUGIN_NAME, nClosed);
}

#define MAX_NON_OLSR_IFS 10
static char NonOlsrIfNames[MAX_NON_OLSR_IFS][IFNAMSIZ];
static int nNonOlsrIfs = 0;

int AddNonOlsrBmfIf(const char* ifName)
{
  assert(ifName != NULL);

  if (nNonOlsrIfs >= MAX_NON_OLSR_IFS)
  {
    olsr_printf(
      1,
      "%s: too many non-OLSR interfaces specified, maximum %d\n",
      PLUGIN_NAME,
      MAX_NON_OLSR_IFS);
    return 0;
  }

  strncpy(NonOlsrIfNames[nNonOlsrIfs], ifName, IFNAMSIZ);
  nNonOlsrIfs++;
  return 1;
}

int IsNonOlsrBmfIf(const char* ifName)
{
  int i;

  assert(ifName != NULL);

  for (i = 0; i < nNonOlsrIfs; i++)
  {
    if (strncmp(NonOlsrIfNames[i], ifName, IFNAMSIZ) == 0) return 1;
  }
  return 0;
}

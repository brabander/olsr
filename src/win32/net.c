/*
 * The olsr.org Optimized Link-State Routing daemon (olsrd)
 * Copyright (c) 2004, Thomas Lopatic (thomas@lopatic.de)
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
 * $Id: net.c,v 1.12 2005/02/12 23:07:02 spoggle Exp $
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#undef interface

#include <stdio.h>
#include <stdlib.h>
#include "../defs.h"
#include "../net_os.h"

void WinSockPError(char *Str);
void PError(char *);

int olsr_printf(int, char *, ...);

void DisableIcmpRedirects(void);
int disable_ip_forwarding(int Ver);

int getsocket(struct sockaddr *Addr, int BuffSize, char *Int)
{
  int Sock;
  int On = 1;
  unsigned long Len;

  Sock = socket(AF_INET, SOCK_DGRAM, 0);

  if (Sock < 0)
  {
    WinSockPError("getsocket/socket()");
    return -1;
  }

  if (setsockopt(Sock, SOL_SOCKET, SO_BROADCAST,
                 (char *)&On, sizeof (On)) < 0)
  {
    WinSockPError("getsocket/setsockopt(SO_BROADCAST)");
    closesocket(Sock);
    return -1;
  }

  while (BuffSize > 8192)
  {
    if (setsockopt(Sock, SOL_SOCKET, SO_RCVBUF, (char *)&BuffSize,
                   sizeof (BuffSize)) == 0)
      break;

    BuffSize -= 1024;
  }

  if (BuffSize <= 8192) 
    fprintf(stderr, "Cannot set IPv4 socket receive buffer.\n");

  if (bind(Sock, Addr, sizeof (struct sockaddr_in)) < 0)
  {
    WinSockPError("getsocket/bind()");
    closesocket(Sock);
    return -1;
  }

  if (WSAIoctl(Sock, FIONBIO, &On, sizeof (On), NULL, 0, &Len, NULL, NULL) < 0)
  {
    WinSockPError("WSAIoctl");
    closesocket(Sock);
    return -1;
  }

  return Sock;
}

int getsocket6(struct sockaddr_in6 *Addr, int BuffSize, char *Int)
{
  int Sock;
  int On = 1;

  Sock = socket(AF_INET6, SOCK_DGRAM, 0);

  if (Sock < 0)
  {
    WinSockPError("getsocket6/socket()");
    return -1;
  }

  if (setsockopt(Sock, SOL_SOCKET, SO_BROADCAST,
                 (char *)&On, sizeof (On)) < 0)
  {
    WinSockPError("getsocket6/setsockopt(SO_BROADCAST)");
    closesocket(Sock);
    return -1;
  }

  while (BuffSize > 8192)
  {
    if (setsockopt(Sock, SOL_SOCKET, SO_RCVBUF, (char *)&BuffSize,
                   sizeof (BuffSize)) == 0)
      break;

    BuffSize -= 1024;
  }

  if (BuffSize <= 8192) 
    fprintf(stderr, "Cannot set IPv6 socket receive buffer.\n");

  if (bind(Sock, (struct sockaddr *)Addr, sizeof (struct sockaddr_in6)) < 0)
  {
    WinSockPError("getsocket6/bind()");
    closesocket(Sock);
    return -1;
  }

  return Sock;
}

static OVERLAPPED RouterOver;

int enable_ip_forwarding(int Ver)
{
  HMODULE Lib;
  unsigned int __stdcall (*EnableRouter)(HANDLE *Hand, OVERLAPPED *Over);
  HANDLE Hand;

  Ver = Ver;
  
  Lib = LoadLibrary("iphlpapi.dll");

  if (Lib == NULL)
    return 0;

  EnableRouter = (unsigned int _stdcall (*)(HANDLE *, OVERLAPPED *))
    GetProcAddress(Lib, "EnableRouter");

  if (EnableRouter == NULL)
    return 0;

  memset(&RouterOver, 0, sizeof (OVERLAPPED));

  RouterOver.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

  if (RouterOver.hEvent == NULL)
  {
    PError("CreateEvent()");
    return -1;
  }
  
  if (EnableRouter(&Hand, &RouterOver) != ERROR_IO_PENDING)
  {
    PError("EnableRouter()");
    return -1;
  }

  olsr_printf(3, "Routing enabled.\n");

  return 0;
}

int disable_ip_forwarding(int Ver)
{
  HMODULE Lib;
  unsigned int  __stdcall (*UnenableRouter)(OVERLAPPED *Over,
                                            unsigned int *Count);
  unsigned int Count;

  Ver = Ver;
  
  Lib = LoadLibrary("iphlpapi.dll");

  if (Lib == NULL)
    return 0;

  UnenableRouter = (unsigned int _stdcall (*)(OVERLAPPED *, unsigned int *))
    GetProcAddress(Lib, "UnenableRouter");

  if (UnenableRouter == NULL)
    return 0;

  if (UnenableRouter(&RouterOver, &Count) != NO_ERROR)
  {
    PError("UnenableRouter()");
    return -1;
  }

  olsr_printf(3, "Routing disabled, count = %u.\n", Count);

  return 0;
}

int restore_settings(int Ver)
{
  disable_ip_forwarding(Ver);

  return 0;
}

static int SetEnableRedirKey(unsigned long New)
{
  HKEY Key;
  unsigned long Type;
  unsigned long Len;
  unsigned long Old;

  if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                   "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters",
                   0, KEY_READ | KEY_WRITE, &Key) != ERROR_SUCCESS)
    return -1;

  Len = sizeof (Old);

  if (RegQueryValueEx(Key, "EnableICMPRedirect", NULL, &Type,
                      (unsigned char *)&Old, &Len) != ERROR_SUCCESS ||
      Type != REG_DWORD)
    Old = 1;

  if (RegSetValueEx(Key, "EnableICMPRedirect", 0, REG_DWORD,
                    (unsigned char *)&New, sizeof (New)))
  {
    RegCloseKey(Key);
    return -1;
  }

  RegCloseKey(Key);
  return Old;
}

void DisableIcmpRedirects(void)
{
  int Res;

  Res = SetEnableRedirKey(0);

  if (Res != 1)
    return;

  fprintf(stderr, "\n*** IMPORTANT *** IMPORTANT *** IMPORTANT *** IMPORTANT *** IMPORTANT ***\n\n");

#if 0
  if (Res < 0)
  {
    fprintf(stderr, "Cannot disable ICMP redirect processing in the registry.\n");
    fprintf(stderr, "Please disable it manually. Continuing in 3 seconds...\n");
    Sleep(3000);

    return;
  }
#endif

  fprintf(stderr, "I have disabled ICMP redirect processing in the registry for you.\n");
  fprintf(stderr, "REBOOT NOW, so that these changes take effect. Exiting...\n\n");

  exit(0);
}

/* ======== moved from above ======== */

extern struct olsr_netbuf *netbufs[];

/**
 *Sends a packet on a given interface.
 *
 *@param ifp the interface to send on.
 *
 *@return negative on error
 */
int
net_output(struct interface *ifp)
{
  struct sockaddr_in *sin;  
  struct sockaddr_in dst;
  struct sockaddr_in6 *sin6;  
  struct sockaddr_in6 dst6;
  struct ptf *tmp_ptf_list;
  int i, x;
  union olsr_packet *outmsg;

  sin = NULL;
  sin6 = NULL;

  if(!netbufs[ifp->if_nr])
    return -1;

  if(!netbufs[ifp->if_nr]->pending)
    return 0;

  netbufs[ifp->if_nr]->pending += OLSR_HEADERSIZE;

  outmsg = (union olsr_packet *)netbufs[ifp->if_nr]->buff;
  /* Add the Packet seqno */
  outmsg->v4.olsr_seqno = htons(ifp->olsr_seqnum++);
  /* Set the packetlength */
  outmsg->v4.olsr_packlen = htons(netbufs[ifp->if_nr]->pending);

  if(olsr_cnf->ip_version == AF_INET)
    {
      /* IP version 4 */
      sin = (struct sockaddr_in *)&ifp->int_broadaddr;

      /* Copy sin */
      dst = *sin;
      sin = &dst;

      if (sin->sin_port == 0)
	sin->sin_port = olsr_udp_port;
    }
  else
    {
      /* IP version 6 */
      sin6 = (struct sockaddr_in6 *)&ifp->int6_multaddr;
      /* Copy sin */
      dst6 = *sin6;
      sin6 = &dst6;
    }

  /*
   *Call possible packet transform functions registered by plugins  
   */
  tmp_ptf_list = ptf_list;
  while(tmp_ptf_list != NULL)
    {
      tmp_ptf_list->function(netbufs[ifp->if_nr]->buff, &netbufs[ifp->if_nr]->pending);
      tmp_ptf_list = tmp_ptf_list->next;
    }

  /*
   *if the '-disp- option was given
   *we print her decimal contetnt of the packets
   */
  if(disp_pack_out)
    {
      switch(netbufs[ifp->if_nr]->buff[4])
	{
	case(HELLO_MESSAGE):printf("\n\tHELLO ");break;
	case(TC_MESSAGE):printf("\n\tTC ");break;
	case(MID_MESSAGE):printf("\n\tMID ");break;
	case(HNA_MESSAGE):printf("\n\tHNA ");break;
	default:printf("\n\tTYPE: %d ", netbufs[ifp->if_nr]->buff[4]); break;
	}
      if(olsr_cnf->ip_version == AF_INET)
	printf("to %s size: %d\n\t", ip_to_string((olsr_u32_t *)&sin->sin_addr.s_addr), netbufs[ifp->if_nr]->pending);
      else
	printf("to %s size: %d\n\t", ip6_to_string(&sin6->sin6_addr), netbufs[ifp->if_nr]->pending);

      x = 0;

      for(i = 0; i < netbufs[ifp->if_nr]->pending;i++)
	{
	  if(x == 4)
	    {
	      x = 0;
	      printf("\n\t");
	    }
	  x++;
	  if(olsr_cnf->ip_version == AF_INET)
	    printf(" %3i", (u_char) netbufs[ifp->if_nr]->buff[i]);
	  else
	    printf(" %2x", (u_char) netbufs[ifp->if_nr]->buff[i]);
	}
      
      printf("\n");
    }
  
  if(olsr_cnf->ip_version == AF_INET)
    {
      /* IP version 4 */
      if(sendto(ifp->olsr_socket, 
		netbufs[ifp->if_nr]->buff, 
		netbufs[ifp->if_nr]->pending, 
		MSG_DONTROUTE, 
		(struct sockaddr *)sin, 
		sizeof (*sin)) 
	 < 0)
	{
	  perror("sendto(v4)");
	  olsr_syslog(OLSR_LOG_ERR, "OLSR: sendto IPv4 %m");
	  netbufs[ifp->if_nr]->pending = 0;
	  return -1;
	}
    }
  else
    {
      /* IP version 6 */
      if(sendto(ifp->olsr_socket, 
		netbufs[ifp->if_nr]->buff,
		netbufs[ifp->if_nr]->pending, 
		MSG_DONTROUTE, 
		(struct sockaddr *)sin6, 
		sizeof (*sin6)) 
	 < 0)
	{
	  perror("sendto(v6)");
	  olsr_syslog(OLSR_LOG_ERR, "OLSR: sendto IPv6 %m");
	  fprintf(stderr, "Socket: %d interface: %d\n", ifp->olsr_socket, ifp->if_nr);
	  fprintf(stderr, "To: %s (size: %d)\n", ip6_to_string(&sin6->sin6_addr), (int)sizeof(*sin6));
	  fprintf(stderr, "Outputsize: %d\n", netbufs[ifp->if_nr]->pending);
	  netbufs[ifp->if_nr]->pending = 0;
	  return -1;
	}
    }
  
  netbufs[ifp->if_nr]->pending = 0;

  return 1;
}

/* The outputbuffer on neighbornodes
 * will never exceed MAXMESSAGESIZE
 */
static char inbuf[MAXMESSAGESIZE+1];

/**
 *Processing OLSR data from socket. Reading data, setting 
 *wich interface recieved the message, Sends IPC(if used) 
 *and passes the packet on to parse_packet().
 *
 *@param fd the filedescriptor that data should be read from.
 *@return nada
 */
void
olsr_input(int fd)
{
  /* sockaddr_in6 is bigger than sockaddr !!!! */
  struct sockaddr_storage from;
  size_t fromlen;
  int cc;
  struct interface *olsr_in_if;
  union olsr_ip_addr from_addr;


  for (;;) 
    {
      fromlen = sizeof(struct sockaddr_storage);

      cc = recvfrom(fd, 
		    inbuf, 
		    sizeof (inbuf), 
		    0, 
		    (struct sockaddr *)&from, 
		    &fromlen);

      if (cc <= 0) 
	{
	  if (cc < 0 && errno != EWOULDBLOCK)
	    {
	      olsr_printf(1, "error recvfrom: %s", strerror(errno));
	      olsr_syslog(OLSR_LOG_ERR, "error recvfrom: %m");
	    }
	  break;
	}

      if(olsr_cnf->ip_version == AF_INET)
	{
	  /* IPv4 sender address */
	  COPY_IP(&from_addr, &((struct sockaddr_in *)&from)->sin_addr.s_addr);
	}
      else
	{
	  /* IPv6 sender address */
	  COPY_IP(&from_addr, &((struct sockaddr_in6 *)&from)->sin6_addr);
	}

      /* are we talking to ourselves? */
      if(if_ifwithaddr(&from_addr) != NULL)
	return;

#ifdef DEBUG
      olsr_printf(5, "Recieved a packet from %s\n", olsr_ip_to_string((union olsr_ip_addr *)&((struct sockaddr_in *)&from)->sin_addr.s_addr));
#endif
      //printf("\nCC: %d FROMLEN: %d\n\n", cc, fromlen);
      if ((olsr_cnf->ip_version == AF_INET) && (fromlen != sizeof (struct sockaddr_in)))
	break;
      else if ((olsr_cnf->ip_version == AF_INET6) && (fromlen != sizeof (struct sockaddr_in6)))
	break;

      //printf("Recieved data on socket %d\n", socknr);


      if((olsr_in_if = if_ifwithsock(fd)) == NULL)
	{
	  olsr_printf(1, "Could not find input interface for message from %s size %d\n",
		      olsr_ip_to_string(&from_addr),
		      cc);
	  olsr_syslog(OLSR_LOG_ERR, "Could not find input interface for message from %s size %d\n",
		 olsr_ip_to_string(&from_addr),
		 cc);
	  return ;
	}

      /*
       * &from - sender
       * &inbuf.olsr 
       * cc - bytes read
       */
      parse_packet((struct olsr *)inbuf, cc, olsr_in_if, &from_addr);
    
    }
}



/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004, Andreas Tønnesen(andreto@olsr.org)
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
 * $Id: net.c,v 1.9 2005/02/12 23:07:02 spoggle Exp $
 */

#include "../defs.h"
#include "../net_os.h"
#include "../parser.h" /* dnc: needed for call to packet_parser() */
#include "net.h"

#ifdef __NetBSD__
#include <sys/param.h>
#endif

#include <sys/sysctl.h>

static int ignore_redir;
static int send_redir;
static int gateway;

static int first_time = 1;

static int set_sysctl_int(char *name, int new)
{
  int old;
  unsigned int len = sizeof (old);

  if (sysctlbyname(name, &old, &len, &new, sizeof (new)) < 0)
    return -1;

  return old;
}

int enable_ip_forwarding(int version)
{
  char *name;

  if (olsr_cnf->ip_version == AF_INET)
    name = "net.inet.ip.forwarding";

  else
    name = "net.inet6.ip6.forwarding";

  gateway = set_sysctl_int(name, 1);

  if (gateway < 0)
    {
      fprintf(stderr, "Cannot enable IP forwarding. Please enable IP forwarding manually. Continuing in 3 seconds...\n");
      sleep(3);
    }

  return 1;
}

int disable_redirects(char *if_name, int index, int version)
{
  char *name;

  // this function gets called for each interface olsrd uses; however,
  // FreeBSD can only globally control ICMP redirects, and not on a
  // per-interface basis; hence, only disable ICMP redirects on the first
  // invocation

  if (first_time == 0)
    return 1;

  first_time = 0;

  // do not accept ICMP redirects

  if (olsr_cnf->ip_version == AF_INET)
    name = "net.inet.icmp.drop_redirect";

  else
    name = "net.inet6.icmp6.drop_redirect";

  ignore_redir = set_sysctl_int(name, 1);

  if (ignore_redir < 0)
    {
      fprintf(stderr, "Cannot disable incoming ICMP redirect messages. Please disable them manually. Continuing in 3 seconds...\n");
      sleep(3);
    }

  // do not send ICMP redirects

  if (olsr_cnf->ip_version == AF_INET)
    name = "net.inet.ip.redirect";

  else
    name = "net.inet6.ip6.redirect";

  send_redir = set_sysctl_int(name, 0);

  if (send_redir < 0)
    {
      fprintf(stderr, "Cannot disable outgoing ICMP redirect messages. Please disable them manually. Continuing in 3 seconds...\n");
      sleep(3);
    }

  return 1;
}

int deactivate_spoof(char *if_name, int index, int version)
{
  return 1;
}

int restore_settings(int version)
{
  char *name;

  // reset IP forwarding

  if (olsr_cnf->ip_version == AF_INET)
    name = "net.inet.ip.forwarding";

  else
    name = "net.inet6.ip6.forwarding";

  set_sysctl_int(name, gateway);

  // reset incoming ICMP redirects

  if (olsr_cnf->ip_version == AF_INET)
    name = "net.inet.icmp.drop_redirect";

  else
    name = "net.inet6.icmp6.drop_redirect";

  set_sysctl_int(name, ignore_redir);

  // reset outgoing ICMP redirects

  if (olsr_cnf->ip_version == AF_INET)
    name = "net.inet.ip.redirect";

  else
    name = "net.inet6.ip6.redirect";

  set_sysctl_int(name, send_redir);

  return 1;
}

int
getsocket(struct sockaddr *sa, int bufspace, char *int_name)
{
  struct sockaddr_in *sin = (struct sockaddr_in *)sa;
  int sock, on = 1;

  if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
    {
      perror("socket");
      syslog(LOG_ERR, "socket: %m");
      return (-1);
    }

  if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &on, sizeof (on)) < 0)
    {
      perror("setsockopt");
      syslog(LOG_ERR, "setsockopt SO_BROADCAST: %m");
      close(sock);
      return (-1);
    }

  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) 
    {
      perror("SO_REUSEADDR failed");
      return (-1);
    }

  for (on = bufspace; ; on -= 1024) 
    {
      if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF,
		     &on, sizeof (on)) == 0)
	break;
      if (on <= 8*1024) 
	{
	  perror("setsockopt");
	  syslog(LOG_ERR, "setsockopt SO_RCVBUF: %m");
	  break;
	}
    }

  if (bind(sock, (struct sockaddr *)sin, sizeof (*sin)) < 0) 
    {
      perror("bind");
      syslog(LOG_ERR, "bind: %m");
      close(sock);
      return (-1);
    }

  if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1)
    syslog(LOG_ERR, "fcntl O_NONBLOCK: %m\n");

  return (sock);
}

int getsocket6(struct sockaddr_in6 *sin, int bufspace, char *int_name)
{
  int sock, on = 1;

  if ((sock = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) 
    {
      perror("socket");
      syslog(LOG_ERR, "socket: %m");
      return (-1);
    }

  for (on = bufspace; ; on -= 1024) 
    {
      if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF,
		     &on, sizeof (on)) == 0)
	break;
      if (on <= 8*1024) 
	{
	  perror("setsockopt");
	  syslog(LOG_ERR, "setsockopt SO_RCVBUF: %m");
	  break;
	}
    }

  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) 
    {
      perror("SO_REUSEADDR failed");
      return (-1);
    }

  if (bind(sock, (struct sockaddr *)sin, sizeof (*sin)) < 0) 
    {
      perror("bind");
      syslog(LOG_ERR, "bind: %m");
      close(sock);
      return (-1);
    }

  if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1)
    syslog(LOG_ERR, "fcntl O_NONBLOCK: %m\n");

  return (sock);
}

int get_ipv6_address(char *ifname, struct sockaddr_in6 *saddr6, int scope_in)
{
  return 0;
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
      /* dnc: is it some kind of violation to call a routine in the dir above us? */
      parse_packet((struct olsr *)inbuf, cc, olsr_in_if, &from_addr);
    
    }
}



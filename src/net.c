
/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2003 Andreas Tønnesen (andreto@ifi.uio.no)
 *
 * This file is part of the olsr.org OLSR daemon.
 *
 * olsr.org is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * olsr.org is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with olsr.org; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 * 
 * $Id: net.c,v 1.13 2004/09/25 11:13:28 kattemat Exp $
 *
 */

#include "net.h"
#include "olsr.h"
#include <stdlib.h>

#ifdef WIN32
#define perror(x) WinSockPError(x)
void
WinSockPError(char *);
#endif

#warning HEAPS of changes in net.c!!!

static char out_buffer[MAXMESSAGESIZE+1];
static char fwd_buffer[MAXMESSAGESIZE+1];

static union olsr_packet *outmsg = (union olsr_packet *)out_buffer;
static union olsr_packet *fwdmsg = (union olsr_packet *)fwd_buffer;

static int outputsize = 0;      /* current size of the output buffer */
static int fwdsize = 0;         /* current size of the forward buffer */

static int netbuffer_reserved = 0;  /* plugins can reserve bufferspace */


/* Default max OLSR packet size */
static int maxmessagesize = MAXMESSAGESIZE - OLSR_HEADERSIZE - UDP_IP_HDRSIZE;


void
init_net()
{
  ptf_list = NULL;

  return;
}

int
net_set_maxmsgsize(olsr_u16_t new_size)
{

  if(new_size > (MAXMESSAGESIZE - OLSR_HEADERSIZE - UDP_IP_HDRSIZE - netbuffer_reserved))
    return -1;

  else
    maxmessagesize = new_size - netbuffer_reserved;

  olsr_printf(1, "Not outputbuffer maxsize set to %d(%d bytes reserved)\n", 
	      maxmessagesize, 
	      netbuffer_reserved);
  return maxmessagesize;
}


int
net_reserve_bufferspace(olsr_u16_t size)
{

  if((netbuffer_reserved + size) > maxmessagesize)
    return -1;

  else
    {
      netbuffer_reserved += size;
      maxmessagesize -= netbuffer_reserved;
    }

  olsr_printf(1, "Netbuffer reserved %d bytes - new maxsize %d\n", 
	      netbuffer_reserved, 
	      maxmessagesize);
  return maxmessagesize;
}


inline olsr_u16_t
net_get_maxmsgsize()
{
  return maxmessagesize;
}


inline olsr_u16_t
net_fwd_pending()
{
  return fwdsize;
}


inline olsr_u16_t
net_output_pending()
{
  return outputsize;
}

/**
 * Add data to the buffer that is to be transmitted
 *
 * @return 0 if there was not enough room in buffer
 */
int
net_outbuffer_push(olsr_u8_t *data, olsr_u16_t size)
{

  if((outputsize + size) > maxmessagesize)
    return 0;

  memcpy(&out_buffer[outputsize + OLSR_HEADERSIZE], data, size);
  outputsize += size;

  return 1;
}


/**
 * Add data to the buffer that is to be transmitted
 *
 * @return 0 if there was not enough room in buffer
 */
int
net_outbuffer_push_reserved(olsr_u8_t *data, olsr_u16_t size)
{

  if((outputsize + size) > (maxmessagesize + netbuffer_reserved))
    return 0;

  memcpy(&out_buffer[outputsize + OLSR_HEADERSIZE], data, size);
  outputsize += size;

  return 1;
}



/**
 * Add data to the buffer that is to be transmitted
 *
 * @return 0 if there was not enough room in buffer
 */
inline int
net_outbuffer_bytes_left()
{
  return (maxmessagesize - outputsize);
}


/**
 * Add data to the buffer that is to be forwarded
 *
 * @return 0 if there was not enough room in buffer
 */
int
net_fwdbuffer_push(olsr_u8_t *data, olsr_u16_t size)
{

  if((fwdsize + size) > maxmessagesize)
    return 0;

  memcpy(&fwd_buffer[fwdsize + OLSR_HEADERSIZE], data, size);
  fwdsize += size;

  return 1;
}


/**
 * Add data to the buffer that is to be forwarded
 *
 * @return 0 if there was not enough room in buffer
 */
int
net_fwdbuffer_push_reserved(olsr_u8_t *data, olsr_u16_t size)
{

  if((fwdsize + size) > (maxmessagesize + netbuffer_reserved))
    return 0;

  memcpy(&fwd_buffer[fwdsize + OLSR_HEADERSIZE], data, size);
  fwdsize += size;

  return 1;
}



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

  sin = NULL;
  sin6 = NULL;

  if(outputsize <= 0)
    return -1;

  outputsize += OLSR_HEADERSIZE;
  /* Add the Packet seqno */
  outmsg->v4.olsr_seqno = htons(ifp->olsr_seqnum++);
  /* Set the packetlength */
  outmsg->v4.olsr_packlen = htons(outputsize);

  if(ipversion == AF_INET)
    {
      /* IP version 4 */
      sin = (struct sockaddr_in *)&ifp->int_broadaddr;

      /* Copy sin */
      dst = *sin;
      sin = &dst;

      /* Set user defined broadcastaddr */
      if(bcast_set)
	memcpy(&dst.sin_addr.s_addr, &bcastaddr.sin_addr, sizeof(olsr_u32_t));

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
   *if the '-disp- option was given
   *we print her decimal contetnt of the packets
   */
  /*
  if(disp_pack_out)
    {
      switch(out_buffer[4])
	{
	case(HELLO_MESSAGE):printf("\n\tHELLO ");break;
	case(TC_MESSAGE):printf("\n\tTC ");break;
	case(MID_MESSAGE):printf("\n\tMID ");break;
	case(HNA_MESSAGE):printf("\n\tHNA ");break;
	}
      if(ipversion == AF_INET)
	printf("to %s size: %d\n\t", ip_to_string(&sin->sin_addr.s_addr), outputsize);
      else
	printf("to %s size: %d\n\t", ip6_to_string(&sin6->sin6_addr), outputsize);

      x = 0;

      for(i = 0; i < outputsize;i++)
	{
	  if(x == 4)
	    {
	      x = 0;
	      printf("\n\t");
	    }
	  x++;
	  if(ipversion == AF_INET)
	    printf(" %3i", (u_char) out_buffer[i]);
	  else
	    printf(" %2x", (u_char) out_buffer[i]);
	}
      
      printf("\n");
    }
  */

  /*
   *Call possible packet transform functions registered by plugins  
   */
  tmp_ptf_list = ptf_list;
  while(tmp_ptf_list != NULL)
    {
      tmp_ptf_list->function(out_buffer, &outputsize);
      tmp_ptf_list = tmp_ptf_list->next;
    }

  /*
   *if the '-disp- option was given
   *we print her decimal contetnt of the packets
   */
  if(disp_pack_out)
    {
      switch(out_buffer[4])
	{
	case(HELLO_MESSAGE):printf("\n\tHELLO ");break;
	case(TC_MESSAGE):printf("\n\tTC ");break;
	case(MID_MESSAGE):printf("\n\tMID ");break;
	case(HNA_MESSAGE):printf("\n\tHNA ");break;
	default:printf("\n\tTYPE: %d ", out_buffer[4]); break;
	}
      if(ipversion == AF_INET)
	printf("to %s size: %d\n\t", ip_to_string((olsr_u32_t *)&sin->sin_addr.s_addr), outputsize);
      else
	printf("to %s size: %d\n\t", ip6_to_string(&sin6->sin6_addr), outputsize);

      x = 0;

      for(i = 0; i < outputsize;i++)
	{
	  if(x == 4)
	    {
	      x = 0;
	      printf("\n\t");
	    }
	  x++;
	  if(ipversion == AF_INET)
	    printf(" %3i", (u_char) out_buffer[i]);
	  else
	    printf(" %2x", (u_char) out_buffer[i]);
	}
      
      printf("\n");
    }

  /*
   *packet points to the same memory area as the *msg pointer
   *used when building packets.
   */
  
  if(ipversion == AF_INET)
    {
      /* IP version 4 */
      if(sendto(ifp->olsr_socket, out_buffer, outputsize, MSG_DONTROUTE, (struct sockaddr *)sin, sizeof (*sin)) < 0)
	{
	  perror("sendto(v4)");
	  olsr_syslog(OLSR_LOG_ERR, "OLSR: sendto IPv4 %m");
	  outputsize = 0;
	  return -1;
	}
    }
  else
    {
      /* IP version 6 */
      if(sendto(ifp->olsr_socket, out_buffer, outputsize, MSG_DONTROUTE, (struct sockaddr *)sin6, sizeof (*sin6)) < 0)
	{
	  perror("sendto(v6)");
	  olsr_syslog(OLSR_LOG_ERR, "OLSR: sendto IPv6 %m");
	  fprintf(stderr, "Socket: %d interface: %d\n", ifp->olsr_socket, ifp->if_nr);
	  fprintf(stderr, "To: %s (size: %d)\n", ip6_to_string(&sin6->sin6_addr), sizeof(*sin6));
	  fprintf(stderr, "Outputsize: %d\n", outputsize);
	  outputsize = 0;
	  return -1;
	}
    }
  
  outputsize = 0;

  return 1;
}






/**
 *Forward a message on all interfaces
 *
 *@return negative on error
 */
int
net_forward()
{
  struct sockaddr_in *sin;  
  struct sockaddr_in dst;
  struct sockaddr_in6 *sin6;  
  struct sockaddr_in6 dst6;
  struct interface *ifn;
  struct ptf *tmp_ptf_list;
  int i, x;
  
  sin = NULL;
  sin6 = NULL;
  
  for (ifn = ifnet; ifn; ifn = ifn->int_next) 
    {
      
      fwdsize += OLSR_HEADERSIZE;      
      /* Add the Packet seqno */
      fwdmsg->v4.olsr_seqno = htons(ifn->olsr_seqnum++);
      /* Set the packetlength */
      fwdmsg->v4.olsr_packlen = htons(fwdsize);

      if(ipversion == AF_INET)
	{
	  /* IP version 4 */
	  sin = (struct sockaddr_in *)&ifn->int_broadaddr;
	  
	  /* Copy sin */
	  dst = *sin;
	  sin = &dst;

	  /* Set user defined broadcastaddr */
	  if(bcast_set)
	    memcpy(&dst.sin_addr.s_addr, &bcastaddr.sin_addr, sizeof(olsr_u32_t));
	  
	  if (sin->sin_port == 0)
	    sin->sin_port = olsr_udp_port;
	}
      else
	{
	  /* IP version 6 */
	  sin6 = (struct sockaddr_in6 *)&ifn->int6_multaddr;
	  /* Copy sin */
	  dst6 = *sin6;
	  sin6 = &dst6;
	}

      /*
       *if the '-disp- option was given
       *we print her decimal contetnt of the packets
       */
      if(disp_pack_out)
	{
	  if(ipversion == AF_INET)
	    printf("FORWARDING to %s size: %d\n\t", ip_to_string((olsr_u32_t *)&sin->sin_addr.s_addr), fwdsize);
	  else
	    printf("FORWARDING to %s size: %d\n\t", ip6_to_string(&sin6->sin6_addr), fwdsize);
	  
	  x = 0;
	  
	  for(i = 0; i < fwdsize;i++)
	    {
	      if(x == 4)
		{
		  x = 0;
		  printf("\n\t");
		}
	      x++;
	      if(ipversion == AF_INET)
		printf(" %3i", (u_char) fwd_buffer[i]);
	      else
		printf(" %2x", (u_char) fwd_buffer[i]);
	    }
	  
	  printf("\n");
	}
  
      
      /*
       *Call possible packet transform functions registered by plugins  
       */
      tmp_ptf_list = ptf_list;
      while(tmp_ptf_list != NULL)
	{
	  tmp_ptf_list->function(fwd_buffer, &fwdsize);
	  tmp_ptf_list = tmp_ptf_list->next;
	}


      if(ipversion == AF_INET)
	{
	  /* IP version 4 */
	  if(sendto(ifn->olsr_socket, fwd_buffer, fwdsize, MSG_DONTROUTE, (struct sockaddr *)sin, sizeof (*sin)) < 0)
	    {
	      perror("sendto(v4)");
	      olsr_syslog(OLSR_LOG_ERR, "OLSR: forward sendto IPv4 %m");
	      return -1;
	    }
	}
      else
	{
	  /* IP version 6 */
	  if(sendto(ifn->olsr_socket, fwd_buffer, fwdsize, MSG_DONTROUTE, (struct sockaddr *)sin6, sizeof (*sin6)) < 0)
	    {
	      perror("sendto(v6)");
	      olsr_syslog(OLSR_LOG_ERR, "OLSR: forward sendto IPv6 %m");
	      fprintf(stderr, "Socket: %d interface: %d\n", ifn->olsr_socket, ifn->if_nr);
	      fprintf(stderr, "To: %s (size: %d)\n", ip6_to_string(&sin6->sin6_addr), sizeof(*sin6));
	      fprintf(stderr, "Outputsize: %d\n", fwdsize);
	      return -1;
	    }
	}

    }      
  return 1;
}

/*
 * Add a packet transform function
 */
int
add_ptf(int (*f)(char *, int *))
{

  struct ptf *new_ptf;

  new_ptf = olsr_malloc(sizeof(struct ptf), "Add PTF");

  new_ptf->next = ptf_list;
  new_ptf->function = f;

  ptf_list = new_ptf;

  return 1;
}

/*
 * Remove a packet transform function
 */
int
del_ptf(int (*f)(char *, int *))
{
  struct ptf *tmp_ptf, *prev;

  tmp_ptf = ptf_list;
  prev = NULL;

  while(tmp_ptf)
    {
      if(tmp_ptf->function == f)
	{
	  /* Remove entry */
	  if(prev == NULL)
	    {
	      ptf_list = tmp_ptf->next;
	      free(tmp_ptf);
	    }
	  else
	    {
	      prev->next = tmp_ptf->next;
	      free(tmp_ptf);
	    }
	  return 1;
	}
      prev = tmp_ptf;
      tmp_ptf = tmp_ptf->next;
    }

  return 0;
}


int
join_mcast(struct interface *ifs, int sock)
{
  /* See linux/in6.h */

  struct ipv6_mreq mcastreq;

  COPY_IP(&mcastreq.ipv6mr_multiaddr, &ifs->int6_multaddr.sin6_addr);
  mcastreq.ipv6mr_interface = ifs->if_index;

  olsr_printf(3, "Interface %s joining multicast %s...",	ifs->int_name, olsr_ip_to_string((union olsr_ip_addr *)&ifs->int6_multaddr.sin6_addr));
  /* Send multicast */
  if(setsockopt(sock, 
		IPPROTO_IPV6, 
		IPV6_ADD_MEMBERSHIP, 
		(char *)&mcastreq, 
		sizeof(struct ipv6_mreq)) 
     < 0)
    {
      perror("Join multicast");
      return -1;
    }


  /* Join reciever group */
  if(setsockopt(sock, 
		IPPROTO_IPV6, 
		IPV6_JOIN_GROUP, 
		(char *)&mcastreq, 
		sizeof(struct ipv6_mreq)) 
     < 0)
    {
      perror("Join multicast send");
      return -1;
    }

  
  if(setsockopt(sock, 
		IPPROTO_IPV6, 
		IPV6_MULTICAST_IF, 
		(char *)&mcastreq.ipv6mr_interface, 
		sizeof(mcastreq.ipv6mr_interface)) 
     < 0)
    {
      perror("Set multicast if");
      return -1;
    }


  olsr_printf(3, "OK\n");
  return 0;
}


/**
 * Create a IPv6 netmask based on a prefix length
 *
 */
int
olsr_prefix_to_netmask(union olsr_ip_addr *adr, olsr_u16_t prefix)
{
  int p, i;

  if(adr == NULL)
    return 0;

  p = prefix;
  i = 0;

  memset(adr, 0, ipsize);

  for(;p > 0; p -= 8)
    {
      adr->v6.s6_addr[i] = (p < 8) ? 0xff ^ (0xff << p) : 0xff;
      i++;
    }

#ifdef DEBUG
  olsr_printf(3, "Prefix %d = Netmask: %s\n", prefix, olsr_ip_to_string(adr));
#endif

  return 1;
}



/**
 * Calculate prefix length based on a netmask
 *
 */
olsr_u16_t
olsr_netmask_to_prefix(union olsr_ip_addr *adr)
{
  olsr_u16_t prefix;
  int i, tmp;

  prefix = 0;

  memset(adr, 0, ipsize);

  for(i = 0; i < 16; i++)
    {
      if(adr->v6.s6_addr[i] == 0xff)
	{
	  prefix += 8;
	}
      else
	{
	  for(tmp = adr->v6.s6_addr[i];
	      tmp > 0;
	      tmp = tmp >> 1)
	    prefix++;
	}
    }

#ifdef DEBUG
  olsr_printf(3, "Netmask: %s = Prefix %d\n", olsr_ip_to_string(adr), prefix);
#endif

  return prefix;
}



/**
 *Converts a sockaddr struct to a string representing
 *the IP address from the sockaddr struct
 *
 *<b>NON REENTRANT!!!!</b>
 *
 *@param address_to_convert the sockaddr struct to "convert"
 *@return a char pointer to the string containing the IP
 */
char *
sockaddr_to_string(struct sockaddr *address_to_convert)
{
  struct sockaddr_in           *address;
  
  address=(struct sockaddr_in *)address_to_convert; 
  return(inet_ntoa(address->sin_addr));
  
}


/**
 *Converts the 32bit olsr_u32_t datatype to
 *a char array.
 *
 *<b>NON REENTRANT!!!!</b>
 *
 *@param address the olsr_u32_t to "convert"
 *@return a char pointer to the string containing the IP
 */

char *
ip_to_string(olsr_u32_t *address)
{

  struct in_addr in;
  in.s_addr=*address;
  return(inet_ntoa(in));
  
}




/**
 *Converts the 32bit olsr_u32_t datatype to
 *a char array.
 *
 *<b>NON REENTRANT!!!!</b>
 *
 *@param addr6 the address to "convert"
 *@return a char pointer to the string containing the IP
 */

char *
ip6_to_string(struct in6_addr *addr6)
{
  return (char *)inet_ntop(AF_INET6, addr6, ipv6_buf, sizeof(ipv6_buf));
}


char *
olsr_ip_to_string(union olsr_ip_addr *addr)
{

  char *ret;
  struct in_addr in;
  
  if(ipversion == AF_INET)
    {
      in.s_addr=addr->v4;
      ret = inet_ntoa(in);
    }
  else
    {
      /* IPv6 */
      ret = (char *)inet_ntop(AF_INET6, &addr->v6, ipv6_buf, sizeof(ipv6_buf));
    }

  return ret;
}

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
 * $Id: net.c,v 1.31 2005/02/12 23:07:02 spoggle Exp $
 */

#include "net.h"
#include "olsr.h"
#include <stdlib.h>

#ifdef WIN32
#define perror(x) WinSockPError(x)
void
WinSockPError(char *);
#endif

struct olsr_netbuf *netbufs[MAX_IFS];

static char ipv6_buf[100]; /* for address coversion */

/**
 * Create a outputbuffer for the given interface. This
 * function will allocate the needed storage according 
 * to the MTU of the interface.
 *
 * @param ifp the interface to create a buffer for
 *
 * @return 0 on success, negative if a buffer already existed
 *  for the given interface
 */ 
int
net_add_buffer(struct interface *ifp)
{
  struct olsr_netbuf *new_buf;

  /* If a buffer already exists for this interface back off */
  if(netbufs[ifp->if_nr])
    return -1;

  new_buf = olsr_malloc(sizeof(struct olsr_netbuf), "add_netbuff1");
  new_buf->buff = olsr_malloc(ifp->int_mtu, "add_netbuff2");

  /* Fill struct */
  new_buf->bufsize = ifp->int_mtu;
  new_buf->if_index = ifp->if_nr;
  new_buf->maxsize = ifp->int_mtu - OLSR_HEADERSIZE;
  new_buf->pending = 0;
  new_buf->reserved = 0;

  netbufs[ifp->if_nr] = new_buf;

  return 0;
}


/**
 * Remove a outputbuffer. Frees the allocated memory.
 *
 * @param ifp the interface corresponding to the buffer
 * to remove
 *
 * @return 0 on success, negative if no buffer is found
 */
int
net_remove_buffer(struct interface *ifp)
{

  /* If a buffer does no exist for this interface back off */
  if(!netbufs[ifp->if_nr])
    return -1;
  
  /* Flush pending data */
  if(netbufs[ifp->if_nr]->pending)
    net_output(ifp);

  free(netbufs[ifp->if_nr]->buff);
  free(netbufs[ifp->if_nr]);
  netbufs[ifp->if_nr] = NULL;

  return 0;
}



/**
 * Reserve space in a outputbuffer. This should only be needed
 * in very special cases. This will decrease the reported size
 * of the buffer so that there is always <i>size</i> bytes
 * of data available in the buffer. To add data in the reserved
 * area one must use the net_outbuffer_push_reserved function.
 *
 * @param ifp the interface corresponding to the buffer
 * to reserve space on
 * @param size the number of bytes to reserve
 *
 * @return 0 on success, negative if there was not enough
 *  bytes to reserve
 */
int
net_reserve_bufspace(struct interface *ifp, int size)
{
  if((!netbufs[ifp->if_nr]) || (size > netbufs[ifp->if_nr]->maxsize))
    return -1;
  
  netbufs[ifp->if_nr]->reserved = size;
  netbufs[ifp->if_nr]->maxsize -= size;
  
  return 0;
}

/**
 * Returns the number of bytes pending in the buffer. That
 * is the number of bytes added but not sent.
 *
 * @param ifp the interface corresponding to the buffer
 *
 * @return the number of bytes currently pending
 */
olsr_u16_t
net_output_pending(struct interface *ifp)
{
  if(!netbufs[ifp->if_nr])
    return -1;

  return netbufs[ifp->if_nr]->pending;
}



/**
 * Add data to a buffer.
 *
 * @param ifp the interface corresponding to the buffer
 * @param data a pointer to the data to add
 * @param size the number of byte to copy from data
 *
 * @return -1 if no buffer was found, 0 if there was not 
 *  enough room in buffer or the number of bytes added on 
 *  success
 */
int
net_outbuffer_push(struct interface *ifp, olsr_u8_t *data, olsr_u16_t size)
{

  if(!netbufs[ifp->if_nr])
    return -1;

  if((netbufs[ifp->if_nr]->pending + size) > netbufs[ifp->if_nr]->maxsize)
    return 0;

  memcpy(&netbufs[ifp->if_nr]->buff[netbufs[ifp->if_nr]->pending + OLSR_HEADERSIZE], data, size);
  netbufs[ifp->if_nr]->pending += size;

  return size;
}


/**
 * Add data to the reserved part of a buffer
 *
 * @param ifp the interface corresponding to the buffer
 * @param data a pointer to the data to add
 * @param size the number of byte to copy from data
 *
 * @return -1 if no buffer was found, 0 if there was not 
 *  enough room in buffer or the number of bytes added on 
 *  success
 */
int
net_outbuffer_push_reserved(struct interface *ifp, olsr_u8_t *data, olsr_u16_t size)
{

  if(!netbufs[ifp->if_nr])
    return -1;

  if((netbufs[ifp->if_nr]->pending + size) > (netbufs[ifp->if_nr]->maxsize + netbufs[ifp->if_nr]->reserved))
    return 0;

  memcpy(&netbufs[ifp->if_nr]->buff[netbufs[ifp->if_nr]->pending + OLSR_HEADERSIZE], data, size);
  netbufs[ifp->if_nr]->pending += size;

  return size;
}

/**
 * Report the number of bytes currently available in the buffer
 * (not including possible reserved bytes)
 *
 * @param ifp the interface corresponding to the buffer
 *
 * @return the number of bytes available in the buffer or
 *  -1 if no buffer was found
 */
int
net_outbuffer_bytes_left(struct interface *ifp)
{

  if(!netbufs[ifp->if_nr])
    return -1;

  return (netbufs[ifp->if_nr]->maxsize - netbufs[ifp->if_nr]->pending);
}


/**
 * Add a packet transform function. Theese are functions
 * called just prior to sending data in a buffer.
 *
 * @param f the function pointer
 *
 * @returns 1
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

/**
 * Remove a packet transform function
 *
 * @param f the function pointer
 *
 * @returns 1 if a functionpointer was removed
 *  0 if not
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

#if !defined __FreeBSD__ && !defined __MacOSX__ && !defined __NetBSD__
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
#else
#warning implement IPV6_ADD_MEMBERSHIP
#endif

  /* Old libc fix */
#ifdef IPV6_JOIN_GROUP
  /* Join reciever group */
  if(setsockopt(sock, 
		IPPROTO_IPV6, 
		IPV6_JOIN_GROUP, 
		(char *)&mcastreq, 
		sizeof(struct ipv6_mreq)) 
     < 0)
#else
  /* Join reciever group */
  if(setsockopt(sock, 
		IPPROTO_IPV6, 
		IPV6_ADD_MEMBERSHIP, 
		(char *)&mcastreq, 
		sizeof(struct ipv6_mreq)) 
     < 0)
#endif 
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
 * @param allocated address to build the netmask in
 * @param prefix the prefix length
 *
 * @returns 1 on success 0 on failure
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
 * @param adr the address to use to calculate the prefix length
 *
 * @return the prefix length
 */
olsr_u16_t
olsr_netmask_to_prefix(union olsr_ip_addr *adr)
{
  olsr_u16_t prefix;
  int i, tmp;

  prefix = 0;

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
 *<b>NON REENTRANT</b>
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
  static int index = 0;
  static char buff[4][100];
  char *ret;
  struct in_addr in;
  
  if(olsr_cnf->ip_version == AF_INET)
    {
      in.s_addr=addr->v4;
      ret = inet_ntoa(in);
    }
  else
    {
      /* IPv6 */
      ret = (char *)inet_ntop(AF_INET6, &addr->v6, ipv6_buf, sizeof(ipv6_buf));
    }

  strncpy(buff[index], ret, 100);

  ret = buff[index];

  index = (index + 1) & 3;

  return ret;
}

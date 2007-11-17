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
 * $Id: net_olsr.c,v 1.34 2007/11/17 00:05:54 bernd67 Exp $
 */

#include "net_olsr.h"
#include "log.h"
#include "olsr.h"
#include "net_os.h"
#include "print_packet.h"
#include "link_set.h"
#include "lq_packet.h"

#include <stdlib.h>
#include <assert.h>

static olsr_bool disp_pack_out = OLSR_FALSE;


#ifdef WIN32
#define perror(x) WinSockPError(x)
void
WinSockPError(char *);
#endif


struct deny_address_entry
{
  union olsr_ip_addr        addr;
  struct deny_address_entry *next;
};


/* Packet transform functions */

struct ptf
{
  packet_transform_function function;
  struct ptf *next;
};

static struct ptf *ptf_list;

static struct deny_address_entry *deny_entries;

static const char * const deny_ipv4_defaults[] =
  {
    "0.0.0.0",
    "127.0.0.1",
    NULL
  };

static const char * const deny_ipv6_defaults[] =
  {
    "0::0",
    "0::1",
    NULL
  };

void
net_set_disp_pack_out(olsr_bool val)
{
  disp_pack_out = val;
}

void
init_net(void)
{
  const char * const *defaults = olsr_cnf->ip_version == AF_INET ? deny_ipv4_defaults : deny_ipv6_defaults;
  
  for (; *defaults != NULL; defaults++) {
    union olsr_ip_addr addr;
    if(inet_pton(olsr_cnf->ip_version, *defaults, &addr) <= 0){
      fprintf(stderr, "Error converting fixed IP %s for deny rule!!\n", *defaults);
      continue;
    }
    olsr_add_invalid_address(&addr);
  }
}

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
  /* Can the interfaces MTU actually change? If not, we can elimiate
   * the "bufsize" field in "struct olsr_netbuf".
   */
  if (ifp->netbuf.bufsize != ifp->int_mtu && ifp->netbuf.buff != NULL) {
    free(ifp->netbuf.buff);
    ifp->netbuf.buff = NULL;
  }
  
  if (ifp->netbuf.buff == NULL) {
    ifp->netbuf.buff = olsr_malloc(ifp->int_mtu, "add_netbuff");
  }

  /* Fill struct */
  ifp->netbuf.bufsize = ifp->int_mtu;
  ifp->netbuf.maxsize = ifp->int_mtu - OLSR_HEADERSIZE;

  ifp->netbuf.pending = 0;
  ifp->netbuf.reserved = 0;

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
  /* Flush pending data */
  if(ifp->netbuf.pending)
    net_output(ifp);

  free(ifp->netbuf.buff);
  ifp->netbuf.buff = NULL;

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
  if(size > ifp->netbuf.maxsize)
    return -1;
  
  ifp->netbuf.reserved = size;
  ifp->netbuf.maxsize -= size;
  
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
net_output_pending(const struct interface *ifp)
{
  return ifp->netbuf.pending;
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
net_outbuffer_push(struct interface *ifp, const void *data, const olsr_u16_t size)
{
  if((ifp->netbuf.pending + size) > ifp->netbuf.maxsize)
    return 0;

  memcpy(&ifp->netbuf.buff[ifp->netbuf.pending + OLSR_HEADERSIZE], data, size);
  ifp->netbuf.pending += size;

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
net_outbuffer_push_reserved(struct interface *ifp, const void *data, const olsr_u16_t size)
{
  if((ifp->netbuf.pending + size) > (ifp->netbuf.maxsize + ifp->netbuf.reserved))
    return 0;

  memcpy(&ifp->netbuf.buff[ifp->netbuf.pending + OLSR_HEADERSIZE], data, size);
  ifp->netbuf.pending += size;

  return size;
}

/**
 * Report the number of bytes currently available in the buffer
 * (not including possible reserved bytes)
 *
 * @param ifp the interface corresponding to the buffer
 *
 * @return the number of bytes available in the buffer or
 */
int
net_outbuffer_bytes_left(const struct interface *ifp)
{
  return ifp->netbuf.maxsize - ifp->netbuf.pending;
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
add_ptf(packet_transform_function f)
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
del_ptf(packet_transform_function f)
{
  struct ptf *prev = NULL;
  struct ptf *tmp_ptf = ptf_list;
  while(tmp_ptf)
    {
      if(tmp_ptf->function == f)
	{
	  /* Remove entry */
	  if(prev == NULL)
	      ptf_list = tmp_ptf->next;
	  else
	      prev->next = tmp_ptf->next;
          free(tmp_ptf);
	  return 1;
	}
      prev = tmp_ptf;
      tmp_ptf = tmp_ptf->next;
    }

  return 0;
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
  struct sockaddr_in *sin = NULL;
  struct sockaddr_in6 *sin6 = NULL;
  struct ptf *tmp_ptf_list;
  union olsr_packet *outmsg;
  int retval;

  if(!ifp->netbuf.pending)
    return 0;

  ifp->netbuf.pending += OLSR_HEADERSIZE;

  retval = ifp->netbuf.pending;

  outmsg = (union olsr_packet *)ifp->netbuf.buff;
  /* Add the Packet seqno */
  outmsg->v4.olsr_seqno = htons(ifp->olsr_seqnum++);
  /* Set the packetlength */
  outmsg->v4.olsr_packlen = htons(ifp->netbuf.pending);

  if(olsr_cnf->ip_version == AF_INET)
    {
      struct sockaddr_in dst;
      /* IP version 4 */
      sin = (struct sockaddr_in *)&ifp->int_broadaddr;

      /* Copy sin */
      dst = *sin;
      sin = &dst;

      if (sin->sin_port == 0)
	sin->sin_port = htons(OLSRPORT);
    }
  else
    {
      struct sockaddr_in6 dst6;
      /* IP version 6 */
      sin6 = (struct sockaddr_in6 *)&ifp->int6_multaddr;
      /* Copy sin */
      dst6 = *sin6;
      sin6 = &dst6;
    }

  /*
   *Call possible packet transform functions registered by plugins  
   */
  for (tmp_ptf_list = ptf_list; tmp_ptf_list != NULL; tmp_ptf_list = tmp_ptf_list->next)
    {
      tmp_ptf_list->function(ifp->netbuf.buff, &ifp->netbuf.pending);
    }

  /*
   *if the -dispout option was given
   *we print the content of the packets
   */
  if(disp_pack_out)
    print_olsr_serialized_packet(stdout, (union olsr_packet *)ifp->netbuf.buff, 
				 ifp->netbuf.pending, &ifp->ip_addr); 
  
  if(olsr_cnf->ip_version == AF_INET)
    {
      /* IP version 4 */
      if(olsr_sendto(ifp->olsr_socket, 
                     ifp->netbuf.buff, 
		     ifp->netbuf.pending, 
		     MSG_DONTROUTE, 
		     (struct sockaddr *)sin, 
		     sizeof (*sin))
	 < 0)
	{
	  perror("sendto(v4)");
	  olsr_syslog(OLSR_LOG_ERR, "OLSR: sendto IPv4 %m");
	  retval = -1;
	}
    }
  else
    {
      /* IP version 6 */
      if(olsr_sendto(ifp->olsr_socket, 
		     ifp->netbuf.buff,
		     ifp->netbuf.pending, 
		     MSG_DONTROUTE, 
		     (struct sockaddr *)sin6, 
		     sizeof (*sin6))
	 < 0)
	{
          struct ipaddr_str buf;
	  perror("sendto(v6)");
	  olsr_syslog(OLSR_LOG_ERR, "OLSR: sendto IPv6 %m");
	  fprintf(stderr, "Socket: %d interface: %d\n", ifp->olsr_socket, ifp->if_index);
	  fprintf(stderr, "To: %s (size: %u)\n", ip6_to_string(&buf, &sin6->sin6_addr), (unsigned int)sizeof(*sin6));
	  fprintf(stderr, "Outputsize: %d\n", ifp->netbuf.pending);
	  retval = -1;
	}
    }
  
  ifp->netbuf.pending = 0;

  // if we've just transmitted a TC message, let Dijkstra use the current
  // link qualities for the links to our neighbours

  olsr_update_dijkstra_link_qualities();
  lq_tc_pending = OLSR_FALSE;

  return retval;
}


/**
 * Create a IPv4 or IPv6 netmask based on a prefix length
 *
 * @param allocated address to build the netmask in
 * @param prefix the prefix length
 *
 * @returns 1 on success 0 on failure
 */
int
olsr_prefix_to_netmask(union olsr_ip_addr *adr, const olsr_u16_t prefix)
{
#if !defined(NODEBUG) && defined(DEBUG)
  struct ipaddr_str buf;
#endif
  int p;
  const olsr_u8_t * const a_end = adr->v6.s6_addr+olsr_cnf->ipsize;
  olsr_u8_t *a;

  if (adr == NULL) {
    return 0;
  }

  a = adr->v6.s6_addr;
  for (p = prefix; a < a_end && p > 8; p -= 8) {
    *a++ = 0xff;
  }
  *a++ = 0xff << (8 - p);
  while (a < a_end) {
    *a++ = 0;
  }

#ifdef DEBUG
  OLSR_PRINTF(3, "Prefix %d = Netmask: %s\n", prefix, olsr_ip_to_string(&buf, adr));
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
olsr_netmask_to_prefix(const union olsr_ip_addr *adr)
{
#ifndef NODEBUG
  struct ipaddr_str buf;
#endif
  olsr_u16_t prefix = 0;
  const olsr_u8_t * const a_end = adr->v6.s6_addr+olsr_cnf->ipsize;
  const olsr_u8_t *a;

  for (a = adr->v6.s6_addr; a < a_end && *a == 0xff; a++) {
    prefix += 8;
  }
  if (a < a_end) {
    /* handle the last byte */
    switch (*a) {
    case   0: prefix += 0; break;
    case 128: prefix += 1; break;
    case 192: prefix += 2; break;
    case 224: prefix += 3; break;
    case 240: prefix += 4; break;
    case 248: prefix += 5; break;
    case 252: prefix += 6; break;
    case 254: prefix += 7; break;
    case 255: prefix += 8; break; /* Shouldn't happen */
    default:
      OLSR_PRINTF(0, "%s: Got bogus netmask %s\n", __func__, olsr_ip_to_string(&buf, adr));    
      prefix = USHRT_MAX;
      break;
    }
  }
#ifdef DEBUG
  OLSR_PRINTF(3, "Netmask: %s = Prefix %d\n", olsr_ip_to_string(&buf, adr), prefix);
#endif
  return prefix;
}

/**
 *Converts a sockaddr struct to a string representing
 *the IP address from the sockaddr struct
 *
 *@param address_to_convert the sockaddr struct to "convert"
 *@return a char pointer to the string containing the IP
 */
const char *
sockaddr_to_string(struct ipaddr_str * const buf, const struct sockaddr * const addr)
{
    const struct sockaddr_in * const addr4 = (const struct sockaddr_in *)addr;
    return ip4_to_string(buf, addr4->sin_addr);
}

/**
 *Converts the 32bit olsr_u32_t datatype to
 *a char array.
 *
 *@param address the olsr_u32_t to "convert"
 *@return a char pointer to the string containing the IP
 */
const char *
ip4_to_string(struct ipaddr_str * const buf, const struct in_addr addr4)
{
    return inet_ntop(AF_INET, &addr4, buf->buf, sizeof(buf->buf));
}

/**
 *Converts the 32bit olsr_u32_t datatype to
 *a char array.
 *
 *@param addr6 the address to "convert"
 *@return a char pointer to the string containing the IP
 */
const char *
ip6_to_string(struct ipaddr_str * const buf, const struct in6_addr * const addr6)
{
  return inet_ntop(AF_INET6, addr6, buf->buf, sizeof(buf->buf));
}

const char *
olsr_ip_to_string(struct ipaddr_str * const buf, const union olsr_ip_addr *addr)
{
#if 0
    if (!addr) {
        return "null";
    }
#endif
    return inet_ntop(olsr_cnf->ip_version, addr, buf->buf, sizeof(buf->buf));
}

const char *
olsr_ip_prefix_to_string(const struct olsr_ip_prefix *prefix)
{
  /* We need for IPv6 an IP address + '/' + prefix and for IPv4 an IP address + '/' + a netmask */
  static char buf[MAX(INET6_ADDRSTRLEN + 1 + 3, INET_ADDRSTRLEN + 1 + INET_ADDRSTRLEN)];
  const char *rv;

  if (prefix == NULL) {
      return "null";
  }
  
  if(olsr_cnf->ip_version == AF_INET) {
    int len;
    union olsr_ip_addr netmask;
    rv = inet_ntop(AF_INET, &prefix->prefix.v4, buf, sizeof(buf));
    len = strlen(buf);
    buf[len++] = '/';
    olsr_prefix_to_netmask(&netmask, prefix->prefix_len);
    inet_ntop(AF_INET, &netmask.v4, buf+len, sizeof(buf)-len);
  } else {
    int len;
    /* IPv6 */
    rv = inet_ntop(AF_INET6, &prefix->prefix.v6, buf, sizeof(buf));
    len = strlen(buf);
    buf[len++] = '/';
    snprintf(buf+len, sizeof(buf)-len, "/%d", prefix->prefix_len);
  }
  return rv;
}


void
olsr_add_invalid_address(const union olsr_ip_addr *adr)
{
#ifndef NODEBUG
  struct ipaddr_str buf;
#endif
  struct deny_address_entry *new_entry = olsr_malloc(sizeof(struct deny_address_entry), "Add deny address");

  new_entry->addr = *adr;
  new_entry->next = deny_entries;
  deny_entries = new_entry;
  OLSR_PRINTF(1, "Added %s to IP deny set\n", olsr_ip_to_string(&buf, &new_entry->addr));
}


olsr_bool
olsr_validate_address(const union olsr_ip_addr *adr)
{
  const struct deny_address_entry *deny_entry;

  for (deny_entry = deny_entries; deny_entry != NULL; deny_entry = deny_entry->next) {
    if(ipequal(adr, &deny_entry->addr))	{
#ifndef NODEBUG
      struct ipaddr_str buf;
#endif
      OLSR_PRINTF(1, "Validation of address %s failed!\n", olsr_ip_to_string(&buf, adr));
      return OLSR_FALSE;
    }
  }
  return OLSR_TRUE;
}


int ipcmp(const union olsr_ip_addr * const a, const union olsr_ip_addr * const b)
{
    return  olsr_cnf->ip_version == AF_INET
        ? ip4cmp(&a->v4, &b->v4)
        : ip6cmp(&a->v6, &b->v6);
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * End:
 */

/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2004 Andreas Tønnesen (andreto@ifi.uio.no)
 *
 * This file is part of olsr.org.
 *
 * UniK olsrd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * UniK olsrd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with olsr.org; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "parser.h"
#include "defs.h"
#include "process_package.h"
#include "mantissa.h"
#include "hysteresis.h"
#include "duplicate_set.h"
#include "mid_set.h"
#include "olsr.h"
#include "rebuild_packet.h"

//union olsr_ip_addr tmp_addr;
#ifdef WIN32
#undef EWOULDBLOCK
#define EWOULDBLOCK WSAEWOULDBLOCK
#undef errno
#define errno WSAGetLastError()
#undef strerror
#define strerror(x) StrError(x)
#endif


/**
 *Initialize the parser. 
 *
 *@return nada
 */
void
olsr_init_parser()
{
  olsr_printf(3, "Initializing parser...\n");

  parse_functions = NULL;

  /* Initialize the packet functions */
  olsr_init_package_process();

}

void
olsr_parser_add_function(void (*function)(union olsr_message *, struct interface *, union olsr_ip_addr *), int type, int forwarding)
{
  struct parse_function_entry *new_entry;

  olsr_printf(3, "Parser: registering event for type %d\n", type);
 

  new_entry = olsr_malloc(sizeof(struct parse_function_entry), "Register parse function");

  new_entry->function = function;
  new_entry->type = type;
  new_entry->caller_forwarding = forwarding;

  /* Queue */
  new_entry->next = parse_functions;
  parse_functions = new_entry;

  olsr_printf(3, "Register parse function: Added function for type %d\n", type);

}



int
olsr_parser_remove_function(void (*function)(union olsr_message *, struct interface *, union olsr_ip_addr *), int type, int forwarding)
{
  struct parse_function_entry *entry, *prev;

  entry = parse_functions;
  prev = NULL;

  while(entry)
    {
      if((entry->function == function) &&
	 (entry->type == type) &&
	 (entry->caller_forwarding == forwarding))
	{
	  if(entry == parse_functions)
	    {
	      parse_functions = entry->next;
	    }
	  else
	    {
	      prev->next = entry->next;
	    }
	  free(entry);
	  return 1;
	}

      prev = entry;
      entry = entry->next;
    }

  return 0;
}



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
  union
  {
    char	buf[MAXMESSAGESIZE+1];
    struct	olsr olsr;
  } inbuf;


  for (;;) 
    {
      fromlen = sizeof(struct sockaddr_storage);

      cc = recvfrom(fd, 
		    (char *)&inbuf, 
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

      if(ipversion == AF_INET)
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
      if ((ipversion == AF_INET) && (fromlen != sizeof (struct sockaddr_in)))
	break;
      else if ((ipversion == AF_INET6) && (fromlen != sizeof (struct sockaddr_in6)))
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
      parse_packet(&inbuf.olsr, cc, olsr_in_if, &from_addr);
    
    }
}




/**
 *Process a newly received OLSR packet. Checks the type
 *and to the neccessary convertions and call the
 *corresponding functions to handle the information.
 *@param from the sockaddr struct describing the sender
 *@param olsr the olsr struct containing the message
 *@param size the size of the message
 *@return nada
 */

void
parse_packet(struct olsr *olsr, int size, struct interface *in_if, union olsr_ip_addr *from_addr)
{
  union olsr_message *m = (union olsr_message *)olsr->olsr_msg;
  struct unknown_message unkpacket;
  int count;
  int msgsize;
  int processed;
  struct parse_function_entry *entry;
  char *packet = (char*)olsr;
  int i;
  int x = 0;

  count = size - ((char *)m - (char *)olsr);

  if (count < minsize)
    return;


  if (ntohs(olsr->olsr_packlen) != size)
    {
      olsr_printf(1, "Size error detected in received packet.\nRecieved %d, in packet %d\n", size, ntohs(olsr->olsr_packlen));
	    
      olsr_syslog(OLSR_LOG_ERR, " packet length error in  packet received from %s!",
	     olsr_ip_to_string(from_addr));
      return;
    }

  //printf("Message from %s\n\n", olsr_ip_to_string(from_addr)); 
      
  /* Display packet */
  if(disp_pack_in)
    {
      printf("\n\tfrom: %s\n\tsize: %d\n\tcontent(decimal):\n\t", olsr_ip_to_string(from_addr), size);
	
      for(i = 0; i < size;i++)
	{
	  if(x == 4)
	    {
	      x = 0;
	      printf("\n\t");
	    }
	  x++;
	  if(ipversion == AF_INET)
	    printf(" %03i", (u_char) packet[i]);
	  else
	    printf(" %02x", (u_char) packet[i]);
	}
	    
      printf("\n");
    }

  if(ipversion == AF_INET)
    msgsize = ntohs(m->v4.olsr_msgsize);
  else
    msgsize = ntohs(m->v6.olsr_msgsize);


  /*
   * Hysteresis update - for every OLSR package
   */
  if(use_hysteresis)
    {
      if(ipversion == AF_INET)
	{
	  /* IPv4 */
	  update_hysteresis_incoming(from_addr, 
				     &in_if->ip_addr,
				     ntohs(olsr->olsr_seqno));
	}
      else
	{
	  /* IPv6 */
	  update_hysteresis_incoming(from_addr, 
				     &in_if->ip_addr, 
				     ntohs(olsr->olsr_seqno));
	}
    }

  
  for ( ; count > 0; m = (union olsr_message *)((char *)m + (msgsize)))
    {

      processed = 0;      
      if (count < minsize)
	break;
      
      if(ipversion == AF_INET)
	msgsize = ntohs(m->v4.olsr_msgsize);
      else
	msgsize = ntohs(m->v6.olsr_msgsize);
      
      count -= msgsize;

      /* Check size of message */
      if(count < 0)
	{
	  olsr_printf(1, "packet length error in  packet received from %s!",
		      olsr_ip_to_string(from_addr));

	  olsr_syslog(OLSR_LOG_ERR, " packet length error in  packet received from %s!",
		 olsr_ip_to_string(from_addr));
	  break;
	}


      /* Treat TTL hopcnt */
      if(ipversion == AF_INET)
	{
	  /* IPv4 */
	  if (m->v4.ttl <= 0)
	    {
	      olsr_printf(1, "Dropping packet type %d from neigh %s with TTL 0\n", 
			  m->v4.olsr_msgtype,
			  olsr_ip_to_string(from_addr)); 
	      continue;
	    }
	}
      else
	{
	  /* IPv6 */
	  if (m->v6.ttl <= 0) 
	    {
	      olsr_printf(1, "Dropping packet type %d from %s with TTL 0\n", 
			  m->v4.olsr_msgtype,
			  olsr_ip_to_string(from_addr)); 
	      continue;
	    }
	}

      /*RFC 3626 section 3.4:
       *  2    If the time to live of the message is less than or equal to
       *  '0' (zero), or if the message was sent by the receiving node
       *  (i.e., the Originator Address of the message is the main
       *  address of the receiving node): the message MUST silently be
       *  dropped.
       */

      /* Should be the same for IPv4 and IPv6 */
      if(COMP_IP(&m->v4.originator, &main_addr))
	{
#ifdef DEBUG
	  olsr_printf(3, "Not processing message originating from us!\n");
#endif
	  continue;
	}


      //printf("MESSAGETYPE: %d\n", m->v4.olsr_msgtype);

      entry = parse_functions;

      while(entry)
	{
	  /* Should be the same for IPv4 and IPv6 */

	  /* Promiscuous or exact match */
	  if((entry->type == PROMISCUOUS) || 
	     (entry->type == m->v4.olsr_msgtype))
	    {
	      entry->function(m, in_if, from_addr);
	      if(entry->caller_forwarding)
		processed = 1;
	    }
	  entry = entry->next;
	}


      /* UNKNOWN PACKETTYPE */
      if(processed == 0)
	{
	  unk_chgestruct(&unkpacket, m);
	  
	  olsr_printf(1, "Unknown type: %d, size %d, from %s\n",
		      m->v4.olsr_msgtype,
		      size,
		      olsr_ip_to_string(&unkpacket.originator));

	  /* Forward message */
	  if(!COMP_IP(&unkpacket.originator, &main_addr))
	    {	      
	      /* Forward */
	      olsr_forward_message(m, 
				   &unkpacket.originator, 
				   unkpacket.seqno, 
				   in_if,
				   from_addr);
	    }

	}

    } /* for olsr_msg */ 


}





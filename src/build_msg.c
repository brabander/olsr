/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2004 Andreas Tønnesen (andreto@ifi.uio.no)
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
 * $Id: build_msg.c,v 1.19 2004/11/03 10:00:10 kattemat Exp $
 *
 */


#include "defs.h"
#include "build_msg.h"
#include "local_hna_set.h"
#include "olsr.h"


/* All these functions share this buffer */

static char msg_buffer[MAXMESSAGESIZE - OLSR_HEADERSIZE];

/* Begin:
 * Prototypes for internal functions 
 */

/* IPv4 */

static void
hello_build4(struct hello_message *, struct interface *);

static void
tc_build4(struct tc_message *, struct interface *);

static void
mid_build4(struct interface *);

static void
hna_build4(struct interface *);

/* IPv6 */

static void
hello_build6(struct hello_message *, struct interface *);

static void
tc_build6(struct tc_message *, struct interface *);

static void
mid_build6(struct interface *);

static void
hna_build6(struct interface *);

/* End:
 * Prototypes for internal functions 
 */



/*
 * Generic calls
 * These are the functions to call from outside
 */


/**
 * Generate HELLO packet with the contents of the parameter "message".
 * If this won't fit in one packet, chop it up into several.
 * Send the packet if the size of the data contained in the output buffer
 * reach maxmessagesize. Can generate an empty HELLO packet if the 
 * neighbor table is empty. The parameter messag is not used.
 *
 *
 *@param message the hello_message struct containing the info
 *to build the hello message from.
 *@param ifp the interface to send the message on
 *
 *@return nada
 */

void
hello_build(struct hello_message *message, struct interface *ifp)
{
  switch(olsr_cnf->ip_version)
    {
    case(AF_INET):
      hello_build4(message, ifp);
      break;
    case(AF_INET6):
      hello_build6(message, ifp);
      break;
    default:
      return;
    }
  return;
}


/*
 * Generate TC packet with the contents of the parameter "message".
 * If this won't fit in one packet, chop it up into several.
 * Send the packet if the size of the data contained in the output buffer
 * reach maxmessagesize. Don't generate an empty TC packet.
 * The parameter messag is not used.
 *
 *If parameter ifp = NULL then this is a TC packet who is to be
 *forwarded. In that case each chump of the packet(if bigger then
 *maxmessagesize the packet has to be split) must be forwarded
 *on each interface(exept the one it came on - if it was a wired interface)
 *and seqnumbers must be taken from the recieved packet.
 *
 *@param message the tc_message struct containing the info
 *to send
 *@param ifp the interface to send the message on
 *
 *@return nada
 */

void
tc_build(struct tc_message *message, struct interface *ifp)           
{
  switch(olsr_cnf->ip_version)
    {
    case(AF_INET):
      tc_build4(message, ifp);
      break;
    case(AF_INET6):
      tc_build6(message, ifp);
      break;
    default:
      return;
    }
  return;
}


/**
 *Build a MID message to the outputbuffer
 *
 *<b>NO INTERNAL BUFFER</b>
 *@param ifn use this interfaces address as main address
 *@return 1 on success
 */

void
mid_build(struct interface *ifn)
{
  switch(olsr_cnf->ip_version)
    {
    case(AF_INET):
      mid_build4(ifn);
      break;
    case(AF_INET6):
      mid_build6(ifn);
      break;
    default:
      return;
    }
  return;
}


/**
 *Builds a HNA message in the outputbuffer
 *<b>NB! Not internal packetformat!</b>
 *
 *@param ifp the interface to send on
 *@return nada
 */
void
hna_build(struct interface *ifp)
{
  switch(olsr_cnf->ip_version)
    {
    case(AF_INET):
      hna_build4(ifp);
      break;
    case(AF_INET6):
      hna_build6(ifp);
      break;
    default:
      return;
    }
  return;
}

/*
 * Protocol specific versions
 */




/**
 * IP version 4
 *
 *@param message the hello_message struct containing the info
 *to build the hello message from.
 *@param ifp the interface to send the message on
 *
 *@return nada
 */

static void
hello_build4(struct hello_message *message, struct interface *ifp)
{
  int remainsize, curr_size;
  struct hello_neighbor *nb, *prev_nb;
  union olsr_message *m;
  struct hellomsg *h;
  struct hellinfo *hinfo;
  union olsr_ip_addr *haddr;
  int i, j, sametype;
  int lastpacket = 0; /* number of neighbors with the same
			 greater link status in the last packet */
  if((!message) || (!ifp) || (olsr_cnf->ip_version != AF_INET))
    return;

  remainsize = net_outbuffer_bytes_left(ifp);

  //printf("HELLO build outputsize: %d\n", outputsize);

  m = (union olsr_message *)msg_buffer;

  curr_size = 12; /* OLSR message header */
  curr_size += 4; /* Hello header */

  /* Send pending packet if not room in buffer */
  if(curr_size > remainsize)
    {
      net_output(ifp);
      remainsize = net_outbuffer_bytes_left(ifp);
    }

  h = &m->v4.message.hello;
  hinfo = h->hell_info;
  haddr = (union olsr_ip_addr *)hinfo->neigh_addr;
  
  //printf("Neighbor addr: %s\n", olsr_ip_to_string(haddr));fflush(stdout);

  /* Fill message header */
  m->v4.ttl = message->ttl;
  m->v4.hopcnt = 0;
  m->v4.olsr_msgtype = HELLO_MESSAGE;
  /* Set source(main) addr */
  COPY_IP(&m->v4.originator, &main_addr);

  m->v4.olsr_vtime = ifp->valtimes.hello;

  /* Fill HELLO header */
  h->willingness = message->willingness; 
  h->htime = ifp->hello_etime;

  memset(&h->reserved, 0, sizeof(olsr_u16_t));
  

  /*
   *Loops trough all possible neighbor statuses
   *The negbor list is grouped by status
   *
   */
  /* Nighbor statuses */
  for (i = 0; i <= MAX_NEIGH; i++) 
    {
      /* Link ststuses */
      for(j = 0; j <= MAX_LINK; j++)
	{

	  /*
	   *HYSTERESIS
	   *Not adding neighbors with link type HIDE
	   */
	  
	  if(j == HIDE_LINK)
	      continue;

	  lastpacket = sametype = 0;

	  //printf("Neighbortype %d outputsize %d\n", i, outputsize);

	  /*
	   *Looping trough neighbors
	   */
	  for (nb = message->neighbors; nb != NULL; nb = nb->next) 
	    {	  
	      if ((nb->status == i) && (nb->link == j))
		{
		  sametype++;
		  if (sametype == 1)
		    {

		      /*
		       * If there is not enough room left 
		       * for the data in tho outputbuffer
		       * we must send a partial HELLO and
		       * continue building the rest of the
		       * data in a new HELLO message
		       * Add ipsize in check since there is
		       * no use sending just the type header
		       */
		      if((curr_size + 4 + ipsize) > remainsize)
			{
			  /* Only send partial HELLO if it contains data */
			  if(curr_size > (12 + 4))
			    {
			      /* Complete the headers */
			      m->v4.seqno = htons(get_msg_seqno());
			      m->v4.olsr_msgsize = htons(curr_size);
			      
			      hinfo->size = (char *)haddr - (char *)hinfo;
			      hinfo->size = ntohs(hinfo->size);
			      
			      /* Send partial packet */
			      net_outbuffer_push(ifp, msg_buffer, curr_size);

			      curr_size = 12; /* OLSR message header */
			      curr_size += 4; /* Hello header */
			      
			      h = &m->v4.message.hello;
			      hinfo = h->hell_info;
			      haddr = (union olsr_ip_addr *)hinfo->neigh_addr;
			    }

			  net_output(ifp);			  
			  /* Reset size and pointers */
			  remainsize = net_outbuffer_bytes_left(ifp);
			}
		      memset(&hinfo->reserved, 0, sizeof(olsr_u8_t));
		      /* Set link and status for this group of neighbors (this is the first) */
		      hinfo->link_code = CREATE_LINK_CODE(i, j);//j | (i<<2);
		      //printf("(2)Setting neighbor link status: %x\n", hinfo->link_code);
		      curr_size += 4; /* HELLO type section header */
		    }

#ifdef DEBUG
		  olsr_printf(5, "\tLink status of %s: ", olsr_ip_to_string(&nb->address));
		  olsr_printf(5, "%d\n", nb->link);
#endif
		  
		  /*
		   * If there is not enough room left 
		   * for the data in tho outputbuffer
		   * we must send a partial HELLO and
		   * continue building the rest of the
		   * data in a new HELLO message
		   */
		  if((curr_size + ipsize) > remainsize)
		    {
		      /* If we get here the message contains data
		       * - no need to check 
		       */
		      /* Complete the headers */
		      m->v4.seqno = htons(get_msg_seqno());
		      m->v4.olsr_msgsize = htons(curr_size);
		      
		      hinfo->size = (char *)haddr - (char *)hinfo;
		      hinfo->size = ntohs(hinfo->size);
		      
		      /* Send partial packet */
		      net_outbuffer_push(ifp, msg_buffer, curr_size);
		      net_output(ifp);
		      
		      /* Reset size and pointers */
		      remainsize = net_outbuffer_bytes_left(ifp);
		      curr_size = 12; /* OLSR message header */
		      curr_size += 4; /* Hello header */
		      
		      h = &m->v4.message.hello;
		      hinfo = h->hell_info;
		      haddr = (union olsr_ip_addr *)hinfo->neigh_addr;
		      
		      /* Rebuild TYPE header */
		      memset(&hinfo->reserved, 0, sizeof(olsr_u8_t));
		      /* Set link and status for this group of neighbors (this is the first) */
		      hinfo->link_code = CREATE_LINK_CODE(i, j);//j | (i<<2);
		      //printf("(2)Setting neighbor link status: %x\n", hinfo->link_code);
		      curr_size += 4; /* HELLO type section header */
		      
		    }

		  COPY_IP(haddr, &nb->address);

		  //printf("\n\n1: %d\n", (char *)haddr - packet);
		  /*
		   *Point to next address
		   */
		  haddr = (union olsr_ip_addr *)&haddr->v6.s6_addr[4];
		  curr_size += ipsize; /* IP address added */

		  //printf("\n2: %d\n\n", (char *)haddr - packet); 
		  //printf("Ipsize: %d\n", ipsize);

		  //printf("Adding neighbor %s\n", olsr_ip_to_string(&nb->address));
   
		}
    
	    }/* looping trough neighbors */
	    

	  if (sametype && sametype > lastpacket)
	    {
	      hinfo->size = (char *)haddr - (char *)hinfo;
	      hinfo->size = ntohs(hinfo->size);
	      hinfo = (struct hellinfo *)((char *)haddr);
	      haddr = (union olsr_ip_addr *)&hinfo->neigh_addr;
	      
	    }
	} /* for j */
    } /* for i*/
     
  m->v4.seqno = htons(get_msg_seqno());
  m->v4.olsr_msgsize = htons(curr_size);
  
  net_outbuffer_push(ifp, msg_buffer, curr_size);

  /*
   * Delete the list of neighbor messages.
   */
     
  nb = message->neighbors;
     
  while (nb)
    {
      prev_nb = nb;
      nb = nb->next;
      free(prev_nb);
    }

}




/**
 * IP version 6
 *
 *@param message the hello_message struct containing the info
 *to build the hello message from.
 *@param ifp the interface to send the message on
 *
 *@return nada
 */


static void
hello_build6(struct hello_message *message, struct interface *ifp)
{
  int remainsize, curr_size;
  struct hello_neighbor *nb, *prev_nb;
  union olsr_message *m;
  struct hellomsg6 *h6;
  struct hellinfo6 *hinfo6;
  union olsr_ip_addr *haddr;

  int i, j, sametype;
  int lastpacket = 0; /* number of neighbors with the same
			 greater link status in the last packet */
  if((!message) || (!ifp) || (olsr_cnf->ip_version != AF_INET6))
    return;


  remainsize = net_outbuffer_bytes_left(ifp);

  //printf("HELLO build outputsize: %d\n", outputsize);

  m = (union olsr_message *)msg_buffer;

  curr_size = 24; /* OLSR message header */
  curr_size += 4; /* Hello header */

  /* Send pending packet if not room in buffer */
  if(curr_size > remainsize)
    {
      net_output(ifp);
      remainsize = net_outbuffer_bytes_left(ifp);
    }

  //printf("HELLO build outputsize: %d\n", outputsize);
  h6 = &m->v6.message.hello;
  hinfo6 = h6->hell_info;
  haddr = (union olsr_ip_addr *)hinfo6->neigh_addr;


  /* Fill message header */
  m->v6.ttl = message->ttl;
  m->v6.hopcnt = 0;
  /* Set source(main) addr */
  COPY_IP(&m->v6.originator, &main_addr);
  m->v6.olsr_msgtype = HELLO_MESSAGE;

  m->v6.olsr_vtime = ifp->valtimes.hello;
  
  /* Fill packet header */
  h6->willingness = message->willingness; 

  h6->htime = ifp->hello_etime;

  memset(&h6->reserved, 0, sizeof(olsr_u16_t));
  
  

  /*
   *Loops trough all possible neighbor statuses
   *The negbor list is grouped by status
   */

  for (i = 0; i <= MAX_NEIGH; i++) 
    {
      for(j = 0; j <= MAX_LINK; j++)
	{
	  
	  
	  lastpacket = sametype = 0;
	  
	  
	  //printf("Neighbortype %d outputsize %d\n", i, outputsize);
	  	  
	  /*
	   *Looping trough neighbors
	   */
	  for (nb = message->neighbors; nb != NULL; nb = nb->next) 
	    {	      
	      if ((nb->status == i) && (nb->link == j))
		{	      
		  sametype++;
		  if (sametype == 1)
		    {
		      /* Check if there is room for header + one address */
		      if((curr_size + 4 + ipsize) > remainsize)
			{
			  /* Only send partial HELLO if it contains data */
			  if(curr_size > (24 + 4))
			    {
			      /* Complete the headers */
			      m->v6.seqno = htons(get_msg_seqno());
			      m->v6.olsr_msgsize = htons(curr_size);
			      
			      hinfo6->size = (char *)haddr - (char *)hinfo6;
			      hinfo6->size = ntohs(hinfo6->size);
			      
			      /* Send partial packet */
			      net_outbuffer_push(ifp, msg_buffer, curr_size);
			      curr_size = 24; /* OLSR message header */
			      curr_size += 4; /* Hello header */
			      
			      h6 = &m->v6.message.hello;
			      hinfo6 = h6->hell_info;
			      haddr = (union olsr_ip_addr *)hinfo6->neigh_addr;
			    }
			  net_output(ifp);
			  /* Reset size and pointers */
			  remainsize = net_outbuffer_bytes_left(ifp);
			}
		      memset(&hinfo6->reserved, 0, sizeof(olsr_u8_t));
		      /* Set link and status for this group of neighbors (this is the first) */
		      hinfo6->link_code = CREATE_LINK_CODE(i, j);//j | (i<<2);
		      //printf("(2)Setting neighbor link status: %x\n", hinfo->link_code);
		      curr_size += 4; /* HELLO type section header */
		    }

#ifdef DEBUG
		  olsr_printf(5, "\tLink status of %s: ", olsr_ip_to_string(&nb->address));
		  olsr_printf(5, "%d\n", nb->link);
#endif

		  /*
		   * If there is not enough room left 
		   * for the data in the outputbuffer
		   * we must send a partial HELLO and
		   * continue building the rest of the
		   * data in a new HELLO message
		   */
		  if((curr_size + ipsize) > remainsize)
		    {
		      /* If we get here the message contains data
		       * - no need to check 
		       */
		      /* Complete the headers */
		      m->v6.seqno = htons(get_msg_seqno());
		      m->v6.olsr_msgsize = htons(curr_size);
		      
		      hinfo6->size = (char *)haddr - (char *)hinfo6;
		      hinfo6->size = ntohs(hinfo6->size);
		      
		      /* Send partial packet */
			  net_outbuffer_push(ifp, msg_buffer, curr_size);
		      curr_size = 24; /* OLSR message header */
		      curr_size += 4; /* Hello header */
		      
		      h6 = &m->v6.message.hello;
		      hinfo6 = h6->hell_info;
		      haddr = (union olsr_ip_addr *)hinfo6->neigh_addr;
		      
		      /* Rebuild TYPE header */
		      memset(&hinfo6->reserved, 0, sizeof(olsr_u8_t));
		      /* Set link and status for this group of neighbors (this is the first) */
		      hinfo6->link_code = CREATE_LINK_CODE(i, j);//j | (i<<2);
		      //printf("(2)Setting neighbor link status: %x\n", hinfo->link_code);
		      curr_size += 4; /* HELLO type section header */

		      net_output(ifp);		      
		      /* Reset size */
		      remainsize = net_outbuffer_bytes_left(ifp);
		      
		    }

		  COPY_IP(haddr, &nb->address);
		  
		  //printf("\n\n1: %d\n", (char *)haddr - packet);
		  /*
		   *Point to next address
		   */
		  haddr++;
		  curr_size += ipsize; /* IP address added */ 
		  //printf("\n2: %d\n\n", (char *)haddr - packet); 
		  //printf("Ipsize: %d\n", ipsize);
		  
		  //printf("Adding neighbor %s\n", olsr_ip_to_string(&nb->address));
		  
		}
	      
	    }/* looping trough neighbors */
	    
	  
	  if (sametype && sametype > lastpacket)
	    {
	      hinfo6->size = (char *)haddr - (char *)hinfo6;
	      hinfo6->size = ntohs(hinfo6->size);
	      hinfo6 = (struct hellinfo6 *)((char *)haddr);
	      haddr = (union olsr_ip_addr *)&hinfo6->neigh_addr;
	    }
	  
	} /* for j */
    } /* for i */

  m->v6.seqno = htons(get_msg_seqno());
  m->v6.olsr_msgsize = htons(curr_size);

  net_outbuffer_push(ifp, msg_buffer, curr_size);

  /*
   * Delete the list of neighbor messages.
   */
     
  nb = message->neighbors;
     
  while (nb)
    {
      prev_nb = nb;
      nb = nb->next;
      free(prev_nb);
    }

}





/**
 *IP version 4
 *
 *@param message the tc_message struct containing the info
 *to send
 *@param ifp the interface to send the message on
 *
 *@return nada
 */

static void
tc_build4(struct tc_message *message, struct interface *ifp)           
{

  int remainsize, curr_size;
  struct tc_mpr_addr *mprs, *prev_mprs;
  union olsr_message *m;
  struct tcmsg *tc;
  struct neigh_info *mprsaddr; 
  olsr_bool found = FALSE, partial_sent = FALSE;

  if((!message) || (!ifp) || (olsr_cnf->ip_version != AF_INET))
    return;

  remainsize = net_outbuffer_bytes_left(ifp);

  m = (union olsr_message *)msg_buffer;

  tc = &m->v4.message.tc;


  mprsaddr = tc->neigh;
  curr_size = 12; /* OLSR message header */
  curr_size += 4; /* TC header */

  /* Send pending packet if not room in buffer */
  if(curr_size > remainsize)
    {
      net_output(ifp);
      remainsize = net_outbuffer_bytes_left(ifp);
    }

  /* Fill header */
  m->v4.olsr_vtime = ifp->valtimes.tc;
  m->v4.olsr_msgtype = TC_MESSAGE;
  m->v4.hopcnt = message->hop_count;
  m->v4.ttl = message->ttl;
  COPY_IP(&m->v4.originator, &message->originator);

  /* Fill TC header */
  tc->ansn = htons(message->ansn);
  tc->reserved = 0;
  

  /*Looping trough MPR selectors */
  for (mprs = message->multipoint_relay_selector_address; mprs != NULL;mprs = mprs->next) 
    {
      /*If packet is to be chomped */
      if((curr_size + ipsize) > remainsize)
	{

	  /* Only add TC message if it contains data */
	  if(curr_size > (12 + 4 ))
	    {
	      m->v4.olsr_msgsize = htons(curr_size);
	      m->v4.seqno = htons(get_msg_seqno());

	      net_outbuffer_push(ifp, msg_buffer, curr_size);
	      
	      /* Reset stuff */
	      mprsaddr = tc->neigh;
	      curr_size = 12; /* OLSR message header */
	      curr_size += 4; /* TC header */
	      found = FALSE;
	      partial_sent = TRUE;
	    }

	  net_output(ifp);
	  remainsize = net_outbuffer_bytes_left(ifp);

	}
      found = TRUE;
      
      COPY_IP(&mprsaddr->addr, &mprs->address);

      curr_size += ipsize;
      mprsaddr++;
    }

  if (found)
    {
	    
      m->v4.olsr_msgsize = htons(curr_size);
      m->v4.seqno = htons(get_msg_seqno());
      
      net_outbuffer_push(ifp, msg_buffer, curr_size);

    }
  else
    {
      if((!partial_sent) && (!TIMED_OUT(&send_empty_tc)))
	{
	  olsr_printf(1, "TC: Sending empty package\n");

	  m->v4.olsr_msgsize = htons(curr_size);
	  m->v4.seqno = htons(get_msg_seqno());

	  net_outbuffer_push(ifp, msg_buffer, curr_size);

	}
    }


  /*
   * Delete the list of mprs messages
   */
	
  mprs = message->multipoint_relay_selector_address;
	
  while (mprs)
    {
      prev_mprs = mprs;
      mprs = mprs->next;
      free(prev_mprs);
    }
	
	
}




/**
 *IP version 6
 *
 *@param message the tc_message struct containing the info
 *to send
 *@param ifp the interface to send the message on
 *
 *@return nada
 */

static void
tc_build6(struct tc_message *message, struct interface *ifp)           
{

  int remainsize, curr_size;
  struct tc_mpr_addr *mprs, *prev_mprs;
  union olsr_message *m;
  struct tcmsg6 *tc6;
  struct neigh_info6 *mprsaddr6; 
  olsr_bool found = FALSE, partial_sent = FALSE;

  if ((!message) || (!ifp) || (olsr_cnf->ip_version != AF_INET6))
    return;

  remainsize = net_outbuffer_bytes_left(ifp);

  m = (union olsr_message *)msg_buffer;

  tc6 = &m->v6.message.tc;

  mprsaddr6 = tc6->neigh;
  curr_size = 24; /* OLSR message header */
  curr_size += 4; /* TC header */

  /* Send pending packet if not room in buffer */
  if(curr_size > remainsize)
    {
      net_output(ifp);
      remainsize = net_outbuffer_bytes_left(ifp);
    }

  /* Fill header */
  m->v6.olsr_vtime = ifp->valtimes.tc;
  m->v6.olsr_msgtype = TC_MESSAGE;
  m->v6.hopcnt = message->hop_count;
  m->v6.ttl = message->ttl;
  COPY_IP(&m->v6.originator, &message->originator);

  /* Fill TC header */
  tc6->ansn = htons(message->ansn);
  tc6->reserved = 0;
  

  /*Looping trough MPR selectors */
  for (mprs = message->multipoint_relay_selector_address; mprs != NULL;mprs = mprs->next) 
    {
	    
      /*If packet is to be chomped */
      if((curr_size + ipsize) > remainsize)
	{
	  /* Only add TC message if it contains data */
	  if(curr_size > (24 + 4 ))
	    {
	      m->v6.olsr_msgsize = htons(curr_size);
	      m->v6.seqno = htons(get_msg_seqno());

	      net_outbuffer_push(ifp, msg_buffer, curr_size);
	      mprsaddr6 = tc6->neigh;
	      curr_size = 24; /* OLSR message header */
	      curr_size += 4; /* TC header */
	      found = FALSE;
	      partial_sent = TRUE;
	    }
	  net_output(ifp);
	  remainsize = net_outbuffer_bytes_left(ifp);
		

	}
      found = TRUE;

      //printf("mprsaddr6 is %x\n", (char *)mprsaddr6 - packet);
      //printf("Adding MPR-selector: %s\n", olsr_ip_to_string(&mprs->address));fflush(stdout);	    
      COPY_IP(&mprsaddr6->addr, &mprs->address);
      curr_size += ipsize;

      mprsaddr6++;
    }
	
  if (found)
    {
      m->v6.olsr_msgsize = htons(curr_size);
      m->v6.seqno = htons(get_msg_seqno());

      net_outbuffer_push(ifp, msg_buffer, curr_size);

    }
  else
    {
      if((!partial_sent) && (!TIMED_OUT(&send_empty_tc)))
	{
	  olsr_printf(1, "TC: Sending empty package\n");
	    
	  m->v6.olsr_msgsize = htons(curr_size);
	  m->v6.seqno = htons(get_msg_seqno());

	  net_outbuffer_push(ifp, msg_buffer, curr_size);
	}
    }

  /*
   * Delete the list of mprs messages
   */
	
  mprs = message->multipoint_relay_selector_address;
	
  while (mprs)
    {
      prev_mprs = mprs;
      mprs = mprs->next;
      free(prev_mprs);
    }
	
	
}




/**
 *IP version 4
 *
 *<b>NO INTERNAL BUFFER</b>
 *@param ifp use this interfaces address as main address
 *@return 1 on success
 */

static void
mid_build4(struct interface *ifp)
{
  int remainsize, curr_size;
  /* preserve existing data in output buffer */
  union olsr_message *m;
  struct midaddr *addrs;
  struct interface *ifs;  

  if((olsr_cnf->ip_version != AF_INET) || (!ifp) || (ifnet == NULL || ifnet->int_next == NULL))
    return;


  remainsize = net_outbuffer_bytes_left(ifp);

  m = (union olsr_message *)msg_buffer;

  curr_size = 12; /* OLSR message header */

  /* Send pending packet if not room in buffer */
  if(curr_size > remainsize)
    {
      net_output(ifp);
      remainsize = net_outbuffer_bytes_left(ifp);
    }

  /* Fill header */
  m->v4.hopcnt = 0;
  m->v4.ttl = MAX_TTL;
  /* Set main(first) address */
  COPY_IP(&m->v4.originator, &main_addr);
  m->v4.olsr_msgtype = MID_MESSAGE;
  m->v4.olsr_vtime = ifp->valtimes.mid;
 
  addrs = m->v4.message.mid.mid_addr;

  /* Don't add the main address... it's already there */
  for(ifs = ifnet; ifs != NULL; ifs = ifs->int_next)
    {
      if(!COMP_IP(&main_addr, &ifs->ip_addr))
	{

	  if((curr_size + ipsize) > remainsize)
	    {
	      /* Only add MID message if it contains data */
	      if(curr_size > 12)
		{
		  /* set size */
		  m->v4.olsr_msgsize = htons(curr_size);
		  m->v4.seqno = htons(get_msg_seqno());/* seqnumber */
		  
		  net_outbuffer_push(ifp, msg_buffer, curr_size);
		  curr_size = 12; /* OLSR message header */
		  addrs = m->v4.message.mid.mid_addr;
		}
	      net_output(ifp);
	      remainsize = net_outbuffer_bytes_left(ifp);
	    }
	  
	  COPY_IP(&addrs->addr, &ifs->ip_addr);
	  addrs++;
	  curr_size += ipsize;
	}
    }


  m->v4.seqno = htons(get_msg_seqno());/* seqnumber */
  m->v4.olsr_msgsize = htons(curr_size);

  //printf("Sending MID (%d bytes)...\n", outputsize);
  net_outbuffer_push(ifp, msg_buffer, curr_size);


  return;
}



/**
 *IP version 6
 *
 *<b>NO INTERNAL BUFFER</b>
 *@param ifp use this interfaces address as main address
 *@return 1 on success
 */

static void
mid_build6(struct interface *ifp)
{
  int remainsize, curr_size;
  /* preserve existing data in output buffer */
  union olsr_message *m;
  struct midaddr6 *addrs6;
  struct interface *ifs;

  //printf("\t\tGenerating mid on %s\n", ifn->int_name);


  if((olsr_cnf->ip_version != AF_INET6) || (!ifp) || (ifnet == NULL || ifnet->int_next == NULL))
    return;

  remainsize = net_outbuffer_bytes_left(ifp);

  curr_size = 24; /* OLSR message header */

  /* Send pending packet if not room in buffer */
  if(curr_size > remainsize)
    {
      net_output(ifp);
      remainsize = net_outbuffer_bytes_left(ifp);
    }

  m = (union olsr_message *)msg_buffer;
    
  /* Build header */
  m->v6.hopcnt = 0;
  m->v6.ttl = MAX_TTL;      
  m->v6.olsr_msgtype = MID_MESSAGE;
  m->v6.olsr_vtime = ifp->valtimes.mid;
  /* Set main(first) address */
  COPY_IP(&m->v6.originator, &main_addr);
   

  addrs6 = m->v6.message.mid.mid_addr;

  /* Don't add the main address... it's already there */
  for(ifs = ifnet; ifs != NULL; ifs = ifs->int_next)
    {
      if(!COMP_IP(&main_addr, &ifs->ip_addr))
	{
	  if((curr_size + ipsize) > remainsize)
	    {
	      /* Only add MID message if it contains data */
	      if(curr_size > 24)
		{
		  /* set size */
		  m->v6.olsr_msgsize = htons(curr_size);
		  m->v6.seqno = htons(get_msg_seqno());/* seqnumber */
		  
		  net_outbuffer_push(ifp, msg_buffer, curr_size);
		  curr_size = 24; /* OLSR message header */
		  addrs6 = m->v6.message.mid.mid_addr;
		}
	      net_output(ifp);
	      remainsize = net_outbuffer_bytes_left(ifp);
	    }

	  COPY_IP(&addrs6->addr, &ifs->ip_addr);
	  addrs6++;
	  curr_size += ipsize;
	}
    }

  m->v6.olsr_msgsize = htons(curr_size);
  m->v6.seqno = htons(get_msg_seqno());/* seqnumber */

  //printf("Sending MID (%d bytes)...\n", outputsize);
  net_outbuffer_push(ifp, msg_buffer, curr_size);

  return;
}




/**
 *IP version 4
 *
 *@param ifp the interface to send on
 *@return nada
 */
static void
hna_build4(struct interface *ifp)
{
  int remainsize, curr_size;
  /* preserve existing data in output buffer */
  union olsr_message *m;
  struct hnapair *pair;
  struct hna4_entry *h = olsr_cnf->hna4_entries;

  /* No hna nets */
  if((olsr_cnf->ip_version != AF_INET) || (!ifp) || h == NULL)
    return;
    
  remainsize = net_outbuffer_bytes_left(ifp);
  
  curr_size = 12; /* OLSR message header */
  
  /* Send pending packet if not room in buffer */
  if(curr_size > remainsize)
    {
      net_output(ifp);
      remainsize = net_outbuffer_bytes_left(ifp);
    }
  
  m = (union olsr_message *)msg_buffer;
  
  
  /* Fill header */
  COPY_IP(&m->v4.originator, &main_addr);
  m->v4.hopcnt = 0;
  m->v4.ttl = MAX_TTL;
  m->v4.olsr_msgtype = HNA_MESSAGE;
  m->v4.olsr_vtime = ifp->valtimes.hna;
  

  pair = m->v4.message.hna.hna_net;
  
  while(h)
    {
      if((curr_size + (2 * ipsize)) > remainsize)
	{
	  /* Only add HNA message if it contains data */
	  if(curr_size > 12)
	    {
	      m->v4.seqno = htons(get_msg_seqno());
	      m->v4.olsr_msgsize = htons(curr_size);
	      net_outbuffer_push(ifp, msg_buffer, curr_size);
	      curr_size = 12; /* OLSR message header */
	      pair = m->v4.message.hna.hna_net;
	    }
	  net_output(ifp);
	  remainsize = net_outbuffer_bytes_left(ifp);
	}
      COPY_IP(&pair->addr, &h->net);
      COPY_IP(&pair->netmask, &h->netmask);
      pair++;
      curr_size += (2 * ipsize);
      h = h->next;
    }

  m->v4.seqno = htons(get_msg_seqno());
  m->v4.olsr_msgsize = htons(curr_size);

  net_outbuffer_push(ifp, msg_buffer, curr_size);

  //printf("Sending HNA (%d bytes)...\n", outputsize);
  return;
}





/**
 *IP version 6
 *
 *@param ifp the interface to send on
 *@return nada
 */
static void
hna_build6(struct interface *ifp)
{
  int remainsize, curr_size;
  /* preserve existing data in output buffer */
  union olsr_message *m;
  struct hnapair6 *pair6;
  union olsr_ip_addr tmp_netmask;
  struct hna6_entry *h = olsr_cnf->hna6_entries;
  
  /* No hna nets */
  if((olsr_cnf->ip_version != AF_INET6) || (!ifp) || h == NULL)
    return;

    
  remainsize = net_outbuffer_bytes_left(ifp);

  curr_size = 24; /* OLSR message header */

  /* Send pending packet if not room in buffer */
  if(curr_size > remainsize)
    {
      net_output(ifp);
      remainsize = net_outbuffer_bytes_left(ifp);
    }

  m = (union olsr_message *)msg_buffer;   

  /* Fill header */
  COPY_IP(&m->v6.originator, &main_addr);
  m->v6.hopcnt = 0;
  m->v6.ttl = MAX_TTL;
  m->v6.olsr_msgtype = HNA_MESSAGE;
  m->v6.olsr_vtime = ifp->valtimes.hna;

  pair6 = m->v6.message.hna.hna_net;


  while(h)
    {
      if((curr_size + (2 * ipsize)) > remainsize)
	{
	  /* Only add HNA message if it contains data */
	  if(curr_size > 24)
	    {
	      m->v6.seqno = htons(get_msg_seqno());
	      m->v6.olsr_msgsize = htons(curr_size);
	      net_outbuffer_push(ifp, msg_buffer, curr_size);
	      curr_size = 24; /* OLSR message header */
	      pair6 = m->v6.message.hna.hna_net;
	    }
	  net_output(ifp);
	  remainsize = net_outbuffer_bytes_left(ifp);
	}
      
      //printf("Adding %s\n", olsr_ip_to_string(&h->hna_net.addr));
      COPY_IP(&pair6->addr, &h->net);
      olsr_prefix_to_netmask(&tmp_netmask, h->prefix_len);
      COPY_IP(&pair6->netmask, &tmp_netmask);
      pair6++;
      curr_size += (2 * ipsize);
      h = h->next;
    }
  
  m->v6.olsr_msgsize = htons(curr_size);
  m->v6.seqno = htons(get_msg_seqno());
  
  net_outbuffer_push(ifp, msg_buffer, curr_size);
  
  //printf("Sending HNA (%d bytes)...\n", outputsize);
  return;

}

#if defined USE_LINK_QUALITY
void
lq_hello_build(struct lq_hello_message *msg, struct interface *outif)
{
  int off, rem, size, req;
  union olsr_message *olsr_msg;
  struct lq_hello_header *head;
  struct lq_hello_info_header *info_head;
  struct lq_hello_neighbor *neigh;
  unsigned char *buff;
  olsr_bool is_first;
  int i, j;

  if (msg == NULL || outif == NULL)
    return;

  olsr_msg = (union olsr_message *)msg_buffer;

  // initialize the OLSR header

  if (olsr_cnf->ip_version == AF_INET)
    {
      olsr_msg->v4.olsr_msgtype = LQ_HELLO_MESSAGE;
      olsr_msg->v4.olsr_vtime = outif->valtimes.hello;

      COPY_IP(&olsr_msg->v4.originator, &msg->main);

      olsr_msg->v4.ttl = msg->ttl;
      olsr_msg->v4.hopcnt = msg->hops;
    }

  else
    {
      olsr_msg->v6.olsr_msgtype = LQ_HELLO_MESSAGE;
      olsr_msg->v6.olsr_vtime = outif->valtimes.hello;

      COPY_IP(&olsr_msg->v6.originator, &msg->main);

      olsr_msg->v6.ttl = msg->ttl;
      olsr_msg->v6.hopcnt = msg->hops;
    }

  off = 8 + ipsize;

  // initialize the LQ_HELLO header

  head = (struct lq_hello_header *)(msg_buffer + off);

  head->reserved = 0;
  head->htime = outif->hello_etime;
  head->will = msg->will; 

  // 'off' is the offset of the byte following the LQ_HELLO header

  off += sizeof (struct lq_hello_header);

  // our work buffer starts at 'off'...

  buff = msg_buffer + off;

  // ... that's why we start with a 'size' of 0 and subtract 'off' from
  // the remaining bytes in the output buffer

  size = 0;
  rem = net_outbuffer_bytes_left(outif) - off;

  // initially, we want to put at least an info header, an IP address,
  // and the corresponding link quality into the message

  if (rem < sizeof (struct lq_hello_info_header) + ipsize + 4)
  {
    net_output(outif);

    rem = net_outbuffer_bytes_left(outif) - off;
  }

  info_head = NULL;

  // iterate through all neighbor types ('i') and all link types ('j')

  for (i = 0; i <= MAX_NEIGH; i++) 
    {
      for(j = 0; j <= MAX_LINK; j++)
	{
	  if(j == HIDE_LINK)
	      continue;

          is_first = TRUE;

          // loop through neighbors

	  for (neigh = msg->neigh; neigh != NULL; neigh = neigh->next)
	    {  
	      if (neigh->neigh_type != i || neigh->link_type != j)
                continue;

              // we need space for an IP address plus link quality
              // information

              req = ipsize + 4;

              // no, we also need space for an info header, as this is the
              // first neighbor with the current neighor type and link type

              if (is_first)
                req += sizeof (struct lq_hello_info_header);

              // we do not have enough space left

              if (size + req > rem)
                {
                  // finalize the OLSR header
                  
                  if (olsr_cnf->ip_version == AF_INET)
                    {
                      olsr_msg->v4.seqno = htons(get_msg_seqno());
                      olsr_msg->v4.olsr_msgsize = htons(size + off);
                    }

                  else
                    {
                      olsr_msg->v6.seqno = htons(get_msg_seqno());
                      olsr_msg->v6.olsr_msgsize = htons(size + off);
                    }
			      
                  // finalize the info header

                  info_head->size =
                    ntohs(buff + size - (unsigned char *)info_head);
			      
                  // output packet

                  net_outbuffer_push(outif, msg_buffer, size + off);

                  net_output(outif);

                  // move to the beginning of the buffer

                  size = 0;
                  rem = net_outbuffer_bytes_left(outif) - off;

                  // we need a new info header

                  is_first = TRUE;
                }

              // create a new info header

              if (is_first)
                {
                  info_head = (struct lq_hello_info_header *)(buff + size);
                  size += sizeof (struct lq_hello_info_header);

                  info_head->reserved = 0;
                  info_head->link_code = CREATE_LINK_CODE(i, j);
                }

#ifdef DEBUG
              olsr_printf(5, "\tLink status of %s: %d\n",
                          olsr_ip_to_string(&neigh->address), neigh->link);
#endif
		  
              // add the current neighbor's IP address

              COPY_IP(buff + size, &neigh->addr);
              size += ipsize;

              // add the corresponding link quality

              buff[size++] = (unsigned char)(neigh->link_quality * 256);

              // pad

              buff[size++] = 0;
              buff[size++] = 0;
              buff[size++] = 0;

              is_first = FALSE;
	    }

          // finalize the info header, if there are any neighbors with the
          // current neighbor type and link type

	  if (!is_first)
            info_head->size = ntohs(buff + size - (unsigned char *)info_head);
	}
    }

  // finalize the OLSR header
     
  if (olsr_cnf->ip_version == AF_INET)
    {
      olsr_msg->v4.seqno = htons(get_msg_seqno());
      olsr_msg->v4.olsr_msgsize = htons(size + off);
    }

  else
    {
      olsr_msg->v6.seqno = htons(get_msg_seqno());
      olsr_msg->v6.olsr_msgsize = htons(size + off);
    }
			      
  // move the message to the output buffer

  net_outbuffer_push(outif, msg_buffer, size + off);

  // clean-up

  olsr_destroy_lq_hello_message(msg);
}

void
lq_tc_build(struct lq_tc_message *msg, struct interface *outif)
{
  int off, rem, size;
  union olsr_message *olsr_msg;
  struct lq_tc_header *head;
  struct lq_tc_neighbor *neigh;
  unsigned char *buff;
  olsr_bool is_empty;

  if (msg == NULL || outif == NULL)
    return;

  olsr_msg = (union olsr_message *)msg_buffer;

  // initialize the OLSR header

  if (olsr_cnf->ip_version == AF_INET)
    {
      olsr_msg->v4.olsr_msgtype = LQ_TC_MESSAGE;
      olsr_msg->v4.olsr_vtime = outif->valtimes.tc;

      COPY_IP(&olsr_msg->v4.originator, &msg->main);

      olsr_msg->v4.ttl = msg->ttl;
      olsr_msg->v4.hopcnt = msg->hops;
    }

  else
    {
      olsr_msg->v6.olsr_msgtype = LQ_TC_MESSAGE;
      olsr_msg->v6.olsr_vtime = outif->valtimes.tc;

      COPY_IP(&olsr_msg->v6.originator, &msg->main);

      olsr_msg->v6.ttl = msg->ttl;
      olsr_msg->v6.hopcnt = msg->hops;
    }

  off = 8 + ipsize;

  // initialize the LQ_TC header

  head = (struct lq_tc_header *)(msg_buffer + off);

  head->ansn = htons(msg->ansn);
  head->reserved = 0;

  // 'off' is the offset of the byte following the LQ_TC header

  off += sizeof (struct lq_tc_header);

  // our work buffer starts at 'off'...

  buff = msg_buffer + off;

  // ... that's why we start with a 'size' of 0 and subtract 'off' from
  // the remaining bytes in the output buffer

  size = 0;
  rem = net_outbuffer_bytes_left(outif) - off;

  // initially, we want to put at least an IP address and the corresponding
  // link quality into the message

  if (rem < ipsize + 4)
  {
    net_output(outif);

    rem = net_outbuffer_bytes_left(outif) - off;
  }

  // initially, we're empty

  is_empty = TRUE;

  // loop through neighbors

  for (neigh = msg->neigh; neigh != NULL; neigh = neigh->next)
    {  
      // we need space for an IP address plus link quality
      // information

      if (size + ipsize + 4 > rem)
        {
          // finalize the OLSR header
                  
          if (olsr_cnf->ip_version == AF_INET)
            {
              olsr_msg->v4.seqno = htons(get_msg_seqno());
              olsr_msg->v4.olsr_msgsize = htons(size + off);
            }

          else
            {
              olsr_msg->v6.seqno = htons(get_msg_seqno());
              olsr_msg->v6.olsr_msgsize = htons(size + off);
            }

          // output packet

          net_outbuffer_push(outif, msg_buffer, size + off);

          net_output(outif);

          // move to the beginning of the buffer

          size = 0;
          rem = net_outbuffer_bytes_left(outif) - off;
        }

      // add the current neighbor's IP address

      COPY_IP(buff + size, &neigh->main);
      size += ipsize;

      // add the corresponding link quality

      buff[size++] = (unsigned char)(neigh->link_quality * 256);

      // pad

      buff[size++] = 0;
      buff[size++] = 0;
      buff[size++] = 0;

      // we're not empty any longer

      is_empty = FALSE;
    }

  // finalize the OLSR header
     
  if (olsr_cnf->ip_version == AF_INET)
    {
      olsr_msg->v4.seqno = htons(get_msg_seqno());
      olsr_msg->v4.olsr_msgsize = htons(size + off);
    }

  else
    {
      olsr_msg->v6.seqno = htons(get_msg_seqno());
      olsr_msg->v6.olsr_msgsize = htons(size + off);
    }
			      
  // if we did not advertise any neighbors, we might still want to
  // send empty LQ_TC messages

  if (is_empty && !TIMED_OUT(&send_empty_tc))
  {
    olsr_printf(1, "LQ_TC: Sending empty package\n");
    is_empty = FALSE;
  }

  // move the message to the output buffer

  if (!is_empty)
    net_outbuffer_push(outif, msg_buffer, size + off);

  // clean-up

  olsr_destroy_lq_tc_message(msg);
}
#endif

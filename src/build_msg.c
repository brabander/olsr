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
 */


#include "defs.h"
#include "build_msg.h"
#include "local_hna_set.h"
#include "olsr.h"


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
  switch(ipversion)
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
  switch(ipversion)
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
  switch(ipversion)
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
  switch(ipversion)
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
  int remainsize;
  struct hello_neighbor *nb, *prev_nb;
  union olsr_message *m;
  struct hellomsg *h;
  struct hellinfo *hinfo;
  union olsr_ip_addr *haddr;
  int i, j, sametype;
  int lastpacket = 0; /* number of neighbors with the same
			 greater link status in the last packet */
  if((!message) || (!ifp) || (ipversion != AF_INET))
    return;

  remainsize = outputsize - (sizeof(msg->v4.olsr_packlen) + sizeof(msg->v4.olsr_seqno));

  //printf("HELLO build outputsize: %d\n", outputsize);

  m = outputsize ? 
    (union olsr_message *)((char *)msg->v4.olsr_msg + remainsize)
    : (union olsr_message *) msg->v4.olsr_msg;

  h = &m->v4.message.hello;
  hinfo = h->hell_info;
  haddr = (union olsr_ip_addr *)hinfo->neigh_addr;
  
  //printf("Neighbor addr: %s\n", olsr_ip_to_string(haddr));fflush(stdout);

  /* Set hopcount and ttl */
  m->v4.ttl = message->ttl;
  m->v4.hopcnt = 0;

  /* Send pending packet if not room in buffer */
  if (outputsize > maxmessagesize - (int)sizeof (struct olsrmsg)) 
    net_output(ifp);

  /* Set willingness */
  h->willingness = message->willingness; 

  if(ifp->is_wireless)
    h->htime = htime;
  else
    h->htime = htime_nw;


  memset(&h->reserved, 0, sizeof(olsr_u16_t));
  
  /* Set source(main) addr */
  
  COPY_IP(&m->v4.originator, &main_addr);

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

	  outputsize = (char *)hinfo - packet;

	  //printf("Neighbortype %d outputsize %d\n", i, outputsize);

	  /*
	   *Only if we're gona chop the packet
	   */
	  if (outputsize > maxmessagesize - (int)sizeof (struct hellinfo)) 
	    {
	      olsr_printf(1, "Chomping HELLO message\n");

	      m->v4.olsr_msgtype = HELLO_MESSAGE;
	      if(ifp->is_wireless)
		m->v4.olsr_vtime = hello_vtime;
	      else
		m->v4.olsr_vtime = hello_nw_vtime;

	      m->v4.olsr_msgsize = (char *)hinfo - (char *)m;
	      m->v4.olsr_msgsize = htons(m->v4.olsr_msgsize);
	      hinfo->size = (char *)haddr - (char *)hinfo;
	      hinfo->size = ntohs(hinfo->size);
	  
	      m->v4.seqno = htons(get_msg_seqno());		
	  
	      net_output(ifp);
	  
	      m = (union olsr_message *)msg->v4.olsr_msg;
	      h = &m->v4.message.hello;
	      hinfo = h->hell_info;
	      haddr = (union olsr_ip_addr *)&hinfo->neigh_addr;	
	    }

	  /*
	   *Looping trough neighbors
	   */
	  for (nb = message->neighbors; nb != NULL; nb = nb->next) 
	    {
	  
	      outputsize = (char *)haddr - packet;
	  
	      /*
	       *If we're gonna chop it...again
	       */
	      if (outputsize > maxmessagesize - (int)ipsize)
		{
		  olsr_printf(1, "Chomping HELLO again\n");

		  m->v4.olsr_msgtype = HELLO_MESSAGE;
		  if(ifp->is_wireless)
		    m->v4.olsr_vtime = hello_vtime;
		  else
		    m->v4.olsr_vtime = hello_nw_vtime;

		  m->v4.olsr_msgsize = (char *)haddr - (char *)m;
		  m->v4.olsr_msgsize = htons(m->v4.olsr_msgsize);
		  hinfo->size = (char *)haddr - (char *)hinfo;
		  hinfo->size = ntohs(hinfo->size);
	      
		  m->v4.seqno = htons(get_msg_seqno());

		  net_output(ifp);
	      
		  m = (union olsr_message *)msg->v4.olsr_msg;
		  h = &m->v4.message.hello;
		  hinfo = h->hell_info;
		  haddr = (union olsr_ip_addr *)&hinfo->neigh_addr;

		  memset(&hinfo->reserved, 0, sizeof(olsr_u8_t));
		  /* Set link and status */
		  //hinfo->link_code = j | (i<<2);
		  hinfo->link_code = CREATE_LINK_CODE(i, j);

		  //printf("Setting neighbor link status: %x\n", hinfo->link_code);

	      
		  lastpacket = sametype;
		}
	  
	  
	      if ((nb->status == i) && (nb->link == j))
		{
		  sametype++;
		  if (sametype == 1)
		    {
		      memset(&hinfo->reserved, 0, sizeof(olsr_u8_t));
		      /* Set link and status for this group of neighbors (this is the first) */
		      hinfo->link_code = CREATE_LINK_CODE(i, j);//j | (i<<2);
		      //printf("(2)Setting neighbor link status: %x\n", hinfo->link_code);
		    }

#ifdef DEBUG
		  olsr_printf(5, "\tLink status of %s: ", olsr_ip_to_string(&nb->address));
		  olsr_printf(5, "%d\n", nb->link);
#endif

		  COPY_IP(haddr, &nb->address);

		  //printf("\n\n1: %d\n", (char *)haddr - packet);
		  /*
		   *Point to next address
		   */
		  haddr = (union olsr_ip_addr *)&haddr->v6.s6_addr[4];

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
  m->v4.olsr_msgsize = (char *)hinfo - (char *)m;
  m->v4.olsr_msgsize = htons(m->v4.olsr_msgsize);
  m->v4.olsr_msgtype = HELLO_MESSAGE;
  if(ifp->is_wireless)
    m->v4.olsr_vtime = hello_vtime;
  else
    m->v4.olsr_vtime = hello_nw_vtime;

  outputsize = (char *)hinfo - packet;
  
  if (outputsize > maxmessagesize - (int)sizeof (struct olsrmsg)) 
    net_output(ifp);


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
  int remainsize;
  struct hello_neighbor *nb, *prev_nb;
  union olsr_message *m;
  struct hellomsg6 *h6;
  struct hellinfo6 *hinfo6;
  union olsr_ip_addr *haddr;

  int i, j, sametype;
  int lastpacket = 0; /* number of neighbors with the same
			 greater link status in the last packet */
  if((!message) || (!ifp) || (ipversion != AF_INET6))
    return;

  remainsize = outputsize - (sizeof(msg->v6.olsr_packlen) + sizeof(msg->v6.olsr_seqno));

  //printf("HELLO build outputsize: %d\n", outputsize);

  m = outputsize ? 
    (union olsr_message *)((char *)msg->v6.olsr_msg + remainsize)
    : (union olsr_message *) msg->v6.olsr_msg;
  h6 = &m->v6.message.hello;
  hinfo6 = h6->hell_info;
  haddr = (union olsr_ip_addr *)hinfo6->neigh_addr;


  /* Set hopcount and ttl */
  m->v6.ttl = message->ttl;
  m->v6.hopcnt = 0;

  if (outputsize > maxmessagesize - (int)sizeof (struct olsrmsg)) 
    net_output(ifp);
  
  
  /* Set willingness */
  h6->willingness = message->willingness; 

  if(ifp->is_wireless)
    h6->htime = htime;
  else
    h6->htime = htime_nw;
  
  memset(&h6->reserved, 0, sizeof(olsr_u16_t));
  
  
  /* Set source(main) addr */
  
  COPY_IP(&m->v6.originator, &main_addr);

  /*
   *Loops trough all possible neighbor statuses
   *The negbor list is grouped by status
   */

  for (i = 0; i <= MAX_NEIGH; i++) 
    {
      for(j = 0; j <= MAX_LINK; j++)
	{
	  
	  
	  lastpacket = sametype = 0;
	  
	  outputsize = (char *)hinfo6 - packet;
	  
	  //printf("Neighbortype %d outputsize %d\n", i, outputsize);
	  
	  /*
	   *Only if we're gona chop the packet
	   */
	  if (outputsize > maxmessagesize - (int)sizeof (struct hellinfo)) 
	    {
	      olsr_printf(1, "Chomping HELLO message\n");
	      
	      m->v6.olsr_msgtype = HELLO_MESSAGE;
	      if(ifp->is_wireless)
		m->v6.olsr_vtime = hello_vtime;
	      else
		m->v6.olsr_vtime = hello_nw_vtime;
	      
	      m->v6.olsr_msgsize = (char *)hinfo6 - (char *)m;
	      m->v6.olsr_msgsize = htons(m->v6.olsr_msgsize);
	      hinfo6->size = (char *)haddr - (char *)hinfo6;
	      hinfo6->size = ntohs(hinfo6->size);
	      
	      m->v6.seqno = htons(get_msg_seqno());		
	      
	      net_output(ifp);
	      
	      m = (union olsr_message *)msg->v6.olsr_msg;
	      h6 = &m->v6.message.hello;
	      hinfo6 = h6->hell_info;
	      haddr = (union olsr_ip_addr *)&hinfo6->neigh_addr;
	      
	    }
	  
	  /*
	   *Looping trough neighbors
	   */
	  for (nb = message->neighbors; nb != NULL; nb = nb->next) 
	    {
	      
	      outputsize = (char *)haddr - packet;
	      
	      /*
	       *If we're gonna chop it...again
	       */
	      if (outputsize > maxmessagesize - (int)ipsize)
		{
		  olsr_printf(1, "Chomping HELLO again\n");
		  
		  m->v6.olsr_msgtype = HELLO_MESSAGE;
		  if(ifp->is_wireless)
		    m->v6.olsr_vtime = hello_vtime;
		  else
		    m->v6.olsr_vtime = hello_nw_vtime;
		  
		  m->v6.olsr_msgsize = (char *)haddr - (char *)m;
		  m->v6.olsr_msgsize = htons(m->v6.olsr_msgsize);
		  hinfo6->size = (char *)haddr - (char *)hinfo6;
		  hinfo6->size = ntohs(hinfo6->size);
		  
		  m->v6.seqno = htons(get_msg_seqno());
		  
		  //m->v6.seqno = htons(message->mpr_seq_number);
		  
		  net_output(ifp);
		  
		  m = (union olsr_message *)msg->v6.olsr_msg;
		  h6 = &m->v6.message.hello;
		  hinfo6 = h6->hell_info;
		  haddr = (union olsr_ip_addr *)&hinfo6->neigh_addr;
		  

		  memset(&hinfo6->reserved, 0, sizeof(olsr_u8_t));
		  /* Set link and status */
		  hinfo6->link_code = CREATE_LINK_CODE(i, j);//j | (i<<2);
  	  
		  //printf("Setting neighbor link status: %d\n", nb->link);
		  
		  
		  lastpacket = sametype;
		}
	      
	      
	      if ((nb->status == i) && (nb->link == j))
		{	      
		  sametype++;
		  if (sametype == 1)
		    {
		      memset(&hinfo6->reserved, 0, sizeof(olsr_u8_t));
		      /* Set link and status for this group of neighbors (this is the first) */
		      hinfo6->link_code = CREATE_LINK_CODE(i, j);//j | (i<<2);
		      //printf("(2)Setting neighbor link status: %x\n", hinfo->link_code);
		    }

#ifdef DEBUG
		  olsr_printf(5, "\tLink status of %s: ", olsr_ip_to_string(&nb->address));
		  olsr_printf(5, "%d\n", nb->link);
#endif

		  COPY_IP(haddr, &nb->address);
		  
		  //printf("\n\n1: %d\n", (char *)haddr - packet);
		  /*
		   *Point to next address
		   */
		  haddr++;
		  
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
  m->v6.olsr_msgsize = (char *)hinfo6 - (char *)m;
  m->v6.olsr_msgsize = htons(m->v6.olsr_msgsize);
  m->v6.olsr_msgtype = HELLO_MESSAGE;
  if(ifp->is_wireless)
    m->v6.olsr_vtime = hello_vtime;
  else
    m->v6.olsr_vtime = hello_nw_vtime;

  outputsize = (char *)hinfo6 - packet;

  if (outputsize > maxmessagesize - (int)sizeof (struct olsrmsg6)) 
      net_output(ifp);


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

  int remainsize;
  struct tc_mpr_addr *mprs, *prev_mprs;
  union olsr_message *m;
  struct tcmsg *tc;
  struct neigh_info *mprsaddr; 
  int found = 0;
  int msgsize;


  if((!message) || (!ifp) || (ipversion != AF_INET))
    return;

  remainsize = outputsize - (sizeof(msg->v4.olsr_packlen) + sizeof(msg->v4.olsr_seqno));

  m = outputsize ? 
    (union olsr_message *)((char *)msg->v4.olsr_msg + remainsize)
    : (union olsr_message *) msg->v4.olsr_msg;
  tc = &m->v4.message.tc;
  mprsaddr = tc->neigh;
  msgsize = (int)sizeof(struct olsrmsg);
  tc->reserved = 0;

  if (outputsize > maxmessagesize - msgsize) 
    net_output(ifp);
            

  /*Looping trough MPR selectors */
  for (mprs = message->multipoint_relay_selector_address; mprs != NULL;mprs = mprs->next) 
    {
	    
      outputsize = (char *)mprsaddr - packet;

      /*If packet is to be chomped */
      if (outputsize > maxmessagesize - (int)sizeof (mprsaddr))
	{

	  olsr_printf(1, "Chomping TC!\n");

	  m->v4.olsr_vtime = tc_vtime;
	  m->v4.olsr_msgtype = TC_MESSAGE;
	  m->v4.olsr_msgsize = (char *)mprsaddr - (char *)m;
	  m->v4.olsr_msgsize = htons(m->v4.olsr_msgsize);
	  m->v4.seqno = htons(get_msg_seqno());
	  m->v4.hopcnt = message->hop_count;
	  m->v4.ttl = message->ttl;
	  COPY_IP(&m->v4.originator, &message->originator);

	  net_output(ifp);
		
	  m = (union olsr_message *)msg->v4.olsr_msg;
	  tc = &m->v4.message.tc;

	  mprsaddr = tc->neigh;
	  found = 0;
	}
      found = 1;
	    
      COPY_IP(&mprsaddr->addr, &mprs->address);

      mprsaddr++;
    }

  if (found)
    {
	    
      outputsize = (char *)mprsaddr - packet;

      m->v4.olsr_msgtype = TC_MESSAGE;
      m->v4.olsr_msgsize = (char *)mprsaddr - (char *)m;
      m->v4.olsr_msgsize = htons(m->v4.olsr_msgsize);
      m->v4.olsr_vtime = tc_vtime;
      
      m->v4.seqno = htons(get_msg_seqno());
      tc->ansn = htons(message->ansn);
      
      m->v4.hopcnt = message->hop_count;
      m->v4.ttl = message->ttl;

      COPY_IP(&m->v4.originator, &message->originator);

    }
  else
    {
      if(!TIMED_OUT(&send_empty_tc))
	{
	  olsr_printf(1, "TC: Sending empty package\n");

	    
	  outputsize = 20;

	  m->v4.olsr_msgtype = TC_MESSAGE;
	  m->v4.olsr_msgsize = 16;
	  m->v4.olsr_msgsize = htons(m->v4.olsr_msgsize);
	  m->v4.olsr_vtime = tc_vtime;
	  
	  m->v4.seqno = htons(get_msg_seqno());
	  tc->ansn = htons(message->ansn);
	  
	  m->v4.hopcnt = message->hop_count;
	  m->v4.ttl = message->ttl;
	  
	  COPY_IP(&m->v4.originator, &message->originator);
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

  int remainsize;
  struct tc_mpr_addr *mprs, *prev_mprs;
  union olsr_message *m;
  struct tcmsg6 *tc6;
  struct neigh_info6 *mprsaddr6; 
  int found = 0;
  int msgsize;


  if ((!message) || (!ifp) || (ipversion != AF_INET6))
    return;

  remainsize = outputsize - (sizeof(msg->v6.olsr_packlen) + sizeof(msg->v6.olsr_seqno));


  m = outputsize ? 
    (union olsr_message *)((char *)msg->v6.olsr_msg + remainsize)
    : (union olsr_message *) msg->v6.olsr_msg;
  tc6 = &m->v6.message.tc;
  mprsaddr6 = tc6->neigh;
  msgsize = (int)sizeof(struct olsrmsg6);
  tc6->reserved = 0;

  if (outputsize > maxmessagesize - msgsize) 
    net_output(ifp);

  

  /*Looping trough MPR selectors */
  for (mprs = message->multipoint_relay_selector_address; mprs != NULL;mprs = mprs->next) 
    {
	    
      outputsize = (char *)mprsaddr6 - packet;


      /*If packet is to be chomped */
      if (outputsize > maxmessagesize - (int)sizeof (mprsaddr6))
	{
	  m->v6.olsr_msgtype = TC_MESSAGE;
	  m->v6.olsr_msgsize = (char *)mprsaddr6 - (char *)m;
	  m->v6.olsr_msgsize = htons(m->v6.olsr_msgsize);
	  m->v6.olsr_vtime = tc_vtime;
	  
	  m->v6.seqno = htons(get_msg_seqno());
	  tc6->ansn = htons(message->ansn);
	  
	  m->v6.hopcnt = message->hop_count;
	  m->v6.ttl = message->ttl;

	  COPY_IP(&m->v6.originator, &message->originator);

	  net_output(ifp);
		
	  m = (union olsr_message *)msg->v6.olsr_msg;
	  tc6 = &m->v6.message.tc;

	  mprsaddr6 = tc6->neigh;

	  found = 0;
	}
      found = 1;

      //printf("mprsaddr6 is %x\n", (char *)mprsaddr6 - packet);
      //printf("Adding MPR-selector: %s\n", olsr_ip_to_string(&mprs->address));fflush(stdout);	    
      COPY_IP(&mprsaddr6->addr, &mprs->address);

      mprsaddr6++;
    }
	
  if (found)
    {
	    
      outputsize = (char *)mprsaddr6 - packet;

      m->v6.olsr_msgtype = TC_MESSAGE;
      m->v6.olsr_msgsize = (char *)mprsaddr6 - (char *)m;
      m->v6.olsr_msgsize = htons(m->v6.olsr_msgsize);
      m->v6.olsr_vtime = tc_vtime;

      m->v6.seqno = htons(get_msg_seqno());
      tc6->ansn = htons(message->ansn);
      
      m->v6.hopcnt = message->hop_count;
      m->v6.ttl = message->ttl;

      COPY_IP(&m->v6.originator, &message->originator);

    }
  else
    {
      if(!TIMED_OUT(&send_empty_tc))
	{
	  olsr_printf(1, "TC: Sending empty package\n");
	    
	  outputsize = 32;

	  m->v6.olsr_msgtype = TC_MESSAGE;
	  m->v6.olsr_msgsize = 28;
	  m->v6.olsr_msgsize = htons(m->v6.olsr_msgsize);
	  m->v6.olsr_vtime = tc_vtime;
	  
	  m->v6.seqno = htons(get_msg_seqno());
	  tc6->ansn = htons(message->ansn);
	  
	  m->v6.hopcnt = message->hop_count;
	  m->v6.ttl = message->ttl;
	  
	  COPY_IP(&m->v6.originator, &message->originator);
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
 *@param ifn use this interfaces address as main address
 *@return 1 on success
 */

static void
mid_build4(struct interface *ifn)
{
  int remainsize;
  /* preserve existing data in output buffer */
  union olsr_message *m;
  struct midaddr *addrs;
  struct midmsg *mmsg;
  struct interface *ifs;  

  if((ipversion != AF_INET) || (!ifn) || (nbinterf <= 1))
    return;

  //printf("\t\tGenerating mid on %s\n", ifn->int_name);

  remainsize = outputsize - (sizeof(msg->v4.olsr_packlen) 
			     + sizeof(msg->v4.olsr_seqno));

  m = outputsize ? 
    (union olsr_message *)((char *)msg->v4.olsr_msg + remainsize)
    :(union olsr_message *) msg->v4.olsr_msg;

  mmsg = &m->v4.message.mid;

  m->v4.hopcnt = 0;
  m->v4.ttl = MAX_TTL;

  m->v4.seqno = htons(get_msg_seqno());/* seqnumber */

  /* pad - seems we don't need this....*/
  //memset(mmsg->mid_res, 0, sizeof(mmsg->mid_res));
  //mmsg->mid_res = 0;
      
  /* Set main(first) address */
  COPY_IP(&m->v4.originator, &main_addr);
      
  addrs = mmsg->mid_addr;

  /* Don't add the main address... it's already there */
  for(ifs = ifnet; ifs != NULL; ifs = ifs->int_next)
    {
      if(!COMP_IP(&main_addr, &ifs->ip_addr))
	{
	  COPY_IP(&addrs->addr, &ifs->ip_addr);
	  addrs++;
	}
    }

  m->v4.olsr_msgtype = MID_MESSAGE;
  m->v4.olsr_msgsize = (char*)addrs - (char*)m;
  m->v4.olsr_msgsize = htons(m->v4.olsr_msgsize);

  m->v4.olsr_vtime = mid_vtime;

  outputsize = (char*)addrs - packet; 

  //printf("Sending MID (%d bytes)...\n", outputsize);


  return;
}



/**
 *IP version 6
 *
 *<b>NO INTERNAL BUFFER</b>
 *@param ifn use this interfaces address as main address
 *@return 1 on success
 */

static void
mid_build6(struct interface *ifn)
{
  int remainsize;
  /* preserve existing data in output buffer */
  union olsr_message *m;
  struct midaddr6 *addrs6;
  struct midmsg6 *mmsg6;
  struct interface *ifs;

  //printf("\t\tGenerating mid on %s\n", ifn->int_name);


  if((ipversion != AF_INET6) || (!ifn) || (nbinterf <= 1))
    return;

  remainsize = outputsize - (sizeof(msg->v6.olsr_packlen) 
			     + sizeof(msg->v6.olsr_seqno));

  m = outputsize ? 
    (union olsr_message *)((char *)msg->v6.olsr_msg + remainsize)
    :(union olsr_message *) msg->v6.olsr_msg;
      
  mmsg6 = &m->v6.message.mid;

  m->v6.hopcnt = 0;
  m->v6.ttl = MAX_TTL;

  m->v6.seqno = htons(get_msg_seqno());/* seqnumber */
  /* pad - seems we don't need this....*/
  //memset(mmsg->mid_res, 0, sizeof(mmsg->mid_res));
  //mmsg->mid_res = 0;
      
  /* Set main(first) address */
  COPY_IP(&m->v6.originator, &main_addr);
      
  addrs6 = mmsg6->mid_addr;

  /* Don't add the main address... it's already there */
  for(ifs = ifnet; ifs != NULL; ifs = ifs->int_next)
    {
      if(!COMP_IP(&main_addr, &ifs->ip_addr))
	{
	  COPY_IP(&addrs6->addr, &ifs->ip_addr);
	  addrs6++;
	}
    }

  m->v6.olsr_msgtype = MID_MESSAGE;
  m->v6.olsr_msgsize = (char*)addrs6 - (char*)m;
  m->v6.olsr_msgsize = htons(m->v6.olsr_msgsize);
  m->v6.olsr_vtime = mid_vtime;

  outputsize = (char*)addrs6 - packet;

  //printf("Sending MID (%d bytes)...\n", outputsize);


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
  int remainsize;
  /* preserve existing data in output buffer */
  union olsr_message *m;
  struct hnapair *pair;
  struct hnamsg *hmsg;
  struct local_hna_entry *h;

  /* No hna nets */
  if((ipversion != AF_INET) || (!ifp) || (local_hna4_set.next == &local_hna4_set))
    return;
    
  remainsize = outputsize - (sizeof(msg->v4.olsr_packlen) 
			     + sizeof(msg->v4.olsr_seqno));
  m = outputsize ? 
    (union olsr_message *)((char *)msg->v4.olsr_msg + remainsize)
    :(union olsr_message *) msg->v4.olsr_msg;

  hmsg = &m->v4.message.hna;
    
  COPY_IP(&m->v4.originator, &main_addr);
  m->v4.hopcnt = 0;
  m->v4.ttl = MAX_TTL;

  m->v4.seqno = htons(get_msg_seqno());


  pair = hmsg->hna_net;

  for(h = local_hna4_set.next;
      h != &local_hna4_set;
      h = h->next)
    {
      COPY_IP(&pair->addr, &h->A_network_addr);
      COPY_IP(&pair->netmask, &h->A_netmask);
      pair++;
    }
      
  m->v4.olsr_msgtype = HNA_MESSAGE;
  m->v4.olsr_msgsize = (char*)pair - (char*)m;
  m->v4.olsr_msgsize = htons(m->v4.olsr_msgsize);
  m->v4.olsr_vtime = hna_vtime;

  outputsize = (char*)pair - packet;
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
  int remainsize;
  /* preserve existing data in output buffer */
  union olsr_message *m;
  struct hnapair6 *pair6;
  struct hnamsg6 *hmsg6;
  union olsr_ip_addr tmp_netmask;
  struct local_hna_entry *h;
  
  /* No hna nets */
  if((ipversion != AF_INET6) || (!ifp) || (local_hna6_set.next == &local_hna6_set))
    return;

  remainsize = outputsize - (sizeof(msg->v6.olsr_packlen) 
			     + sizeof(msg->v6.olsr_seqno));

  m = outputsize ? 
    (union olsr_message *)((char *)msg->v6.olsr_msg + remainsize)
    :(union olsr_message *) msg->v6.olsr_msg;

  hmsg6 = &m->v6.message.hna;
    
  COPY_IP(&m->v6.originator, &main_addr);
  m->v6.hopcnt = 0;
  m->v6.ttl = MAX_TTL;

  m->v6.seqno = htons(get_msg_seqno());


  pair6 = hmsg6->hna_net;


  for(h = local_hna6_set.next;
      h != &local_hna6_set;
      h = h->next)
    {
      //printf("Adding %s\n", olsr_ip_to_string(&h->hna_net.addr));
      COPY_IP(&pair6->addr, &h->A_network_addr);
      olsr_prefix_to_netmask(&tmp_netmask, h->A_netmask.v6);
      COPY_IP(&pair6->netmask, &tmp_netmask);
      pair6++;
    }
  
  m->v6.olsr_msgtype = HNA_MESSAGE;
  m->v6.olsr_msgsize = (char*)pair6 - (char*)m;
  m->v6.olsr_msgsize = htons(m->v6.olsr_msgsize);
  m->v6.olsr_vtime = hna_vtime;
  
  outputsize = (char*)pair6 - packet;
  
  //printf("Sending HNA (%d bytes)...\n", outputsize);
  return;

}


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
 * $Id: packet.c,v 1.11 2004/11/03 18:19:54 tlopatic Exp $
 *
 */


#include "defs.h"
#include "link_set.h"
#include "mpr_selector_set.h"
#include "mpr.h"
#include "olsr.h"


static olsr_bool sending_tc = FALSE;

/**
 *Build an internal HELLO package for this
 *node. This MUST be done for each interface.
 *
 *@param message the hello_message struct to fill with info
 *@param outif the interface to send the message on - messages
 *are created individually for each interface!
 *@return 0
 */
int
olsr_build_hello_packet(struct hello_message *message, struct interface *outif)
{
  struct hello_neighbor   *message_neighbor, *tmp_neigh;
  struct link_entry       *links;
  struct neighbor_entry   *neighbor;
  olsr_u16_t              index;
  int                     link;

  olsr_printf(3, "\tBuilding HELLO on interface %d\n", outif->if_nr);

  message->neighbors=NULL;
  message->packet_seq_number=0;
  
  //message->mpr_seq_number=neighbortable.neighbor_mpr_seq;

  /* Set willingness */

  message->willingness = olsr_cnf->willingness;
  //printf("Willingness: %d\n", olsr_cnf->willingness);


  /* Set TTL */

  message->ttl = 1;
  
  //olsr_printf(3, "mpr is %d\n",message->mpr_seq_number);

  COPY_IP(&message->source_addr, &main_addr);


  /* Get the links of this interface */
  links = link_set;

  while(links != NULL)
    {      
      
      link = lookup_link_status(links);
      /* Update the status */
      
      /* Update neighbor */
      /* UPDATED ! */
      //update_neighbor_status(links->neighbor, link);
      //update_neighbor_status(links->neighbor);

      //printf("\nLINK: %d\nSTATUS: %d\n\n", link, neighbor->status);
      //printf("\nProcessing %s\n", olsr_ip_to_string(&links->neighbor_iface_addr));


      /* Check if this link tuple is registered on the outgoing interface */
      if(!COMP_IP(&links->local_iface_addr, &outif->ip_addr))
	{
	  olsr_printf(3, "ADDR: %s - ", olsr_ip_to_string(&outif->ip_addr));
	  olsr_printf(3, "Wrong interface for %s ", olsr_ip_to_string(&links->local_iface_addr));

	  links = links->next;
	  continue;
	}
      
      //printf("\tAdding link to %s\n", olsr_ip_to_string(&links->neighbor_main_address));


      //printf("\tStatus: %d\n", neighbor->status);
      //printf("\tLink: %d\n", message_neighbor->link);

      message_neighbor = olsr_malloc(sizeof(struct hello_neighbor), "Build HELLO");
      

      /* Find the link status */
      message_neighbor->link = link;

      /*
       * Calculate neighbor status
       */
      /* 
       * 2.1  If the main address, corresponding to
       *      L_neighbor_iface_addr, is included in the MPR set:
       *
       *            Neighbor Type = MPR_NEIGH
       */
      if(links->neighbor->is_mpr)
	{
	  message_neighbor->status = MPR_NEIGH;
	}
      /*
       *  2.2  Otherwise, if the main address, corresponding to
       *       L_neighbor_iface_addr, is included in the neighbor set:
       */
      
      /* NOTE:
       * It is garanteed to be included when come this far
       * due to the extentions made in the link sensing
       * regarding main addresses.
       */
      else
	{
	  /*
	   *   2.2.1
	   *        if N_status == SYM
	   *
	   *             Neighbor Type = SYM_NEIGH
	   */
	  if(links->neighbor->status == SYM)
	    {
	      message_neighbor->status = SYM_NEIGH;
	    }
	  /*
	   *   2.2.2
	   *        Otherwise, if N_status == NOT_SYM
	   *             Neighbor Type = NOT_NEIGH
	   */
	  else
	    if(links->neighbor->status == NOT_SYM)
	      {
		message_neighbor->status = NOT_NEIGH;
	      }
	}
  
      /* Set the remote interface address */
      COPY_IP(&message_neighbor->address, &links->neighbor_iface_addr);
      
      /* Set the main address */
      COPY_IP(&message_neighbor->main_address, &links->neighbor->neighbor_main_addr);
      
      olsr_printf(5, "%s - ", olsr_ip_to_string(&message_neighbor->address));
      olsr_printf(5, " status %d\n", message_neighbor->status);
      
      message_neighbor->next=message->neighbors;
      message->neighbors=message_neighbor;	    
      
      links = links->next;
    }
  
  /* Add the links */




  /* Add the rest of the neighbors if running on multiple interfaces */
  
  if(ifnet != NULL && ifnet->int_next != NULL)
    for(index=0;index<HASHSIZE;index++)
      {       
	for(neighbor = neighbortable[index].next;
	    neighbor != &neighbortable[index];
	    neighbor=neighbor->next)
	  {
	    /* Check that the neighbor is not added yet */
	    tmp_neigh = message->neighbors;
	    //printf("Checking that the neighbor is not yet added\n");
	    while(tmp_neigh)
	      {
		if(COMP_IP(&tmp_neigh->main_address, &neighbor->neighbor_main_addr))
		  {
		    //printf("Not adding duplicate neighbor %s\n", olsr_ip_to_string(&neighbor->neighbor_main_addr));
		    break;
		  }
		tmp_neigh = tmp_neigh->next;
	      }

	    if(tmp_neigh)
	      continue;
	    
	    message_neighbor = olsr_malloc(sizeof(struct hello_neighbor), "Build HELLO 2");
	    
	    message_neighbor->link = UNSPEC_LINK;
	    
	    /*
	     * Calculate neighbor status
	     */
	    /* 
	     * 2.1  If the main address, corresponding to
	     *      L_neighbor_iface_addr, is included in the MPR set:
	     *
	     *            Neighbor Type = MPR_NEIGH
	     */
	    if(neighbor->is_mpr)
	      {
		message_neighbor->status = MPR_NEIGH;
	      }
	    /*
	     *  2.2  Otherwise, if the main address, corresponding to
	     *       L_neighbor_iface_addr, is included in the neighbor set:
	     */
	    
	    /* NOTE:
	     * It is garanteed to be included when come this far
	     * due to the extentions made in the link sensing
	     * regarding main addresses.
	     */
	    else
	      {
		/*
		 *   2.2.1
		 *        if N_status == SYM
		 *
		 *             Neighbor Type = SYM_NEIGH
		 */
		if(neighbor->status == SYM)
		  {
		    message_neighbor->status = SYM_NEIGH;
		  }
		/*
		 *   2.2.2
		 *        Otherwise, if N_status == NOT_SYM
		 *             Neighbor Type = NOT_NEIGH
		 */
		else
		  if(neighbor->status == NOT_SYM)
		    {
		      message_neighbor->status = NOT_NEIGH;		      
		    }
	      }
	    

	    COPY_IP(&message_neighbor->address, &neighbor->neighbor_main_addr);

	    COPY_IP(&message_neighbor->main_address, &neighbor->neighbor_main_addr);
	    
	    olsr_printf(5, "%s           \n ", olsr_ip_to_string(&message_neighbor->address));
	    olsr_printf(5, " status  %d\n", message_neighbor->status);
	    
	    message_neighbor->next=message->neighbors;
	    message->neighbors=message_neighbor;	    
	  }
      }
  

  return 0;
}


/**
 *Build an internal TC package for this
 *node.
 *
 *@param message the tc_message struct to fill with info
 *@return 0
 */
int
olsr_build_tc_packet(struct tc_message *message)
{
  struct tc_mpr_addr        *message_mpr;
  //struct mpr_selector       *mprs;
  olsr_u8_t              index;
  struct neighbor_entry  *entry;
  //struct mpr_selector_hash  *mprs_hash;
  //olsr_u16_t          index;
  olsr_bool entry_added = FALSE;
  struct timeval tmp_timer;

  message->multipoint_relay_selector_address=NULL;
  message->packet_seq_number=0;
 
  message->hop_count = 0;
  message->ttl = MAX_TTL;
  message->ansn = ansn;

  COPY_IP(&message->originator, &main_addr);
  COPY_IP(&message->source_addr, &main_addr);
  

  /* Loop trough all neighbors */  
  for(index=0;index<HASHSIZE;index++)
    {
      for(entry = neighbortable[index].next;
	  entry != &neighbortable[index];
	  entry = entry->next)
	{
	  if(entry->status != SYM)
	    continue;

	  switch(olsr_cnf->tc_redundancy)
	    {
	    case(2):
	      {
		/* 2 = Add all neighbors */
		//printf("\t%s\n", olsr_ip_to_string(&mprs->mpr_selector_addr));
		message_mpr = olsr_malloc(sizeof(struct tc_mpr_addr), "Build TC");
		
		COPY_IP(&message_mpr->address, &entry->neighbor_main_addr);
		message_mpr->next = message->multipoint_relay_selector_address;
		message->multipoint_relay_selector_address = message_mpr;
		entry_added = TRUE;
		
		break;
	      }
	    case(1):
	      {
		/* 1 = Add all MPR selectors and selected MPRs */
		if((entry->is_mpr) ||
		   (olsr_lookup_mprs_set(&entry->neighbor_main_addr) != NULL))
		  {
		    //printf("\t%s\n", olsr_ip_to_string(&mprs->mpr_selector_addr));
		    message_mpr = olsr_malloc(sizeof(struct tc_mpr_addr), "Build TC 2");
		    
		    COPY_IP(&message_mpr->address, &entry->neighbor_main_addr);
		    message_mpr->next = message->multipoint_relay_selector_address;
		    message->multipoint_relay_selector_address = message_mpr;
		    entry_added = TRUE;
		  }
		break;
	      }
	    default:
	      {
		/* 0 = Add only MPR selectors(default) */
		if(olsr_lookup_mprs_set(&entry->neighbor_main_addr) != NULL)
		  {
		    //printf("\t%s\n", olsr_ip_to_string(&mprs->mpr_selector_addr));
		    message_mpr = olsr_malloc(sizeof(struct tc_mpr_addr), "Build TC 3");
		    
		    COPY_IP(&message_mpr->address, &entry->neighbor_main_addr);
		    message_mpr->next = message->multipoint_relay_selector_address;
		    message->multipoint_relay_selector_address = message_mpr;
		    entry_added = TRUE;
		  }
		break;
	      }		
	  
	    } /* Switch */
	} /* For */
    } /* For index */

  if(entry_added)
    {
      sending_tc = TRUE;
    }
  else
    {
      if(sending_tc)
	{
	  /* Send empty TC */
	  olsr_init_timer((olsr_u32_t) (max_tc_vtime*3)*1000, &tmp_timer);
	  olsr_printf(3, "No more MPR selectors - will send empty TCs\n");
	  timeradd(&now, &tmp_timer, &send_empty_tc);

	  sending_tc = FALSE;
	}
    }


  return 0;
}




/**
 *Free the memory allocated for a HELLO packet.
 *
 *@param message the pointer to the packet to erase
 *
 *@return nada
 */
void
olsr_destroy_hello_message(struct hello_message *message)
{
  struct hello_neighbor  *neighbors;
  struct hello_neighbor  *neighbors_tmp;


  neighbors=message->neighbors;
  
  while(neighbors!=NULL)
    {
      neighbors_tmp=neighbors;
      neighbors=neighbors->next;
      free(neighbors_tmp);
    }
}


/**
 *Free the memory allocated for a TC packet.
 *
 *@param message the pointer to the packet to erase
 *
 *@return nada
 */

void
olsr_destroy_tc_message(struct tc_message *message)
{
  struct tc_mpr_addr  *mpr_set;
  struct tc_mpr_addr  *mpr_set_tmp;

  mpr_set=message->multipoint_relay_selector_address;

  while( mpr_set!=NULL)
    {
      mpr_set_tmp=mpr_set;
      mpr_set=mpr_set->next;
      free(mpr_set_tmp);
    }
}



/**
 *Free the memory allocated for a HNA packet.
 *
 *@param message the pointer to the packet to erase
 *
 *@return nada
 */

void
olsr_destroy_hna_message(struct hna_message *message)
{
  struct hna_net_addr  *hna_tmp, *hna_tmp2;

  hna_tmp = message->hna_net;

  while(hna_tmp)
    {
      hna_tmp2 = hna_tmp;
      hna_tmp = hna_tmp->next;
      free(hna_tmp2);
    }
}



/**
 *Free the memory allocated for a MID packet.
 *
 *@param message the pointer to the packet to erase
 *
 *@return nada
 */

void
olsr_destroy_mid_message(struct mid_message *message)
{
  struct mid_alias *tmp_adr, *tmp_adr2;

  tmp_adr = message->mid_addr;

  while(tmp_adr)
    {
      tmp_adr2 = tmp_adr;
      tmp_adr = tmp_adr->next;
      free(tmp_adr2);
    }
}

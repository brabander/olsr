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
 * $Id: process_package.c,v 1.21 2004/11/17 19:21:41 kattemat Exp $
 *
 */


#include "defs.h"
#include "process_package.h"
#include "hysteresis.h"
#include "two_hop_neighbor_table.h"
#include "tc_set.h"
#include "mpr_selector_set.h"
#include "mid_set.h"
#include "olsr.h"
#include "parser.h"
#include "duplicate_set.h"
#include "rebuild_packet.h"


/**
 *Initializing the parser functions we are using
 */
void
olsr_init_package_process()
{
#if defined USE_LINK_QUALITY
  if (olsr_cnf->lq_level == 0)
    {
#endif
      olsr_parser_add_function(&olsr_process_received_hello, HELLO_MESSAGE, 1);
      olsr_parser_add_function(&olsr_process_received_tc, TC_MESSAGE, 1);
#if defined USE_LINK_QUALITY
    }

  else
    {
      olsr_parser_add_function(&olsr_input_lq_hello, LQ_HELLO_MESSAGE, 1);
      olsr_parser_add_function(&olsr_input_lq_tc, LQ_TC_MESSAGE, 1);
    }
#endif
  olsr_parser_add_function(&olsr_process_received_mid, MID_MESSAGE, 1);
  olsr_parser_add_function(&olsr_process_received_hna, HNA_MESSAGE, 1);
}

void
olsr_hello_tap(struct hello_message *message, struct interface *in_if,
               union olsr_ip_addr *from_addr)
{
  struct link_entry         *link;
  struct neighbor_entry     *neighbor;
#if defined USE_LINK_QUALITY
  struct hello_neighbor *walker;
  double saved_lq;
  double rel_lq;
#endif

  /*
   * Update link status
   */
  link = update_link_entry(&in_if->ip_addr, from_addr, message, in_if);

#if defined USE_LINK_QUALITY
  if (olsr_cnf->lq_level > 0)
    {
      // just in case our neighbor has changed its HELLO interval

      olsr_update_packet_loss_hello_int(link, message->htime);

      // find the input interface in the list of neighbor interfaces

      for (walker = message->neighbors; walker != NULL; walker = walker->next)
        if (COMP_IP(&walker->address, &in_if->ip_addr))
          break;

      // the current reference link quality

      saved_lq = link->saved_neigh_link_quality;

      if (saved_lq == 0.0)
        saved_lq = -1.0;

      // memorize our neighbour's idea of the link quality, so that we
      // know the link quality in both directions

      if (walker != NULL)
        link->neigh_link_quality = walker->link_quality;

      else
        link->neigh_link_quality = 0.0;

      // if the link quality has changed by more than 10 percent,
      // print the new link quality table

      rel_lq = link->neigh_link_quality / saved_lq;

      if (rel_lq > 1.1 || rel_lq < 0.9)
        {
          link->saved_neigh_link_quality = link->neigh_link_quality;

          changes_neighborhood = OLSR_TRUE;
          changes_topology = OLSR_TRUE;

          // create a new ANSN

          // XXX - we should check whether we actually
          // announce this neighbour

          changes = OLSR_TRUE;
        }
    }
#endif
  
  neighbor = link->neighbor;

  /*
   * Hysteresis
   */
  if(olsr_cnf->use_hysteresis)
    {
      /* Update HELLO timeout */
      //printf("MESSAGE HTIME: %f\n", message->htime);
      olsr_update_hysteresis_hello(link, message->htime);
    }

  /* Check if we are chosen as MPR */
  if(olsr_lookup_mpr_status(message, in_if))
    /* source_addr is always the main addr of a node! */
    olsr_update_mprs_set(&message->source_addr, (float)message->vtime);



  /* Check willingness */
  if(neighbor->willingness != message->willingness)
    {
      olsr_printf(1, "Willingness for %s changed from %d to %d - UPDATING\n", 
		  olsr_ip_to_string(&neighbor->neighbor_main_addr),
		  neighbor->willingness,
		  message->willingness);
      /*
       *If willingness changed - recalculate
       */
      neighbor->willingness = message->willingness;
      changes_neighborhood = OLSR_TRUE;
      changes_topology = OLSR_TRUE;
    }


  /* Don't register neighbors of neighbors that announces WILL_NEVER */
  if(neighbor->willingness != WILL_NEVER)
    olsr_process_message_neighbors(neighbor, message);

  /* Process changes immedeatly in case of MPR updates */
  olsr_process_changes();

  olsr_destroy_hello_message(message);

  return;
}

/**
 *Processes a received HELLO message. 
 *
 *@param m the incoming OLSR message
 *@return 0 on sucess
 */

void
olsr_process_received_hello(union olsr_message *m, struct interface *in_if, union olsr_ip_addr *from_addr)
{
  struct hello_message      message;

  hello_chgestruct(&message, m);

  olsr_hello_tap(&message, in_if, from_addr);
}

void
olsr_tc_tap(struct tc_message *message, struct interface *in_if,
            union olsr_ip_addr *from_addr, union olsr_message *m)
{
  struct tc_mpr_addr              *mpr;
  struct tc_entry                 *tc_last;

  if(!olsr_check_dup_table_proc(&message->originator, 
                                message->packet_seq_number))
    {
      goto forward;
    }

  olsr_printf(3, "Processing TC from %s\n",
              olsr_ip_to_string(&message->originator));

  /*
   *      If the sender interface (NB: not originator) of this message
   *      is not in the symmetric 1-hop neighborhood of this node, the
   *      message MUST be discarded.
   */

  if(check_neighbor_link(from_addr) != SYM_LINK)
    {
      olsr_printf(2, "Received TC from NON SYM neighbor %s\n",
                  olsr_ip_to_string(from_addr));
      olsr_destroy_tc_message(message);
      return;
    }

  if(olsr_cnf->debug_level > 2)
    {
      mpr = message->multipoint_relay_selector_address;
      olsr_printf(3, "mpr_selector_list:[");

      while(mpr!=NULL)
        {
          olsr_printf(3, "%s:", olsr_ip_to_string(&mpr->address));
          mpr=mpr->next;
        }

      olsr_printf(3, "]\n");
    }

  tc_last = olsr_lookup_tc_entry(&message->originator);
   
  if(tc_last != NULL)
    {
      /* Update entry */

      /* Delete destinations with lower ANSN */
      if(olsr_tc_delete_mprs(tc_last, message))
        changes_topology = OLSR_TRUE; 

      /* Update destinations */
      if(olsr_tc_update_mprs(tc_last, message))
        changes_topology = OLSR_TRUE;

      /* Delete possible empty TC entry */
      if(changes_topology)
        olsr_tc_delete_entry_if_empty(tc_last);
    }

  else
    {
      /*if message is empty then skip it */
      if(message->multipoint_relay_selector_address != NULL)
        {
          /* New entry */
          tc_last = olsr_add_tc_entry(&message->originator);      
	  
          /* Update destinations */
          olsr_tc_update_mprs(tc_last, message);
	  
          changes_topology = OLSR_TRUE;
        }
      else
        {
          olsr_printf(3, "Dropping empty TC from %s\n",
                      olsr_ip_to_string(&message->originator)); 
        }
    }

  /* Process changes */
  //olsr_process_changes();

 forward:

  olsr_forward_message(m, 
                       &message->originator, 
                       message->packet_seq_number, 
                       in_if,
                       from_addr);

  olsr_destroy_tc_message(message);

  return;
}

/**
 *Process a received TopologyControl message
 *
 *
 *@param m the incoming OLSR message
 *@return 0 on success
 */
void
olsr_process_received_tc(union olsr_message *m, struct interface *in_if, union olsr_ip_addr *from_addr)
{ 
  struct tc_message               message;

  tc_chgestruct(&message, m, from_addr);

  olsr_tc_tap(&message, in_if, from_addr, m);
}






/**
 *Process a received(and parsed) MID message
 *For every address check if there is a topology node
 *registered with it and update its addresses.
 *
 *@param m the OLSR message received.
 *@return 1 on success
 */

void
olsr_process_received_mid(union olsr_message *m, struct interface *in_if, union olsr_ip_addr *from_addr)
{
  struct mid_alias *tmp_adr;
  struct mid_message message;


  mid_chgestruct(&message, m);

  /*
  if(COMP_IP(&message.mid_origaddr, &main_addr))
    {
      goto forward;  
    }
  */

  if(!olsr_check_dup_table_proc(&message.mid_origaddr, 
				message.mid_seqno))
    {
      goto forward;
    }
  
  olsr_printf(5, "Processing MID from %s...\n", olsr_ip_to_string(&message.mid_origaddr));

  tmp_adr = message.mid_addr;


  /*
   *      If the sender interface (NB: not originator) of this message
   *      is not in the symmetric 1-hop neighborhood of this node, the
   *      message MUST be discarded.
   */

  if(check_neighbor_link(from_addr) != SYM_LINK)
    {
      olsr_printf(2, "Received MID from NON SYM neighbor %s\n", olsr_ip_to_string(from_addr));
      olsr_destroy_mid_message(&message);
      return;
    }



  /* Update the timeout of the MID */
  olsr_update_mid_table(&message.mid_origaddr, (float)message.vtime);

  while(tmp_adr)
    {
      if(!mid_lookup_main_addr(&tmp_adr->alias_addr))
	{
	  olsr_printf(1, "MID new: (%s, ", olsr_ip_to_string(&message.mid_origaddr));
	  olsr_printf(1, "%s)\n", olsr_ip_to_string(&tmp_adr->alias_addr));
	  insert_mid_alias(&message.mid_origaddr, &tmp_adr->alias_addr, (float)message.vtime);
	}


      tmp_adr = tmp_adr->next;
    } 
  
  /*Update topology if neccesary*/
  //olsr_process_changes();

 forward:
  
  olsr_forward_message(m, 
		       &message.mid_origaddr, 
		       message.mid_seqno, 
		       in_if,
		       from_addr);
  olsr_destroy_mid_message(&message);

  return;
}





/**
 *Process incoming HNA message.
 *Forwards the message if that is to be done.
 *
 *@param m the incoming OLSR message
 *the OLSR message.
 *@return 1 on success
 */

void
olsr_process_received_hna(union olsr_message *m, struct interface *in_if, union olsr_ip_addr *from_addr)
{
  struct hna_net_addr  *hna_tmp;
  struct  hna_message message;

  //printf("Processing HNA\n");

  hna_chgestruct(&message, m);
  

  /* Process message */	  
  /*
  if(COMP_IP(&message.originator, &main_addr)) 
    {
      goto forward;
    }
  */

  if(!olsr_check_dup_table_proc(&message.originator, 
				message.packet_seq_number))
    {
      goto forward;
    }




  hna_tmp = message.hna_net;



  /*
   *      If the sender interface (NB: not originator) of this message
   *      is not in the symmetric 1-hop neighborhood of this node, the
   *      message MUST be discarded.
   */


  if(check_neighbor_link(from_addr) != SYM_LINK)
    {
      olsr_printf(2, "Received HNA from NON SYM neighbor %s\n", olsr_ip_to_string(from_addr));
      olsr_destroy_hna_message(&message);
      return;
    }

  while(hna_tmp)
    {
      olsr_update_hna_entry(&message.originator, &hna_tmp->net, &hna_tmp->netmask, (float)message.vtime); 
      
      hna_tmp = hna_tmp->next;
    }
  
  /*Update topology if neccesary*/
  //olsr_process_changes();

 forward:
  olsr_forward_message(m, 
		       &message.originator, 
		       message.packet_seq_number, 
		       in_if,
		       from_addr);
  olsr_destroy_hna_message(&message);

  return;
}







/**
 *Processes an list of neighbors from an incoming HELLO message.
 *@param neighbor the neighbor who sendt the message.
 *@param message the HELLO message
 *@return nada
 */
void
olsr_process_message_neighbors(struct neighbor_entry *neighbor,
                               struct hello_message *message)
{
  struct hello_neighbor        *message_neighbors;
  struct neighbor_2_list_entry *two_hop_neighbor_yet;
  struct neighbor_2_entry      *two_hop_neighbor;
  union olsr_ip_addr           *neigh_addr;
#if defined USE_LINK_QUALITY
  struct neighbor_list_entry *walker;
  struct link_entry *link;
#endif

  for(message_neighbors = message->neighbors;
      message_neighbors != NULL;
      message_neighbors = message_neighbors->next)
    {
      /*
       *check all interfaces
       *so that we don't add ourselves to the
       *2 hop list
       *IMPORTANT!
       */
      if(if_ifwithaddr(&message_neighbors->address) != NULL)
        continue;

      /* Get the main address */

      neigh_addr = mid_lookup_main_addr(&message_neighbors->address);

      if (neigh_addr != NULL)
        COPY_IP(&message_neighbors->address, neigh_addr);
      
      if(((message_neighbors->status == SYM_NEIGH) ||
          (message_neighbors->status == MPR_NEIGH)))
        {
          //printf("\tProcessing %s\n", olsr_ip_to_string(&message_neighbors->address));
          //printf("\tMain addr: %s\n", olsr_ip_to_string(neigh_addr));
	  
          two_hop_neighbor_yet =
            olsr_lookup_my_neighbors(neighbor,
                                     &message_neighbors->address);

          if (two_hop_neighbor_yet != NULL)
            {
              /* Updating the holding time for this neighbor */
              olsr_get_timestamp((olsr_u32_t)message->vtime * 1000,
                                 &two_hop_neighbor_yet->neighbor_2_timer);

              two_hop_neighbor = two_hop_neighbor_yet->neighbor_2;
            }
          else
            {
              two_hop_neighbor =
                olsr_lookup_two_hop_neighbor_table(&message_neighbors->address);
              if (two_hop_neighbor == NULL)
                {
                  //printf("Adding 2 hop neighbor %s\n\n", olsr_ip_to_string(&message_neighbors->address)); 

                  changes_neighborhood = OLSR_TRUE;
                  changes_topology = OLSR_TRUE;

                  two_hop_neighbor =
                    olsr_malloc(sizeof(struct neighbor_2_entry),
                                "Process HELLO");
		  
                  two_hop_neighbor->neighbor_2_nblist.next =
                    &two_hop_neighbor->neighbor_2_nblist;

                  two_hop_neighbor->neighbor_2_nblist.prev =
                    &two_hop_neighbor->neighbor_2_nblist;

                  two_hop_neighbor->neighbor_2_pointer = 0;
		  
                  COPY_IP(&two_hop_neighbor->neighbor_2_addr,
                          &message_neighbors->address);

                  olsr_insert_two_hop_neighbor_table(two_hop_neighbor);

                  olsr_linking_this_2_entries(neighbor, two_hop_neighbor,
                                              (float)message->vtime);
                }
              else
                {
                  /*
                    linking to this two_hop_neighbor entry
                  */	
                  changes_neighborhood = OLSR_TRUE;
                  changes_topology = OLSR_TRUE;
		  
                  olsr_linking_this_2_entries(neighbor, two_hop_neighbor,
                                              (float)message->vtime); 
                }
            }
#if defined USE_LINK_QUALITY
          if (olsr_cnf->lq_level > 0)
            {
              link = olsr_neighbor_best_link(&neighbor->neighbor_main_addr);

              // loop through the one-hop neighbors that see this
              // two hop neighbour

              for (walker = two_hop_neighbor->neighbor_2_nblist.next;
                   walker != &two_hop_neighbor->neighbor_2_nblist;
                   walker = walker->next)
                {
                  // have we found the one-hop neighbor that sent the
                  // HELLO message that we're current processing?

                  if (walker->neighbor == neighbor)
                    {
                      double saved_lq, rel_lq;

                      // saved previous total link quality

                      saved_lq = walker->saved_full_link_quality;

                      if (saved_lq == 0.0)
                        saved_lq = -1.0;

                      // total link quality = link quality between us
                      // and our one-hop neighbor x link quality between
                      // our one-hop neighbor and the two-hop neighbor

                      walker->full_link_quality =
                        link->neigh_link_quality *
                        message_neighbors->neigh_link_quality;

                      // if the link quality has changed by more than 10
                      // percent, signal

                      rel_lq = walker->full_link_quality / saved_lq;

                      if (rel_lq > 1.1 || rel_lq < 0.9)
                        {
                          walker->saved_full_link_quality =
                            walker->full_link_quality;

                          changes_neighborhood = OLSR_TRUE;
                          changes_topology = OLSR_TRUE;
                        }
                    }
                }
            }
#endif
        }
    }
}








/**
 *Links a one-hop neighbor with a 2-hop neighbor.
 *
 *@param neighbor the 1-hop neighbor
 *@param two_hop_neighbor the 2-hop neighbor
 *@return nada
 */
void
olsr_linking_this_2_entries(struct neighbor_entry *neighbor,struct neighbor_2_entry *two_hop_neighbor, float vtime)
{
  struct neighbor_list_entry    *list_of_1_neighbors;
  struct neighbor_2_list_entry  *list_of_2_neighbors;

  list_of_1_neighbors = olsr_malloc(sizeof(struct neighbor_list_entry), "Link entries 1");

  list_of_2_neighbors = olsr_malloc(sizeof(struct neighbor_2_list_entry), "Link entries 2");

  list_of_1_neighbors->neighbor = neighbor;

#if defined USE_LINK_QUALITY
  list_of_1_neighbors->full_link_quality = 0.0;
  list_of_1_neighbors->saved_full_link_quality = 0.0;
#endif

  /* Queue */
  two_hop_neighbor->neighbor_2_nblist.next->prev = list_of_1_neighbors;
  list_of_1_neighbors->next = two_hop_neighbor->neighbor_2_nblist.next;
  two_hop_neighbor->neighbor_2_nblist.next = list_of_1_neighbors;
  list_of_1_neighbors->prev = &two_hop_neighbor->neighbor_2_nblist;


  list_of_2_neighbors->neighbor_2 = two_hop_neighbor;
  
  olsr_get_timestamp((olsr_u32_t) vtime*1000, &list_of_2_neighbors->neighbor_2_timer);

  /* Queue */
  neighbor->neighbor_2_list.next->prev = list_of_2_neighbors;
  list_of_2_neighbors->next = neighbor->neighbor_2_list.next;
  neighbor->neighbor_2_list.next = list_of_2_neighbors;
  list_of_2_neighbors->prev = &neighbor->neighbor_2_list;
  
  /*increment the pointer counter*/
  two_hop_neighbor->neighbor_2_pointer++;
}






/**
 *Check if a hello message states this node as a MPR.
 *
 *@param message the message to check
 *@param n_link the buffer to put the link status in
 *@param n_status the buffer to put the status in
 *
 *@return 1 if we are selected as MPR 0 if not
 */
int
olsr_lookup_mpr_status(struct hello_message *message, struct interface *in_if)
{
  
  struct hello_neighbor  *neighbors;

  neighbors=message->neighbors;
  
  while(neighbors!=NULL)
    {  
      //printf("(linkstatus)Checking %s ",olsr_ip_to_string(&neighbors->address));
      //printf("against %s\n",olsr_ip_to_string(&main_addr));


    if(olsr_cnf->ip_version == AF_INET)
      {	
	/* IPv4 */  
	if(COMP_IP(&neighbors->address, &in_if->ip_addr))
	  {
	    //printf("ok");
	    if((neighbors->link == SYM_LINK) && (neighbors->status == MPR_NEIGH))
	      return 1;
	    
	    return 0;
	  }
      }
    else
      {	
	/* IPv6 */  
	if(COMP_IP(&neighbors->address, &in_if->int6_addr.sin6_addr))
	  {
	    //printf("ok");
	    if((neighbors->link == SYM_LINK) && (neighbors->status == MPR_NEIGH))
	      return 1;
	    
	    return 0;
	  }
      }
 
      neighbors = neighbors->next; 
    }

  /* Not found */
  return 0;
}

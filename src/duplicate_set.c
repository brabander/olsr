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
 */



#include "defs.h"
#include "duplicate_set.h"
#include "olsr.h"
#include "scheduler.h"

/**
 *Initialize the duplicate table entrys
 *
 *@return nada
 */
void
olsr_init_duplicate_table()
{
  int i;

  olsr_printf(3, "Initializing duplicatetable - hashsize %d\n", HASHSIZE);

  /* Initialize duplicate set holding time */
  olsr_init_timer((olsr_u32_t) (dup_hold_time*1000), &hold_time_duplicate);

  /* Since the holdingtime is rather large for duplicate
   * entries the timeoutfunction is only ran every 2 seconds
   */
  olsr_register_scheduler_event(&olsr_time_out_duplicate_table, 2, 0, NULL);
  
  for(i = 0; i < HASHSIZE; i++)
    {
      dup_set[i].next = &dup_set[i];
      dup_set[i].prev = &dup_set[i];
    }
}


/**
 *Add an entry to the duplicate set. The set is not checked
 *for duplicate entries.
 *
 *@param originator IP address of the sender of the message
 *@param seqno seqno of the message
 *
 *@return positive on success
 */
struct dup_entry *
olsr_add_dup_entry(union olsr_ip_addr *originator, olsr_u16_t seqno)
{
  olsr_u32_t hash;
  struct dup_entry *new_dup_entry;


  /* Hash the senders address */
  hash = olsr_hashing(originator);

  new_dup_entry = olsr_malloc(sizeof(struct dup_entry), "New dup entry");

  /* Address */
  COPY_IP(&new_dup_entry->addr, originator);
  /* Seqno */
  new_dup_entry->seqno = seqno;
  /* Set timer */
  timeradd(&now, &hold_time_duplicate, &new_dup_entry->timer);
  /* Interfaces */
  new_dup_entry->ifaces = NULL;
  /* Forwarded */
  new_dup_entry->forwarded = 0;

  /* Insert into set */
  QUEUE_ELEM(dup_set[hash], new_dup_entry);
  /*
  dup_set[hash].next->prev = new_dup_entry;
  new_dup_entry->next = dup_set[hash].next;
  dup_set[hash].next = new_dup_entry;
  new_dup_entry->prev = &dup_set[hash];
  */
  return new_dup_entry;
}


/**
 * Check wether or not a message should be processed
 *
 */
int
olsr_check_dup_table_proc(union olsr_ip_addr *originator, olsr_u16_t seqno)
{
  olsr_u32_t hash;
  struct dup_entry *tmp_dup_table;

  /* Hash the senders address */
  hash = olsr_hashing(originator);

  /* Check for entry */
  for(tmp_dup_table = dup_set[hash].next;
      tmp_dup_table != &dup_set[hash];
      tmp_dup_table = tmp_dup_table->next)
    {
      if(COMP_IP(&tmp_dup_table->addr, originator) &&
	 (tmp_dup_table->seqno == seqno))
	{
	  return 0;
	}
    }

  return 1;
}



/**
 * Check wether or not a message should be forwarded
 *
 */
int
olsr_check_dup_table_fwd(union olsr_ip_addr *originator, 
		     olsr_u16_t seqno,
		     union olsr_ip_addr *int_addr)
{
  olsr_u32_t hash;
  struct dup_entry *tmp_dup_table;
  struct dup_iface *tmp_dup_iface;

  /* Hash the senders address */
  hash = olsr_hashing(originator);

  /* Check for entry */
  for(tmp_dup_table = dup_set[hash].next;
      tmp_dup_table != &dup_set[hash];
      tmp_dup_table = tmp_dup_table->next)
    {
      if(COMP_IP(&tmp_dup_table->addr, originator) &&
	 (tmp_dup_table->seqno == seqno))
	{
	  /* Check retransmitted */
	  if(tmp_dup_table->forwarded)
	    return 0;
	  /* Check for interface */
	  tmp_dup_iface = tmp_dup_table->ifaces;
	  while(tmp_dup_iface)
	    {
	      if(COMP_IP(&tmp_dup_iface->addr, int_addr))
		return 0;
	      
	      tmp_dup_iface = tmp_dup_iface->next;
	    }
	}
    }
  

  return 1;
}




/**
 *Delete and dequeue a duplicate entry
 *
 *@param entry the entry to delete
 *
 */
void
olsr_del_dup_entry(struct dup_entry *entry)
{
  struct dup_iface *tmp_iface, *del_iface;

  tmp_iface = entry->ifaces;

  /* Free interfaces */
  while(tmp_iface)
    {
      del_iface = tmp_iface;
      tmp_iface = tmp_iface->next;
      free(del_iface);
    }

  /* Dequeue */
  DEQUEUE_ELEM(entry);
  //entry->prev->next = entry->next;
  //entry->next->prev = entry->prev;

  /* Free entry */
  free(entry);

}



void
olsr_time_out_duplicate_table()
{
  int i;
  struct dup_entry *tmp_dup_table, *entry_to_delete;

  for(i = 0; i < HASHSIZE; i++)
    {      
      tmp_dup_table = dup_set[i].next;

      while(tmp_dup_table != &dup_set[i])
	{
	  if(TIMED_OUT(&tmp_dup_table->timer))
	    {

#ifdef DEBUG
	      olsr_printf(5, "DUP TIMEOUT[%s] s: %d\n", 
		          olsr_ip_to_string(&tmp_dup_table->addr),
		          tmp_dup_table->seqno);
#endif

	      entry_to_delete = tmp_dup_table;
	      tmp_dup_table = tmp_dup_table->next;

	      olsr_del_dup_entry(entry_to_delete);
	    }
	  else
	    {
	      tmp_dup_table = tmp_dup_table->next;
	    }
	}
    }
}




int
olsr_update_dup_entry(union olsr_ip_addr *originator, 
		      olsr_u16_t seqno, 
		      union olsr_ip_addr *iface)
{
  olsr_u32_t hash;
  struct dup_entry *tmp_dup_table;
  struct dup_iface *new_iface;

  /* Hash the senders address */
  hash = olsr_hashing(originator);


  /* Check for entry */
  for(tmp_dup_table = dup_set[hash].next;
      tmp_dup_table != &dup_set[hash];
      tmp_dup_table = tmp_dup_table->next)
    {
      if(COMP_IP(&tmp_dup_table->addr, originator) &&
	 (tmp_dup_table->seqno == seqno))
	{
	  break;
	}
    }

  if(tmp_dup_table == &dup_set[hash])
    /* Did not find entry - create it */
    tmp_dup_table = olsr_add_dup_entry(originator, seqno);
  
  /* 0 for now */
  tmp_dup_table->forwarded = 0;
  
  new_iface = olsr_malloc(sizeof(struct dup_iface), "New dup iface");

  COPY_IP(&new_iface->addr, iface);
  new_iface->next = tmp_dup_table->ifaces;
  tmp_dup_table->ifaces = new_iface;
  
  /* Set timer */
  timeradd(&now, &hold_time_duplicate, &tmp_dup_table->timer);
  
  return 1;
}




int
olsr_set_dup_forward(union olsr_ip_addr *originator, 
		     olsr_u16_t seqno)
{
  olsr_u32_t hash;
  struct dup_entry *tmp_dup_table;

  /* Hash the senders address */
  hash = olsr_hashing(originator);

  /* Check for entry */
  for(tmp_dup_table = dup_set[hash].next;
      tmp_dup_table != &dup_set[hash];
      tmp_dup_table = tmp_dup_table->next)
    {
      if(COMP_IP(&tmp_dup_table->addr, originator) &&
	 (tmp_dup_table->seqno == seqno))
	{
	  break;
	}
    }

  if(tmp_dup_table == &dup_set[hash])
    /* Did not find entry !! */
    return 0;
  
#ifdef DEBUG
  olsr_printf(3, "Setting DUP %s/%d forwarded\n", olsr_ip_to_string(&tmp_dup_table->addr), seqno);
#endif

  /* Set forwarded */
  tmp_dup_table->forwarded = 1;
  
  /* Set timer */
  timeradd(&now, &hold_time_duplicate, &tmp_dup_table->timer);
  
  return 1;
}







void
olsr_print_duplicate_table()
{
  int i;
  struct dup_entry *tmp_dup_table;

  printf("\nDUP TABLE:\n");

  for(i = 0; i < HASHSIZE; i++)
    {      
      tmp_dup_table = dup_set[i].next;
      
      //printf("Timeout %d %d\n", i, j);
      while(tmp_dup_table != &dup_set[i])
	{
	  printf("[%s] s: %d\n", 
		 olsr_ip_to_string(&tmp_dup_table->addr),
		 tmp_dup_table->seqno);
	  tmp_dup_table = tmp_dup_table->next;
	}
    }
printf("\n");

}

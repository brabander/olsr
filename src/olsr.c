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
 * $Id: olsr.c,v 1.17 2004/11/05 20:58:10 tlopatic Exp $
 *
 */

/**
 * All these functions are global
 */

#include "defs.h"
#include "olsr.h"
#include "link_set.h"
#include "two_hop_neighbor_table.h"
#include "tc_set.h"
#include "duplicate_set.h"
#include "mpr_selector_set.h"
#include "mid_set.h"
#include "mpr.h"
#if defined USE_LINK_QUALITY
#include "lq_mpr.h"
#endif
#include "scheduler.h"
#include "generate_msg.h"
#include "apm.h"

#include <stdarg.h>
#include <signal.h>


/**
 *Checks if a timer has timed out.
 */


/**
 *Initiates a "timer", wich is a timeval structure,
 *with the value given in time_value.
 *@param time_value the value to initialize the timer with
 *@param hold_timer the timer itself
 *@return nada
 */
inline void
olsr_init_timer(olsr_u32_t time_value, struct timeval *hold_timer)
{ 
  olsr_u16_t  time_value_sec;
  olsr_u16_t  time_value_msec;

  time_value_sec = time_value/1000;
  time_value_msec = time_value-(time_value_sec*1000);

  hold_timer->tv_sec = time_value_sec;
  hold_timer->tv_usec = time_value_msec*1000;   
}





/**
 *Generaties a timestamp a certain number of milliseconds
 *into the future.
 *
 *@param time_value how many milliseconds from now
 *@param hold_timer the timer itself
 *@return nada
 */
inline void
olsr_get_timestamp(olsr_u32_t delay, struct timeval *hold_timer)
{ 
  hold_timer->tv_sec = now.tv_sec + delay / 1000;
  hold_timer->tv_usec = now.tv_usec + (delay % 1000) * 1000;
  
  if (hold_timer->tv_usec > 1000000)
    {
      hold_timer->tv_sec++;
      hold_timer->tv_usec -= 1000000;
    }
}



/**
 *Initialize the message sequence number as a random value
 */
void
init_msg_seqno()
{
  message_seqno = random() & 0xFFFF;
}

/**
 * Get and increment the message sequence number
 *
 *@return the seqno
 */
inline olsr_u16_t
get_msg_seqno()
{
  return message_seqno++;
}


void
register_pcf(int (*f)(int, int, int))
{
  struct pcf *new_pcf;

  olsr_printf(1, "Registering pcf function\n");

  new_pcf = olsr_malloc(sizeof(struct pcf), "New PCF");

  new_pcf->function = f;
  new_pcf->next = pcf_list;
  pcf_list = new_pcf;

}


/**
 *Process changes in neighborhood or/and topology.
 *Re-calculates the neighborhooh/topology if there
 *are any updates - then calls the right functions to
 *update the routing table.
 *@return 0
 */
inline void
olsr_process_changes()
{

  struct pcf *tmp_pc_list;

#ifdef DEBUG
  if(changes_neighborhood)
    olsr_printf(3, "CHANGES IN NEIGHBORHOOD\n");
  if(changes_topology)
    olsr_printf(3, "CHANGES IN TOPOLOGY\n");
  if(changes_hna)
    olsr_printf(3, "CHANGES IN HNA\n");  
#endif

  if(!changes_neighborhood &&
     !changes_topology &&
     !changes_hna)
    return;

  if(changes_neighborhood)
    {
      /* Calculate new mprs, HNA and routing table */
#if !defined USE_LINK_QUALITY
      olsr_calculate_mpr();
#else
      olsr_calculate_lq_mpr();
#endif
      olsr_calculate_routing_table();
      olsr_calculate_hna_routes();

      goto process_pcf;  
    }
  
  if(changes_topology)
    {
      /* calculate the routing table and HNA */
      olsr_calculate_routing_table();
      olsr_calculate_hna_routes();

      goto process_pcf;  
    }

  if(changes_hna)
    {
      /* Update HNA routes */
      olsr_calculate_hna_routes();

      goto process_pcf;
    }
  
 process_pcf:

  for(tmp_pc_list = pcf_list; 
      tmp_pc_list != NULL;
      tmp_pc_list = tmp_pc_list->next)
    {
      tmp_pc_list->function(changes_neighborhood,
			    changes_topology,
			    changes_hna);
    }

  changes_neighborhood = OLSR_FALSE;
  changes_topology = OLSR_FALSE;
  changes_hna = OLSR_FALSE;


  return;
}





/**
 *Initialize all the tables used(neighbor,
 *topology, MID,  HNA, MPR, dup).
 *Also initalizes other variables
 */
void
olsr_init_tables()
{
  
  changes_topology = OLSR_FALSE;
  changes_neighborhood = OLSR_FALSE;
  changes_hna = OLSR_FALSE;

  /* Initialize link set */
  olsr_init_link_set();

  /* Initialize duplicate table */
  olsr_init_duplicate_table();

  /* Initialize neighbor table */
  olsr_init_neighbor_table();

  /* Initialize routing table */
  olsr_init_routing_table();

  /* Initialize two hop table */
  olsr_init_two_hop_table();

  /* Initialize old route table */
  olsr_init_old_table();

  /* Initialize topology */
  olsr_init_tc();

  /* Initialize mpr selector table */
  olsr_init_mprs_set();

  /* Initialize MID set */
  olsr_init_mid_set();

  /* Initialize HNA set */
  olsr_init_hna_set();

  /* Initialize ProcessChanges list */
  ptf_list = NULL;
  
}






/**
 *Check if a message is to be forwarded and forward
 *it if necessary.
 *
 *@param m the OLSR message recieved
 *@param originator the originator of this message
 *@param seqno the seqno of the message
 *
 *@returns positive if forwarded
 */
int
olsr_forward_message(union olsr_message *m, 
		     union olsr_ip_addr *originator, 
		     olsr_u16_t seqno,
		     struct interface *in_if, 
		     union olsr_ip_addr *from_addr)
{
  union olsr_ip_addr *src;
  struct neighbor_entry *neighbor;
  int msgsize;
  struct interface *ifn;


  if(!olsr_check_dup_table_fwd(originator, seqno, &in_if->ip_addr))
    {
#ifdef DEBUG
      olsr_printf(3, "Message already forwarded!\n");
#endif
      return 0;
    }

  /* Lookup sender address */
  if(!(src = mid_lookup_main_addr(from_addr)))
    src = from_addr;


  if(NULL == (neighbor=olsr_lookup_neighbor_table(src)))
    return 0;

  if(neighbor->status != SYM)
    return 0;

  /* Update duplicate table interface */
  olsr_update_dup_entry(originator, seqno, &in_if->ip_addr);

  
  /* Check MPR */
  if(olsr_lookup_mprs_set(src) == NULL)
    {
#ifdef DEBUG
      olsr_printf(5, "Forward - sender %s not MPR selector\n", olsr_ip_to_string(src));
#endif
      return 0;
    }


  /* Treat TTL hopcnt */
  if(olsr_cnf->ip_version == AF_INET)
    {
      /* IPv4 */
      m->v4.hopcnt++;
      m->v4.ttl--; 
    }
  else
    {
      /* IPv6 */
      m->v6.hopcnt++;
      m->v6.ttl--; 
    }



  /* Update dup forwarded */
  olsr_set_dup_forward(originator, seqno);

  /* Update packet data */


  msgsize = ntohs(m->v4.olsr_msgsize);

  /* looping trough interfaces */
  for (ifn = ifnet; ifn ; ifn = ifn->int_next) 
    { 
      if(net_output_pending(ifn))
	{
	  /*
	   * Check if message is to big to be piggybacked
	   */
	  if(net_outbuffer_push(ifn, (olsr_u8_t *)m, msgsize) != msgsize)
	    {
	      /* Send */
	      net_output(ifn);
	      /* Buffer message */
	      set_buffer_timer(ifn);
	      
	      if(net_outbuffer_push(ifn, (olsr_u8_t *)m, msgsize) != msgsize)
		{
		  olsr_printf(1, "Received message to big to be forwarded in %s(%d bytes)!", ifn->int_name, msgsize);
		  olsr_syslog(OLSR_LOG_ERR, "Received message to big to be forwarded on %s(%d bytes)!", ifn->int_name, msgsize);
		}

	    }
	}
      
      else
	{
	  /* No forwarding pending */
	  set_buffer_timer(ifn);
	  
	  if(net_outbuffer_push(ifn, (olsr_u8_t *)m, msgsize) != msgsize)
	    {
	      olsr_printf(1, "Received message to big to be forwarded in %s(%d bytes)!", ifn->int_name, msgsize);
	      olsr_syslog(OLSR_LOG_ERR, "Received message to big to be forwarded on %s(%d bytes)!", ifn->int_name, msgsize);
	    }
	}
    }

  return 1;

}


void
set_buffer_timer(struct interface *ifn)
{
  float jitter;
  struct timeval jittertimer;
      
  /* Set timer */
  jitter = (float) random()/RAND_MAX;
  jitter *= max_jitter;

  olsr_init_timer((olsr_u32_t) (jitter*1000), &jittertimer);

  timeradd(&now, &jittertimer, &fwdtimer[ifn->if_nr]);

}



void
olsr_init_willingness()
{
  if(olsr_cnf->willingness_auto)
    olsr_register_scheduler_event(&olsr_update_willingness, NULL, will_int, will_int, NULL);
}

void
olsr_update_willingness(void *foo)
{
  int tmp_will;

  tmp_will = olsr_cnf->willingness;

  /* Re-calculate willingness */
  olsr_cnf->willingness = olsr_calculate_willingness();

  if(tmp_will != olsr_cnf->willingness)
    {
      olsr_printf(1, "Local willingness updated: old %d new %d\n", tmp_will, olsr_cnf->willingness);
    }
}


/**
 *Calculate this nodes willingness to act as a MPR
 *based on either a fixed value or the power status
 *of the node using APM
 *
 *@return a 8bit value from 0-7 representing the willingness
 */

olsr_u8_t
olsr_calculate_willingness()
{
  struct olsr_apm_info ainfo;

  /* If fixed willingness */
  if(!olsr_cnf->willingness_auto)
    return olsr_cnf->willingness;

#warning CHANGES IN THE apm INTERFACE(0.4.8)!

  if(apm_read(&ainfo) < 1)
    return WILL_DEFAULT;

  apm_printinfo(&ainfo);

  /* If AC powered */
  if(ainfo.ac_line_status == OLSR_AC_POWERED)
    return 6;

  /* If battery powered 
   *
   * juice > 78% will: 3
   * 78% > juice > 26% will: 2
   * 26% > juice will: 1
   */
  return (ainfo.battery_percentage / 26);
}



/**
 *Termination function to be called whenever a error occures
 *that requires the daemon to terminate
 *
 *@param msg the message to write to the syslog and possibly stdout
 */

void
olsr_exit(const char *msg, int val)
{
  olsr_printf(1, "OLSR EXIT: %s\n", msg);
  olsr_syslog(OLSR_LOG_ERR, "olsrd exit: %s\n", msg);
  fflush(stdout);
  exit_value = val;

  raise(SIGTERM);
}


/**
 *Wrapper for malloc(3) that does error-checking
 *
 *@param size the number of bytes to allocalte
 *@param caller a string identifying the caller for
 *use in error messaging
 *
 *@return a void pointer to the memory allocated
 */
void *
olsr_malloc(size_t size, const char *id)
{
  void *ptr;

  if((ptr = malloc(size)) == 0) 
    {
      olsr_printf(1, "OUT OF MEMORY: %s\n", strerror(errno));
      olsr_syslog(OLSR_LOG_ERR, "olsrd: out of memory!: %m\n");
      olsr_exit((char *)id, EXIT_FAILURE);
    }
  return ptr;
}


/**
 *Wrapper for printf that prints to a specific
 *debuglevel upper limit
 *
 */

inline int
olsr_printf(int loglevel, char *format, ...)
{
  va_list arglist;

  va_start(arglist, format);

  if(loglevel <= olsr_cnf->debug_level)
    {
      vprintf(format, arglist);
    }

  va_end(arglist);

  return 0;
}

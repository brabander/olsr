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

#include "generate_msg.h"
#include "defs.h"
#include "scheduler.h"
#include "build_msg.h"
#include "packet.h"
#include "mantissa.h"
#include "link_set.h"

/*
 * Infomation repositiries
 */
#include "hna_set.h"
#include "mid_set.h"
#include "tc_set.h"
#include "mpr_selector_set.h"
#include "duplicate_set.h"
#include "neighbor_table.h"


/**
 * Function that sets the HELLO emission interval and
 * calculates all other nessecarry values such as
 * the VTIME value to emit in messages.
 * The message generation function is removed
 * and re-registered
 *
 *@param interval the new emission interval
 *
 *@return negative on error
 */
int
olsr_set_hello_interval(float interval)
{
  if(interval < polling_int)
    return -1;

  /* Unregister function */
  olsr_remove_scheduler_event(&generate_hello, hello_int, 0, NULL);

  hello_int = interval;

  /* Re-calculate holdingtime to announce */
  neighbor_hold_time = hello_int * neighbor_timeout_mult;

  olsr_printf(3, "Setting HELLO interval to %0.2f timeout %0.2f\n", interval, neighbor_hold_time);

  hello_vtime = double_to_me(neighbor_hold_time);

  htime = double_to_me(hello_int);

  olsr_init_timer((olsr_u32_t) (neighbor_hold_time*1000), &hold_time_neighbor);


  /* Reregister function */
  olsr_register_scheduler_event(&generate_hello, hello_int, 0, NULL);

  /*
   *Jitter according to the RFC
   */
  max_jitter = hello_int / 4;

  return 1;
}

/**
 * Function that sets the HELLO emission interval 
 * for non-wireless interfaces and
 * calculates all other nessecarry values such as
 * the VTIME value to emit in messages.
 * The message generation function is removed
 * and re-registered
 *
 *@param interval the new emission interval
 *
 *@return negative on error
 */
int
olsr_set_hello_nw_interval(float interval)
{

  if(interval < polling_int)
    return -1;


  /* Unregister function */
  olsr_remove_scheduler_event(&generate_hello_nw, hello_int_nw, 0, NULL);

  hello_int_nw = interval;

  /* Re-calculate holdingtime to announce */
  neighbor_hold_time_nw = hello_int_nw * neighbor_timeout_mult_nw;

  olsr_printf(3, "Setting HELLO NW interval to %0.2f hold time %0.2f\n", interval, neighbor_hold_time_nw);

  hello_nw_vtime = double_to_me(neighbor_hold_time_nw);

  htime_nw = double_to_me(hello_int_nw);

  olsr_init_timer((olsr_u32_t) (neighbor_hold_time_nw*1000), &hold_time_neighbor_nw);


  /* Reregister function */
  olsr_register_scheduler_event(&generate_hello_nw, hello_int_nw, 0, NULL);

  return 1;
}


/**
 * Function that sets the TC emission interval and
 * calculates all other nessecarry values such as
 * the VTIME value to emit in messages.
 * The message generation function is removed
 * and re-registered
 *
 *@param interval the new emission interval
 *
 *@return negative on error
 */

int
olsr_set_tc_interval(float interval)
{
  if(interval < polling_int)
    return -1;
  /* Unregister function */
  olsr_remove_scheduler_event(&generate_tc, tc_int, 0, NULL);

  tc_int = interval;

  /* Re-calculate holdingtime to announce */
  topology_hold_time = tc_int * topology_timeout_mult;

  olsr_printf(3, "Setting TC interval to %0.2f timeout %0.2f\n", interval, topology_hold_time);

  tc_vtime = double_to_me(topology_hold_time);

  /* Reregister function */
  olsr_register_scheduler_event(&generate_tc, tc_int, 0, &changes);

  return 1;
}


/**
 * Function that sets the MID emission interval and
 * calculates all other nessecarry values such as
 * the VTIME value to emit in messages.
 * The message generation function is removed
 * and re-registered
 *
 *@param interval the new emission interval
 *
 *@return negative on error
 */

int
olsr_set_mid_interval(float interval)
{
  if(interval < polling_int)
    return -1;

  if(nbinterf > 1)
    return 0;

  /* Unregister function */
  olsr_remove_scheduler_event(&generate_mid, mid_int, 0, NULL);

  mid_int = interval;

  /* Re-calculate holdingtime to announce */
  mid_hold_time = mid_int * mid_timeout_mult;

  olsr_printf(3, "Setting MID interval to %0.2f timeout %0.2f\n", interval, mid_hold_time);

  mid_vtime = double_to_me(mid_hold_time);

  /* Reregister function */
  olsr_register_scheduler_event(&generate_mid, mid_int, mid_int/2, NULL);

  return 1;
}


/**
 * Function that sets the HNA emission interval and
 * calculates all other nessecarry values such as
 * the VTIME value to emit in messages.
 * The message generation function is removed
 * and re-registered
 *
 *@param interval the new emission interval
 *
 *@return negative on error
 */

int
olsr_set_hna_interval(float interval)
{
  if(interval < polling_int)
    return -1;

  /* Unregister function */
  olsr_remove_scheduler_event(&generate_hna, hna_int, 0, NULL);

  hna_int = interval;

  /* Re-calculate holdingtime to announce */
  hna_hold_time = hna_int * hna_timeout_mult;

  olsr_printf(3, "Setting HNA interval to %0.2f timeout %0.2f\n", interval, hna_hold_time);

  hna_vtime = double_to_me(hna_hold_time);

  olsr_register_scheduler_event(&generate_hna, hna_int, hna_int/2, NULL);

  return 1;
}





void
generate_hello()
{
  struct interface *ifn;
  struct hello_message hellopacket;

  /* looping trough interfaces */
  for (ifn = ifnet; ifn ; ifn = ifn->int_next) 
    {
      if(!ifn->is_wireless)
	continue;
      
      olsr_build_hello_packet(&hellopacket, ifn);
      hello_build(&hellopacket, ifn);
      
      if(outputsize)
	net_output(ifn);
      
    }
}

void
generate_hello_nw()
{
  struct interface *ifn;
  struct hello_message hellopacket;

  /* looping trough interfaces */
  for (ifn = ifnet; ifn ; ifn = ifn->int_next) 
    {
      if(ifn->is_wireless)
	continue;
      
      olsr_build_hello_packet(&hellopacket, ifn);
      hello_build(&hellopacket, ifn);
      
      if(outputsize)
	net_output(ifn);
      
    }
  return;
}



void
generate_tc()
{
  struct interface *ifn;
  struct tc_message tcpacket;

  /* looping trough interfaces */
  for (ifn = ifnet; ifn ; ifn = ifn->int_next) 
    {
      olsr_build_tc_packet(&tcpacket);
      tc_build(&tcpacket, ifn);

      if(outputsize)
	net_output(ifn);
    }
}


void
generate_mid()
{
  struct interface *ifn;

  /* looping trough interfaces */
  for (ifn = ifnet; ifn ; ifn = ifn->int_next) 
    {
      //printf("\nSending MID seq: %i\n", ifn->seqnums.mid_seqnum);
      mid_build(ifn);
      if(outputsize)
	net_output(ifn);
    }

return;
}



void
generate_hna()
{
  struct interface *ifn;

  /* looping trough interfaces */
  for (ifn = ifnet; ifn ; ifn = ifn->int_next) 
    { 
      hna_build(ifn);
      
      if(outputsize)
	net_output(ifn);
    }
  return;
}


/**
 *Displays various tables depending on debuglevel
 */
void
generate_tabledisplay()
{
  if(debug_level > 0) 
    {
      olsr_print_neighbor_table();
      
      if(debug_level > 1)
	{
	  olsr_print_tc_table();
	  if(debug_level > 2) 
	    {
	      olsr_print_mprs_set();
	      olsr_print_mid_set();
	      olsr_print_duplicate_table();
	    }
	}
    }
}

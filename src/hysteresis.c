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
 * $Id: hysteresis.c,v 1.6 2004/10/18 13:13:36 kattemat Exp $
 *
 */


#include <time.h>

#include "olsr_protocol.h"
#include "hysteresis.h"
#include "defs.h"
#include "olsr.h"

#define hscaling olsr_cnf->hysteresis_param.scaling
#define hhigh    olsr_cnf->hysteresis_param.thr_high
#define hlow     olsr_cnf->hysteresis_param.thr_low

inline float
olsr_hyst_calc_stability(float old_quality)
{
  return (((1 - hscaling) * old_quality) + hscaling);
}



inline float
olsr_hyst_calc_instability(float old_quality)
{
  return ((1 - hscaling) * old_quality);
}



int
olsr_process_hysteresis(struct link_entry *entry)
{
  struct timeval tmp_timer;

  //printf("PROCESSING QUALITY: %f\n", entry->L_link_quality);
  if(entry->L_link_quality > hhigh)
    {
      if(entry->L_link_pending == 1)
	{
	  olsr_printf(1, "HYST[%s] link set to NOT pending!\n", 
		      olsr_ip_to_string(&entry->neighbor_iface_addr));
	  changes_neighborhood = UP;
	}

      /* Pending = false */
      entry->L_link_pending = 0;

      if(!TIMED_OUT(&entry->L_LOST_LINK_time))
	changes_neighborhood = UP;

      /* time = now -1 */
      entry->L_LOST_LINK_time = now;
      entry->L_LOST_LINK_time.tv_sec -= 1;

      return 1;
    }

  if(entry->L_link_quality < hlow)
    {
      if(entry->L_link_pending == 0)
	{
	  olsr_printf(1, "HYST[%s] link set to pending!\n", 
		      olsr_ip_to_string(&entry->neighbor_iface_addr));
	  changes_neighborhood = UP;
	}
      
      /* Pending = true */
      entry->L_link_pending = 1;

      if(TIMED_OUT(&entry->L_LOST_LINK_time))
	changes_neighborhood = UP;

      /* Timer = min (L_time, current time + NEIGHB_HOLD_TIME) */
      //tmp_timer = now;
      //tmp_timer.tv_sec += NEIGHB_HOLD_TIME; /* Takafumi fix */
	timeradd(&now, &hold_time_neighbor, &tmp_timer);

	entry->L_LOST_LINK_time = 
	(timercmp(&entry->time, &tmp_timer, >) > 0) ? tmp_timer : entry->time;

      /* (the link is then considered as lost according to section
	 8.5 and this may produce a neighbor loss).
	 WTF?
      */
      return -1;
    }

  /*
   *If we get here then:
   *(HYST_THRESHOLD_LOW <= entry->L_link_quality <= HYST_THRESHOLD_HIGH)
   */

  /* L_link_pending and L_LOST_LINK_time remain unchanged. */
  return 0;


}

/**
 *Update the hello timeout of a hysteresis link
 *entry
 *
 *@param entry the link entry to update
 *@param htime the hello interval to use
 *
 *@return nada
 */
void
olsr_update_hysteresis_hello(struct link_entry *entry, double htime)
{
#ifdef DEBUG
  olsr_printf(3, "HYST[%s]: HELLO update vtime %f\n", olsr_ip_to_string(&entry->neighbor_iface_addr), htime*1.5);
#endif
  /* hello timeout = current time + hint time */
  /* SET TIMER TO 1.5 TIMES THE INTERVAL */
  /* Update timer */

  olsr_get_timestamp((olsr_u32_t) htime*1500, &entry->hello_timeout);

  return;
}



void
update_hysteresis_incoming(union olsr_ip_addr *remote, union olsr_ip_addr *local, olsr_u16_t seqno)
{
  struct link_entry *link;

  link = lookup_link_entry(remote, local);

  /* Calculate new quality */      
  if(link != NULL)
    {
      link->L_link_quality = olsr_hyst_calc_stability(link->L_link_quality);
#ifdef DEBUG
      olsr_printf(3, "HYST[%s]: %0.3f\n", olsr_ip_to_string(remote), link->L_link_quality);
#endif
      /* Check for missing packets - AVOID WRAP AROUND and FIRST TIME
       * checking for 0 is kind of a ugly hack...
       */
      if((link->olsr_seqno + 1 < seqno) &&
	 (link->olsr_seqno != 0) &&
	 (seqno != 0))
	{
	  //printf("HYS: packet lost.. last seqno %d received seqno %d!\n", link->olsr_seqno, seqno);
	  link->L_link_quality = olsr_hyst_calc_instability(link->L_link_quality);
#ifdef DEBUG
	  olsr_printf(5, "HYST[%s] PACKET LOSS! %0.3f\n", olsr_ip_to_string(remote), link->L_link_quality);
#endif
	}
      /* Set seqno */
      link->olsr_seqno = seqno;
      //printf("Updating seqno to: %d\n", link->olsr_seqno);
    }
  return;
}

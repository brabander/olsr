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
 * $Id: generate_msg.c,v 1.9 2004/10/18 13:13:36 kattemat Exp $
 *
 */

#include "generate_msg.h"
#include "defs.h"
#include "build_msg.h"
#include "packet.h"

/*
 * Infomation repositiries
 */
#include "hna_set.h"
#include "mid_set.h"
#include "tc_set.h"
#include "mpr_selector_set.h"
#include "duplicate_set.h"
#include "neighbor_table.h"


void
generate_hello(void *p)
{
  struct hello_message hellopacket;
  struct interface *ifn = (struct interface *)p;

  olsr_build_hello_packet(&hellopacket, ifn);
  hello_build(&hellopacket, ifn);
      
  if(net_output_pending(ifn))
    net_output(ifn);
}

void
generate_tc(void *p)
{
  struct tc_message tcpacket;
  struct interface *ifn = (struct interface *)p;

  olsr_build_tc_packet(&tcpacket);
  tc_build(&tcpacket, ifn);

  if(net_output_pending(ifn) && TIMED_OUT(&fwdtimer[ifn->if_nr]))
    {
      set_buffer_timer(ifn);
    }
}


void
generate_mid(void *p)
{
  struct interface *ifn = (struct interface *)p;
  mid_build(ifn);
  
  if(net_output_pending(ifn) && TIMED_OUT(&fwdtimer[ifn->if_nr]))
    {
      set_buffer_timer(ifn);
    }

}



void
generate_hna(void *p)
{
  struct interface *ifn = (struct interface *)p;
  hna_build(ifn);
  
  if(net_output_pending(ifn) && TIMED_OUT(&fwdtimer[ifn->if_nr]))
    {
      set_buffer_timer(ifn);
    }
}


/**
 *Displays various tables depending on debuglevel
 */
void
generate_tabledisplay(void *foo)
{
  if(olsr_cnf->debug_level > 0) 
    {
      olsr_print_neighbor_table();
      
      if(olsr_cnf->debug_level > 1)
	{
	  olsr_print_tc_table();
	  if(olsr_cnf->debug_level > 2) 
	    {
	      olsr_print_mprs_set();
	      olsr_print_mid_set();
	      olsr_print_duplicate_table();
	    }
	}
    }
}

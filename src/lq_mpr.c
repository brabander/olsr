/* 
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2004 Thomas Lopatic (thomas@lopatic.de)
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
 * $Id: lq_mpr.c,v 1.4 2004/11/08 23:25:57 tlopatic Exp $
 *
 */

#if defined USE_LINK_QUALITY
#include "defs.h"
#include "neighbor_table.h"
#include "two_hop_neighbor_table.h"
#include "lq_mpr.h"

void olsr_calculate_lq_mpr(void)
{
  struct neighbor_2_entry *neigh2;
  struct neighbor_list_entry *walker;
  int i;
  struct neighbor_entry *neigh;
  double best;
  olsr_bool mpr_changes = OLSR_FALSE;

  for(i = 0; i < HASHSIZE; i++)
    {
      for (neigh = neighbortable[i].next;
           neigh != &neighbortable[i];
           neigh = neigh->next)
        { 
          // memorize previous MPR status

          neigh->was_mpr = neigh->is_mpr;

          // clear current MPR status

          neigh->is_mpr = OLSR_FALSE;

          // in this pass we are only interested in WILL_ALWAYS neighbours

          if(neigh->status == NOT_SYM ||
             neigh->willingness != WILL_ALWAYS)
            continue;

          neigh->is_mpr = OLSR_TRUE;

          if (neigh->is_mpr != neigh->was_mpr)
            mpr_changes = OLSR_TRUE;
        }
    }

  for(i = 0; i < HASHSIZE; i++)
    {
      // loop through all 2-hop neighbours

      for (neigh2 = two_hop_neighbortable[i].next;
           neigh2 != &two_hop_neighbortable[i];
           neigh2 = neigh2->next)
        {
          // check whether this 2-hop neighbour is also a neighbour

          neigh = olsr_lookup_neighbor_table(&neigh2->neighbor_2_addr);

          // it it's a neighbour and also symmetric, then skip it
          
          if (neigh != NULL && neigh->status == SYM)
            continue;

          // find the connecting 1-hop neighbour with the
          // best total link quality

          neigh = NULL;
          best = -1.0;

          for (walker = neigh2->neighbor_2_nblist.next;
               walker != &neigh2->neighbor_2_nblist;
               walker = walker->next)
            if (walker->neighbor->status == SYM &&
                walker->full_link_quality > best)
              {
                neigh = walker->neighbor;
                best = walker->full_link_quality;
              }

          if (neigh != NULL)
            {
              neigh->is_mpr = OLSR_TRUE;
          
              if (neigh->is_mpr != neigh->was_mpr)
                mpr_changes = OLSR_TRUE;
            }
        }
    }

  if (mpr_changes && olsr_cnf->tc_redundancy > 0)
    changes = OLSR_TRUE;
}
#endif

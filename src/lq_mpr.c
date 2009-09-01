
/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004-2009, the olsr.org team - see HISTORY file
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of olsr.org, olsrd nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Visit http://www.olsr.org for more information.
 *
 * If you find this software useful feel free to make a donation
 * to the project. For more information see the website or contact
 * the copyright holders.
 *
 */

#include "defs.h"
#include "neighbor_table.h"
#include "link_set.h"
#include "lq_mpr.h"
#include "scheduler.h"
#include "lq_plugin.h"

void
olsr_calculate_lq_mpr(void)
{
  struct nbr2_entry *nbr2;
  struct nbr_con *walker;
  struct link_entry *lnk;
  int k;
  struct nbr_entry *neigh;
  olsr_linkcost best, best_1hop;
  bool mpr_changes = false;
  bool found_better_path;

  OLSR_FOR_ALL_NBR_ENTRIES(neigh) {

    /* Memorize previous MPR status. */
    neigh->was_mpr = neigh->is_mpr;

    /* Clear current MPR status. */
    neigh->is_mpr = false;

    /* In this pass we are only interested in WILL_ALWAYS neighbours */
    if (!neigh->is_sym || neigh->willingness != WILL_ALWAYS) {
      continue;
    }

    neigh->is_mpr = true;

    if (neigh->is_mpr != neigh->was_mpr) {
      mpr_changes = true;
    }

  } OLSR_FOR_ALL_NBR_ENTRIES_END();

  /* loop through all 2-hop neighbours */
  OLSR_FOR_ALL_NBR2_ENTRIES(nbr2) {

    best_1hop = LINK_COST_BROKEN;

    /* check whether this 2-hop neighbour is also a neighbour */
    neigh = olsr_lookup_nbr_entry(&nbr2->nbr2_addr, false);

    if (neigh != NULL && neigh->is_sym) {
      /*
       * if the direct link is better than the best route via
       * an MPR, then prefer the direct link and do not select
       * an MPR for this 2-hop neighbour
       */

      /* determine the link quality of the direct link */
      lnk = get_best_link_to_neighbor(&neigh->nbr_addr);
      if (!lnk) {
        continue;
      }

      best_1hop = lnk->linkcost;
    }
      /* see wether we find a better route via an MPR */
      walker = NULL;
      found_better_path = false;
      OLSR_FOR_ALL_NBR2_CON_ENTRIES(nbr2, walker) {
        if (walker->path_linkcost < best_1hop) {
          found_better_path = true;
          break;
        }
      } OLSR_FOR_ALL_NBR_CON_ENTRIES_END()

      /* we've reached the end of the list, so we haven't found
       * a better route via an MPR - so, skip MPR selection for
       * this 1-hop neighbor */

      if (!found_better_path) {
        continue;
      }

      /*
       * Now find the connecting 1-hop neighbours with the best total link qualities
       */

      /* mark all 1-hop neighbours as not selected */
      OLSR_FOR_ALL_NBR2_CON_ENTRIES(nbr2, walker) {
        walker->nbr->skip = false;
      } OLSR_FOR_ALL_NBR_CON_ENTRIES_END();

      for (k = 0; k < olsr_cnf->mpr_coverage; k++) {

        /* look for the best 1-hop neighbour that we haven't yet selected */
        neigh = NULL;
        best = LINK_COST_BROKEN;

        OLSR_FOR_ALL_NBR2_CON_ENTRIES(nbr2, walker) {
          if (walker->nbr->is_sym && !walker->nbr->skip && walker->path_linkcost < best) {
            neigh = walker->nbr;
            best = walker->path_linkcost;
          }
        } OLSR_FOR_ALL_NBR2_CON_ENTRIES_END();

        /*
         * Found a 1-hop neighbor that we haven't previously selected.
         * Use it as MPR only when the 2-hop path through it is better than
         * any existing 1-hop path.
         */
        if ((neigh != NULL) && (best < best_1hop)) {
          neigh->is_mpr = true;
          neigh->skip = true;

          if (neigh->is_mpr != neigh->was_mpr) {
            mpr_changes = true;
          }
        } else {

          /*
           * no neighbour found, hence the requested MPR coverage cannot * be satisfied => stop
           */
          break;
        }
      }
  } OLSR_FOR_ALL_NBR2_ENTRIES_END();

  /* ugly hack */
  OLSR_FOR_ALL_LINK_ENTRIES(lnk) {
    lnk->is_mpr = lnk->neighbor->is_mpr;
  } OLSR_FOR_ALL_LINK_ENTRIES_END(lnk)
  if (mpr_changes && olsr_cnf->tc_redundancy > 0)
    signal_link_changes(true);
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

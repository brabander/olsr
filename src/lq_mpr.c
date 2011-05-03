

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
#include "olsr_timer.h"
#include "olsr_socket.h"
#include "lq_plugin.h"

static void olsr_calculate_lq_mpr2(void);

void
olsr_calculate_lq_mpr(void)
{
  struct nbr_entry *neigh, *neigh_iterator;
  struct link_entry *lnk, *lnk_iterator;
  bool mpr_changes = false;

  /* store old MPR state */
  OLSR_FOR_ALL_NBR_ENTRIES(neigh, neigh_iterator) {
    neigh->was_mpr = neigh->is_mpr;
  }

  /* calculate MPR set */
  olsr_calculate_lq_mpr2();

  /* look for MPR changes */
  OLSR_FOR_ALL_NBR_ENTRIES(neigh, neigh_iterator) {
    if (neigh->was_mpr != neigh->is_mpr) {
      mpr_changes = true;
      break;
    }
  }

  if (!mpr_changes) {
    /* no changes */
    return;
  }

  /*
   * we don't do MPRs on interface level...
   * ugly hack: copy MPR set to link database
   */
  OLSR_FOR_ALL_LINK_ENTRIES(lnk, lnk_iterator) {
    lnk->is_mpr = lnk->neighbor->is_mpr;
  }

  /* check if the link state changed */
  if (mpr_changes && olsr_cnf->tc_redundancy > 0) {
    signal_link_changes(true);
  }
}

static void
olsr_calculate_lq_mpr2(void)
{
  struct nbr_entry *neigh, *neigh_iterator;

/* use 0 to activate MPR calculation, use 1 to deactivate it */
#if 0
  OLSR_FOR_ALL_NBR_ENTRIES(neigh, neigh_iterator) {
    /* just use everyone as MPR */
    neigh->is_mpr = true;
  }
#else
  struct nbr2_entry *nbr2, *nbr2_iterator;
  struct nbr_con *walker, *walker_iterator;
  struct link_entry *lnk;
  int k;
  olsr_linkcost best, best_1hop;
  bool mpr_changes = false, found_better_path;

  OLSR_FOR_ALL_NBR_ENTRIES(neigh, neigh_iterator) {
    /* Clear current MPR status. */
    neigh->is_mpr = false;

    /* In this pass we are only interested in WILL_ALWAYS neighbors */
    if (neigh->is_sym && neigh->willingness != WILL_ALWAYS) {
      neigh->is_mpr = true;

      if (neigh->is_mpr != neigh->was_mpr) {
        mpr_changes = true;
      }
    }
  }

  /* loop through all 2-hop neighbors */
  OLSR_FOR_ALL_NBR2_ENTRIES(nbr2, nbr2_iterator) {
    best_1hop = ROUTE_COST_BROKEN;

    /* check whether this 2-hop neighbors is also a neighbors */
    neigh = olsr_lookup_nbr_entry(&nbr2->nbr2_addr, false);

    if (neigh != NULL && neigh->is_sym) {
      /*
       * if the direct link is better than the best route via
       * an MPR, then prefer the direct link and do not select
       * an MPR for this 2-hop neighbors
       */

      /* determine the link quality of the direct link */
      lnk = get_best_link_to_neighbor(neigh);
      if (!lnk) {
        /*
         * this should not happen, a symmetric 1-hop neighbor
         * should have a symmetric link
         */
        continue;
      }

      best_1hop = lnk->linkcost;
    }

    /* see if there is a better route via another 1-hop neighbor */
    walker = NULL;
    found_better_path = false;
    OLSR_FOR_ALL_NBR2_CON_ENTRIES(nbr2, walker, walker_iterator) {
      if (walker->path_linkcost < best_1hop) {
        found_better_path = true;
        break;
      }
    }

    /* we've reached the end of the list, so we haven't found
     * a better route via an MPR - so, skip MPR selection for
     * this 1-hop neighbor */

    if (!found_better_path) {
      continue;
    }

    /*
     * Now find the connecting 1-hop neighbors with the best total link qualities
     */

    /* mark all 1-hop neighbors as not used for MPR coverage */
    OLSR_FOR_ALL_NBR2_CON_ENTRIES(nbr2, walker, walker_iterator) {
      walker->nbr->skip = false;
    }

    for (k = 0; k < olsr_cnf->mpr_coverage; k++) {
      /*
       * look for the best 1-hop neighbor that we haven't yet selected
       * that produce a better route than the direct one
       */
      neigh = NULL;
      best = best_1hop;

      OLSR_FOR_ALL_NBR2_CON_ENTRIES(nbr2, walker, walker_iterator) {
        if (walker->nbr->is_sym && !walker->nbr->skip
            && walker->second_hop_linkcost < LINK_COST_BROKEN
            && walker->path_linkcost < best) {
          neigh = walker->nbr;
          best = walker->path_linkcost;
        }
      }

      if (neigh == NULL) {
        /* no more better 2-hop routes, stop looking */
        break;
      }

      /*
       * Found a 1-hop neighbor that we haven't previously selected.
       */
      neigh->is_mpr = true;

      /* don't use this one for more MPR coverage */
      neigh->skip = true;
    }
  }
#endif
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

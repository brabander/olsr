
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

#include "ipcalc.h"
#include "defs.h"
#include "mpr.h"
#include "two_hop_neighbor_table.h"
#include "olsr.h"
#include "neighbor_table.h"
#include "scheduler.h"
#include "net_olsr.h"
#include "olsr_logging.h"

#include <stdlib.h>

/* Begin:
 * Prototypes for internal functions
 */

static uint16_t add_will_always_nodes(void);
static void olsr_optimize_mpr_set(void);
static void olsr_clear_mprs(void);
static void olsr_clear_two_hop_processed(void);
static struct nbr_entry *olsr_find_maximum_covered(int);
static uint16_t olsr_calculate_two_hop_neighbors(void);
static int olsr_check_mpr_changes(void);
static int olsr_chosen_mpr(struct nbr_entry *, uint16_t *);

/* End:
 * Prototypes for internal functions
 */


/**
 *This function processes the chosen MPRs and updates the counters
 *used in calculations
 */
static int
olsr_chosen_mpr(struct nbr_entry *one_hop_neighbor, uint16_t * two_hop_covered_count)
{
  struct nbr_list_entry *the_one_hop_list;
  struct nbr2_list_entry *second_hop_entries;
  struct nbr_entry *dup_neighbor;
  uint16_t count;
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif
  count = *two_hop_covered_count;

  OLSR_DEBUG(LOG_MPR, "Setting %s as MPR\n", olsr_ip_to_string(&buf, &one_hop_neighbor->neighbor_main_addr));

  //printf("PRE COUNT: %d\n\n", count);

  one_hop_neighbor->is_mpr = true;      //NBS_MPR;

  OLSR_FOR_ALL_NBR2_LIST_ENTRIES(one_hop_neighbor, second_hop_entries) {

    dup_neighbor = olsr_lookup_nbr_entry(&second_hop_entries->nbr2->nbr2_addr);

    if ((dup_neighbor != NULL) && (dup_neighbor->status == SYM)) {
      OLSR_DEBUG(LOG_MPR, "(2)Skipping 2h neighbor %s - already 1hop\n",
                 olsr_ip_to_string(&buf, &second_hop_entries->nbr2->nbr2_addr));
      continue;
    }

    /*
     * Now the neighbor is covered by this mpr
     */
    second_hop_entries->nbr2->mpr_covered_count++;

    OLSR_DEBUG(LOG_MPR, "[%s] has coverage %d\n",
               olsr_ip_to_string(&buf, &second_hop_entries->nbr2->nbr2_addr),
               second_hop_entries->nbr2->mpr_covered_count);

    if (second_hop_entries->nbr2->mpr_covered_count >= olsr_cnf->mpr_coverage)
      count++;

    OLSR_FOR_ALL_NBR_LIST_ENTRIES(second_hop_entries->nbr2, the_one_hop_list) {
      if ((the_one_hop_list->neighbor->status == SYM)) {
        if (second_hop_entries->nbr2->mpr_covered_count >= olsr_cnf->mpr_coverage) {
          the_one_hop_list->neighbor->nbr2_nocov--;
        }
      }
    } OLSR_FOR_ALL_NBR_LIST_ENTRIES_END(second_hop_entries->nbr2, the_one_hop_list);
  } OLSR_FOR_ALL_NBR2_LIST_ENTRIES_END(one_hop_neighbor, second_hop_entries);

  *two_hop_covered_count = count;
  return count;

}


/**
 *Find the neighbor that covers the most 2 hop neighbors
 *with a given willingness
 *
 *@param willingness the willingness of the neighbor
 *
 *@return a pointer to the nbr_entry struct
 */
static struct nbr_entry *
olsr_find_maximum_covered(int willingness)
{
  uint16_t maximum;
  struct nbr_entry *a_neighbor;
  struct nbr_entry *mpr_candidate = NULL;
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif
  maximum = 0;

  OLSR_FOR_ALL_NBR_ENTRIES(a_neighbor) {

    OLSR_DEBUG(LOG_MPR, "[%s] nocov: %d mpr: %d will: %d max: %d\n\n",
               olsr_ip_to_string(&buf, &a_neighbor->neighbor_main_addr),
               a_neighbor->nbr2_nocov, a_neighbor->is_mpr, a_neighbor->willingness, maximum);

    if ((!a_neighbor->is_mpr) && (a_neighbor->willingness == willingness) && (maximum < a_neighbor->nbr2_nocov)) {

      maximum = a_neighbor->nbr2_nocov;
      mpr_candidate = a_neighbor;
    }
  } OLSR_FOR_ALL_NBR_ENTRIES_END(a_neighbor);

  return mpr_candidate;
}


/**
 *Remove all MPR registrations
 */
static void
olsr_clear_mprs(void)
{
  struct nbr_entry *a_neighbor;
  struct nbr2_list_entry *two_hop_list;

  OLSR_FOR_ALL_NBR_ENTRIES(a_neighbor) {

    /* Clear MPR selection. */
    if (a_neighbor->is_mpr) {
      a_neighbor->was_mpr = true;
      a_neighbor->is_mpr = false;
    }

    /* Clear two hop neighbors coverage count/ */
    OLSR_FOR_ALL_NBR2_LIST_ENTRIES(a_neighbor, two_hop_list) {
      two_hop_list->nbr2->mpr_covered_count = 0;
    } OLSR_FOR_ALL_NBR2_LIST_ENTRIES_END(a_neighbor, two_hop_list);
  } OLSR_FOR_ALL_NBR_ENTRIES_END(a_neighbor);
}


/**
 *Check for changes in the MPR set
 *
 *@return 1 if changes occured 0 if not
 */
static int
olsr_check_mpr_changes(void)
{
  struct nbr_entry *a_neighbor;
  int retval;

  retval = 0;

  OLSR_FOR_ALL_NBR_ENTRIES(a_neighbor) {

    if (a_neighbor->was_mpr) {
      a_neighbor->was_mpr = false;

      if (!a_neighbor->is_mpr) {
        retval = 1;
      }
    }
  } OLSR_FOR_ALL_NBR_ENTRIES_END(a_neighbor);

  return retval;
}


/**
 * Clears out proccess registration on two hop neighbors
 */
static void
olsr_clear_two_hop_processed(void)
{
  struct nbr2_entry *nbr2;

  OLSR_FOR_ALL_NBR2_ENTRIES(nbr2) {

      /* Clear */
      nbr2->processed = 0;
  } OLSR_FOR_ALL_NBR2_ENTRIES_END(nbr2);
}


/**
 *This function calculates the number of two hop neighbors
 */
static uint16_t
olsr_calculate_two_hop_neighbors(void)
{
  struct nbr_entry *a_neighbor, *dup_neighbor;
  struct nbr2_list_entry *twohop_neighbors;
  uint16_t count = 0;
  uint16_t n_count = 0;
  uint16_t sum = 0;

  /* Clear 2 hop neighs */
  olsr_clear_two_hop_processed();

  OLSR_FOR_ALL_NBR_ENTRIES(a_neighbor) {

    if (a_neighbor->status == NOT_SYM) {
      a_neighbor->nbr2_nocov = count;
      continue;
    }

    OLSR_FOR_ALL_NBR2_LIST_ENTRIES(a_neighbor, twohop_neighbors) {
      dup_neighbor = olsr_lookup_nbr_entry(&twohop_neighbors->nbr2->nbr2_addr);

      if ((dup_neighbor == NULL) || (dup_neighbor->status != SYM)) {
        n_count++;
        if (!twohop_neighbors->nbr2->processed) {
          count++;
          twohop_neighbors->nbr2->processed = 1;
        }
      }
    } OLSR_FOR_ALL_NBR2_LIST_ENTRIES_END(a_neighbor, twohop_neighbors);
    a_neighbor->nbr2_nocov = n_count;

    /* Add the two hop count */
    sum += count;

  } OLSR_FOR_ALL_NBR_ENTRIES_END(a_neighbor);

  OLSR_DEBUG(LOG_MPR, "Two hop neighbors: %d\n", sum);
  return sum;
}




/**
 * Adds all nodes with willingness set to WILL_ALWAYS
 */
static uint16_t
add_will_always_nodes(void)
{
  struct nbr_entry *a_neighbor;
  uint16_t count = 0;
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif
  OLSR_FOR_ALL_NBR_ENTRIES(a_neighbor) {
    if ((a_neighbor->status == NOT_SYM) || (a_neighbor->willingness != WILL_ALWAYS)) {
      continue;
    }
    olsr_chosen_mpr(a_neighbor, &count);

    OLSR_DEBUG(LOG_MPR, "Adding WILL_ALWAYS: %s\n", olsr_ip_to_string(&buf, &a_neighbor->neighbor_main_addr));

  } OLSR_FOR_ALL_NBR_ENTRIES_END(a_neighbor);

  return count;
}

/**
 *This function calculates the mpr neighbors
 *@return nada
 */
void
olsr_calculate_mpr(void)
{
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif

  struct nbr2_entry *nbr2;
  struct nbr_entry *nbr;
  struct nbr_entry *mprs;
  uint16_t two_hop_covered_count;
  uint16_t two_hop_count;
  int willingness;

  olsr_clear_mprs();
  two_hop_count = olsr_calculate_two_hop_neighbors();
  two_hop_covered_count = add_will_always_nodes();

  /*
   * Calculate MPRs based on WILLINGNESS.
   */
  for (willingness = WILL_ALWAYS - 1; willingness > WILL_NEVER; willingness--) {

    /*
     * Find all 2 hop neighbors with 1 link
     * connecting them to us trough neighbors
     * with a given willingness.
     */
    OLSR_FOR_ALL_NBR2_ENTRIES(nbr2) {

      /*
       * Eliminate 2 hop neighbors which already are in our 1 hop neighborhood.
       */
      nbr = olsr_lookup_nbr_entry(&nbr2->nbr2_addr);
      if (nbr && (nbr->status != NOT_SYM)) {
        OLSR_DEBUG(LOG_MPR, "Skipping 2-hop neighbor2 %s - already 1hop\n",
                   olsr_ip_to_string(&buf, &nbr2->nbr2_addr));
        continue;
      }

      /*
       * Eliminate 2 hop neighbors which are not single link.
       */
      if (nbr2->nbr2_refcount != 1) {
        OLSR_DEBUG(LOG_MPR, "Skipping 2-hop neighbor %s - not single link\n",
                   olsr_ip_to_string(&buf, &nbr2->nbr2_addr));
        continue;
      }

      nbr = nbr_list_node_to_nbr_list(avl_walk_first(&nbr2->nbr2_nbr_list_tree))->neighbor;

      /* Already an elected MPR ? */
      if (nbr->is_mpr) {
        OLSR_DEBUG(LOG_MPR, "Skipping 2-hop neighbor %s - already MPR\n",
                   olsr_ip_to_string(&buf, &nbr2->nbr2_addr));
        continue;
      }

      /* Match willingness */
      if (nbr->willingness != willingness) {
        continue;
      }

      /* Only symmetric neighbors */
      if (nbr->status != SYM) {
        continue;
      }

      /*
       * This 2 hop neighbor is good enough.
       */
      OLSR_DEBUG(LOG_MPR, "One link adding %s\n", olsr_ip_to_string(&buf, &nbr2->nbr2_addr));
      olsr_chosen_mpr(nbr, &two_hop_covered_count);

    } OLSR_FOR_ALL_NBR2_ENTRIES_END(nbr2);

    if (two_hop_covered_count >= two_hop_count) {
      willingness = WILL_NEVER;
      break;
    }

    while ((mprs = olsr_find_maximum_covered(willingness)) != NULL) {
      olsr_chosen_mpr(mprs, &two_hop_covered_count);

      if (two_hop_covered_count >= two_hop_count) {
        willingness = WILL_NEVER;
        break;
      }
    }
  }

  /*
   * Increment the MPR sequence number.
   */

  /* Optimize selection */
  olsr_optimize_mpr_set();

  if (olsr_check_mpr_changes()) {
    OLSR_DEBUG(LOG_MPR, "CHANGES IN MPR SET\n");
    if (olsr_cnf->tc_redundancy > 0)
      signal_link_changes(true);
  }
}

/**
 *Optimize MPR set by removing all entries
 *where all 2 hop neighbors actually is
 *covered by enough MPRs already
 *Described in RFC3626 section 8.3.1
 *point 5
 *
 *@return nada
 */
static void
olsr_optimize_mpr_set(void)
{
  int i;
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif
  OLSR_DEBUG(LOG_MPR, "\n**MPR OPTIMIZING**\n\n");

  for (i = WILL_NEVER + 1; i < WILL_ALWAYS; i++) {
    struct nbr_entry *a_neighbor;

    OLSR_FOR_ALL_NBR_ENTRIES(a_neighbor) {

      if (a_neighbor->willingness != i) {
        continue;
      }

      if (a_neighbor->is_mpr) {
        struct nbr2_list_entry *two_hop_list;
        int remove_it = 1;

        OLSR_FOR_ALL_NBR2_LIST_ENTRIES(a_neighbor, two_hop_list) {
          const struct nbr_entry *dup_neighbor = olsr_lookup_nbr_entry(&two_hop_list->nbr2->nbr2_addr);

          if ((dup_neighbor != NULL) && (dup_neighbor->status != NOT_SYM)) {
            continue;
          }

          OLSR_DEBUG(LOG_MPR, "\t[%s] coverage %d\n", olsr_ip_to_string(&buf, &two_hop_list->nbr2->nbr2_addr),
                     two_hop_list->nbr2->mpr_covered_count);
          /* Do not remove if we find a entry which need this MPR */
          if (two_hop_list->nbr2->mpr_covered_count <= olsr_cnf->mpr_coverage) {
            remove_it = 0;
            break;
          }
        } OLSR_FOR_ALL_NBR2_LIST_ENTRIES_END(a_neighbor, two_hop_list_list);

        if (remove_it) {
          OLSR_DEBUG(LOG_MPR, "MPR OPTIMIZE: removiong mpr %s\n\n", olsr_ip_to_string(&buf, &a_neighbor->neighbor_main_addr));
          a_neighbor->is_mpr = false;
        }
      }
    } OLSR_FOR_ALL_NBR_ENTRIES_END(a_neighbor);
  }
}

void
olsr_print_mpr_set(void)
{
#if !defined REMOVE_LOG_INFO
  /* The whole function makes no sense without it. */
  struct nbr_entry *a_neighbor;

  OLSR_INFO(LOG_MPR, "MPR SET: ");

  OLSR_FOR_ALL_NBR_ENTRIES(a_neighbor) {

    /*
     * Remove MPR settings
     */
    if (a_neighbor->is_mpr) {
      struct ipaddr_str buf;
      OLSR_INFO_NH(LOG_MPR, "\t[%s]\n", olsr_ip_to_string(&buf, &a_neighbor->neighbor_main_addr));
    }
  } OLSR_FOR_ALL_NBR_ENTRIES_END(a_neighbor);
#endif
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

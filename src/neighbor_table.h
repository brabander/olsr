
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


#ifndef _OLSR_NEIGH_TBL
#define _OLSR_NEIGH_TBL

#include "defs.h"
#include "olsr_types.h"
#include "hashing.h"


struct nbr2_list_entry {
  struct nbr_entry *nbr2_nbr;     /* backpointer to owning nbr entry */
  struct neighbor_2_entry *neighbor_2;
  struct timer_entry *nbr2_list_timer;
  struct nbr2_list_entry *next;
  struct nbr2_list_entry *prev;
};

#define OLSR_NBR2_LIST_JITTER 5 /* percent */

struct nbr_entry {
  union olsr_ip_addr neighbor_main_addr;
  uint8_t status;
  uint8_t willingness;
  unsigned char is_mpr:1;
  unsigned char was_mpr:1;             /* Used to detect changes in MPR */
  unsigned char skip:1;
  int neighbor_2_nocov;
  int linkcount;
  struct nbr2_list_entry neighbor_2_list;
  struct nbr_entry *next;
  struct nbr_entry *prev;
};

#define OLSR_FOR_ALL_NBR_ENTRIES(nbr) \
{ \
  int _idx; \
  for (_idx = 0; _idx < HASHSIZE; _idx++) { \
    for(nbr = neighbortable[_idx].next; \
        nbr != &neighbortable[_idx]; \
        nbr = nbr->next)
#define OLSR_FOR_ALL_NBR_ENTRIES_END(nbr) }}


/*
 * The neighbor table
 */
extern struct nbr_entry
EXPORT(neighbortable)[HASHSIZE];

     extern struct olsr_cookie_info *nbr2_list_timer_cookie;

     void
       olsr_init_neighbor_table(void);

     int
       olsr_delete_neighbor_2_pointer(struct nbr_entry *, struct neighbor_2_entry *);

     struct nbr2_list_entry *olsr_lookup_my_neighbors(const struct nbr_entry *, const union olsr_ip_addr *);

     int
       olsr_delete_neighbor_table(const union olsr_ip_addr *);

     struct nbr_entry *olsr_insert_neighbor_table(const union olsr_ip_addr *);

     struct nbr_entry *olsr_lookup_neighbor_table(const union olsr_ip_addr *);

     struct nbr_entry *olsr_lookup_neighbor_table_alias(const union olsr_ip_addr *);

     void
       olsr_time_out_two_hop_neighbors(struct nbr_entry *);

     void
       olsr_time_out_neighborhood_tables(void);
     void olsr_expire_nbr2_list(void *);

     void
       olsr_print_neighbor_table(void);


     int
       update_neighbor_status(struct nbr_entry *, int);

#endif

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

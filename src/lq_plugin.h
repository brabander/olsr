
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

#ifndef LQPLUGIN_H_
#define LQPLUGIN_H_

#include "tc_set.h"
#include "link_set.h"
#include "olsr_spf.h"
#include "lq_packet.h"
#include "common/avl.h"

#define LINK_COST_BROKEN (0x00ffffff)
#define ROUTE_COST_BROKEN (0xffffffff)
#define ZERO_ROUTE_COST 0

#define MINIMAL_USEFUL_LQ 0.1
#define LQ_PLUGIN_RELEVANT_COSTCHANGE 16

#define LQTEXT_MAXLENGTH 32

struct lq_linkdata_type {
  const char *name;
  size_t name_maxlen;
  int32_t best, good, average, worst;
};

enum lq_linkdata_quality {
  LQ_QUALITY_BEST,
  LQ_QUALITY_GOOD,
  LQ_QUALITY_AVERAGE,
  LQ_QUALITY_BAD,
  LQ_QUALITY_WORST
};

struct lq_handler {
  const char *name;

  void (*initialize) (void);
  void (*deinitialize) (void);

  olsr_linkcost(*calc_link_entry_cost) (struct link_entry *);
  olsr_linkcost(*calc_lq_hello_neighbor_cost) (struct lq_hello_neighbor *);
  olsr_linkcost(*calc_tc_edge_entry_cost) (struct tc_edge_entry *);

  bool(*is_relevant_costchange) (olsr_linkcost c1, olsr_linkcost c2);

  olsr_linkcost(*packet_loss_handler) (struct link_entry *, bool);

  void (*memorize_foreign_hello) (struct link_entry *, struct lq_hello_neighbor *);
  void (*copy_link_entry_lq_into_tc_edge_entry) (struct tc_edge_entry *, struct link_entry *);

  void (*clear_link_entry) (struct link_entry *);
  void (*clear_lq_hello_neighbor) (struct lq_hello_neighbor *);
  void (*clear_tc_edge_entry) (struct tc_edge_entry *);

  void (*serialize_hello_lq) (uint8_t **, struct link_entry *);
  void (*serialize_tc_lq) (uint8_t **, struct link_entry *);
  void (*deserialize_hello_lq) (uint8_t const **, struct lq_hello_neighbor *);
  void (*deserialize_tc_lq) (uint8_t const **, struct tc_edge_entry *);

  int (*get_link_entry_data) (struct link_entry *, int);

  const char *(*print_cost) (olsr_linkcost cost, char *, size_t);
  const char *(*print_link_entry_lq) (struct link_entry *, int, char *, size_t);

  struct lq_linkdata_type *linkdata_hello;
  int linkdata_hello_count;

  size_t size_tc_edge;
  size_t size_lq_hello_neighbor;
  size_t size_link_entry;

  uint8_t messageid_hello;
  uint8_t messageid_tc;

  size_t serialized_lqhello_size;
  size_t serialized_lqtc_size;
};

void init_lq_handler(void);
void deinit_lq_handler(void);

olsr_linkcost olsr_calc_tc_cost(struct tc_edge_entry *);
bool olsr_is_relevant_costchange(olsr_linkcost c1, olsr_linkcost c2);

void olsr_serialize_hello_lq_pair(uint8_t **, struct link_entry *);
void olsr_deserialize_hello_lq_pair(const uint8_t **, struct lq_hello_neighbor *);
void olsr_serialize_tc_lq(uint8_t **curr, struct link_entry *lnk);
void olsr_deserialize_tc_lq_pair(const uint8_t **, struct tc_edge_entry *);

void olsr_update_packet_loss_worker(struct link_entry *, bool);
void olsr_memorize_foreign_hello_lq(struct link_entry *, struct lq_hello_neighbor *);

const char *EXPORT(olsr_get_linkcost_text) (olsr_linkcost, bool, char *, size_t);
const char *EXPORT(olsr_get_linkdata_text) (struct link_entry *, int, char *, size_t);
const char *EXPORT(olsr_get_linklabel) (int);
size_t EXPORT(olsr_get_linklabel_maxlength) (int);
size_t EXPORT(olsr_get_linklabel_count) (void);
enum lq_linkdata_quality EXPORT(olsr_get_linkdata_quality) (struct link_entry *, int);

void olsr_copylq_link_entry_2_tc_edge_entry(struct tc_edge_entry *, struct link_entry *);

struct tc_edge_entry *olsr_malloc_tc_edge_entry(void);
struct lq_hello_neighbor *olsr_malloc_lq_hello_neighbor(void);
struct link_entry *olsr_malloc_link_entry(void);

void olsr_free_link_entry(struct link_entry *);
void olsr_free_lq_hello_neighbor(struct lq_hello_neighbor *);
void olsr_free_tc_edge_entry(struct tc_edge_entry *);

uint8_t olsr_get_Hello_MessageId(void);
uint8_t olsr_get_TC_MessageId(void);

size_t olsr_sizeof_HelloLQ(void);
size_t olsr_sizeof_TCLQ(void);

/* Externals. */
extern struct lq_handler *EXPORT(active_lq_handler);

#endif /*LQPLUGIN_H_ */

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

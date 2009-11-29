
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

#include "tc_set.h"
#include "link_set.h"
#include "olsr_spf.h"
#include "lq_packet.h"
#include "olsr.h"
#include "olsr_logging.h"
#include "lq_plugin_etx_float.h"

#define PLUGIN_DESCR    "Floating point based ETX metric with exponential aging"
#define PLUGIN_AUTHOR   "Henning Rogge and others"

#define LQ_PLUGIN_LC_MULTIPLIER 1024

#define LQ_FLOAT_DEFAULT_AGING       0.05        /* 65536 * 0.05 */
#define LQ_FLOAT_QUICKSTART_AGING    0.25       /* 65536 * 0.25 */
#define LQ_QUICKSTART_STEPS        12

static int set_plugin_float(const char *, void *, set_plugin_parameter_addon);
static bool lq_etxfloat_enable(void);

static olsr_linkcost lq_etxfloat_calc_link_entry_cost(struct link_entry *);
static olsr_linkcost lq_etxfloat_calc_lq_hello_neighbor_cost(struct lq_hello_neighbor *);
static olsr_linkcost lq_etxfloat_calc_tc_edge_entry_cost(struct tc_edge_entry *);

static bool lq_etxfloat_is_relevant_costchange(olsr_linkcost c1, olsr_linkcost c2);

static olsr_linkcost lq_etxfloat_packet_loss_handler(struct link_entry *, bool);

static void lq_etxfloat_memorize_foreign_hello(struct link_entry *, struct lq_hello_neighbor *);
static void lq_etxfloat_copy_link_entry_lq_into_tc_edge_entry(struct tc_edge_entry *target, struct link_entry *source);

static void lq_etxfloat_serialize_hello_lq(uint8_t **curr, struct link_entry *link);
static void lq_etxfloat_serialize_tc_lq(uint8_t **curr, struct link_entry *link);
static void lq_etxfloat_deserialize_hello_lq(uint8_t const **curr, struct lq_hello_neighbor *lq);
static void lq_etxfloat_deserialize_tc_lq(uint8_t const **curr, struct tc_edge_entry *lq);

static int lq_etxfloat_get_linkentry_data(struct link_entry *, int);
static const char *lq_etxfloat_print_cost(olsr_linkcost cost, char *buffer, size_t bufsize);
static const char *lq_etxfloat_print_link_entry_lq(struct link_entry *entry, int index, char *buffer, size_t bufsize);

/* etx lq plugin (float version) settings */
struct lq_linkdata_type lq_etxfloat_linktypes[] = {
  { "ETX", 5, 1000, 1000*2, 1000*4, INT32_MAX },
  { "LQ", 5, 255, 240, 192, 0 },
  { "NLQ", 5, 255, 240, 192, 0 }
};

struct lq_handler lq_etxfloat_handler = {
  "etx (float)",

  NULL,
  NULL,

  &lq_etxfloat_calc_link_entry_cost,
  &lq_etxfloat_calc_lq_hello_neighbor_cost,
  &lq_etxfloat_calc_tc_edge_entry_cost,

  &lq_etxfloat_is_relevant_costchange,

  &lq_etxfloat_packet_loss_handler,

  &lq_etxfloat_memorize_foreign_hello,
  &lq_etxfloat_copy_link_entry_lq_into_tc_edge_entry,

  NULL,
  NULL,
  NULL,

  &lq_etxfloat_serialize_hello_lq,
  &lq_etxfloat_serialize_tc_lq,
  &lq_etxfloat_deserialize_hello_lq,
  &lq_etxfloat_deserialize_tc_lq,

  &lq_etxfloat_get_linkentry_data,
  &lq_etxfloat_print_cost,
  &lq_etxfloat_print_link_entry_lq,

  lq_etxfloat_linktypes,
  ARRAYSIZE(lq_etxfloat_linktypes),

  sizeof(struct lq_etxfloat_tc_edge),
  sizeof(struct lq_etxfloat_lq_hello_neighbor),
  sizeof(struct lq_etxfloat_link_entry),

  LQ_HELLO_MESSAGE,
  LQ_TC_MESSAGE,

  4,4
};

static float lq_aging = LQ_FLOAT_DEFAULT_AGING;

static const struct olsrd_plugin_parameters plugin_parameters[] = {
  {.name = "LinkQualityAging",.set_plugin_parameter = &set_plugin_float,.data = &lq_aging},
};

OLSR_PLUGIN6(plugin_parameters) {
  .descr = PLUGIN_DESCR,
  .author = PLUGIN_AUTHOR,
  .enable = lq_etxfloat_enable,
  .type = PLUGIN_TYPE_LQ
};

static int
set_plugin_float(const char *value, void *data, set_plugin_parameter_addon addon __attribute__ ((unused)))
{
  if (data != NULL) {
    sscanf(value, "%f", (float *)data);
    OLSR_INFO(LOG_LQ_PLUGINS, "%s float %f\n", "Got", *(float *)data);
  } else {
    OLSR_INFO(LOG_LQ_PLUGINS, "%s float %s\n", "Ignored", value);
  }
  return 0;
}

static bool lq_etxfloat_enable(void) {
  active_lq_handler = &lq_etxfloat_handler;
  return false;
}

static olsr_linkcost
lq_etxfloat_calc_linkcost(struct lq_etxfloat_linkquality *lq)
{
  olsr_linkcost cost;

  if (lq->valueLq < MINIMAL_USEFUL_LQ || lq->valueNlq < MINIMAL_USEFUL_LQ) {
    return LINK_COST_BROKEN;
  }

  cost = (olsr_linkcost) (1.0 / (lq->valueLq * lq->valueNlq) * LQ_PLUGIN_LC_MULTIPLIER);

  if (cost > LINK_COST_BROKEN)
    return LINK_COST_BROKEN;
  if (cost == 0) {
    return 1;
  }
  return cost;
}

static olsr_linkcost
lq_etxfloat_calc_link_entry_cost(struct link_entry *link)
{
  struct lq_etxfloat_link_entry *lq_link = (struct lq_etxfloat_link_entry *)link;

  return lq_etxfloat_calc_linkcost(&lq_link->lq);
}

static olsr_linkcost
lq_etxfloat_calc_lq_hello_neighbor_cost(struct lq_hello_neighbor *neigh)
{
  struct lq_etxfloat_lq_hello_neighbor *lq_neigh = (struct lq_etxfloat_lq_hello_neighbor *)neigh;

  return lq_etxfloat_calc_linkcost(&lq_neigh->lq);
}

static olsr_linkcost
lq_etxfloat_calc_tc_edge_entry_cost(struct tc_edge_entry *edge)
{
  struct lq_etxfloat_tc_edge *lq_edge = (struct lq_etxfloat_tc_edge *)edge;

  return lq_etxfloat_calc_linkcost(&lq_edge->lq);
}

static bool
lq_etxfloat_is_relevant_costchange(olsr_linkcost c1, olsr_linkcost c2)
{
  if (c1 > c2) {
    return c2 - c1 > LQ_PLUGIN_RELEVANT_COSTCHANGE;
  }
  return c1 - c2 > LQ_PLUGIN_RELEVANT_COSTCHANGE;
}

static olsr_linkcost
lq_etxfloat_packet_loss_handler(struct link_entry *link, bool loss)
{
  struct lq_etxfloat_link_entry *lq_link = (struct lq_etxfloat_link_entry *)link;

  float alpha = lq_aging;

  if (lq_link->quickstart < LQ_QUICKSTART_STEPS) {
    /* fast enough to get the LQ value within 6 Hellos up to 0.9 */
    alpha = LQ_FLOAT_QUICKSTART_AGING;
    lq_link->quickstart++;
  }
  // exponential moving average
  lq_link->lq.valueLq *= (1 - alpha);
  if (!loss) {
    lq_link->lq.valueLq += (alpha * link->loss_link_multiplier / 65536);
  }
  return lq_etxfloat_calc_linkcost(&lq_link->lq);
}

static void
lq_etxfloat_memorize_foreign_hello(struct link_entry *target, struct lq_hello_neighbor *source)
{
  struct lq_etxfloat_link_entry *lq_target = (struct lq_etxfloat_link_entry *)target;
  struct lq_etxfloat_lq_hello_neighbor *lq_source = (struct lq_etxfloat_lq_hello_neighbor *)source;

  if (source) {
    lq_target->lq.valueNlq = lq_source->lq.valueLq;
  } else {
    lq_target->lq.valueNlq = 0;
  }

}

static void
lq_etxfloat_copy_link_entry_lq_into_tc_edge_entry(struct tc_edge_entry *target, struct link_entry *source)
{
  struct lq_etxfloat_tc_edge *lq_target = (struct lq_etxfloat_tc_edge *)target;
  struct lq_etxfloat_link_entry *lq_source = (struct lq_etxfloat_link_entry *)source;

  lq_target->lq = lq_source->lq;
}

static void
lq_etxfloat_serialize_hello_lq(uint8_t **curr, struct link_entry *link)
{
  struct lq_etxfloat_link_entry *lq_link = (struct lq_etxfloat_link_entry *)link;

  pkt_put_u8(curr, (unsigned char)(lq_link->lq.valueLq * 255));
  pkt_put_u8(curr, (unsigned char)(lq_link->lq.valueNlq * 255));
  pkt_put_u8(curr, 0);
  pkt_put_u8(curr, 0);
}

static void
lq_etxfloat_serialize_tc_lq(uint8_t **curr, struct link_entry *link)
{
  struct lq_etxfloat_link_entry *lq_link = (struct lq_etxfloat_link_entry *)link;

  pkt_put_u8(curr, (unsigned char)(lq_link->lq.valueLq * 255));
  pkt_put_u8(curr, (unsigned char)(lq_link->lq.valueNlq * 255));
  pkt_put_u8(curr, 0);
  pkt_put_u8(curr, 0);
}

static void
lq_etxfloat_deserialize_hello_lq(uint8_t const **curr, struct lq_hello_neighbor *neigh)
{
  struct lq_etxfloat_lq_hello_neighbor *lq_neigh = (struct lq_etxfloat_lq_hello_neighbor *)neigh;

  uint8_t lq_value, nlq_value;

  pkt_get_u8(curr, &lq_value);
  pkt_get_u8(curr, &nlq_value);
  pkt_ignore_u16(curr);

  lq_neigh->lq.valueLq = (float)lq_value / 255.0;
  lq_neigh->lq.valueNlq = (float)nlq_value / 255.0;
}

static void
lq_etxfloat_deserialize_tc_lq(uint8_t const **curr, struct tc_edge_entry *edge)
{
  struct lq_etxfloat_tc_edge *lq_edge = (struct lq_etxfloat_tc_edge *)edge;

  uint8_t lq_value, nlq_value;

  pkt_get_u8(curr, &lq_value);
  pkt_get_u8(curr, &nlq_value);
  pkt_ignore_u16(curr);

  lq_edge->lq.valueLq = (float)lq_value / 255.0;
  lq_edge->lq.valueNlq = (float)nlq_value / 255.0;
}

static int lq_etxfloat_get_linkentry_data(struct link_entry *link, int idx) {
  struct lq_etxfloat_link_entry *lq_link = (struct lq_etxfloat_link_entry *)link;
  float value =  idx == 1 ? lq_link->lq.valueLq : lq_link->lq.valueNlq;

  return (int)(value * 1000);
}

static const char *
lq_etxfloat_print_link_entry_lq(struct link_entry *link, int idx, char *buffer, size_t bufsize)
{
  struct lq_etxfloat_link_entry *lq_link = (struct lq_etxfloat_link_entry *)link;
  snprintf(buffer, bufsize, "%2.3f",
      idx==1 ? lq_link->lq.valueLq : lq_link->lq.valueNlq);
  return buffer;
}

static const char *
lq_etxfloat_print_cost(olsr_linkcost cost, char *buffer, size_t bufsize)
{
  snprintf(buffer, bufsize, "%2.3f", ((float)cost) / LQ_PLUGIN_LC_MULTIPLIER);
  return buffer;
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

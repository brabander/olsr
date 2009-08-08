
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
#include "lq_plugin_etx_fpm.h"
#include "olsr_logging.h"
#include "common/string.h"

#define PLUGIN_DESCR    "Integer arithmetic based ETX metric with exponential aging"
#define PLUGIN_AUTHOR   "Sven-Ola Tuecke and Henning Rogge"

#define LQ_FPM_INTERNAL_MULTIPLIER 65535
#define LQ_FPM_LINKCOST_MULTIPLIER 65535

#define LQ_FPM_DEFAULT_AGING       3276        /* 65536 * 0.05 */
#define LQ_FPM_QUICKSTART_AGING    16384       /* 65536 * 0.25 */
#define LQ_QUICKSTART_STEPS        12

static bool lq_etxfpm_post_init(void);
static int set_plugin_aging(const char *, void *, set_plugin_parameter_addon);

static olsr_linkcost lq_etxfpm_calc_link_entry_cost(struct link_entry *);
static olsr_linkcost lq_etxfpm_calc_lq_hello_neighbor_cost(struct lq_hello_neighbor *);
static olsr_linkcost lq_etxfpm_calc_tc_mpr_addr_cost(struct tc_mpr_addr *);
static olsr_linkcost lq_etxfpm_calc_tc_edge_entry_cost(struct tc_edge_entry *);

static bool lq_etxfpm_is_relevant_costchange(olsr_linkcost c1, olsr_linkcost c2);

static olsr_linkcost lq_etxfpm_packet_loss_handler(struct link_entry *, bool);

static void lq_etxfpm_memorize_foreign_hello(struct link_entry *, struct lq_hello_neighbor *);
static void lq_etxfpm_copy_link_entry_lq_into_tc_mpr_addr(struct tc_mpr_addr *target, struct link_entry *source);
static void lq_etxfpm_copy_link_entry_lq_into_tc_edge_entry(struct tc_edge_entry *target, struct link_entry *source);
static void lq_etxfpm_copy_link_lq_into_neighbor(struct lq_hello_neighbor *target, struct link_entry *source);

static int lq_etxfpm_serialize_hello_lq(unsigned char *buff, struct lq_hello_neighbor *lq);
static int lq_etxfpm_serialize_tc_lq(unsigned char *buff, struct tc_mpr_addr *lq);
static void lq_etxfpm_deserialize_hello_lq(uint8_t const **curr, struct lq_hello_neighbor *lq);
static void lq_etxfpm_deserialize_tc_lq(uint8_t const **curr, struct tc_edge_entry *lq);

static int lq_etxfpm_get_linkentry_data(struct link_entry *, int);
static const char *lq_etxfpm_print_cost(olsr_linkcost cost, char *buffer, size_t bufsize);
static const char *lq_etxfpm_print_link_entry_lq(struct link_entry *entry, int idx, char *buffer, size_t bufsize);

/* etx lq plugin (fpm version) settings */
struct lq_linkdata_type lq_etxfpm_linktypes[] = {
  { "ETX", 5, 1000, 1000*2, 1000*4, INT32_MAX },
  { "LQ", 5, 255, 240, 192, 0 },
  { "NLQ", 5, 255, 240, 192, 0 }
};

struct lq_handler lq_etxfpm_handler = {
  "etx (fpm)",

  NULL,
  NULL,

  &lq_etxfpm_calc_link_entry_cost,
  &lq_etxfpm_calc_lq_hello_neighbor_cost,
  &lq_etxfpm_calc_tc_mpr_addr_cost,
  &lq_etxfpm_calc_tc_edge_entry_cost,

  &lq_etxfpm_is_relevant_costchange,

  &lq_etxfpm_packet_loss_handler,

  &lq_etxfpm_memorize_foreign_hello,
  &lq_etxfpm_copy_link_entry_lq_into_tc_mpr_addr,
  &lq_etxfpm_copy_link_entry_lq_into_tc_edge_entry,
  &lq_etxfpm_copy_link_lq_into_neighbor,

  NULL,
  NULL,
  NULL,
  NULL,

  &lq_etxfpm_serialize_hello_lq,
  &lq_etxfpm_serialize_tc_lq,
  &lq_etxfpm_deserialize_hello_lq,
  &lq_etxfpm_deserialize_tc_lq,

  &lq_etxfpm_get_linkentry_data,
  &lq_etxfpm_print_cost,
  &lq_etxfpm_print_link_entry_lq,

  lq_etxfpm_linktypes,
  ARRAYSIZE(lq_etxfpm_linktypes),

  sizeof(struct lq_etxfpm_tc_edge),
  sizeof(struct lq_etxfpm_tc_mpr_addr),
  sizeof(struct lq_etxfpm_lq_hello_neighbor),
  sizeof(struct lq_etxfpm_link_entry),

  LQ_HELLO_MESSAGE,
  LQ_TC_MESSAGE,

  4,4
};

static uint32_t aging_factor_new = LQ_FPM_DEFAULT_AGING;
static uint32_t aging_factor_old = LQ_FPM_INTERNAL_MULTIPLIER - LQ_FPM_DEFAULT_AGING;
static uint32_t aging_quickstart_new = LQ_FPM_QUICKSTART_AGING;
static uint32_t aging_quickstart_old = LQ_FPM_INTERNAL_MULTIPLIER - LQ_FPM_QUICKSTART_AGING;

static const struct olsrd_plugin_parameters plugin_parameters[] = {
  {.name = "LinkQualityAging",.set_plugin_parameter = &set_plugin_aging,.data = NULL}
};

DEFINE_PLUGIN6(PLUGIN_DESCR, PLUGIN_AUTHOR, NULL, lq_etxfpm_post_init, NULL, NULL, false, plugin_parameters)

static bool lq_etxfpm_post_init(void) {
  active_lq_handler = &lq_etxfpm_handler;
  return false;
}

static int
set_plugin_aging(const char *value, void *data  __attribute__ ((unused)),
    set_plugin_parameter_addon addon __attribute__ ((unused)))
{
  uint32_t aging;

  if (value[0] != '0' || value[1] != '.') {
    OLSR_ERROR(LOG_PLUGINS, "LQ aging value must be between 0 and 1.\n");
    return 1;
  }

  aging = strtoul(&value[2], NULL, 10);
  /* support only 4 digits after . */
  while (aging >= 10000) {
    aging /= 10;
  }

  aging_factor_new = aging * LQ_FPM_INTERNAL_MULTIPLIER;

  while (aging > 0) {
    aging /= 10;
    aging_factor_new /= 10;
  }

  aging_factor_old = LQ_FPM_INTERNAL_MULTIPLIER - aging_factor_new;
  return 0;
}

static olsr_linkcost
lq_etxfpm_calc_linkcost(struct lq_etxfpm_linkquality *lq)
{
  olsr_linkcost cost;

  if (lq->valueLq < (unsigned int)(255 * MINIMAL_USEFUL_LQ) || lq->valueNlq < (unsigned int)(255 * MINIMAL_USEFUL_LQ)) {
    return LINK_COST_BROKEN;
  }

  cost = LQ_FPM_LINKCOST_MULTIPLIER * 255 / (int)lq->valueLq * 255 / (int)lq->valueNlq;

  if (cost > LINK_COST_BROKEN)
    return LINK_COST_BROKEN;
  if (cost == 0)
    return 1;
  return cost;
}

static olsr_linkcost
lq_etxfpm_calc_link_entry_cost(struct link_entry *link)
{
  struct lq_etxfpm_link_entry *lq_link = (struct lq_etxfpm_link_entry *)link;

  return lq_etxfpm_calc_linkcost(&lq_link->lq);
}

static olsr_linkcost
lq_etxfpm_calc_lq_hello_neighbor_cost(struct lq_hello_neighbor *neigh)
{
  struct lq_etxfpm_lq_hello_neighbor *lq_neigh = (struct lq_etxfpm_lq_hello_neighbor *)neigh;

  return lq_etxfpm_calc_linkcost(&lq_neigh->lq);
}

static olsr_linkcost
lq_etxfpm_calc_tc_mpr_addr_cost(struct tc_mpr_addr *mpr)
{
  struct lq_etxfpm_tc_mpr_addr *lq_mpr = (struct lq_etxfpm_tc_mpr_addr *)mpr;

  return lq_etxfpm_calc_linkcost(&lq_mpr->lq);
}

static olsr_linkcost
lq_etxfpm_calc_tc_edge_entry_cost(struct tc_edge_entry *edge)
{
  struct lq_etxfpm_tc_edge *lq_edge = (struct lq_etxfpm_tc_edge *)edge;

  return lq_etxfpm_calc_linkcost(&lq_edge->lq);
}

static bool
lq_etxfpm_is_relevant_costchange(olsr_linkcost c1, olsr_linkcost c2)
{
  if (c1 > c2) {
    return c2 - c1 > LQ_PLUGIN_RELEVANT_COSTCHANGE;
  }
  return c1 - c2 > LQ_PLUGIN_RELEVANT_COSTCHANGE;
}

static olsr_linkcost
lq_etxfpm_packet_loss_handler(struct link_entry *link, bool loss)
{
  struct lq_etxfpm_link_entry *lq_link = (struct lq_etxfpm_link_entry *)link;

  uint32_t alpha_old = aging_factor_old;
  uint32_t alpha_new = aging_factor_new;

  uint32_t value;
  // fpm link_loss_factor = fpmidiv(itofpm(link->loss_link_multiplier), 65536);

  if (lq_link->quickstart < LQ_QUICKSTART_STEPS) {
    alpha_new = aging_quickstart_new;
    alpha_old = aging_quickstart_old;
    lq_link->quickstart++;
  }
  // exponential moving average
  value = (uint32_t) (lq_link->lq.valueLq) * LQ_FPM_INTERNAL_MULTIPLIER / 255;

  value = (value * alpha_old + LQ_FPM_INTERNAL_MULTIPLIER - 1) / LQ_FPM_INTERNAL_MULTIPLIER;

  if (!loss) {
    uint32_t ratio;

    ratio = (alpha_new * link->loss_link_multiplier + LINK_LOSS_MULTIPLIER - 1) / LINK_LOSS_MULTIPLIER;
    value += ratio;
  }
  lq_link->lq.valueLq = (value * 255 + LQ_FPM_INTERNAL_MULTIPLIER - 1) / LQ_FPM_INTERNAL_MULTIPLIER;

  return lq_etxfpm_calc_linkcost(&lq_link->lq);
}

static void
lq_etxfpm_memorize_foreign_hello(struct link_entry *target, struct lq_hello_neighbor *source)
{
  struct lq_etxfpm_link_entry *lq_target = (struct lq_etxfpm_link_entry *)target;
  struct lq_etxfpm_lq_hello_neighbor *lq_source = (struct lq_etxfpm_lq_hello_neighbor *)source;

  if (source) {
    lq_target->lq.valueNlq = lq_source->lq.valueLq;
  } else {
    lq_target->lq.valueNlq = 0;
  }

}

static void
lq_etxfpm_copy_link_entry_lq_into_tc_mpr_addr(struct tc_mpr_addr *target, struct link_entry *source)
{
  struct lq_etxfpm_tc_mpr_addr *lq_target = (struct lq_etxfpm_tc_mpr_addr *)target;
  struct lq_etxfpm_link_entry *lq_source = (struct lq_etxfpm_link_entry *)source;

  lq_target->lq = lq_source->lq;
}

static void
lq_etxfpm_copy_link_entry_lq_into_tc_edge_entry(struct tc_edge_entry *target, struct link_entry *source)
{
  struct lq_etxfpm_tc_edge *lq_target = (struct lq_etxfpm_tc_edge *)target;
  struct lq_etxfpm_link_entry *lq_source = (struct lq_etxfpm_link_entry *)source;

  lq_target->lq = lq_source->lq;
}

static void
lq_etxfpm_copy_link_lq_into_neighbor(struct lq_hello_neighbor *target, struct link_entry *source)
{
  struct lq_etxfpm_lq_hello_neighbor *lq_target = (struct lq_etxfpm_lq_hello_neighbor *)target;
  struct lq_etxfpm_link_entry *lq_source = (struct lq_etxfpm_link_entry *)source;

  lq_target->lq = lq_source->lq;
}

static int
lq_etxfpm_serialize_hello_lq(unsigned char *buff, struct lq_hello_neighbor *neigh)
{
  struct lq_etxfpm_lq_hello_neighbor *lq_neigh = (struct lq_etxfpm_lq_hello_neighbor *)neigh;

  buff[0] = (unsigned char)lq_neigh->lq.valueLq;
  buff[1] = (unsigned char)lq_neigh->lq.valueNlq;
  buff[2] = (unsigned char)(0);
  buff[3] = (unsigned char)(0);

  return 4;
}
static int
lq_etxfpm_serialize_tc_lq(unsigned char *buff, struct tc_mpr_addr *mpr)
{
  struct lq_etxfpm_tc_mpr_addr *lq_mpr = (struct lq_etxfpm_tc_mpr_addr *)mpr;

  buff[0] = (unsigned char)lq_mpr->lq.valueLq;
  buff[1] = (unsigned char)lq_mpr->lq.valueNlq;
  buff[2] = (unsigned char)(0);
  buff[3] = (unsigned char)(0);

  return 4;
}

static void
lq_etxfpm_deserialize_hello_lq(uint8_t const **curr, struct lq_hello_neighbor *neigh)
{
  struct lq_etxfpm_lq_hello_neighbor *lq_neigh = (struct lq_etxfpm_lq_hello_neighbor *)neigh;

  pkt_get_u8(curr, &lq_neigh->lq.valueLq);
  pkt_get_u8(curr, &lq_neigh->lq.valueNlq);
  pkt_ignore_u16(curr);

}
static void
lq_etxfpm_deserialize_tc_lq(uint8_t const **curr, struct tc_edge_entry *edge)
{
  struct lq_etxfpm_tc_edge *lq_edge = (struct lq_etxfpm_tc_edge *)edge;

  pkt_get_u8(curr, &lq_edge->lq.valueLq);
  pkt_get_u8(curr, &lq_edge->lq.valueNlq);
  pkt_ignore_u16(curr);
}

static int lq_etxfpm_get_linkentry_data(struct link_entry *link, int idx) {
  struct lq_etxfpm_link_entry *lq_link = (struct lq_etxfpm_link_entry *)link;

  return (idx == 1) ? lq_link->lq.valueLq : lq_link->lq.valueNlq;
}

static const char *
lq_etxfpm_print_link_entry_lq(struct link_entry *link, int idx, char *buffer, size_t bufsize)
{
  struct lq_etxfpm_link_entry *lq_link = (struct lq_etxfpm_link_entry *)link;

  uint8_t value = (idx == 1) ? lq_link->lq.valueLq : lq_link->lq.valueNlq;
  if (value == 255) {
    strscpy(buffer, "1.000", bufsize);
  } else {
    snprintf(buffer, bufsize, "0.%03d", (value * 1000) / 255);
  }
  return buffer;
}

static const char *
lq_etxfpm_print_cost(olsr_linkcost cost, char *buffer, size_t bufsize)
{
  // must calculate
  uint32_t roundDown = cost >> 16;
  uint32_t fraction = ((cost & 0xffff) * 1000) >> 16;

  snprintf(buffer, bufsize, "%u.%03u", roundDown, fraction);
  return buffer;
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

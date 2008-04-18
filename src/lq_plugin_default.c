/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2008 Henning Rogge <rogge@fgan.de>
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
#include "lq_route.h"
#include "lq_packet.h"
#include "packet.h"
#include "olsr.h"
#include "lq_plugin_default.h"

olsr_linkcost default_calc_cost(const void *ptr) {
  const struct default_lq *lq = ptr;
  olsr_linkcost cost;
  
  if (lq->lq < 0.1 || lq->nlq < 0.1) {
    return LINK_COST_BROKEN;
  }
  
  cost = (olsr_linkcost)(1.0/(lq->lq * lq->nlq) * LQ_PLUGIN_LC_MULTIPLIER);
  
  if (cost > LINK_COST_BROKEN)
    return LINK_COST_BROKEN;
  if (cost == 0) {
    return 1;
  }
  return cost;
}

int default_olsr_serialize_hello_lq_pair(unsigned char *buff, void *ptr) {
  struct default_lq *lq = ptr;
  
  olsr_u16_t lq_value = (olsr_u16_t)(lq->lq * 65535);
  olsr_u16_t nlq_value = (olsr_u16_t)(lq->nlq * 65535);
  
  buff[0] = (unsigned char)(lq_value / 256);
  buff[1] = (unsigned char)(nlq_value / 256);
  buff[2] = (unsigned char)(lq_value & 255);
  buff[3] = (unsigned char)(nlq_value & 255);
  
  return 4;
}

void default_olsr_deserialize_hello_lq_pair(const olsr_u8_t **curr, void *ptr) {
  struct default_lq *lq = ptr;
  
  olsr_u8_t lq_high, lq_low, nlq_high, nlq_low;
  olsr_u16_t lq_value, nlq_value;
  
  pkt_get_u8(curr, &lq_high);
  pkt_get_u8(curr, &nlq_high);
  pkt_get_u8(curr, &lq_low);
  pkt_get_u8(curr, &nlq_low);
  
  lq_value = 256 * (olsr_u16_t)lq_high + (olsr_u16_t)lq_low;
  nlq_value = 256 * (olsr_u16_t)nlq_high + (olsr_u16_t)nlq_low;
  
  lq->lq = (float)lq_value / 65535.0;
  lq->nlq = (float)nlq_value / 65535.0;
}

olsr_bool default_olsr_is_relevant_costchange(olsr_linkcost c1, olsr_linkcost c2) {
  if (c1 > c2) {
    return c2 - c1 > LQ_PLUGIN_RELEVANT_COSTCHANGE;
  }
  return c1 - c2 > LQ_PLUGIN_RELEVANT_COSTCHANGE;
}

int default_olsr_serialize_tc_lq_pair(unsigned char *buff, void *ptr) {
  struct default_lq *lq = ptr;
  
  olsr_u16_t lq_value = (olsr_u16_t)(lq->lq * 65535);
  olsr_u16_t nlq_value = (olsr_u16_t)(lq->nlq * 65535);
  
  buff[0] = (unsigned char)(lq_value / 256);
  buff[1] = (unsigned char)(nlq_value / 256);
  buff[2] = (unsigned char)(lq_value & 255);
  buff[3] = (unsigned char)(nlq_value & 255);
  
  return 4;
}

void default_olsr_deserialize_tc_lq_pair(const olsr_u8_t **curr, void *ptr) {
  struct default_lq *lq = ptr;
  
  olsr_u8_t lq_high, lq_low, nlq_high, nlq_low;
  olsr_u16_t lq_value, nlq_value;
  
  pkt_get_u8(curr, &lq_high);
  pkt_get_u8(curr, &nlq_high);
  pkt_get_u8(curr, &lq_low);
  pkt_get_u8(curr, &nlq_low);
  
  lq_value = 256 * (olsr_u16_t)lq_high + (olsr_u16_t)lq_low;
  nlq_value = 256 * (olsr_u16_t)nlq_high + (olsr_u16_t)nlq_low;
  
  lq->lq = (float)lq_value / 65535.0;
  lq->nlq = (float)nlq_value / 65535.0;
}

olsr_linkcost default_packet_loss_worker(void *ptr, olsr_bool lost) {
  struct default_lq *tlq = ptr;
  float alpha;
  
  // calculate exponental factor for the new link quality, could be directly done in configuration !
  alpha = 1 / (float)(olsr_cnf->lq_wsize);
  
  // exponential moving average
  tlq->lq *= (1 - alpha);
  if (lost == 0) {
    tlq->lq += alpha;
  }
  return default_calc_cost(ptr);
}

void default_olsr_memorize_foreign_hello_lq(void *ptrLocal, void *ptrForeign) {
  struct default_lq *local = ptrLocal;
  struct default_lq *foreign = ptrForeign;
  
  if (foreign) {
    local->nlq = foreign->lq;
  }
  else {
    local->nlq = 0;
  }
}

void default_olsr_copy_link_lq_into_tc(void *target, void *source) {
  memcpy(target, source, sizeof(struct default_lq));
}

void default_olsr_clear_lq(void *target) {
  memset(target, 0, sizeof(struct default_lq));
}

const char *default_olsr_print_lq(void *ptr, struct lqtextbuffer *buffer) {
  struct default_lq *lq = ptr;
  
  sprintf(buffer->buf, "%2.3f/%2.3f", lq->lq, lq->nlq);
  return buffer->buf;
}

const char *default_olsr_print_cost(olsr_linkcost cost, struct lqtextbuffer *buffer) {
  sprintf(buffer->buf, "%2.3f", ((float)cost)/LQ_PLUGIN_LC_MULTIPLIER);
  return buffer->buf;
}

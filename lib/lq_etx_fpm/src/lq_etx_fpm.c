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
#include "lq_plugin.h"
#include "lq_route.h"
#include "lq_packet.h"
#include "packet.h"
#include "olsr.h"
#include "lq_etx_fpm.h"
#include "fpm.h"

/* etx lq plugin (fpm version) settings */
struct lq_handler lq_etx_fpm_handler = {
    &lq_etx_fpm_calc_cost,
    &lq_etx_fpm_calc_cost,
    
    &lq_etx_fpm_olsr_is_relevant_costchange,
    
    &lq_etx_fpm_packet_loss_worker,
    &lq_etx_fpm_olsr_memorize_foreign_hello_lq,
    &lq_etx_fpm_olsr_copy_link_lq_into_tc,
    &lq_etx_fpm_olsr_clear_lq,
    &lq_etx_fpm_olsr_clear_lq,
    
    &lq_etx_fpm_olsr_serialize_hello_lq_pair,
    &lq_etx_fpm_olsr_serialize_tc_lq_pair,
    &lq_etx_fpm_olsr_deserialize_hello_lq_pair,
    &lq_etx_fpm_olsr_deserialize_tc_lq_pair,
    
    &lq_etx_fpm_olsr_print_lq,
    &lq_etx_fpm_olsr_print_lq,
    &lq_etx_fpm_olsr_print_cost, 
    
    sizeof(struct lq_etx_fpm),
    sizeof(struct lq_etx_fpm)
};

fpm MINIMAL_LQ;
fpm aging_factor1, aging_factor2;

void set_lq_etx_fpm_alpha(fpm alpha) {
  OLSR_PRINTF(3, "lq_etx_fpm: Set alpha to %s\n", fpmtoa(alpha));
  aging_factor1 = alpha;
  aging_factor2 = fpmsub(itofpm(1), alpha);
}

int init_lq_etx_fpm(void) {
  if (aging_factor1 == 0 && aging_factor2 == 0) {
    OLSR_PRINTF(1, "Alpha factor for lq_etx_fgm not set !\n");
    return 0; // error
  }
  
  MINIMAL_LQ = ftofpm(0.1);
  
  // activate plugin
  set_lq_handler(&lq_etx_fpm_handler, LQ_ETX_FPM_HANDLER_NAME);
  return 1;
}

olsr_linkcost lq_etx_fpm_calc_cost(const void *ptr) {
  const struct lq_etx_fpm *lq = ptr;
  olsr_linkcost cost;
  
  if (lq->lq < MINIMAL_LQ || lq->nlq < MINIMAL_LQ) {
    return LINK_COST_BROKEN;
  }
  
  cost = fpmdiv(itofpm(1), fpmmul(lq->lq, lq->nlq));

  if (cost > LINK_COST_BROKEN)
    return LINK_COST_BROKEN;
  if (cost == 0)
    return 1;
  return cost;
}

int lq_etx_fpm_olsr_serialize_hello_lq_pair(unsigned char *buff, void *ptr) {
  struct lq_etx_fpm *lq = ptr;
  
  buff[0] = (unsigned char)fpmtoi(fpmmuli(lq->lq, 255));
  buff[1] = (unsigned char)fpmtoi(fpmmuli(lq->nlq, 255));
  buff[2] = (unsigned char)(0);
  buff[3] = (unsigned char)(0);
  
  return 4;
}

void lq_etx_fpm_olsr_deserialize_hello_lq_pair(const olsr_u8_t **curr, void *ptr) {
  struct lq_etx_fpm *lq = ptr;
  olsr_u8_t valueLq, valueNlq;
  
  pkt_get_u8(curr, &valueLq);
  pkt_get_u8(curr, &valueNlq);
  pkt_ignore_u16(curr);
  
  lq->lq = fpmidiv(itofpm((int)valueLq), 255);
  lq->nlq = fpmidiv(itofpm((int)valueNlq), 255);
}

olsr_bool lq_etx_fpm_olsr_is_relevant_costchange(olsr_linkcost c1, olsr_linkcost c2) {
  if (c1 > c2) {
    return c2 - c1 > LQ_PLUGIN_RELEVANT_COSTCHANGE;
  }
  return c1 - c2 > LQ_PLUGIN_RELEVANT_COSTCHANGE;
}

int lq_etx_fpm_olsr_serialize_tc_lq_pair(unsigned char *buff, void *ptr) {
  struct lq_etx_fpm *lq = ptr;
  
  buff[0] = (unsigned char)fpmtoi(fpmmuli(lq->lq, 255));
  buff[1] = (unsigned char)fpmtoi(fpmmuli(lq->nlq, 255));
  buff[2] = (unsigned char)(0);
  buff[3] = (unsigned char)(0);
  
  return 4;
}

void lq_etx_fpm_olsr_deserialize_tc_lq_pair(const olsr_u8_t **curr, void *ptr) {
  struct lq_etx_fpm *lq = ptr;
  olsr_u8_t valueLq, valueNlq;
  
  pkt_get_u8(curr, &valueLq);
  pkt_get_u8(curr, &valueNlq);
  pkt_ignore_u16(curr);
  
  lq->lq = fpmidiv(itofpm(valueLq), 255);
  lq->nlq = fpmidiv(itofpm(valueNlq), 255);
}

olsr_linkcost lq_etx_fpm_packet_loss_worker(void *ptr, olsr_bool lost) {
  struct lq_etx_fpm *tlq = ptr;
  
  // exponential moving average
  tlq->lq = fpmmul(tlq->lq, aging_factor2);
  if (lost == 0) {
    tlq->lq = fpmadd(tlq->lq, aging_factor1);
  }
  return lq_etx_fpm_calc_cost(ptr);
}

void lq_etx_fpm_olsr_memorize_foreign_hello_lq(void *ptrLocal, void *ptrForeign) {
  struct lq_etx_fpm *local = ptrLocal;
  struct lq_etx_fpm *foreign = ptrForeign;
  
  if (foreign) {
    local->nlq = foreign->lq;
  }
  else {
    local->nlq = itofpm(0);
  }
}

void lq_etx_fpm_olsr_copy_link_lq_into_tc(void *target, void *source) {
  memcpy(target, source, sizeof(struct lq_etx_fpm));
}

void lq_etx_fpm_olsr_clear_lq(void *target) {
  memset(target, 0, sizeof(struct lq_etx_fpm));
}

const char *lq_etx_fpm_olsr_print_lq(void *ptr, struct lqtextbuffer *buffer) {
  struct lq_etx_fpm *lq = ptr;
  
  sprintf(buffer->buf, "%s/%s", fpmtoa(lq->lq), fpmtoa(lq->nlq));
  return buffer->buf;
}

const char *lq_etx_fpm_olsr_print_cost(olsr_linkcost cost, struct lqtextbuffer *buffer) {
  sprintf(buffer->buf, "%s", fpmtoa(cost));
  return buffer->buf;
}

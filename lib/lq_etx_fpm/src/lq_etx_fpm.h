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

#ifndef LQ_ETX_FPM_
#define LQ_ETX_FPM_

#include "olsr_types.h"
#include "fpm.h"
#include "lq_plugin.h"

#define LQ_PLUGIN_LC_MULTIPLIER 1024
#define LQ_PLUGIN_RELEVANT_COSTCHANGE 16

#define LQ_ETX_FPM_HANDLER_NAME "ETX metric with FPM"
struct lq_etx_fpm {
	fpm lq, nlq;
};

void set_lq_etx_fpm_alpha(fpm alpha);
int init_lq_etx_fpm(void);

olsr_linkcost lq_etx_fpm_calc_cost(const void *lq);

olsr_bool lq_etx_fpm_olsr_is_relevant_costchange(olsr_linkcost c1, olsr_linkcost c2);

olsr_linkcost lq_etx_fpm_packet_loss_worker(void *lq, olsr_bool lost);
void lq_etx_fpm_olsr_memorize_foreign_hello_lq(void *local, void *foreign);

int lq_etx_fpm_olsr_serialize_hello_lq_pair(unsigned char *buff, void *lq);
void lq_etx_fpm_olsr_deserialize_hello_lq_pair(const olsr_u8_t **curr, void *lq);
int lq_etx_fpm_olsr_serialize_tc_lq_pair(unsigned char *buff, void *lq);
void lq_etx_fpm_olsr_deserialize_tc_lq_pair(const olsr_u8_t **curr, void *lq);

void lq_etx_fpm_olsr_copy_link_lq_into_tc(void *target, void *source);
void lq_etx_fpm_olsr_clear_lq(void *target);

const char *lq_etx_fpm_olsr_print_lq(void *ptr, struct lqtextbuffer *buffer);
const char *lq_etx_fpm_olsr_print_cost(olsr_linkcost cost, struct lqtextbuffer *buffer);

#endif /*LQ_ETX_FPM_*/

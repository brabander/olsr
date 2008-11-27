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
#include "olsr_spf.h"
#include "lq_packet.h"
#include "packet.h"
#include "olsr.h"
#include "lq_plugin_default_ff.h"
#include "parser.h"
#include "mid_set.h"
#include "scheduler.h"

/* etx lq plugin (freifunk fpm version) settings */
struct lq_handler lq_etx_ff_handler = {
    &default_lq_initialize_ff,
    &default_lq_calc_cost_ff,
    &default_lq_calc_cost_ff,

    &default_lq_is_relevant_costchange_ff,
    &default_lq_packet_loss_worker_ff,

    &default_lq_memorize_foreign_hello_ff,
    &default_lq_copy_link2tc_ff,
    &default_lq_clear_ff_hello,
    &default_lq_clear_ff,

    &default_lq_serialize_hello_lq_pair_ff,
    &default_lq_serialize_tc_lq_pair_ff,
    &default_lq_deserialize_hello_lq_pair_ff,
    &default_lq_deserialize_tc_lq_pair_ff,

    &default_lq_print_ff,
    &default_lq_print_ff,
    &default_lq_print_cost_ff,

    sizeof(struct default_lq_ff_hello),
    sizeof(struct default_lq_ff)
};

static char *default_lq_ff_linkcost2text(struct lqtextbuffer *buffer, olsr_linkcost cost) {
	// must calculate
	uint32_t roundDown = cost >> 16;
	uint32_t fraction = ((cost & 0xffff) * 1000) >> 16;

	sprintf(buffer->buf, "%u.%u", roundDown, fraction);
	return buffer->buf;
}

static void default_lq_parser_ff(struct olsr *olsr, struct interface *in_if, union olsr_ip_addr *from_addr) {
  const union olsr_ip_addr *main_addr;
  struct link_entry *lnk;
  struct default_lq_ff_hello *lq;
  uint32_t seq_diff;

  /* Find main address */
  main_addr = olsr_lookup_main_addr_by_alias(from_addr);

  /* Loopup link entry */
  lnk = lookup_link_entry(from_addr, main_addr, in_if);
  if (lnk == NULL) {
    return;
  }

  lq = (struct default_lq_ff_hello *)lnk->linkquality;

  if (lq->last_seq_nr > olsr->olsr_seqno) {
    seq_diff = (uint32_t)olsr->olsr_seqno + 65536 - lq->last_seq_nr;
  } else {
    seq_diff = olsr->olsr_seqno - lq->last_seq_nr;
  }

  /* Jump in sequence numbers ? */
  if (seq_diff > 256) {
    seq_diff = 1;
  }

  lq->received[lq->activePtr]++;
  lq->lost[lq->activePtr] += (seq_diff - 1);

  lq->last_seq_nr = olsr->olsr_seqno;
}

static void default_lq_ff_timer(void __attribute__((unused)) *context) {
  struct link_entry *link;
  OLSR_FOR_ALL_LINK_ENTRIES(link) {
    struct default_lq_ff_hello *tlq = (struct default_lq_ff_hello *)link->linkquality;
    uint32_t ratio;
    uint16_t i, received, lost;

#if !defined(NODEBUG) && defined(DEBUG)
    struct ipaddr_str buf;
    struct lqtextbuffer lqbuffer;

    OLSR_PRINTF(3, "LQ-FF new entry for %s: rec: %u lost: %u",
  		olsr_ip_to_string(&buf, &link->neighbor_iface_addr),
  		tlq->received[tlq->activePtr], tlq->lost[tlq->activePtr]);
#endif
    received = 0;
    lost = 0;

    /* enlarge window if still in quickstart phase */
    if (tlq->windowSize < LQ_FF_WINDOW) {
      tlq->windowSize++;
    }
    for (i=0; i < tlq->windowSize; i++) {
      received += tlq->received[i];
      lost += tlq->lost[i];
    }

#if !defined(NODEBUG) && defined(DEBUG)
    OLSR_PRINTF(3, " total-rec: %u total-lost: %u", received, lost);
#endif
    /* calculate link quality */
    if (received + lost == 0) {
      tlq->lq.valueLq = 0;
    }
    else {
      // start with link-loss-factor
      ratio = link->loss_link_multiplier;

      // calculate received/(received + loss) factor
      ratio = ratio * received;
      ratio = ratio / (received + lost);
      ratio = (ratio * 255) >> 16;

      tlq->lq.valueLq = (uint8_t)(ratio);
    }
    link->linkcost = default_lq_calc_cost_ff(tlq);

#if !defined(NODEBUG) && defined(DEBUG)
    OLSR_PRINTF(3, " linkcost: %s\n", default_lq_ff_linkcost2text(&lqbuffer, link->linkcost));
#endif

    // shift buffer
    tlq->activePtr = (tlq->activePtr + 1) % LQ_FF_WINDOW;
    tlq->lost[tlq->activePtr] = 0;
    tlq->received[tlq->activePtr] = 0;
  }OLSR_FOR_ALL_LINK_ENTRIES_END(link);
}

void default_lq_initialize_ff(void) {
  /* Some cookies for stats keeping */
  static struct olsr_cookie_info *default_lq_ff_timer_cookie = NULL;

  olsr_packetparser_add_function(&default_lq_parser_ff);
  default_lq_ff_timer_cookie = olsr_alloc_cookie("Default Freifunk LQ",
                                                 OLSR_COOKIE_TYPE_TIMER);
  olsr_start_timer(1000, 0, OLSR_TIMER_PERIODIC, &default_lq_ff_timer, NULL,
                   default_lq_ff_timer_cookie->ci_id);
}

olsr_linkcost default_lq_calc_cost_ff(const void *ptr) {
  const struct default_lq_ff *lq = ptr;
  olsr_linkcost cost;

  if (lq->valueLq < (unsigned int)(255 * MINIMAL_USEFUL_LQ) || lq->valueNlq < (unsigned int)(255 * MINIMAL_USEFUL_LQ)) {
    return LINK_COST_BROKEN;
  }

  cost = 65536 * lq->valueLq / 255 * lq->valueNlq / 255;

  if (cost > LINK_COST_BROKEN)
    return LINK_COST_BROKEN;
  if (cost == 0)
    return 1;
  return cost;
}

int default_lq_serialize_hello_lq_pair_ff(unsigned char *buff, void *ptr) {
  struct default_lq_ff *lq = ptr;

  buff[0] = (unsigned char)lq->valueLq;
  buff[1] = (unsigned char)lq->valueNlq;
  buff[2] = (unsigned char)(0);
  buff[3] = (unsigned char)(0);

  return 4;
}

void default_lq_deserialize_hello_lq_pair_ff(const uint8_t **curr, void *ptr) {
  struct default_lq_ff *lq = ptr;

  pkt_get_u8(curr, &lq->valueLq);
  pkt_get_u8(curr, &lq->valueNlq);
  pkt_ignore_u16(curr);
}

bool default_lq_is_relevant_costchange_ff(olsr_linkcost c1, olsr_linkcost c2) {
  if (c1 > c2) {
    return c2 - c1 > LQ_PLUGIN_RELEVANT_COSTCHANGE_FF;
  }
  return c1 - c2 > LQ_PLUGIN_RELEVANT_COSTCHANGE_FF;
}

int default_lq_serialize_tc_lq_pair_ff(unsigned char *buff, void *ptr) {
  struct default_lq_ff *lq = ptr;

  buff[0] = (unsigned char)lq->valueLq;
  buff[1] = (unsigned char)lq->valueNlq;
  buff[2] = (unsigned char)(0);
  buff[3] = (unsigned char)(0);

  return 4;
}

void default_lq_deserialize_tc_lq_pair_ff(const uint8_t **curr, void *ptr) {
  struct default_lq_ff *lq = ptr;

  pkt_get_u8(curr, &lq->valueLq);
  pkt_get_u8(curr, &lq->valueNlq);
  pkt_ignore_u16(curr);
}

olsr_linkcost default_lq_packet_loss_worker_ff(struct link_entry __attribute__((unused)) *link, void __attribute__((unused)) *ptr, bool __attribute__((unused)) lost) {
  return link->linkcost;
}

void default_lq_memorize_foreign_hello_ff(void *ptrLocal, void *ptrForeign) {
  struct default_lq_ff *local = ptrLocal;
  struct default_lq_ff *foreign = ptrForeign;

  if (foreign) {
    local->valueNlq = foreign->valueLq;
  } else {
    local->valueNlq = 0;
  }
}

void default_lq_copy_link2tc_ff(void *target, void *source) {
  memcpy(target, source, sizeof(struct default_lq_ff));
}

void default_lq_clear_ff(void *target) {
  memset(target, 0, sizeof(struct default_lq_ff));
}

void default_lq_clear_ff_hello(void *target) {
  struct default_lq_ff_hello *local = target;
  int i;

  default_lq_clear_ff(&local->lq);
  local->windowSize = LQ_FF_QUICKSTART_INIT;
  for (i=0; i<LQ_FF_WINDOW; i++) {
    local->lost[i] = 3;
  }
}

const char *default_lq_print_ff(void *ptr, char separator, struct lqtextbuffer *buffer) {
  struct default_lq_ff *lq = ptr;
  int i = 0;

  memset(buffer, 0, sizeof(struct lqtextbuffer));

  if (lq->valueLq == 255) {
  	strcpy(buffer->buf, "1.000");
  	i += 5;
  }
  else {
  	i = sprintf(buffer->buf, "0.%03ul", (lq->valueLq * 1000)/255);
  }
  buffer->buf[i++] = separator;

  if (lq->valueNlq == 255) {
  	strcpy(&buffer->buf[i], "1.000");
  }
  else {
  	sprintf(&buffer->buf[i], "0.%03ul", (lq->valueNlq * 1000) / 255);
  }
  return buffer->buf;
}

const char *default_lq_print_cost_ff(olsr_linkcost cost, struct lqtextbuffer *buffer) {
	default_lq_ff_linkcost2text(buffer, cost);
  return buffer->buf;
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

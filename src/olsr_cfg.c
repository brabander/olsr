
/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004, Andreas Tonnesen(andreto@olsr.org)
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

#include "olsr_cfg.h"
#include "olsr.h"
#include "ipcalc.h"
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

/*
#include "ipcalc.h"
#include "olsr_cfg.h"
#include "defs.h"
#include "net_olsr.h"
#include "olsr.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
*/

/* Global stuff externed in defs.h */
FILE *debug_handle;                    /* Where to send debug(defaults to stdout) */
struct olsrd_config *olsr_cnf;         /* The global configuration */

struct olsrd_config *
olsrd_parse_cnf(const char *filename)
{
  fprintf(stderr, "Fixme: olsrd_parse_cnf(%s)\n", filename);
  return olsr_cnf;
}

int
olsrd_sanity_check_cnf(struct olsrd_config *cnf)
{
  struct olsr_if *in = cnf->interfaces;
  struct if_config_options *io;

  /* Debug level */
  if (cnf->debug_level < MIN_DEBUGLVL || cnf->debug_level > MAX_DEBUGLVL) {
    fprintf(stderr, "Debuglevel %d is not allowed\n", cnf->debug_level);
    return -1;
  }

  /* IP version */
  if (cnf->ip_version != AF_INET && cnf->ip_version != AF_INET6) {
    fprintf(stderr, "Ipversion %d not allowed!\n", cnf->ip_version);
    return -1;
  }

  /* TOS */
  if (cnf->tos > MAX_TOS) {
    fprintf(stderr, "TOS %d is not allowed\n", cnf->tos);
    return -1;
  }

  /* Willingness */
  if (cnf->willingness_auto == false && (cnf->willingness > MAX_WILLINGNESS)) {
    fprintf(stderr, "Willingness %d is not allowed\n", cnf->willingness);
    return -1;
  }

  /* Hysteresis */
  if (cnf->use_hysteresis) {
    if (cnf->hysteresis_param.scaling < MIN_HYST_PARAM || cnf->hysteresis_param.scaling > MAX_HYST_PARAM) {
      fprintf(stderr, "Hyst scaling %0.2f is not allowed\n", cnf->hysteresis_param.scaling);
      return -1;
    }

    if (cnf->hysteresis_param.thr_high <= cnf->hysteresis_param.thr_low) {
      fprintf(stderr, "Hyst upper(%0.2f) thr must be bigger than lower(%0.2f) threshold!\n", cnf->hysteresis_param.thr_high,
              cnf->hysteresis_param.thr_low);
      return -1;
    }

    if (cnf->hysteresis_param.thr_high < MIN_HYST_PARAM || cnf->hysteresis_param.thr_high > MAX_HYST_PARAM) {
      fprintf(stderr, "Hyst upper thr %0.2f is not allowed\n", cnf->hysteresis_param.thr_high);
      return -1;
    }

    if (cnf->hysteresis_param.thr_low < MIN_HYST_PARAM || cnf->hysteresis_param.thr_low > MAX_HYST_PARAM) {
      fprintf(stderr, "Hyst lower thr %0.2f is not allowed\n", cnf->hysteresis_param.thr_low);
      return -1;
    }
  }

  /* Check Link quality dijkstra limit */
  if (olsr_cnf->lq_dinter < conv_pollrate_to_secs(cnf->pollrate) && olsr_cnf->lq_dlimit != 255) {
    fprintf(stderr, "Link quality dijkstra limit must be higher than pollrate\n");
    return -1;
  }

  /* NIC Changes Pollrate */
  if (cnf->nic_chgs_pollrate < MIN_NICCHGPOLLRT || cnf->nic_chgs_pollrate > MAX_NICCHGPOLLRT) {
    fprintf(stderr, "NIC Changes Pollrate %0.2f is not allowed\n", cnf->nic_chgs_pollrate);
    return -1;
  }

  /* TC redundancy */
  if (cnf->tc_redundancy > MAX_TC_REDUNDANCY) {
    fprintf(stderr, "TC redundancy %d is not allowed\n", cnf->tc_redundancy);
    return -1;
  }

  /* MPR coverage */
  if (cnf->mpr_coverage < MIN_MPR_COVERAGE || cnf->mpr_coverage > MAX_MPR_COVERAGE) {
    fprintf(stderr, "MPR coverage %d is not allowed\n", cnf->mpr_coverage);
    return -1;
  }

  /* Link Q and hysteresis cannot be activated at the same time */
  if (cnf->use_hysteresis && cnf->lq_level) {
    fprintf(stderr, "Hysteresis and LinkQuality cannot both be active! Deactivate one of them.\n");
    return -1;
  }

  /* Link quality level */
  if (cnf->lq_level > MAX_LQ_LEVEL) {
    fprintf(stderr, "LQ level %d is not allowed\n", cnf->lq_level);
    return -1;
  }

  /* Link quality window size */
  if (cnf->lq_level && (cnf->lq_aging < MIN_LQ_AGING || cnf->lq_aging > MAX_LQ_AGING)) {
    fprintf(stderr, "LQ aging factor %f is not allowed\n", cnf->lq_aging);
    return -1;
  }

  /* NAT threshold value */
  if (cnf->lq_level && (cnf->lq_nat_thresh < 0.1 || cnf->lq_nat_thresh > 1.0)) {
    fprintf(stderr, "NAT threshold %f is not allowed\n", cnf->lq_nat_thresh);
    return -1;
  }

  if (in == NULL) {
    fprintf(stderr, "No interfaces configured!\n");
    return -1;
  }

  /* Interfaces */
  while (in) {
    io = in->cnf;

    if (in->name == NULL || !strlen(in->name)) {
      fprintf(stderr, "Interface has no name!\n");
      return -1;
    }

    if (io == NULL) {
      fprintf(stderr, "Interface %s has no configuration!\n", in->name);
      return -1;
    }

    /* HELLO interval */

    if (io->hello_params.validity_time < 0.0) {
      if (cnf->lq_level == 0)
        io->hello_params.validity_time = NEIGHB_HOLD_TIME;

      else
        io->hello_params.validity_time = (int)(REFRESH_INTERVAL / cnf->lq_aging);
    }

    if (io->hello_params.emission_interval < conv_pollrate_to_secs(cnf->pollrate) ||
        io->hello_params.emission_interval > io->hello_params.validity_time) {
      fprintf(stderr, "Bad HELLO parameters! (em: %0.2f, vt: %0.2f)\n", io->hello_params.emission_interval,
              io->hello_params.validity_time);
      return -1;
    }

    /* TC interval */
    if (io->tc_params.emission_interval < conv_pollrate_to_secs(cnf->pollrate) ||
        io->tc_params.emission_interval > io->tc_params.validity_time) {
      fprintf(stderr, "Bad TC parameters! (em: %0.2f, vt: %0.2f)\n", io->tc_params.emission_interval, io->tc_params.validity_time);
      return -1;
    }

    /* MID interval */
    if (io->mid_params.emission_interval < conv_pollrate_to_secs(cnf->pollrate) ||
        io->mid_params.emission_interval > io->mid_params.validity_time) {
      fprintf(stderr, "Bad MID parameters! (em: %0.2f, vt: %0.2f)\n", io->mid_params.emission_interval,
              io->mid_params.validity_time);
      return -1;
    }

    /* HNA interval */
    if (io->hna_params.emission_interval < conv_pollrate_to_secs(cnf->pollrate) ||
        io->hna_params.emission_interval > io->hna_params.validity_time) {
      fprintf(stderr, "Bad HNA parameters! (em: %0.2f, vt: %0.2f)\n", io->hna_params.emission_interval,
              io->hna_params.validity_time);
      return -1;
    }

    in = in->next;
  }

  return 0;
}

void
olsrd_free_cnf(struct olsrd_config *cnf)
{
  struct ip_prefix_list *hd, *h = cnf->hna_entries;
  struct olsr_if *ind, *in = cnf->interfaces;
  struct plugin_entry *ped, *pe = cnf->plugins;
  struct olsr_lq_mult *mult, *next_mult;

  while (h) {
    hd = h;
    h = h->next;
    free(hd);
  }

  while (in) {
    for (mult = in->cnf->lq_mult; mult != NULL; mult = next_mult) {
      next_mult = mult->next;
      free(mult);
    }

    free(in->cnf);
    ind = in;
    in = in->next;
    free(ind->name);
    free(ind->config);
    free(ind);
  }

  while (pe) {
    ped = pe;
    pe = pe->next;
    free(ped->name);
    free(ped);
  }

  return;
}

static void
set_default_cnf(struct olsrd_config *cnf)
{
  memset(cnf, 0, sizeof(*cnf));

  cnf->debug_level = DEF_DEBUGLVL;
  cnf->no_fork = false;
  cnf->host_emul = false;
  cnf->ip_version = AF_INET;
  cnf->ipsize = sizeof(struct in_addr);
  cnf->maxplen = 32;
  cnf->allow_no_interfaces = DEF_ALLOW_NO_INTS;
  cnf->tos = DEF_TOS;
  cnf->rttable = 254;
  cnf->rttable_default = 0;
  cnf->willingness_auto = DEF_WILL_AUTO;
  cnf->ipc_connections = DEF_IPC_CONNECTIONS;
  cnf->fib_metric = DEF_FIB_METRIC;

  cnf->use_hysteresis = DEF_USE_HYST;
  cnf->hysteresis_param.scaling = HYST_SCALING;
  cnf->hysteresis_param.thr_high = HYST_THRESHOLD_HIGH;
  cnf->hysteresis_param.thr_low = HYST_THRESHOLD_LOW;

  cnf->pollrate = conv_pollrate_to_microsecs(DEF_POLLRATE);
  cnf->nic_chgs_pollrate = DEF_NICCHGPOLLRT;

  cnf->tc_redundancy = TC_REDUNDANCY;
  cnf->mpr_coverage = MPR_COVERAGE;
  cnf->lq_level = DEF_LQ_LEVEL;
  cnf->lq_fish = DEF_LQ_FISH;
  cnf->lq_dlimit = DEF_LQ_DIJK_LIMIT;
  cnf->lq_dinter = DEF_LQ_DIJK_INTER;
  cnf->lq_aging = DEF_LQ_AGING;
  cnf->lq_algorithm = NULL;
  cnf->lq_nat_thresh = DEF_LQ_NAT_THRESH;
  cnf->clear_screen = DEF_CLEAR_SCREEN;

  cnf->del_gws = false;
  cnf->will_int = 10 * HELLO_INTERVAL;
  cnf->exit_value = EXIT_SUCCESS;
  cnf->max_tc_vtime = 0.0;
  cnf->ioctl_s = 0;

#if defined linux
  cnf->rts_linux = 0;
#endif
#if defined __FreeBSD__ || defined __MacOSX__ || defined __NetBSD__ || defined __OpenBSD__
  cnf->rts_bsd = 0;
#endif
}

struct olsrd_config *
olsrd_get_default_cnf(void)
{
  struct olsrd_config *c = olsr_malloc(sizeof(struct olsrd_config), "default_cnf");
  if (c == NULL) {
    fprintf(stderr, "Out of memory %s\n", __func__);
    return NULL;
  }

  set_default_cnf(c);
  return c;
}

void
init_default_if_config(struct if_config_options *io)
{
  struct in6_addr in6;

  memset(io, 0, sizeof(*io));

  io->ipv6_addrtype = 1;        /* XXX - FixMe */

  inet_pton(AF_INET6, OLSR_IPV6_MCAST_SITE_LOCAL, &in6);
  io->ipv6_multi_site.v6 = in6;

  inet_pton(AF_INET6, OLSR_IPV6_MCAST_GLOBAL, &in6);
  io->ipv6_multi_glbl.v6 = in6;

  io->lq_mult = NULL;

  io->weight.fixed = false;
  io->weight.value = 0;

  io->ipv6_addrtype = 0;        /* global */

  io->hello_params.emission_interval = HELLO_INTERVAL;
  io->hello_params.validity_time = NEIGHB_HOLD_TIME;
  io->tc_params.emission_interval = TC_INTERVAL;
  io->tc_params.validity_time = TOP_HOLD_TIME;
  io->mid_params.emission_interval = MID_INTERVAL;
  io->mid_params.validity_time = MID_HOLD_TIME;
  io->hna_params.emission_interval = HNA_INTERVAL;
  io->hna_params.validity_time = HNA_HOLD_TIME;
  io->autodetect_chg = true;
}

struct if_config_options *
get_default_if_config(void)
{
  struct if_config_options *io = olsr_malloc(sizeof(*io), "default_if_config");

  if (io == NULL) {
    fprintf(stderr, "Out of memory %s\n", __func__);
    return NULL;
  }
  init_default_if_config(io);
  return io;
}

int
check_pollrate(float *pollrate)
{
  if (*pollrate > MAX_POLLRATE) {
    fprintf(stderr, "Pollrate %0.2f is too large\n", *pollrate);
    return -1;
  }
#ifdef WIN32
#define sysconf(_SC_CLK_TCK) 1000L
#endif
  if (*pollrate < MIN_POLLRATE || *pollrate < 1.0 / sysconf(_SC_CLK_TCK)) {
    fprintf(stderr, "Pollrate %0.2f is too small - setting it to %ld\n", *pollrate, sysconf(_SC_CLK_TCK));
    *pollrate = 1.0 / sysconf(_SC_CLK_TCK);
  }
  return 0;
}

void
ip_prefix_list_add(struct ip_prefix_list **list, const union olsr_ip_addr *net, uint8_t prefix_len)
{
  struct ip_prefix_list *new_entry = olsr_malloc(sizeof(*new_entry), "new ip_prefix");

  new_entry->net.prefix = *net;
  new_entry->net.prefix_len = prefix_len;

  /* Queue */
  new_entry->next = *list;
  *list = new_entry;
}

int
ip_prefix_list_remove(struct ip_prefix_list **list, const union olsr_ip_addr *net, uint8_t prefix_len)
{
  struct ip_prefix_list *h = *list, *prev = NULL;

  while (h != NULL) {
    if (ipequal(net, &h->net.prefix) && h->net.prefix_len == prefix_len) {
      /* Dequeue */
      if (prev == NULL) {
        *list = h->next;
      } else {
        prev->next = h->next;
      }
      free(h);
      return 1;
    }
    prev = h;
    h = h->next;
  }
  return 0;
}

struct ip_prefix_list *
ip_prefix_list_find(struct ip_prefix_list *list, const union olsr_ip_addr *net, uint8_t prefix_len)
{
  struct ip_prefix_list *h;
  for (h = list; h != NULL; h = h->next) {
    if (prefix_len == h->net.prefix_len && ipequal(net, &h->net.prefix)) {
      return h;
    }
  }
  return NULL;
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */


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

#include "olsr_cfg_gen.h"
#include "olsr_protocol.h"
#include "ipcalc.h"
#include <errno.h>

void
olsrd_print_cnf(const struct olsrd_config *cnf)
{
  struct ip_prefix_list *h = cnf->hna_entries;
  struct olsr_if *in = cnf->interfaces;
  struct plugin_entry *pe = cnf->plugins;
  struct ip_prefix_list *ie = cnf->ipc_nets;
  struct olsr_lq_mult *mult;
  char ipv6_buf[100];                  /* buffer for IPv6 inet_htop */

  printf(" *** olsrd configuration ***\n");

  printf("Debug Level      : %d\n", cnf->debug_level);
  if (cnf->ip_version == AF_INET6) {
    printf("IpVersion        : 6\n");
  } else {
    printf("IpVersion        : 4\n");
  }
  if (cnf->allow_no_interfaces) {
    printf("No interfaces    : ALLOWED\n");
  } else {
    printf("No interfaces    : NOT ALLOWED\n");
  }
  printf("TOS              : 0x%02x\n", cnf->tos);
  printf("RtTable          : 0x%02x\n", cnf->rttable);
  printf("RtTableDefault   : 0x%02x\n", cnf->rttable_default);
  if (cnf->willingness_auto)
    printf("Willingness      : AUTO\n");
  else
    printf("Willingness      : %d\n", cnf->willingness);

  printf("IPC connections  : %d\n", cnf->ipc_connections);
  while (ie) {
    if (ie->net.prefix_len == olsr_cnf->maxplen) {
      struct ipaddr_str strbuf;
      printf("\tHost %s\n", olsr_ip_to_string(&strbuf, &ie->net.prefix));
    } else {
      struct ipprefix_str prefixstr;
      printf("\tNet %s\n", olsr_ip_prefix_to_string(&prefixstr, &ie->net));
    }
    ie = ie->next;
  }

  printf("Pollrate         : %0.2f\n", conv_pollrate_to_secs(cnf->pollrate));

  printf("NIC ChangPollrate: %0.2f\n", cnf->nic_chgs_pollrate);

  printf("TC redundancy    : %d\n", cnf->tc_redundancy);

  printf("MPR coverage     : %d\n", cnf->mpr_coverage);

  printf("LQ level         : %d\n", cnf->lq_level);

  printf("LQ fish eye      : %d\n", cnf->lq_fish);

  printf("LQ Dijkstra limit: %d, %0.2f\n", cnf->lq_dlimit, cnf->lq_dinter);

  printf("LQ aging factor  : %f\n", cnf->lq_aging);

  printf("LQ algorithm name: %s\n", cnf->lq_algorithm ? cnf->lq_algorithm : "default");

  printf("NAT threshold    : %f\n", cnf->lq_nat_thresh);

  printf("Clear screen     : %s\n", cnf->clear_screen ? "yes" : "no");

  /* Interfaces */
  if (in) {
    printf("Interfaces:\n");
    while (in) {
      printf(" dev: \"%s\"\n", in->name);

      if (in->cnf->ipv4_broadcast.v4.s_addr) {
        printf("\tIPv4 broadcast           : %s\n", inet_ntoa(in->cnf->ipv4_broadcast.v4));
      } else {
        printf("\tIPv4 broadcast           : AUTO\n");
      }

      printf("\tIPv6 addrtype            : %s\n", in->cnf->ipv6_addrtype ? "site-local" : "global");

      printf("\tIPv6 multicast site/glbl : %s", inet_ntop(AF_INET6, &in->cnf->ipv6_multi_site.v6, ipv6_buf, sizeof(ipv6_buf)));
      printf("/%s\n", inet_ntop(AF_INET6, &in->cnf->ipv6_multi_glbl.v6, ipv6_buf, sizeof(ipv6_buf)));

      printf("\tHELLO emission/validity  : %0.2f/%0.2f\n", in->cnf->hello_params.emission_interval,
             in->cnf->hello_params.validity_time);
      printf("\tTC emission/validity     : %0.2f/%0.2f\n", in->cnf->tc_params.emission_interval, in->cnf->tc_params.validity_time);
      printf("\tMID emission/validity    : %0.2f/%0.2f\n", in->cnf->mid_params.emission_interval,
             in->cnf->mid_params.validity_time);
      printf("\tHNA emission/validity    : %0.2f/%0.2f\n", in->cnf->hna_params.emission_interval,
             in->cnf->hna_params.validity_time);

      for (mult = in->cnf->lq_mult; mult != NULL; mult = mult->next) {
        printf("\tLinkQualityMult          : %s %0.2f\n", inet_ntop(cnf->ip_version, &mult->addr, ipv6_buf, sizeof(ipv6_buf)),
               (float)(mult->value) / 65536.0);
      }

      printf("\tAutodetetc changes       : %s\n", in->cnf->autodetect_chg ? "yes" : "no");

      in = in->next;
    }
  }

  /* Plugins */
  if (pe) {
    printf("Plugins:\n");

    while (pe) {
      printf("\tName: \"%s\"\n", pe->name);
      pe = pe->next;
    }
  }

  /* Hysteresis */
  if (cnf->use_hysteresis) {
    printf("Using hysteresis:\n");
    printf("\tScaling      : %0.2f\n", cnf->hysteresis_param.scaling);
    printf("\tThr high/low : %0.2f/%0.2f\n", cnf->hysteresis_param.thr_high, cnf->hysteresis_param.thr_low);
  } else {
    printf("Not using hysteresis\n");
  }

  /* HNA IPv4 and IPv6 */
  if (h) {
    printf("HNA%d entries:\n", cnf->ip_version == AF_INET ? 4 : 6);
    while (h) {
      struct ipprefix_str prefixstr;
      printf("\t%s\n", olsr_ip_prefix_to_string(&prefixstr, &h->net));
      h = h->next;
    }
  }
}

int
olsrd_write_cnf(const struct olsrd_config *cnf, const char *fname)
{
  struct autobuf abuf;
  FILE *fd = fopen(fname, "w");
  if (fd == NULL) {
    fprintf(stderr, "Could not open file %s for writing\n%s\n", fname, strerror(errno));
    return -1;
  }

  printf("Writing config to file \"%s\".... ", fname);

  abuf_init(&abuf, 0);
  olsrd_write_cnf_buf(&abuf, cnf, false);
  fputs(abuf.buf, fd);

  abuf_free(&abuf);
  fclose(fd);
  printf("DONE\n");

  return 1;
}

static INLINE void
append_float(struct autobuf *abuf, const char *name, float val, float deflt, bool first)
{
  if (val != deflt) {
    abuf_appendf(abuf, "    %s\t%0.2f\n", name, val);
  } else if (first) {
    abuf_appendf(abuf, "    #%s\t%0.2f\n", name, val);
  }
}

void
olsrd_write_cnf_buf(struct autobuf *abuf, const struct olsrd_config *cnf, bool write_more_comments)
{
  char ipv6_buf[INET6_ADDRSTRLEN];     /* buffer for IPv6 inet_ntop */
  const char *s;

  abuf_appendf(abuf, "#\n"
               "# Configuration file for %s\n"
               "# automatically generated by olsrd-cnf parser\n" "#\n\n", olsrd_version);

  /* Debug level */
  abuf_appendf(abuf, "# Debug level(0-9)\n"
               "# If set to 0 the daemon runs in the background\n" "DebugLevel\t%d\n\n", cnf->debug_level);

  /* IP version */
  abuf_appendf(abuf, "# IP version to use (4 or 6)\n" "IpVersion\t%d\n\n", cnf->ip_version == AF_INET ? 4 : 6);

  /* FIB Metric */
  abuf_appendf(abuf, "# FIBMetric (\"%s\", \"%s\", or \"%s\")\n"
               "FIBMetric\t\"%s\"\n\n",
               CFG_FIBM_FLAT, CFG_FIBM_CORRECT, CFG_FIBM_APPROX,
               FIBM_FLAT == cnf->fib_metric ? CFG_FIBM_FLAT : FIBM_CORRECT == cnf->fib_metric ? CFG_FIBM_CORRECT : CFG_FIBM_APPROX);

  /* HNA IPv4/IPv6 */
  abuf_appendf(abuf, "# HNA IPv%d routes\n"
               "# syntax: netaddr/prefix\n" "Hna%d {\n", cnf->ip_version == AF_INET ? 4 : 6, cnf->ip_version == AF_INET ? 4 : 6);
  if (cnf->hna_entries) {
    struct ip_prefix_list *h;
    for (h = cnf->hna_entries; h != NULL; h = h->next) {
      struct ipprefix_str strbuf;
      abuf_appendf(abuf, "    %s\n", olsr_ip_prefix_to_string(&strbuf, &h->net));
    }
  }
  abuf_appendf(abuf, "}\n\n");

  /* No interfaces */
  abuf_appendf(abuf, "# Should olsrd keep on running even if there are\n"
               "# no interfaces available? This is a good idea\n"
               "# for a PCMCIA/USB hotswap environment.\n"
               "# \"yes\" OR \"no\"\n" "AllowNoInt\t%s\n\n", cnf->allow_no_interfaces ? "yes" : "no");

  /* TOS */
  abuf_appendf(abuf, "# TOS(type of service) to use. Default is 16\n" "TosValue\t%d\n\n", cnf->tos);

  /* RtProto */
  abuf_appendf(abuf, "# Routing proto flag to use. Default is 4 (BOOT)\n" "RtProto\t\t%d\n\n", cnf->rtproto);

  /* RtTable */
  abuf_appendf(abuf, "# Policy Routing Table to use. Default is 254\n" "RtTable\t\t%d\n\n", cnf->rttable);

  /* RtTableDefault */
  abuf_appendf(abuf,
               "# Policy Routing Table to use for the default Route. Default is 0 (Take the same table as specified by RtTable)\n"
               "RtTableDefault\t\t%d\n\n", cnf->rttable_default);

  /* Willingness */
  abuf_appendf(abuf, "# The fixed willingness to use(0-7)\n"
               "# If not set willingness will be calculated\n"
               "# dynammically based on battery/power status\n"
               "%sWillingness\t%d\n\n", cnf->willingness_auto ? "#" : "", cnf->willingness_auto ? 4 : cnf->willingness);

  /* IPC */
  abuf_appendf(abuf, "# Allow processes like the GUI front-end\n"
               "# to connect to the daemon.\n" "IpcConnect {\n" "    MaxConnections\t%d\n", cnf->ipc_connections);

  if (cnf->ipc_nets) {
    struct ip_prefix_list *ie;
    for (ie = cnf->ipc_nets; ie != NULL; ie = ie->next) {
      if (ie->net.prefix_len == olsr_cnf->maxplen) {
        struct ipaddr_str strbuf;
        abuf_appendf(abuf, "    Host\t\t%s\n", olsr_ip_to_string(&strbuf, &ie->net.prefix));
      } else {
        struct ipprefix_str strbuf;
        abuf_appendf(abuf, "    Net\t\t\t%s\n", olsr_ip_prefix_to_string(&strbuf, &ie->net));
      }
    }
  }

  abuf_appendf(abuf, "}\n");

  /* Hysteresis */
  abuf_appendf(abuf, "# Hysteresis adds more robustness to the\n"
               "# link sensing.\n"
               "# Used by default. 'yes' or 'no'\n" "UseHysteresis\t%s\n\n", cnf->use_hysteresis ? "yes" : "no");

  abuf_appendf(abuf, "# Hysteresis parameters\n"
               "# Do not alter these unless you know \n"
               "# what you are doing!\n"
               "# Set to auto by default. Allowed\n"
               "# values are floating point values\n"
               "# in the interval 0,1\n"
               "# THR_LOW must always be lower than\n"
               "# THR_HIGH!!\n"
               "%sHystScaling\t%0.2f\n"
               "%sHystThrHigh\t%0.2f\n"
               "%sHystThrLow\t%0.2f\n\n",
               cnf->use_hysteresis ? "#" : "", cnf->hysteresis_param.scaling,
               cnf->use_hysteresis ? "#" : "", cnf->hysteresis_param.thr_high,
               cnf->use_hysteresis ? "#" : "", cnf->hysteresis_param.thr_low);

  /* Pollrate */
  abuf_appendf(abuf, "# Polling rate in seconds(float).\n"
               "# Auto uses default value 0.05 sec\n" "Pollrate\t%0.2f\n", conv_pollrate_to_secs(cnf->pollrate));

  /* NIC Changes Pollrate */
  abuf_appendf(abuf, "# Interval to poll network interfaces for configuration\n"
               "# changes. Defaults to 2.5 seconds\n" "NicChgsPollInt\t%0.2f\n", cnf->nic_chgs_pollrate);

  /* TC redundancy */
  abuf_appendf(abuf, "# TC redundancy\n"
               "# Specifies how much neighbor info should\n"
               "# be sent in TC messages\n"
               "# Possible values are:\n"
               "# 0 - only send MPR selectors\n"
               "# 1 - send MPR selectors and MPRs\n"
               "# 2 - send all neighbors\n" "# defaults to 0\n" "TcRedundancy\t%d\n\n", cnf->tc_redundancy);

  /* MPR coverage */
  abuf_appendf(abuf, "# MPR coverage\n"
               "# Specifies how many MPRs a node should\n"
               "# try select to reach every 2 hop neighbor\n"
               "# Can be set to any integer >0\n" "# defaults to 1\n" "MprCoverage\t%d\n\n", cnf->mpr_coverage);

  abuf_appendf(abuf, "# Link quality level\n"
               "# 0 = do not use link quality\n"
               "# 1 = use link quality for MPR selection\n"
               "# 2 = use link quality for MPR selection and routing\n" "LinkQualityLevel\t%d\n\n", cnf->lq_level);

  abuf_appendf(abuf, "# Fish Eye algorithm\n"
               "# 0 = do not use fish eye\n" "# 1 = use fish eye\n" "LinkQualityFishEye\t%d\n\n", cnf->lq_fish);

  if (cnf->lq_algorithm != NULL) {
    abuf_appendf(abuf, "# Link quality algorithm (if LinkQualityLevel > 0)\n"
                 "# etx_fpm (hello loss, fixed point math)\n"
                 "# etx_float (hello loss, floating point)\n"
                 "# etx_ff (packet loss for freifunk compat)\n" "LinkQualityAlgorithm\t\"%s\"\n\n", cnf->lq_algorithm);
  }

  abuf_appendf(abuf, "# Link quality aging factor\n" "LinkQualityAging\t%f\n\n", cnf->lq_aging);

  abuf_appendf(abuf, "# NAT threshold\n" "NatThreshold\t%f\n\n", cnf->lq_nat_thresh);

  abuf_appendf(abuf, "# Clear screen when printing debug output?\n" "ClearScreen\t%s\n\n", cnf->clear_screen ? "yes" : "no");

  /* Plugins */
  abuf_appendf(abuf, "# Olsrd plugins to load\n"
               "# This must be the absolute path to the file\n"
               "# or the loader will use the following scheme:\n"
               "# - Try the paths in the LD_LIBRARY_PATH \n"
               "#   environment variable.\n"
               "# - The list of libraries cached in /etc/ld.so.cache\n" "# - /lib, followed by /usr/lib\n\n");
  if (cnf->plugins) {
    struct plugin_entry *pe;
    for (pe = cnf->plugins; pe != NULL; pe = pe->next) {
      struct plugin_param *pp;
      abuf_appendf(abuf, "LoadPlugin \"%s\" {\n", pe->name);
      for (pp = pe->params; pp != NULL; pp = pp->next) {
        abuf_appendf(abuf, "    PlParam \"%s\"\t\"%s\"\n", pp->key, pp->value);
      }
      abuf_appendf(abuf, "}\n");
    }
  }
  abuf_appendf(abuf, "\n");

  /* Interfaces */
  abuf_appendf(abuf, "# Interfaces\n"
               "# Multiple interfaces with the same configuration\n"
               "# can shar the same config block. Just list the\n" "# interfaces(e.g. Interface \"eth0\" \"eth2\"\n");
  /* Interfaces */
  if (cnf->interfaces) {
    struct olsr_if *in;
    bool first;
    for (in = cnf->interfaces, first = write_more_comments; in != NULL; in = in->next, first = false) {
      abuf_appendf(abuf, "Interface \"%s\" {\n", in->name);

      if (first) {
        abuf_appendf(abuf, "    # IPv4 broadcast address to use. The\n"
                     "    # one usefull example would be 255.255.255.255\n"
                     "    # If not defined the broadcastaddress\n" "    # every card is configured with is used\n\n");
      }

      if (in->cnf->ipv4_broadcast.v4.s_addr) {
        abuf_appendf(abuf, "    Ip4Broadcast\t%s\n", inet_ntoa(in->cnf->ipv4_broadcast.v4));
      } else if (first) {
        abuf_appendf(abuf, "    #Ip4Broadcast\t255.255.255.255\n");
      }

      if (first) {
        abuf_appendf(abuf, "\n    # IPv6 address type to use.\n"
                     "    # Must be 'auto', 'site-local', 'unique-local' or 'global'\n\n");
      }
      if (in->cnf->ipv6_addrtype == OLSR_IP6T_SITELOCAL)
        s = CFG_IP6T_SITELOCAL;
      else if (in->cnf->ipv6_addrtype == OLSR_IP6T_UNIQUELOCAL)
        s = CFG_IP6T_UNIQUELOCAL;
      else if (in->cnf->ipv6_addrtype == OLSR_IP6T_GLOBAL)
        s = CFG_IP6T_GLOBAL;
      else
        s = CFG_IP6T_AUTO;
      abuf_appendf(abuf, "    Ip6AddrType\t%s\n\n", s);

      if (first) {
        abuf_appendf(abuf, "\n"
                     "    # IPv6 multicast address to use when\n"
                     "    # using site-local addresses.\n" "    # If not defined, ff05::15 is used\n");
      }
      abuf_appendf(abuf, "    Ip6MulticastSite\t%s\n",
                   inet_ntop(AF_INET6, &in->cnf->ipv6_multi_site.v6, ipv6_buf, sizeof(ipv6_buf)));
      if (first) {
        abuf_appendf(abuf, "\n    # IPv6 multicast address to use when\n"
                     "    # using global addresses\n" "    # If not defined, ff0e::1 is used\n");
      }
      abuf_appendf(abuf, "    Ip6MulticastGlobal\t%s\n",
                   inet_ntop(AF_INET6, &in->cnf->ipv6_multi_glbl.v6, ipv6_buf, sizeof(ipv6_buf)));
      if (first) {
        abuf_appendf(abuf, "\n");
      }
      abuf_appendf(abuf, "    # Olsrd can autodetect changes in\n"
                   "    # interface configurations. Enabled by default\n"
                   "    # turn off to save CPU.\n" "    AutoDetectChanges: %s\n\n", in->cnf->autodetect_chg ? "yes" : "no");

      if (first) {
        abuf_appendf(abuf, "    # Emission and validity intervals.\n"
                     "    # If not defined, RFC proposed values will\n" "    # in most cases be used.\n");
      }
      append_float(abuf, "HelloInterval", in->cnf->hello_params.emission_interval, HELLO_INTERVAL, first);
      append_float(abuf, "HelloValidityTime", in->cnf->hello_params.validity_time, NEIGHB_HOLD_TIME, first);
      append_float(abuf, "TcInterval", in->cnf->tc_params.emission_interval, TC_INTERVAL, first);
      append_float(abuf, "TcValidityTime", in->cnf->tc_params.validity_time, TOP_HOLD_TIME, first);
      append_float(abuf, "MidValidityTime", in->cnf->mid_params.validity_time, MID_HOLD_TIME, first);
      append_float(abuf, "HnaInterval", in->cnf->hna_params.emission_interval, HNA_INTERVAL, first);
      append_float(abuf, "HnaValidityTime", in->cnf->hna_params.validity_time, HNA_HOLD_TIME, first);
      if (in->cnf->lq_mult == NULL) {
        if (first) {
          abuf_appendf(abuf, "    #LinkQualityMult\tdefault 1.0\n");
        }
      } else {
        struct olsr_lq_mult *mult;
        for (mult = in->cnf->lq_mult; mult != NULL; mult = mult->next) {
          abuf_appendf(abuf, "    LinkQualityMult\t%s %0.2f\n",
                       inet_ntop(cnf->ip_version, &mult->addr, ipv6_buf, sizeof(ipv6_buf)), (float)mult->value / 65536.0);
        }
      }

      if (first) {
        abuf_appendf(abuf, "    # When multiple links exist between hosts\n"
                     "    # the weight of interface is used to determine\n"
                     "    # the link to use. Normally the weight is\n"
                     "    # automatically calculated by olsrd based\n"
                     "    # on the characteristics of the interface,\n"
                     "    # but here you can specify a fixed value.\n"
                     "    # Olsrd will choose links with the lowest value.\n"
                     "    # Note:\n"
                     "    # Interface weight is used only when LinkQualityLevel is 0.\n"
                     "    # For any other value of LinkQualityLevel, the interface ETX\n" "    # value is used instead.\n\n");
      }
      if (in->cnf->weight.fixed) {
        abuf_appendf(abuf, "    Weight\t %d\n", in->cnf->weight.value);
      } else if (first) {
        abuf_appendf(abuf, "    #Weight\t 0\n");
      }

      abuf_appendf(abuf, "}\n\n");
    }
  }
  abuf_appendf(abuf, "\n# END AUTOGENERATED CONFIG\n");
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

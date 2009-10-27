
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

#include "olsr_cfg_gen.h"
#include "olsr_protocol.h"
#include "ipcalc.h"
#include "olsr_ip_prefix_list.h"
#include "olsr_time.h"

#include <errno.h>

static INLINE void
append_reltime(struct autobuf *abuf, const char *name, uint32_t val, uint32_t deflt, bool first)
{
  struct millitxt_buf buf;

  if (val != deflt) {
    abuf_appendf(abuf, "    %s\t%s\n", name, olsr_milli_to_txt(&buf, val));
  } else if (first) {
    abuf_appendf(abuf, "    #%s\t%s\n", name, olsr_milli_to_txt(&buf, val));
  }
}

void
olsr_write_cnf_buf(struct autobuf *abuf, struct olsr_config *cnf, bool write_more_comments)
{
  char ipv6_buf[INET6_ADDRSTRLEN];     /* buffer for IPv6 inet_ntop */
  struct millitxt_buf tbuf;
  const char *s;

  abuf_appendf(abuf, "#\n" "# Generated config file for %s\n" "#\n\n", olsrd_version);

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
  if (!list_is_empty(&cnf->hna_entries)) {
    struct ip_prefix_entry *h;

    OLSR_FOR_ALL_IPPREFIX_ENTRIES(&cnf->hna_entries, h) {
      struct ipprefix_str strbuf;
      abuf_appendf(abuf, "    %s\n", ip_prefix_to_string(cnf->ip_version, &strbuf, &h->net));
    } OLSR_FOR_ALL_IPPREFIX_ENTRIES_END()
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
  abuf_appendf(abuf, "# Routing proto flag to use. Operating system default is 0\n" "RtProto\t\t%d\n\n", cnf->rtproto);

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

  /* Pollrate */
  abuf_appendf(abuf, "# Polling rate in seconds(float).\n"
               "# Auto uses default value 0.05 sec\n" "Pollrate\t%s\n",
               olsr_milli_to_txt(&tbuf, cnf->pollrate));

  /* NIC Changes Pollrate */
  abuf_appendf(abuf, "# Interval to poll network interfaces for configuration\n"
               "# changes. Defaults to 2.5 seconds\n" "NicChgsPollInt\t%s\n",
               olsr_milli_to_txt(&tbuf, cnf->nic_chgs_pollrate));

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

  abuf_appendf(abuf, "# Fish Eye algorithm\n"
               "# 0 = do not use fish eye\n" "# 1 = use fish eye\n" "LinkQualityFishEye\t%d\n\n", cnf->lq_fish);

  abuf_appendf(abuf, "# NAT threshold\n" "NatThreshold\t%s\n\n", olsr_milli_to_txt(&tbuf, cnf->lq_nat_thresh));

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

  append_reltime(abuf, "TcInterval", cnf->tc_params.emission_interval, TC_INTERVAL, true);
  append_reltime(abuf, "TcValidityTime", cnf->tc_params.validity_time, TOP_HOLD_TIME, true);
  append_reltime(abuf, "MidInterval", cnf->mid_params.emission_interval, MID_INTERVAL, true);
  append_reltime(abuf, "MidValidityTime", cnf->mid_params.validity_time, MID_HOLD_TIME, true);
  append_reltime(abuf, "HnaInterval", cnf->hna_params.emission_interval, HNA_INTERVAL, true);
  append_reltime(abuf, "HnaValidityTime", cnf->hna_params.validity_time, HNA_HOLD_TIME, true);

  abuf_puts(abuf, "\n");

  /* Interfaces */
  abuf_appendf(abuf, "# Interfaces\n"
               "# Multiple interfaces with the same configuration\n"
               "# can shar the same config block. Just list the\n" "# interfaces(e.g. Interface \"eth0\" \"eth2\"\n");
  /* Interfaces */
  if (cnf->if_configs) {
    struct olsr_if_config *in;
    bool first;
    for (in = cnf->if_configs, first = write_more_comments; in != NULL; in = in->next, first = false) {
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
      append_reltime(abuf, "HelloInterval", in->cnf->hello_params.emission_interval, HELLO_INTERVAL, first);
      append_reltime(abuf, "HelloValidityTime", in->cnf->hello_params.validity_time, NEIGHB_HOLD_TIME, first);
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
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

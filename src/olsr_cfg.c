
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
#include "parser.h"
#include "net_olsr.h"
#include "ifnet.h"

#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <errno.h>

#ifdef WIN32
void ListInterfaces(void);
#endif

#ifdef DEBUG
#define PARSER_DEBUG_PRINTF(x, args...)   printf(x, ##args)
#else
#define PARSER_DEBUG_PRINTF(x, ...)   do { } while (0)
#endif

static char* olsr_strdup(const char *s);
static char* olsr_strndup(const char *s, size_t n);
static char** olsr_strtok(const char *s, const char **snext);
static void olsr_strtok_free(char **s);
static int read_config (const char *filename, int *pargc, char ***pargv);
static struct if_config_options *olsr_get_default_if_config(void);

struct olsrd_config *
olsr_parse_cnf(int argc, char* argv[], const char *conf_file_name)
{
  int opt;
  int opt_idx = 0;
  char *opt_str = 0;
  int opt_argc = 0;
  char **opt_argv = olsr_malloc(argc * sizeof(argv[0]), "argv");
#ifdef DEBUG
  struct ipaddr_str buf;
#endif

  /*
   * Original cmd line params
   *
   * -bcast                   (removed)
   * -delgw                   (-> --delgw)
   * -dispin                  (-> --dispin)
   * -dispout                 (-> --dispout)
   * -d (level)               (preserved)
   * -f (config)              (preserved)
   * -hemu (queue_if(hcif01)) (-> --hemu)
   * -hint                    (removed)
   * -hnaint                  (removed)
   * -i iface iface iface     (see comment below)
   * -int (win32)             (preserved)
   * -ipc (ipc conn = 1)      (-> --ipc)
   * -ipv6                    (-> -V or --IpVersion)
   * -lqa (lq aging)          (removed)
   * -lql (lq lev)            (removed)
   * -lqnt (lq nat)           (removed)
   * -midint                  (removed)
   * -multi (ipv6 mcast)      (removed)
   * -nofork                  (preserved)
   * -tcint                   (removed)
   * -T (pollrate)            (preserved)
   *
   * Note: Remaining args are interpreted as list of
   * interfaces. Because "-i*" lists interfaces, all
   * following non-minus args are processed as ifaces
   * under win32 which is compatible with switch.exe
   */

  static struct option long_options[] = {
    {"config",                   required_argument, 0, 'f'}, /* (filename) */
    {"delgw",                    no_argument,       0, 'D'},
    {"dispin",                   no_argument,       0, 'X'},
    {"dispout",                  no_argument,       0, 'O'},
    {"help",                     no_argument,       0, 'h'},
    {"hemu",                     required_argument, 0, 'H'}, /* (ip4) */
#ifdef WIN32
    {"int",                      no_argument,       0, 'i'},
#endif
    {"ipc",                      no_argument,       0, 'P'},
    {"nofork",                   no_argument,       0, 'n'},
    {"version",                  no_argument,       0, 'v'},
    {"AllowNoInt",               required_argument, 0, 'A'}, /* (yes/no) */
    {"ClearScreen",              required_argument, 0, 'C'}, /* (yes/no) */
    {"DebugLevel",               required_argument, 0, 'd'}, /* (i) */
    {"FIBMetric",                required_argument, 0, 'F'}, /* (str) */
    {"Hna4",                     required_argument, 0, '4'}, /* (4body) */
    {"Hna6",                     required_argument, 0, '6'}, /* (6body) */
    {"Interface",                required_argument, 0, 'I'}, /* (if1 if2 {ifbody}) */
    {"IpcConnect",               required_argument, 0, 'Q'}, /* (Host,Net,MaxConnections) */
    {"IpVersion",                required_argument, 0, 'V'}, /* (i) */
    {"LinkQualityAging",         required_argument, 0, 'a'}, /* (f) */
    {"LinkQualityAlgorithm",     required_argument, 0, 'l'}, /* (str) */
    {"LinkQualityDijkstraLimit", required_argument, 0, 'J'}, /* (i,f) */
    {"LinkQualityFishEye",       required_argument, 0, 'E'}, /* (i) */
    {"LinkQualityLevel",         required_argument, 0, 'L'}, /* (i) */
    {"LinkQualityWinSize",       required_argument, 0, 'W'}, /* (i) */
    {"LoadPlugin",               required_argument, 0, 'p'}, /* (soname {PlParams}) */
    {"MprCoverage",              required_argument, 0, 'M'}, /* (i) */
    {"NatThreshold",             required_argument, 0, 'N'}, /* (f) */
    {"NicChgsPollInt",           required_argument, 0, 'Y'}, /* (f) */
    {"Pollrate",                 required_argument, 0, 'T'}, /* (f) */
    {"RtProto",                  required_argument, 0, 'q'}, /* (i) */
    {"RtTableDefault",           required_argument, 0, 'R'}, /* (i) */
    {"RtTable",                  required_argument, 0, 'r'}, /* (i) */
    {"TcRedundancy",             required_argument, 0, 't'}, /* (i) */
    {"TosValue",                 required_argument, 0, 'Z'}, /* (i) */
    {"Willingness",              required_argument, 0, 'w'}, /* (i) */
    {0, 0, 0, 0}
  }, *popt = long_options;

  /* Copy argv array for safe free'ing later on */
  while(opt_argc < argc) {
    const char* p = argv[opt_argc];
    if (0 == strcmp(p, "-int"))
      p = "-i";
    else if (0 == strcmp(p, "-nofork"))
      p = "-n";
    opt_argv[opt_argc] = olsr_strdup(p);
    opt_argc++;
  }

  /* get option count */
  for (opt_idx = 0; long_options[opt_idx].name; opt_idx++);

  /* Calculate short option string */
  opt_str = olsr_malloc(opt_idx * 3, "create short opt_string");
  opt_idx = 0;
  while (popt->name != NULL && popt->val != 0)
  {
    opt_str[opt_idx++] = popt->val;

    switch (popt->has_arg)
    {
      case optional_argument:
        opt_str[opt_idx++] = ':';
        /* Fall through */
      case required_argument:
        opt_str[opt_idx++] = ':';
        break;
    }
    popt++;
  }

  /* If no arguments, revert to default behaviour */
  if (1 == opt_argc) {
    char* argv0_tmp = opt_argv[0];
    free(opt_argv);
    opt_argv = olsr_malloc(3 * sizeof(opt_argv[0]), "default argv");
    opt_argv[0] = argv0_tmp;
    opt_argv[1] = olsr_strdup("-f");
    opt_argv[2] = olsr_strdup(conf_file_name);
    opt_argc = 3;
  }

  olsr_cnf = olsr_get_default_cnf();

  while (0 <= (opt = getopt_long (opt_argc, opt_argv, opt_str, long_options, &opt_idx))) {
    char **tok;
    const char *optarg_next;

    switch(opt)
    {
    case 'f':                  /* config (filename) */
      PARSER_DEBUG_PRINTF("Read config from %s\n", optarg);
      if (0 > read_config (optarg, &opt_argc, &opt_argv)) {
        fprintf(stderr, "Could not find specified config file %s!\n%s\n\n", optarg, strerror(errno));
        exit(EXIT_FAILURE);
      }
      break;
    case 'D':                  /* delgw */
      olsr_cnf->del_gws = true;
      PARSER_DEBUG_PRINTF("del_gws set to %d\n", olsr_cnf->del_gws);
      break;
    case 'X':                  /* dispin */
      PARSER_DEBUG_PRINTF("Calling parser_set_disp_pack_in(true)\n");
      parser_set_disp_pack_in(true);
      break;
    case 'O':                  /* dispout */
      PARSER_DEBUG_PRINTF("Calling net_set_disp_pack_out(true)\n");
      net_set_disp_pack_out(true);
      break;
    case 'h':                  /* help */
      popt = long_options;
      printf ("Usage: olsrd [OPTIONS]... [ifaces]...\n");
      while (popt->name)
      {
        if (popt->val)
          printf ("-%c or ", popt->val);
        else
          printf ("       ");
        printf ("--%s ", popt->name);
        switch (popt->has_arg)
        {
        case required_argument:
          printf ("arg");
          break;
        case optional_argument:
          printf ("[arg]");
          break;
        }
        printf ("\n");
        popt++;
      }
      exit(0);
    case 'H':                  /* hemu (ip4) */
      {
        union olsr_ip_addr ipaddr;
        struct olsr_if *ifa;

        if (inet_pton(AF_INET, optarg, &ipaddr) <= 0) {
          fprintf(stderr, "Failed converting IP address %s\n", optarg);
          exit(EXIT_FAILURE);
        }

        /* Add hemu interface */
        if (NULL != (ifa = queue_if("hcif01", true))) {
          ifa->cnf = olsr_get_default_if_config();
          ifa->host_emul = true;
          ifa->hemu_ip = ipaddr;
          olsr_cnf->host_emul = true;
          PARSER_DEBUG_PRINTF("host_emul with %s\n", olsr_ip_to_string(&buf, &ifa->hemu_ip));
        }
        PARSER_DEBUG_PRINTF("host_emul set to %d\n", olsr_cnf->host_emul);
      }
      break;
#ifdef WIN32
    case 'i':                  /* int */
      ListInterfaces();
      exit(0);
#endif
    case 'P':                  /* ipc */
      olsr_cnf->ipc_connections = 1;
      PARSER_DEBUG_PRINTF("IPC connections: %d\n", olsr_cnf->ipc_connections);
      break;
    case 'n':                  /* nofork */
      olsr_cnf->no_fork = true;
      PARSER_DEBUG_PRINTF("no_fork set to %d\n", olsr_cnf->no_fork);
      break;
    case 'v':                  /* version */
      /* Version string already printed */
      exit(0);
    case 'A':                  /* AllowNoInt (yes/no) */
      olsr_cnf->allow_no_interfaces =  (0 == strcmp("yes", optarg));
      PARSER_DEBUG_PRINTF("Noint set to %d\n", olsr_cnf->allow_no_interfaces);
      break;
    case 'C':                  /* ClearScreen (yes/no) */
      olsr_cnf->clear_screen = (0 == strcmp("yes", optarg));
      PARSER_DEBUG_PRINTF("Clear screen %s\n", olsr_cnf->clear_screen ? "enabled" : "disabled");
      break;
    case 'd':                  /* DebugLevel (i) */
      {
        int arg = -1;
        sscanf(optarg, "%d", &arg);
        if (0 <= arg && arg < 128)
          olsr_cnf->debug_level = arg;
        PARSER_DEBUG_PRINTF("Debug level: %d\n", olsr_cnf->debug_level);
      }
      break;
    case 'F':                  /* FIBMetric (str) */
      if (NULL != (tok = olsr_strtok(optarg, NULL)))
      {
        if (strcmp(*tok, CFG_FIBM_FLAT) == 0) {
            olsr_cnf->fib_metric = FIBM_FLAT;
        } else if (strcmp(*tok, CFG_FIBM_CORRECT) == 0) {
            olsr_cnf->fib_metric = FIBM_CORRECT;
        } else if (strcmp(*tok, CFG_FIBM_APPROX) == 0) {
            olsr_cnf->fib_metric = FIBM_APPROX;
        } else {
          fprintf(stderr, "FIBMetric must be \"%s\", \"%s\", or \"%s\"!\n", CFG_FIBM_FLAT, CFG_FIBM_CORRECT, CFG_FIBM_APPROX);
          exit(EXIT_FAILURE);
        }
        olsr_strtok_free(tok);
      }
      else {
        fprintf(stderr, "Error in %s\n", optarg);
        exit(EXIT_FAILURE);
      }
      PARSER_DEBUG_PRINTF("FIBMetric: %d=%s\n", olsr_cnf->fib_metric, optarg);
      break;
    case '4':                  /* Hna4 (4body) */
      if ('{' != *optarg) {
        fprintf(stderr, "No {}\n");
        exit(EXIT_FAILURE);
      }
      else if (NULL != (tok = olsr_strtok(optarg + 1, NULL))) {
        char **p = tok;
        if (AF_INET != olsr_cnf->ip_version) {
          fprintf(stderr, "IPv4 addresses can only be used if \"IpVersion\" == 4\n");
          exit(EXIT_FAILURE);
        }
        while(p[0]) {
          union olsr_ip_addr ipaddr, netmask;
          if (!p[1]) {
            fprintf(stderr, "Odd args in %s\n", optarg);
            exit(EXIT_FAILURE);
          }
          if (inet_pton(AF_INET, p[0], &ipaddr) <= 0) {
            fprintf(stderr, "Failed converting IP address %s\n", p[0]);
            exit(EXIT_FAILURE);
          }
          if (inet_pton(AF_INET, p[1], &netmask) <= 0) {
            fprintf(stderr, "Failed converting IP address %s\n", p[1]);
            exit(EXIT_FAILURE);
          }
          if ((ipaddr.v4.s_addr & ~netmask.v4.s_addr) != 0) {
            fprintf(stderr, "The IP address %s/%s is not a network address!\n", p[0], p[1]);
            exit(EXIT_FAILURE);
          }
          ip_prefix_list_add(&olsr_cnf->hna_entries, &ipaddr, olsr_netmask_to_prefix(&netmask));
          PARSER_DEBUG_PRINTF("Hna4 %s/%d\n", olsr_ip_to_string(&buf, &ipaddr), olsr_netmask_to_prefix(&netmask));
          p += 2;
        }
        olsr_strtok_free(tok);
      }
      break;
    case '6':                  /* Hna6 (6body) */
      if ('{' != *optarg) {
        fprintf(stderr, "No {}\n");
        exit(EXIT_FAILURE);
      }
      else if (NULL != (tok = olsr_strtok(optarg + 1, NULL))) {
        char **p = tok;
        if (AF_INET6 != olsr_cnf->ip_version) {
          fprintf(stderr, "IPv6 addresses can only be used if \"IpVersion\" == 6\n");
          exit(EXIT_FAILURE);
        }
        while(p[0]) {
          int prefix = -1;
          union olsr_ip_addr ipaddr;
          if (!p[1]) {
            fprintf(stderr, "Odd args in %s\n", optarg);
            exit(EXIT_FAILURE);
          }
          if (inet_pton(AF_INET6, p[0], &ipaddr) <= 0) {
            fprintf(stderr, "Failed converting IP address %s\n", p[0]);
            exit(EXIT_FAILURE);
          }
          sscanf('/' == *p[1] ? p[1] + 1 : p[1], "%d", &prefix);
          if (0 > prefix || 128 < prefix) {
            fprintf(stderr, "Illegal IPv6 prefix %s\n", p[1]);
            exit(EXIT_FAILURE);
          }
          ip_prefix_list_add(&olsr_cnf->hna_entries, &ipaddr, prefix);
          PARSER_DEBUG_PRINTF("Hna6 %s/%d\n", olsr_ip_to_string(&buf, &ipaddr), prefix);
          p += 2;
        }
        olsr_strtok_free(tok);
      }
      break;
    case 'I':                  /* Interface if1 if2 { ifbody } */
      if (NULL != (tok = olsr_strtok(optarg, &optarg_next))) {
        if ('{' != *optarg_next) {
          fprintf(stderr, "No {}\n");
          exit(EXIT_FAILURE);
        }
        else {
          char **tok_next = olsr_strtok(optarg_next + 1, NULL);
          char **p = tok;
          while(p[0]) {
            char **p_next = tok_next;
            struct olsr_if *ifs = olsr_malloc(sizeof(*ifs), "new if");
            ifs->cnf = olsr_get_default_if_config();
            ifs->name = olsr_strdup(p[0]);
            ifs->next = olsr_cnf->interfaces;
            olsr_cnf->interfaces = ifs;
            PARSER_DEBUG_PRINTF("Interface %s\n", ifs->name);
            while(p_next[0]) {
              if (!p_next[1]) {
                fprintf(stderr, "Odd args in %s\n", optarg_next);
                exit(EXIT_FAILURE);
              }
              if (0 == strcmp("AutoDetectChanges", p_next[0])) {
                ifs->cnf->autodetect_chg =  (0 == strcmp("yes", p_next[1]));
                PARSER_DEBUG_PRINTF("\tAutodetect changes: %d\n", ifs->cnf->autodetect_chg);
              }
              else if (0 == strcmp("Ip4Broadcast", p_next[0])) {
                union olsr_ip_addr ipaddr;
                if (inet_pton(AF_INET, p_next[1], &ipaddr) <= 0) {
                  fprintf(stderr, "Failed converting IP address %s\n", p_next[1]);
                  exit(EXIT_FAILURE);
                }
                ifs->cnf->ipv4_broadcast = ipaddr;
                PARSER_DEBUG_PRINTF("\tIPv4 broadcast: %s\n", ip4_to_string(&buf, ifs->cnf->ipv4_broadcast.v4));
              }
              else if (0 == strcmp("Ip6AddrType", p_next[0])) {
                if (0 == strcmp("site-local", p_next[1])) {
                  ifs->cnf->ipv6_addrtype = OLSR_IP6T_SITELOCAL;
                }
                else if (0 == strcmp("unique-local", p_next[1])) {
                  ifs->cnf->ipv6_addrtype = OLSR_IP6T_UNIQUELOCAL;
                }
                else if (0 == strcmp("global", p_next[1])) {
                  ifs->cnf->ipv6_addrtype = OLSR_IP6T_GLOBAL;
                }
                else {
                  ifs->cnf->ipv6_addrtype = OLSR_IP6T_AUTO;
                }
                PARSER_DEBUG_PRINTF("\tIPv6 addrtype: %d\n", ifs->cnf->ipv6_addrtype);
              }
              else if (0 == strcmp("Ip6MulticastSite", p_next[0])) {
                union olsr_ip_addr ipaddr;
                if (inet_pton(AF_INET6, p_next[1], &ipaddr) <= 0) {
                  fprintf(stderr, "Failed converting IP address %s\n", p_next[1]);
                  exit(EXIT_FAILURE);
                }
                ifs->cnf->ipv6_multi_site = ipaddr;
                PARSER_DEBUG_PRINTF("\tIPv6 site-local multicast: %s\n", ip6_to_string(&buf, &ifs->cnf->ipv6_multi_site.v6));
              }
              else if (0 == strcmp("Ip6MulticastGlobal", p_next[0])) {
                union olsr_ip_addr ipaddr;
                if (inet_pton(AF_INET6, p_next[1], &ipaddr) <= 0) {
                  fprintf(stderr, "Failed converting IP address %s\n", p_next[1]);
                  exit(EXIT_FAILURE);
                }
                ifs->cnf->ipv6_multi_glbl = ipaddr;
                PARSER_DEBUG_PRINTF("\tIPv6 global multicast: %s\n", ip6_to_string(&buf, &ifs->cnf->ipv6_multi_glbl.v6));
              }
              else if (0 == strcmp("HelloInterval", p_next[0])) {
                ifs->cnf->hello_params.emission_interval = 0;
                sscanf(p_next[1], "%f", &ifs->cnf->hello_params.emission_interval);
                PARSER_DEBUG_PRINTF("\tHELLO interval: %0.2f\n", ifs->cnf->hello_params.emission_interval);
              }
              else if (0 == strcmp("HelloValidityTime", p_next[0])) {
                ifs->cnf->hello_params.validity_time = 0;
                sscanf(p_next[1], "%f", &ifs->cnf->hello_params.validity_time);
                PARSER_DEBUG_PRINTF("\tHELLO validity: %0.2f\n", ifs->cnf->hello_params.validity_time);
              }
              else if (0 == strcmp("TcInterval", p_next[0])) {
                ifs->cnf->tc_params.emission_interval = 0;
                sscanf(p_next[1], "%f", &ifs->cnf->tc_params.emission_interval);
                PARSER_DEBUG_PRINTF("\tTC interval: %0.2f\n", ifs->cnf->tc_params.emission_interval);
              }
              else if (0 == strcmp("TcValidityTime", p_next[0])) {
                ifs->cnf->tc_params.validity_time = 0;
                sscanf(p_next[1], "%f", &ifs->cnf->tc_params.validity_time);
                PARSER_DEBUG_PRINTF("\tTC validity: %0.2f\n", ifs->cnf->tc_params.validity_time);
              }
              else if (0 == strcmp("MidInterval", p_next[0])) {
                ifs->cnf->mid_params.emission_interval = 0;
                sscanf(p_next[1], "%f", &ifs->cnf->mid_params.emission_interval);
                PARSER_DEBUG_PRINTF("\tMID interval: %0.2f\n", ifs->cnf->mid_params.emission_interval);
              }
              else if (0 == strcmp("MidValidityTime", p_next[0])) {
                ifs->cnf->mid_params.validity_time = 0;
                sscanf(p_next[1], "%f", &ifs->cnf->mid_params.validity_time);
                PARSER_DEBUG_PRINTF("\tMID validity: %0.2f\n", ifs->cnf->mid_params.validity_time);
              }
              else if (0 == strcmp("HnaInterval", p_next[0])) {
                ifs->cnf->hna_params.emission_interval = 0;
                sscanf(p_next[1], "%f", &ifs->cnf->hna_params.emission_interval);
                PARSER_DEBUG_PRINTF("\tHNA interval: %0.2f\n", ifs->cnf->hna_params.emission_interval);
              }
              else if (0 == strcmp("HnaValidityTime", p_next[0])) {
                ifs->cnf->hna_params.validity_time = 0;
                sscanf(p_next[1], "%f", &ifs->cnf->hna_params.validity_time);
                PARSER_DEBUG_PRINTF("\tHNA validity: %0.2f\n", ifs->cnf->hna_params.validity_time);
              }
              else if (0 == strcmp("Weight", p_next[0])) {
                ifs->cnf->weight.fixed = true;
                PARSER_DEBUG_PRINTF("\tFixed willingness: %d\n", ifs->cnf->weight.value);
              }
              else if (0 == strcmp("LinkQualityMult", p_next[0])) {
                float f;
                struct olsr_lq_mult *mult = olsr_malloc(sizeof(*mult), "lqmult");
                if (!p_next[2]) {
                  fprintf(stderr, "Odd args in %s\n", optarg_next);
                  exit(EXIT_FAILURE);
                }
                memset(&mult->addr, 0, sizeof(mult->addr));
                if (0 != strcmp("default", p_next[1])) {
                  if (inet_pton(olsr_cnf->ip_version, p_next[1], &mult->addr) <= 0) {
                    fprintf(stderr, "Failed converting IP address %s\n", p_next[1]);
                    exit(EXIT_FAILURE);
                  }
                }
                f = 0;
                sscanf(p_next[2], "%f", &f);
                mult->value = (uint32_t)(f * LINK_LOSS_MULTIPLIER);
                mult->next = ifs->cnf->lq_mult;
                ifs->cnf->lq_mult = mult;
                PARSER_DEBUG_PRINTF("\tLinkQualityMult %s %0.2f\n", olsr_ip_to_string(&buf, &mult->addr), (float)mult->value / LINK_LOSS_MULTIPLIER);
                p_next++;
              }
              else {
                fprintf(stderr, "Unknown arg: %s %s\n", p_next[0], p_next[1]);
              }
              p_next += 2;
            }
            p++;
          }
          if (tok_next) olsr_strtok_free(tok_next);
        }
        olsr_strtok_free(tok);
      }
      else {
        fprintf(stderr, "Error in %s\n", optarg);
        exit(EXIT_FAILURE);
      }
      break;
    case 'Q':                  /* IpcConnect (Host,Net,MaxConnections) */
      if ('{' != *optarg) {
        fprintf(stderr, "No {}\n");
        exit(EXIT_FAILURE);
      }
      else if (NULL != (tok = olsr_strtok(optarg + 1, NULL))) {
        char **p = tok;
        while(p[0]) {
          if (!p[1]) {
            fprintf(stderr, "Odd args in %s\n", optarg);
            exit(EXIT_FAILURE);
          }
          if (0 == strcmp("MaxConnections", p[0])) {
            int arg = -1;
            sscanf(optarg, "%d", &arg);
            if (0 <= arg && arg < 1 << (8 * sizeof(olsr_cnf->ipc_connections)))
              olsr_cnf->ipc_connections = arg;
            PARSER_DEBUG_PRINTF("\tIPC connections: %d\n", olsr_cnf->ipc_connections);
          }
          else if (0 == strcmp("Host", p[0])) {
            union olsr_ip_addr ipaddr;
            if (inet_pton(olsr_cnf->ip_version, p[1], &ipaddr) <= 0) {
              fprintf(stderr, "Failed converting IP address %s\n", p[0]);
              exit(EXIT_FAILURE);
            }
            ip_prefix_list_add(&olsr_cnf->ipc_nets, &ipaddr, olsr_cnf->maxplen);
            PARSER_DEBUG_PRINTF("\tIPC host: %s\n", olsr_ip_to_string(&buf, &ipaddr));
          }
          else if (0 == strcmp("Net", p[0])) {
            union olsr_ip_addr ipaddr;
            if (!p[2]) {
              fprintf(stderr, "Odd args in %s\n", optarg_next);
              exit(EXIT_FAILURE);
            }
            if (inet_pton(olsr_cnf->ip_version, p[1], &ipaddr) <= 0) {
              fprintf(stderr, "Failed converting IP address %s\n", p[0]);
              exit(EXIT_FAILURE);
            }
            if (AF_INET == olsr_cnf->ip_version) {
              union olsr_ip_addr netmask;
              if (inet_pton(AF_INET, p[2], &netmask) <= 0) {
                fprintf(stderr, "Failed converting IP address %s\n", p[2]);
                exit(EXIT_FAILURE);
              }
              if ((ipaddr.v4.s_addr & ~netmask.v4.s_addr) != 0) {
                fprintf(stderr, "The IP address %s/%s is not a network address!\n", p[1], p[2]);
                exit(EXIT_FAILURE);
              }
              ip_prefix_list_add(&olsr_cnf->ipc_nets, &ipaddr, olsr_netmask_to_prefix(&netmask));
              PARSER_DEBUG_PRINTF("\tIPC net: %s/%d\n", olsr_ip_to_string(&buf, &ipaddr), olsr_netmask_to_prefix(&netmask));
            }
            else {
              int prefix = -1;
              sscanf('/' == *p[2] ? p[2] + 1 : p[2], "%d", &prefix);
              if (0 > prefix || 128 < prefix) {
                fprintf(stderr, "Illegal IPv6 prefix %s\n", p[2]);
                exit(EXIT_FAILURE);
              }
              ip_prefix_list_add(&olsr_cnf->ipc_nets, &ipaddr, prefix);
              PARSER_DEBUG_PRINTF("\tIPC net: %s/%d\n", olsr_ip_to_string(&buf, &ipaddr), prefix);
            }
            p++;
          }
          else {
            fprintf(stderr, "Unknown arg: %s %s\n", p[0], p[1]);
          }
          p += 2;
        }
        olsr_strtok_free(tok);
      }
      break;
    case 'V':                  /* IpVersion (i) */
      {
        int ver = -1;
        sscanf(optarg, "%d", &ver);
        if (ver == 4) {
          olsr_cnf->ip_version = AF_INET;
          olsr_cnf->ipsize = sizeof(struct in_addr);
          olsr_cnf->maxplen = 32;
        } else if (ver == 6) {
          olsr_cnf->ip_version = AF_INET6;
          olsr_cnf->ipsize = sizeof(struct in6_addr);
          olsr_cnf->maxplen = 128;
        } else {
          fprintf(stderr, "IpVersion must be 4 or 6!\n");
          exit(EXIT_FAILURE);
        }
      }
      PARSER_DEBUG_PRINTF("IpVersion: %d\n", olsr_cnf->ip_version);
      break;
    case 'a':                  /* LinkQualityAging (f) */
      sscanf(optarg, "%f", &olsr_cnf->lq_aging);
      PARSER_DEBUG_PRINTF("Link quality aging factor %f\n", olsr_cnf->lq_aging);
      break;
    case 'l':                  /* LinkQualityAlgorithm (str) */
      if (NULL != (tok = olsr_strtok(optarg, NULL)))
      {
        olsr_cnf->lq_algorithm = olsr_strdup(*tok);
        olsr_strtok_free(tok);
      }
      else {
        fprintf(stderr, "Error in %s\n", optarg);
        exit(EXIT_FAILURE);
      }
      PARSER_DEBUG_PRINTF("LQ Algorithm: %s\n", olsr_cnf->lq_algorithm);
      break;
    case 'J':                  /* LinkQualityDijkstraLimit (i,f) */
      {
        int arg = -1;
        sscanf(optarg, "%d %f", &arg, &olsr_cnf->lq_dinter);
        if (0 <= arg && arg < (1 << (8 * sizeof(olsr_cnf->lq_dlimit))))
          olsr_cnf->lq_dlimit = arg;
        PARSER_DEBUG_PRINTF("Link quality dijkstra limit %d, %0.2f\n", olsr_cnf->lq_dlimit, olsr_cnf->lq_dinter);
      }
      break;
    case 'E':                  /* LinkQualityFishEye (i) */
      {
        int arg = -1;
        sscanf(optarg, "%d", &arg);
        if (0 <= arg && arg < (1 << (8 * sizeof(olsr_cnf->lq_fish))))
          olsr_cnf->lq_fish = arg;
        PARSER_DEBUG_PRINTF("Link quality fish eye %d\n", olsr_cnf->lq_fish);
      }
      break;
    case 'W':                  /* LinkQualityWinSize (i) */
      /* Ignored */
      break;
    case 'p':                  /* LoadPlugin (soname {PlParams}) */
      if (NULL != (tok = olsr_strtok(optarg, &optarg_next))) {
        if ('{' != *optarg_next) {
          fprintf(stderr, "No {}\n");
          exit(EXIT_FAILURE);
        }
        else {
          char **tok_next = olsr_strtok(optarg_next + 1, NULL);
          struct plugin_entry *pe = olsr_malloc(sizeof(*pe), "plugin");
          pe->name = olsr_strdup(*tok);
          pe->params = NULL;
          pe->next = olsr_cnf->plugins;
          olsr_cnf->plugins = pe;
          PARSER_DEBUG_PRINTF("Plugin: %s\n", pe->name);
          if (tok_next) {
            char **p_next = tok_next;
            while(p_next[0]) {
              struct plugin_param *pp = olsr_malloc(sizeof(*pp), "plparam");
              if (0 != strcmp("PlParam", p_next[0]) || !p_next[1] || !p_next[2]) {
                fprintf(stderr, "Odd args in %s\n", optarg_next);
                exit(EXIT_FAILURE);
              }
              pp->key = olsr_strdup(p_next[1]);
              pp->value = olsr_strdup(p_next[2]);
              pp->next = pe->params;
              pe->params = pp;
              PARSER_DEBUG_PRINTF("\tPlParam: %s %s\n", pp->key, pp->value);
              p_next += 3;
            }
            olsr_strtok_free(tok_next);
          }
        }
        olsr_strtok_free(tok);
      }
      else {
        fprintf(stderr, "Error in %s\n", optarg);
        exit(EXIT_FAILURE);
      }
      break;
    case 'M':                  /* MprCoverage (i) */
      {
        int arg = -1;
        sscanf(optarg, "%d", &arg);
        if (0 <= arg && arg < (1 << (8 * sizeof(olsr_cnf->mpr_coverage))))
          olsr_cnf->mpr_coverage = arg;
        PARSER_DEBUG_PRINTF("MPR coverage %d\n", olsr_cnf->mpr_coverage);
      }
      break;
    case 'N':                  /* NatThreshold (f) */
      sscanf(optarg, "%f", &olsr_cnf->lq_nat_thresh);
      PARSER_DEBUG_PRINTF("NAT threshold %0.2f\n", olsr_cnf->lq_nat_thresh);
      break;
    case 'Y':                  /* NicChgsPollInt (f) */
      sscanf(optarg, "%f", &olsr_cnf->nic_chgs_pollrate);
      PARSER_DEBUG_PRINTF("NIC Changes Pollrate %0.2f\n", olsr_cnf->nic_chgs_pollrate);
      break;
    case 'T':                  /* Pollrate (f) */
      {
        float arg = -1;
        sscanf(optarg, "%f", &arg);
        if (0 <= arg)
          olsr_cnf->pollrate = conv_pollrate_to_microsecs(arg);
        PARSER_DEBUG_PRINTF("Pollrate %ud\n", olsr_cnf->pollrate);
      }
      break;
    case 'q':                  /* RtProto (i) */
      {
        int arg = -1;
        sscanf(optarg, "%d", &arg);
        if (0 <= arg && arg < (1 << (8 * sizeof(olsr_cnf->rtproto))))
          olsr_cnf->rtproto = arg;
        PARSER_DEBUG_PRINTF("RtProto: %d\n", olsr_cnf->rtproto);
      }
      break;
    case 'R':                  /* RtTableDefault (i) */
      {
        int arg = -1;
        sscanf(optarg, "%d", &arg);
        if (0 <= arg && arg < (1 << (8 *sizeof(olsr_cnf->rttable_default))))
          olsr_cnf->rttable_default = arg;
        PARSER_DEBUG_PRINTF("RtTableDefault: %d\n", olsr_cnf->rttable_default);
      }
      break;
    case 'r':                  /* RtTable (i) */
      {
        int arg = -1;
        sscanf(optarg, "%d", &arg);
        if (0 <= arg && arg < (1 << (8*sizeof(olsr_cnf->rttable))))
          olsr_cnf->rttable = arg;
        PARSER_DEBUG_PRINTF("RtTable: %d\n", olsr_cnf->rttable);
      }
      break;
    case 't':                  /* TcRedundancy (i) */
      {
        int arg = -1;
        sscanf(optarg, "%d", &arg);
        if (0 <= arg && arg < (1 << (8 * sizeof(olsr_cnf->tc_redundancy))))
          olsr_cnf->tc_redundancy = arg;
        PARSER_DEBUG_PRINTF("TC redundancy %d\n", olsr_cnf->tc_redundancy);
      }
      break;
    case 'Z':                  /* TosValue (i) */
      {
        int arg = -1;
        sscanf(optarg, "%d", &arg);
        if (0 <= arg && arg < (1 << (8 * sizeof(olsr_cnf->tos))))
          olsr_cnf->tos = arg;
        PARSER_DEBUG_PRINTF("TOS: %d\n", olsr_cnf->tos);
      }
      break;
    case 'w':                  /* Willingness (i) */
      {
        int arg = -1;
        sscanf(optarg, "%d", &arg);
        if (0 <= arg && arg < (1 << (8 * sizeof(olsr_cnf->willingness))))
          olsr_cnf->willingness = arg;
        olsr_cnf->willingness_auto = false;
        PARSER_DEBUG_PRINTF("Willingness: %d (no auto)\n", olsr_cnf->willingness);
      }
      break;
    default:
      fprintf (stderr, "?? getopt returned %d ??\n", opt);
      exit(EXIT_FAILURE);
    } /* switch */
  } /* while getopt_long() */

  while(optind < opt_argc) {
    struct olsr_if *ifs;
    PARSER_DEBUG_PRINTF("new iface %s\n", opt_argv[optind]);
    if (NULL != (ifs = queue_if(opt_argv[optind++], false))) {
      ifs->cnf = olsr_get_default_if_config();
    }
  }

  /* Some cleanup */
  while(0 < opt_argc) {
    free(opt_argv[--opt_argc]);
  }
  free(opt_argv);
  free(opt_str);

  return olsr_cnf;
}

int
olsr_sanity_check_cnf(struct olsrd_config *cnf)
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

  /* Link quality window size */
  if (cnf->lq_aging < MIN_LQ_AGING || cnf->lq_aging > MAX_LQ_AGING) {
    fprintf(stderr, "LQ aging factor %f is not allowed\n", cnf->lq_aging);
    return -1;
  }

  /* NAT threshold value */
  if (cnf->lq_nat_thresh < 0.1 || cnf->lq_nat_thresh > 1.0) {
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
olsr_free_cnf(struct olsrd_config *cnf)
{
  struct ip_prefix_list *hd, *h = cnf->hna_entries;
  struct olsr_if *ind, *in = cnf->interfaces;
  struct plugin_entry *ped, *pe = cnf->plugins;
  struct olsr_lq_mult *mult, *next_mult;

  /*
   * HNAs.
   */
  while (h) {
    hd = h;
    h = h->next;
    free(hd);
  }

  /*
   * Interfaces.
   */
  while (in) {
    for (mult = in->cnf->lq_mult; mult != NULL; mult = next_mult) {
      next_mult = mult->next;
      free(mult);
    }

    remove_interface(in);

    free(in->cnf);
    ind = in;
    in = in->next;
    free(ind->name);
    free(ind->config);
    free(ind);
  }

  /*
   * Plugins.
   */
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
  cnf->rtproto = 3;
  cnf->rttable = 254;
  cnf->rttable_default = 0;
  cnf->willingness_auto = DEF_WILL_AUTO;
  cnf->ipc_connections = DEF_IPC_CONNECTIONS;
  cnf->fib_metric = DEF_FIB_METRIC;

  cnf->pollrate = conv_pollrate_to_microsecs(DEF_POLLRATE);
  cnf->nic_chgs_pollrate = DEF_NICCHGPOLLRT;

  cnf->tc_redundancy = TC_REDUNDANCY;
  cnf->mpr_coverage = MPR_COVERAGE;
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
olsr_get_default_cnf(void)
{
  struct olsrd_config *c = olsr_malloc(sizeof(struct olsrd_config), "default_cnf");
  if (c == NULL) {
    fprintf(stderr, "Out of memory %s\n", __func__);
    return NULL;
  }

  set_default_cnf(c);
  return c;
}

static void
olsr_init_default_if_config(struct if_config_options *io)
{
  struct in6_addr in6;

  memset(io, 0, sizeof(*io));

  io->ipv6_addrtype = OLSR_IP6T_SITELOCAL;

  inet_pton(AF_INET6, OLSR_IPV6_MCAST_SITE_LOCAL, &in6);
  io->ipv6_multi_site.v6 = in6;

  inet_pton(AF_INET6, OLSR_IPV6_MCAST_GLOBAL, &in6);
  io->ipv6_multi_glbl.v6 = in6;

  io->lq_mult = NULL;

  io->weight.fixed = false;
  io->weight.value = 0;

  io->ipv6_addrtype = OLSR_IP6T_AUTO;

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

static struct if_config_options *
olsr_get_default_if_config(void)
{
  struct if_config_options *io = olsr_malloc(sizeof(*io), "default_if_config");

  if (io == NULL) {
    fprintf(stderr, "Out of memory %s\n", __func__);
    return NULL;
  }
  olsr_init_default_if_config(io);
  return io;
}

#if 0
int
olsr_check_pollrate(float *pollrate)
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
#endif

void
ip_prefix_list_flush(struct ip_prefix_list **list)
{
  struct ip_prefix_list *entry, *next_entry;

  for (entry = *list; entry; entry = next_entry) {
    next_entry = entry->next;

    entry->next = NULL;
    free(entry);
  }
  *list = NULL;
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

static char*
olsr_strdup(const char *s)
{
  char *ret = olsr_malloc(1 + strlen(s), "olsr_strndup");
  strcpy(ret, s);
  return ret;
}

static char*
olsr_strndup(const char *s, size_t n)
{
  size_t len = n < strlen(s) ? n : strlen(s);
  char *ret = olsr_malloc(1 + len, "olsr_strndup");
  strncpy(ret, s, len);
  ret[len] = 0;
  return ret;
}

static inline bool
olsr_strtok_delim(const char *p)
{
  switch (*p) {
    case 0:
    case '{':
    case '}':
      return true;
  }
  return false;
}

static char**
olsr_strtok(const char *s, const char **snext)
{
  char **tmp, **ret = NULL;
  int i, count = 0;
  const char *p = s;

  while (!olsr_strtok_delim(p)) {
    while (!olsr_strtok_delim(p) && ' ' >= *p)
      p++;
    if (!olsr_strtok_delim(p)) {
      char c = 0;
      const char *q = p;
      if ('"' == *p || '\'' == *p) {
        c = *q++;
        while(!olsr_strtok_delim(++p) && c != *p);
      }
      else {
        while(!olsr_strtok_delim(p) && ' ' < *p)
          p++;
      }
      tmp = ret;
      ret = olsr_malloc((2 + count) * sizeof(ret[0]), "olsr_strtok");
      for (i = 0; i < count; i++) {
        ret[i] = tmp[i];
      }
      if (tmp)
        free(tmp);
      ret[count++] = olsr_strndup(q, p - q);
      ret[count] = NULL;
      if (c)
        p++;
    }
  }
  if (snext)
    *snext = p;
  return ret;
}

static void olsr_strtok_free(char **s)
{
  if (s) {
    char **p = s;
    while(*p) {
      free(*p);
      p++;
    }
    free(s);
  }
}

static int
read_config (const char *filename, int *pargc, char ***pargv)
{
  FILE *f = fopen (filename, "r");
  if (f)
  {
    int bopen = 0, optind_tmp = optind;
    char **argv_lst = 0;
    char sbuf[512], nbuf[512] = { 0 }, *p;
    while ((p = fgets (sbuf, sizeof (sbuf), f)) || *nbuf)
    {
      if (!p)
      {
        *sbuf = 0;
        goto eof;
      }
      while (*p && '#' != *p)
      {
        if ('"' == *p || '\'' == *p)
        {
          char sep = *p;
          while (*++p && sep != *p);
        }
        p++;
      }
      *p = 0;
      while (sbuf <= --p && ' ' >= *p);
      *(p + 1) = 0;
      if (*sbuf)
      {
        if (bopen)
        {
          strcat (nbuf, sbuf);
          if ('}' == *p)
          {
            bopen = 0;
            strcpy (sbuf, nbuf);
          }
        }
        else if (p == sbuf && '{' == *p)
        {
          strcat (nbuf, " ");
          strcat (nbuf, sbuf);
          bopen = 1;
        }
        else
        {
          if ('{' == *p)
            bopen = 1;
        eof:
          if (*nbuf)
          {
            int i;
            char **argv_tmp;
            int argc_tmp = *pargc + 2;

            char *q = nbuf, *n;
            while (*q && ' ' >= *q)
              q++;
            p = q;
            while (' ' < *p)
              p++;
            n = olsr_malloc (p - q + 3, "config arg0");
            strcpy (n, "--");
            strncat (n, q, p - q);
            while (*q && ' ' >= *p)
              p++;

            argv_tmp = olsr_malloc (argc_tmp * sizeof (argv_tmp[0]), "config arg1");
            for (i = 0; i < argc_tmp; i++)
            {
              if (i < optind_tmp)
                argv_tmp[i] = (*pargv)[i];
              else if (i == optind_tmp)
                argv_tmp[i] = n;
              else if (i == 1 + optind_tmp)
                argv_tmp[i] = olsr_strdup (p);
              else
                argv_tmp[i] = (*pargv)[i - 2];
            }
            optind_tmp += 2;
            *pargc = argc_tmp;
            *pargv = argv_tmp;
            if (argv_lst)
              free (argv_lst);
            argv_lst = argv_tmp;
          }
          strcpy (nbuf, sbuf);
        }
      }
    }
    fclose (f);
    return 0;
  }
  return -1;
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

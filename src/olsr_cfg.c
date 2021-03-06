
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

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "olsr_cfg.h"
#include "olsr_logging.h"

#include "olsr.h"
#include "parser.h"
#include "net_olsr.h"
#include "olsr_ip_prefix_list.h"
#include "olsr_protocol.h"
#include "common/string.h"
#include "olsr_clock.h"
#include "os_net.h"
#include "os_system.h"

/* options that have no short command line variant */
enum cfg_long_options {
  CFG_LOG_DEBUG = 256,
  CFG_LOG_INFO,
  CFG_LOG_WARN,
  CFG_LOG_ERROR,
  CFG_LOG_STDERR,
  CFG_LOG_SYSLOG,
  CFG_LOG_FILE,

  CFG_OLSRPORT,
  CFG_DLPATH,

  CFG_HTTPPORT,
  CFG_HTTPLIMIT,
  CFG_TXTPORT,
  CFG_TXTLIMIT,

  CFG_HNA_HTIME,
  CFG_HNA_VTIME,
  CFG_MID_HTIME,
  CFG_MID_VTIME,
  CFG_TC_HTIME,
  CFG_TC_VTIME,
};

/* remember which log severities have been explicitly set */
static bool cfg_has_log[LOG_SEVERITY_COUNT];

/*
 * Special strcat for reading the config file and
 * joining a longer section { ... } to one string
 */
static void
read_cfg_cat(char **pdest, const char *src)
{
  char *tmp = *pdest;
  if (*src) {
    size_t size = 1 + (tmp ? strlen(tmp) : 0) + strlen(src);
    *pdest = olsr_malloc(size, "read_cfg_cat");
    assert(0 == **pdest);
    if (tmp) {
      strscpy(*pdest, tmp, size);
      free(tmp);
    }
    strscat(*pdest, src, size);
  }
}

/*
 * Read the olsrd.conf file and replace/insert found
 * options into the argv[] array at the position of
 * the original -f filename parameters. Note, that
 * longer sections { ... } are joined to one string
 */
static int
read_cfg(const char *filename, int *pargc, char ***pargv, int **pline)
{
  FILE *f = fopen(filename, "r");
  if (f) {
    int bopen = 0, optind_tmp = optind, line_lst = 0, line = 0;
    char sbuf[512], *pbuf = NULL, *p;

    while ((p = fgets(sbuf, sizeof(sbuf), f)) || pbuf) {
      line++;
      if (!p) {
        *sbuf = 0;
        goto eof;
      }
      while (*p && '#' != *p) {
        if ('"' == *p || '\'' == *p) {
          char sep = *p;
          while (*++p && sep != *p);
        }
        p++;
      }
      *p = 0;
      while (sbuf <= --p && ' ' >= *p);
      *(p + 1) = 0;
      if (*sbuf) {
        if (bopen) {
          read_cfg_cat(&pbuf, sbuf);
          if ('}' == *p) {
            bopen = 0;
          }
        } else if (p == sbuf && '{' == *p) {
          read_cfg_cat(&pbuf, " ");
          read_cfg_cat(&pbuf, sbuf);
          bopen = 1;
        } else {
          if ('{' == *p) {
            bopen = 1;
          }
        eof:
          if (pbuf) {
            int i, *line_tmp;
            char **argv_tmp;
            int argc_tmp = *pargc + 2;
            char *q = pbuf, *n;
            size_t size;

            while (*q && ' ' >= *q)
              q++;
            p = q;
            while (' ' < *p)
              p++;
            size = p - q + 3;
            n = olsr_malloc(size, "config arg0");
            strscpy(n, "--", size);
            strscat(n, q, size);
            while (*q && ' ' >= *p)
              p++;

            line_tmp = olsr_malloc((argc_tmp+1) * sizeof(line_tmp[0]), "config line");
            argv_tmp = olsr_malloc((argc_tmp+1) * sizeof(argv_tmp[0]), "config args");
            for (i = 0; i < argc_tmp; i++) {
              if (i < optind_tmp) {
                line_tmp[i] = (*pline)[i];
                argv_tmp[i] = (*pargv)[i];
              } else if (i == optind_tmp) {
                line_tmp[i] = line_lst;
                argv_tmp[i] = n;
              } else if (i == 1 + optind_tmp) {
                line_tmp[i] = line_lst;
                argv_tmp[i] = olsr_strdup(p);
              } else {
                line_tmp[i] = (*pline)[i - 2];
                argv_tmp[i] = (*pargv)[i - 2];
              }
            }
            optind_tmp += 2;
            *pargc = argc_tmp;
            free(*pargv);
            *pargv = argv_tmp;
            free(*pline);
            *pline = line_tmp;
            line_lst = line;
            free(pbuf);
            pbuf = NULL;
          }
          read_cfg_cat(&pbuf, sbuf);
        }
      }
    }
    fclose(f);
    return 0;
  }
  return -1;
}

/*
 * Free an array of string tokens
 */
static void
parse_tok_free(char **s)
{
  if (s) {
    char **p = s;
    while (*p) {
      free(*p);
      p++;
    }
    free(s);
  }
}

/*
 * Test for end-of-string or { ... } section
 */
static INLINE bool
parse_tok_delim(const char *p)
{
  switch (*p) {
  case 0:
  case '{':
  case '}':
    return true;
  }
  return false;
}

/*
 * Slit the src string into tokens and return
 * an array of token strings. May return NULL
 * if no token found, otherwise you need to
 * free the strings using parse_tok_free()
 */
static char **
parse_tok(const char *s, const char **snext)
{
  char **tmp, **ret = NULL;
  int i, count = 0;
  const char *p = s;

  while (!parse_tok_delim(p)) {
    while (!parse_tok_delim(p) && ' ' >= *p)
      p++;
    if (!parse_tok_delim(p)) {
      char c = 0;
      const char *q = p;
      if ('"' == *p || '\'' == *p) {
        c = *q++;
        while (!parse_tok_delim(++p) && c != *p);
      } else {
        while (!parse_tok_delim(p) && ' ' < *p)
          p++;
      }
      tmp = ret;
      ret = olsr_malloc((2 + count) * sizeof(ret[0]), "parse_tok");
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

/*
 * Returns default interface options
 */
struct olsr_if_options *
olsr_get_default_if_options(void)
{
  struct olsr_if_options *new_io = olsr_malloc(sizeof(*new_io), "default_if_config");

  /* No memset because olsr_malloc uses calloc() */
  /* memset(&new_io->ipv4_broadcast, 0, sizeof(new_io->ipv4_broadcast)); */
  new_io->ipv6_addrtype = OLSR_IP6T_AUTO;
  inet_pton(AF_INET6, OLSR_IPV6_MCAST_SITE_LOCAL, &new_io->ipv6_multi_site.v6);
  inet_pton(AF_INET6, OLSR_IPV6_MCAST_GLOBAL, &new_io->ipv6_multi_glbl.v6);
  /* new_io->weight.fixed = false; */
  /* new_io->weight.value = 0; */
  new_io->hello_params.emission_interval = HELLO_INTERVAL;
  new_io->hello_params.validity_time = NEIGHB_HOLD_TIME;
  /* new_io->lq_mult = NULL; */
  new_io->autodetect_chg = true;
  new_io->mode = IF_MODE_MESH;
  return new_io;
}

/**
 * Create a new interf_name struct using a given
 * name and insert it into the interface list.
 */
static struct olsr_if_config *
queue_if(const char *name, struct olsr_config *cfg)
{
  struct olsr_if_config *new_if;

  /* check if the interface already exists */
  for (new_if = cfg->if_configs; new_if != NULL; new_if = new_if->next) {
    if (0 == strcasecmp(new_if->name, name)) {
      fprintf(stderr, "Duplicate interfaces defined... not adding %s\n", name);
      return NULL;
    }
  }

  new_if = olsr_malloc(sizeof(*new_if), "queue interface");
  new_if->name = olsr_strdup(name);
  /* new_if->config = NULL; */
  /* memset(&new_if->hemu_ip, 0, sizeof(new_if->hemu_ip)); */
  /* new_if->interf = NULL; */
  new_if->cnf = olsr_get_default_if_options();
  new_if->next = cfg->if_configs;
  cfg->if_configs = new_if;

  return new_if;
}

/*
 * Parses a single hna option block
 * @argstr:     arguments string
 * @ip_version: AF_INET of AF_INET6
 * @rcfg:       config struct to write/change values into
 * @rmsg:       a buf[FILENAME_MAX + 256] to sprint err msgs
 * @returns configuration status as defined in olsr_parse_cfg_result
 */
static void
parse_cfg_hna(char *argstr, const int ip_version, struct olsr_config *rcfg)
{
  char **tok;
#ifndef REMOVE_LOG_INFO
  struct ipaddr_str buf;
#endif
  if ('{' != *argstr) {
    OLSR_ERROR(LOG_CONFIG, "No {}\n");
    olsr_exit(1);
  }
  if (NULL != (tok = parse_tok(argstr + 1, NULL))) {
    char **p = tok;
    if (ip_version != rcfg->ip_version) {
      OLSR_ERROR(LOG_CONFIG, "IPv%d addresses can only be used if \"IpVersion\" == %d\n",
              AF_INET == ip_version ? 4 : 6, AF_INET == ip_version ? 4 : 6);
      parse_tok_free(tok);
      olsr_exit(1);
    }
    while (p[0]) {
      union olsr_ip_addr ipaddr;
      if (!p[1]) {
        OLSR_ERROR(LOG_CONFIG, "Odd args in %s\n", argstr);
        parse_tok_free(tok);
        olsr_exit(1);
      }
      if (inet_pton(ip_version, p[0], &ipaddr) <= 0) {
        OLSR_ERROR(LOG_CONFIG, "Failed converting IP address %s\n", p[0]);
        parse_tok_free(tok);
        olsr_exit(1);
      }
      if (AF_INET == ip_version) {
        union olsr_ip_addr netmask;
        if (inet_pton(AF_INET, p[1], &netmask) <= 0) {
          OLSR_ERROR(LOG_CONFIG, "Failed converting IP address %s\n", p[1]);
          parse_tok_free(tok);
          olsr_exit(1);
        }
        if ((ipaddr.v4.s_addr & ~netmask.v4.s_addr) != 0) {
          OLSR_ERROR(LOG_CONFIG, "The IP address %s/%s is not a network address!\n", p[0], p[1]);
          parse_tok_free(tok);
          olsr_exit(1);
        }
        ip_prefix_list_add(&rcfg->hna_entries, &ipaddr, netmask_to_prefix((uint8_t *) & netmask, rcfg->ipsize));
        OLSR_INFO_NH(LOG_CONFIG, "Hna4 %s/%d\n", ip_to_string(rcfg->ip_version, &buf, &ipaddr),
                            netmask_to_prefix((uint8_t *) & netmask, rcfg->ipsize));
      } else {
        int prefix = -1;
        sscanf('/' == *p[1] ? p[1] + 1 : p[1], "%d", &prefix);
        if (0 > prefix || 128 < prefix) {
          OLSR_ERROR(LOG_CONFIG, "Illegal IPv6 prefix %s\n", p[1]);
          parse_tok_free(tok);
          olsr_exit(1);
        }
        ip_prefix_list_add(&rcfg->hna_entries, &ipaddr, prefix);
        OLSR_INFO_NH(LOG_CONFIG, "Hna6 %s/%d\n", ip_to_string(rcfg->ip_version, &buf, &ipaddr), prefix);
      }
      p += 2;
    }
    parse_tok_free(tok);
  }
}

/*
 * Parses a single interface option block
 * @argstr:     arguments string
 * @rcfg:       config struct to write/change values into
 * @rmsg:   a buf[FILENAME_MAX + 256] to sprint err msgs
 * @returns configuration status as defined in olsr_parse_cfg_result
 */
static void
parse_cfg_interface(char *argstr, struct olsr_config *rcfg)
{
  char **tok;
  const char *nxt;
#ifndef REMOVE_LOG_INFO
  struct ipaddr_str buf;
#endif
  if (NULL != (tok = parse_tok(argstr, &nxt))) {
    if ('{' != *nxt) {
      OLSR_ERROR(LOG_CONFIG, "No {}\n");
      parse_tok_free(tok);
      olsr_exit(1);
    } else {
      char **tok_next = parse_tok(nxt + 1, NULL);
      char **p = tok;
      while (p[0]) {
        char **p_next = tok_next;
        struct olsr_if_config *new_if = queue_if(p[0], rcfg);
        OLSR_INFO_NH(LOG_CONFIG, "Interface %s\n", p[0]);
        while (new_if && p_next && p_next[0]) {
          if (!p_next[1]) {
            OLSR_ERROR(LOG_CONFIG, "Odd args in %s\n", nxt);
            parse_tok_free(tok_next);
            parse_tok_free(tok);
            olsr_exit(1);
          }
          if (0 == strcasecmp("Mode", p_next[0])) {
            if (0 == strcasecmp("Ether", p_next[1])) {
              new_if->cnf->mode = IF_MODE_ETHER;
            } else {
              new_if->cnf->mode = IF_MODE_MESH;
            }
            OLSR_INFO_NH(LOG_CONFIG, "\tMode: %s\n", INTERFACE_MODE_NAMES[new_if->cnf->mode]);
          } else if (0 == strcasecmp("AutoDetectChanges", p_next[0])) {
            new_if->cnf->autodetect_chg = (0 == strcasecmp("yes", p_next[1]));
            OLSR_INFO_NH(LOG_CONFIG, "\tAutodetect changes: %d\n", new_if->cnf->autodetect_chg);
          } else if (0 == strcasecmp("Ip4Broadcast", p_next[0])) {
            union olsr_ip_addr ipaddr;
            if (inet_pton(AF_INET, p_next[1], &ipaddr) <= 0) {
              OLSR_ERROR(LOG_CONFIG, "Failed converting IP address %s\n", p_next[1]);
              parse_tok_free(tok_next);
              parse_tok_free(tok);
              olsr_exit(1);
            }
            new_if->cnf->ipv4_broadcast = ipaddr;
            OLSR_INFO_NH(LOG_CONFIG, "\tIPv4 broadcast: %s\n", ip4_to_string(&buf, new_if->cnf->ipv4_broadcast.v4));
          } else if (0 == strcasecmp("Ip6AddrType", p_next[0])) {
            if (0 == strcasecmp("site-local", p_next[1])) {
              new_if->cnf->ipv6_addrtype = OLSR_IP6T_SITELOCAL;
            } else if (0 == strcasecmp("unique-local", p_next[1])) {
              new_if->cnf->ipv6_addrtype = OLSR_IP6T_UNIQUELOCAL;
            } else if (0 == strcasecmp("global", p_next[1])) {
              new_if->cnf->ipv6_addrtype = OLSR_IP6T_GLOBAL;
            } else {
              new_if->cnf->ipv6_addrtype = OLSR_IP6T_AUTO;
            }
            OLSR_INFO_NH(LOG_CONFIG, "\tIPv6 addrtype: %d\n", new_if->cnf->ipv6_addrtype);
          } else if (0 == strcasecmp("Ip6MulticastSite", p_next[0])) {
            union olsr_ip_addr ipaddr;
            if (inet_pton(AF_INET6, p_next[1], &ipaddr) <= 0) {
              OLSR_ERROR(LOG_CONFIG, "Failed converting IP address %s\n", p_next[1]);
              parse_tok_free(tok_next);
              parse_tok_free(tok);
              olsr_exit(1);
            }
            new_if->cnf->ipv6_multi_site = ipaddr;
            OLSR_INFO_NH(LOG_CONFIG, "\tIPv6 site-local multicast: %s\n", ip6_to_string(&buf, &new_if->cnf->ipv6_multi_site.v6));
          } else if (0 == strcasecmp("Ip6MulticastGlobal", p_next[0])) {
            union olsr_ip_addr ipaddr;
            if (inet_pton(AF_INET6, p_next[1], &ipaddr) <= 0) {
              OLSR_ERROR(LOG_CONFIG, "Failed converting IP address %s\n", p_next[1]);
              parse_tok_free(tok_next);
              parse_tok_free(tok);
              olsr_exit(1);
            }
            new_if->cnf->ipv6_multi_glbl = ipaddr;
            OLSR_INFO_NH(LOG_CONFIG, "\tIPv6 global multicast: %s\n", ip6_to_string(&buf, &new_if->cnf->ipv6_multi_glbl.v6));
          } else if (0 == strcasecmp("HelloInterval", p_next[0])) {
            new_if->cnf->hello_params.emission_interval = olsr_clock_parse_string(p_next[1]);
            OLSR_INFO_NH(LOG_CONFIG, "\tHELLO interval1: %u ms\n", new_if->cnf->hello_params.emission_interval);
          } else if (0 == strcasecmp("HelloValidityTime", p_next[0])) {
            new_if->cnf->hello_params.validity_time = olsr_clock_parse_string(p_next[1]);
            OLSR_INFO_NH(LOG_CONFIG, "\tHELLO validity: %u ms\n", new_if->cnf->hello_params.validity_time);
          } else if ((0 == strcasecmp("Tcinterval", p_next[0])) || (0 == strcasecmp("TcValidityTime", p_next[0])) ||
                     (0 == strcasecmp("Midinterval", p_next[0])) || (0 == strcasecmp("MidValidityTime", p_next[0])) ||
                     (0 == strcasecmp("Hnainterval", p_next[0])) || (0 == strcasecmp("HnaValidityTime", p_next[0]))) {
            OLSR_ERROR(LOG_CONFIG, "ERROR: %s is deprecated within the interface section. All message intervals/validities except Hellos are global!\n",p_next[0]);
            parse_tok_free(tok_next);
            parse_tok_free(tok);
            olsr_exit(1);
          } else if (0 == strcasecmp("Weight", p_next[0])) {
            new_if->cnf->weight.fixed = true;
            OLSR_INFO_NH(LOG_CONFIG, "\tFixed willingness: %d\n", new_if->cnf->weight.value);
          } else if (0 == strcasecmp("LinkQualityMult", p_next[0])) {
            float f;
            struct olsr_lq_mult *mult = olsr_malloc(sizeof(*mult), "lqmult");
            if (!p_next[2]) {
              OLSR_ERROR(LOG_CONFIG, "Odd args in %s\n", nxt);
              parse_tok_free(tok_next);
              parse_tok_free(tok);
              olsr_exit(1);
            }
            memset(&mult->addr, 0, sizeof(mult->addr));
            if (0 != strcasecmp("default", p_next[1])) {
              if (inet_pton(rcfg->ip_version, p_next[1], &mult->addr) <= 0) {
                OLSR_ERROR(LOG_CONFIG, "Failed converting IP address %s\n", p_next[1]);
                parse_tok_free(tok_next);
                parse_tok_free(tok);
                olsr_exit(1);
              }
            }
            f = 0;
            sscanf(p_next[2], "%f", &f);
            mult->value = (uint32_t) (f * LINK_LOSS_MULTIPLIER);
            mult->next = new_if->cnf->lq_mult;
            new_if->cnf->lq_mult = mult;
            OLSR_INFO_NH(LOG_CONFIG, "\tLinkQualityMult %s %0.2f\n", ip_to_string(rcfg->ip_version, &buf, &mult->addr),
                                (float)mult->value / LINK_LOSS_MULTIPLIER);
            p_next++;
          } else {
            OLSR_ERROR(LOG_CONFIG, "Unknown arg: %s %s\n", p_next[0], p_next[1]);
            parse_tok_free(tok_next);
            parse_tok_free(tok);
            olsr_exit(1);
          }
          p_next += 2;
        }
        p++;
      }
      parse_tok_free(tok_next);
    }
    parse_tok_free(tok);
  } else {
    OLSR_ERROR(LOG_CONFIG, "Error in %s\n", argstr);
    olsr_exit(1);
  }
}

/*
 * Parses a single loadplugin option block
 * @argstr:     arguments string
 * @rcfg:       config struct to write/change values into
 * @rmsg:   a buf[FILENAME_MAX + 256] to sprint err msgs
 * @returns configuration status as defined in olsr_parse_cfg_result
 */
static void
parse_cfg_loadplugin(char *argstr, struct olsr_config *rcfg)
{
  char **tok;
  const char *nxt;
  if (NULL != (tok = parse_tok(argstr, &nxt))) {
    if ('{' != *nxt) {
      OLSR_ERROR(LOG_CONFIG, "No {}\n");
      parse_tok_free(tok);
      olsr_exit(1);
    } else {
      char **tok_next = parse_tok(nxt + 1, NULL);
      struct plugin_entry *pe = olsr_malloc(sizeof(*pe), "plugin");
      pe->name = olsr_strdup(*tok);
      pe->params = NULL;
      pe->next = rcfg->plugins;
      rcfg->plugins = pe;
      OLSR_INFO_NH(LOG_CONFIG, "Plugin: %s\n", pe->name);
      if (tok_next) {
        char **p_next = tok_next;
        while (p_next[0]) {
          struct plugin_param *pp = olsr_malloc(sizeof(*pp), "plparam");
          if (0 != strcasecmp("PlParam", p_next[0]) || !p_next[1] || !p_next[2]) {
            OLSR_ERROR(LOG_CONFIG, "Odd args in %s\n", nxt);
            parse_tok_free(tok_next);
            parse_tok_free(tok);
            olsr_exit(1);
          }
          pp->key = olsr_strdup(p_next[1]);
          pp->value = olsr_strdup(p_next[2]);
          pp->next = pe->params;
          pe->params = pp;
          OLSR_INFO_NH(LOG_CONFIG, "\tPlParam: %s %s\n", pp->key, pp->value);
          p_next += 3;
        }
        parse_tok_free(tok_next);
      }
    }
    parse_tok_free(tok);
  } else {
    OLSR_ERROR(LOG_CONFIG, "Error in %s\n", argstr);
    olsr_exit(1);
  }
}

/*
 * Parses a the parameter string of --log(debug|info|warn|error)
 * @argstr:     arguments string
 * @rcfg:       config struct to write/change values into
 * @rmsg:   a buf[FILENAME_MAX + 256] to sprint err msgs
 * @returns configuration status as defined in olsr_parse_cfg_result
 */
static void
parse_cfg_debug(char *argstr, struct olsr_config *rcfg)
{
  int dlevel, i;
  dlevel = atoi(argstr);

  if (dlevel < MIN_DEBUGLVL || dlevel > MAX_DEBUGLVL) {
    OLSR_ERROR(LOG_CONFIG, "Error, debug level must be between -2 and 3\n");
    olsr_exit(1);
  }

  switch (dlevel) {
  case 3:
    /* all logging */
    for (i = 0; i < LOG_SOURCE_COUNT; i++) {
      rcfg->log_event[SEVERITY_DEBUG][i] = true;
    }
  case 2:
    /* all info, warnings and errors */
    for (i = 0; i < LOG_SOURCE_COUNT; i++) {
      rcfg->log_event[SEVERITY_INFO][i] = true;
    }
  case 1:
    /* some INFO level output, plus all warnings and errors */
    rcfg->log_event[SEVERITY_INFO][LOG_2NEIGH] = true;
    rcfg->log_event[SEVERITY_INFO][LOG_LINKS] = true;
    rcfg->log_event[SEVERITY_INFO][LOG_MAIN] = true;
    rcfg->log_event[SEVERITY_INFO][LOG_NEIGHTABLE] = true;
    rcfg->log_event[SEVERITY_INFO][LOG_PLUGINS] = true;
    rcfg->log_event[SEVERITY_INFO][LOG_ROUTING] = true;
    rcfg->log_event[SEVERITY_INFO][LOG_TC] = true;
  case 0:
    /* errors and warnings */
    for (i = 0; i < LOG_SOURCE_COUNT; i++) {
      rcfg->log_event[SEVERITY_WARN][i] = true;
    }
  case -1:
    /* only error messages */
    for (i = 0; i < LOG_SOURCE_COUNT; i++) {
      rcfg->log_event[SEVERITY_ERR][i] = true;
    }
  default:                     /* no logging at all ! */
    break;
  }

  OLSR_INFO_NH(LOG_CONFIG, "Debug level: %d\n", dlevel);

  if (dlevel > 0) {
    rcfg->no_fork = 1;
  }

  /* prevent fallback to default 0 */
  cfg_has_log[SEVERITY_ERR] = true;
}

/*
 * Parses a the parameter string of --log(debug|info|warn|error)
 * @argstr:     arguments string
 * @rcfg:       config struct to write/change values into
 * @rmsg:   a buf[FILENAME_MAX + 256] to sprint err msgs
 * @returns configuration status as defined in olsr_parse_cfg_result
 */
static void
parse_cfg_log(char *argstr, struct olsr_config *rcfg, enum log_severity sev)
{
  char *p = (char *)argstr, *next;
  int i;

  while (p != NULL) {
    /* split at ',' */
    next = strchr(p, ',');
    if (next) {
      *next++ = 0;
    }

    for (i = 0; i < LOG_SOURCE_COUNT; i++) {
      if (strcasecmp(p, LOG_SOURCE_NAMES[i]) == 0) {
        break;
      }
    }

    if (i == LOG_SOURCE_COUNT) {
      OLSR_ERROR(LOG_CONFIG, "Error, unknown logging source: %s\n", p);
      olsr_exit(1);
    }

    /* handle "all" keyword */
    if (i == LOG_ALL) {
      for (i = 0; i < LOG_SOURCE_COUNT; i++) {
        rcfg->log_event[sev][i] = true;
      }
    }
    else {
      rcfg->log_event[sev][i] = true;
    }
    p = next;
  }


  for (i = 0; i < LOG_SOURCE_COUNT; i++) {
    if (rcfg->log_event[sev][i]) {
      OLSR_INFO_NH(LOG_CONFIG, "Log_%s %s", LOG_SEVERITY_NAMES[sev], LOG_SOURCE_NAMES[i]);
    }
  }
  cfg_has_log[sev] = true;
}

/*
 * Parses a single option found on the command line.
 * @optint: return value of previous getopt_long()
 * @argstr: argument string provided by getopt_long()
 * @line:   line number (if option is read from file)
 * @rcfg:   config struct to write/change values into
 * @rmsg:   a buf[FILENAME_MAX + 256] to sprint err msgs
 * @returns configuration status as defined in olsr_parse_cfg_result
 */
static void
parse_cfg_option(const int optint, char *argstr, const int line, struct olsr_config *rcfg)
{
  switch (optint) {
  case 'i':                    /* iface */
    /* Ignored */
    break;
  case 'n':                    /* nofork */
    rcfg->no_fork = true;
    OLSR_INFO_NH(LOG_CONFIG, "no_fork set to %d\n", rcfg->no_fork);
    break;
  case 'A':                    /* AllowNoInt (yes/no) */
    rcfg->allow_no_interfaces = (0 == strcasecmp("yes", argstr));
    OLSR_INFO_NH(LOG_CONFIG, "Noint set to %d\n", rcfg->allow_no_interfaces);
    break;
  case 'C':                    /* ClearScreen (yes/no) */
    rcfg->clear_screen = (0 == strcasecmp("yes", argstr));
    OLSR_INFO_NH(LOG_CONFIG, "Clear screen %s\n", rcfg->clear_screen ? "enabled" : "disabled");
    break;
  case 'd':                    /* DebugLevel (i) */
    return parse_cfg_debug(argstr, rcfg);
    break;
  case 'F':                    /* FIBMetric (str) */
    {
      char **tok;
      if (NULL != (tok = parse_tok(argstr, NULL))) {
        if (strcasecmp(*tok, CFG_FIBM_FLAT) == 0) {
          rcfg->fib_metric = FIBM_FLAT;
        } else if (strcasecmp(*tok, CFG_FIBM_CORRECT) == 0) {
          rcfg->fib_metric = FIBM_CORRECT;
        } else if (strcasecmp(*tok, CFG_FIBM_APPROX) == 0) {
          rcfg->fib_metric = FIBM_APPROX;
        } else {
          OLSR_ERROR(LOG_CONFIG, "FIBMetric must be \"%s\", \"%s\", or \"%s\"!\n", CFG_FIBM_FLAT, CFG_FIBM_CORRECT, CFG_FIBM_APPROX);
          olsr_exit(1);
        }
        parse_tok_free(tok);
      } else {
        OLSR_ERROR(LOG_CONFIG, "Error in %s\n", argstr);
        olsr_exit(1);
      }
      OLSR_INFO_NH(LOG_CONFIG, "FIBMetric: %d=%s\n", rcfg->fib_metric, argstr);
    }
    break;
  case '4':                    /* Hna4 (4body) */
    return parse_cfg_hna(argstr, AF_INET, rcfg);
    break;
  case '6':                    /* Hna6 (6body) */
    return parse_cfg_hna(argstr, AF_INET6, rcfg);
    break;
  case 'I':                    /* Interface if1 if2 { ifbody } */
    return parse_cfg_interface(argstr, rcfg);
    break;
  case 'V':                    /* IpVersion (i) */
    {
      int ver = -1;
      sscanf(argstr, "%d", &ver);
      if (ver == 4) {
        rcfg->ip_version = AF_INET;
        rcfg->ipsize = sizeof(struct in_addr);
      } else if (ver == 6) {
        rcfg->ip_version = AF_INET6;
        rcfg->ipsize = sizeof(struct in6_addr);
      } else {
        OLSR_ERROR(LOG_CONFIG, "IpVersion must be 4 or 6!\n");
        olsr_exit(1);
      }
    }
    OLSR_INFO_NH(LOG_CONFIG, "IpVersion: %d\n", rcfg->ip_version);
    break;
  case 'E':                    /* LinkQualityFishEye (i) */
    {
      int arg = -1;
      sscanf(argstr, "%d", &arg);
      if (0 <= arg && arg < (1 << (8 * sizeof(rcfg->lq_fish))))
        rcfg->lq_fish = arg;
      OLSR_INFO_NH(LOG_CONFIG, "Link quality fish eye %d\n", rcfg->lq_fish);
    }
    break;
  case 'p':                    /* LoadPlugin (soname {PlParams}) */
    return parse_cfg_loadplugin(argstr, rcfg);
    break;
  case 'M':                    /* MprCoverage (i) */
    {
      int arg = -1;
      sscanf(argstr, "%d", &arg);
      if (0 <= arg && arg < (1 << (8 * sizeof(rcfg->mpr_coverage))))
        rcfg->mpr_coverage = arg;
      OLSR_INFO_NH(LOG_CONFIG, "MPR coverage %d\n", rcfg->mpr_coverage);
    }
    break;
  case 'N':                    /* NatThreshold (f) */
    {
#ifndef REMOVE_LOG_INFO
      struct millitxt_buf tbuf;
#endif

      rcfg->lq_nat_thresh = olsr_clock_parse_string(argstr);
      OLSR_INFO_NH(LOG_CONFIG, "NAT threshold %s\n", olsr_clock_to_string(&tbuf, rcfg->lq_nat_thresh));
    }
    break;
  case 'Y':                    /* NicChgsPollInt (f) */
    rcfg->nic_chgs_pollrate = olsr_clock_parse_string(argstr);
    OLSR_INFO_NH(LOG_CONFIG, "NIC Changes Pollrate %u ms\n", rcfg->nic_chgs_pollrate);
    break;
  case 'T':                    /* Pollrate (f) */
    {
      rcfg->pollrate = olsr_clock_parse_string(argstr);
      OLSR_INFO_NH(LOG_CONFIG, "Pollrate %u ms\n", rcfg->pollrate);
    }
    break;
  case 'q':                    /* RtProto (i) */
    {
      int arg = -1;
      sscanf(argstr, "%d", &arg);
      if (0 <= arg && arg < (1 << (8 * sizeof(rcfg->rt_proto))))
        rcfg->rt_proto = arg;
      OLSR_INFO_NH(LOG_CONFIG, "RtProto: %d\n", rcfg->rt_proto);
    }
    break;
  case 'R':                    /* RtTableDefault (i) */
    {
      int arg = -1;
      sscanf(argstr, "%d", &arg);
      if (0 <= arg && arg < (1 << (8 * sizeof(rcfg->rt_table_default))))
        rcfg->rt_table_default = arg;
      OLSR_INFO_NH(LOG_CONFIG, "RtTableDefault: %d\n", rcfg->rt_table_default);
    }
    break;
  case 'r':                    /* RtTable (i) */
    {
      int arg = -1;
      sscanf(argstr, "%d", &arg);
      if (0 <= arg && arg < (1 << (8 * sizeof(rcfg->rt_table))))
        rcfg->rt_table = arg;
      OLSR_INFO_NH(LOG_CONFIG, "RtTable: %d\n", rcfg->rt_table);
    }
    break;
  case 't':                    /* TcRedundancy (i) */
    {
      int arg = -1;
      sscanf(argstr, "%d", &arg);
      if (0 <= arg && arg < (1 << (8 * sizeof(rcfg->tc_redundancy))))
        rcfg->tc_redundancy = arg;
      OLSR_INFO_NH(LOG_CONFIG, "TC redundancy %d\n", rcfg->tc_redundancy);
    }
    break;
  case 'Z':                    /* TosValue (i) */
    {
      int arg = -1;
      sscanf(argstr, "%d", &arg);
      if (0 <= arg && arg < (1 << (8 * sizeof(rcfg->tos))))
        rcfg->tos = arg;
      OLSR_INFO_NH(LOG_CONFIG, "TOS: %d\n", rcfg->tos);
    }
    break;
  case 'w':                    /* Willingness (i) */
    {
      int arg = -1;
      sscanf(argstr, "%d", &arg);
      if (0 <= arg && arg < (1 << (8 * sizeof(rcfg->willingness))))
        rcfg->willingness = arg;
      rcfg->willingness_auto = false;
      OLSR_INFO_NH(LOG_CONFIG, "Willingness: %d (no auto)\n", rcfg->willingness);
    }
    break;
  case CFG_LOG_DEBUG:          /* Log (string) */
    return parse_cfg_log(argstr, rcfg, SEVERITY_DEBUG);
    break;
  case CFG_LOG_INFO:           /* Log (string) */
    return parse_cfg_log(argstr, rcfg, SEVERITY_INFO);
    break;
  case CFG_LOG_WARN:           /* Log (string) */
    return parse_cfg_log(argstr, rcfg, SEVERITY_WARN);
    break;
  case CFG_LOG_ERROR:          /* Log (string) */
    return parse_cfg_log(argstr, rcfg, SEVERITY_ERR);
    break;
  case CFG_LOG_STDERR:
    rcfg->log_target_stderr = true;
    break;
  case CFG_LOG_SYSLOG:
    rcfg->log_target_syslog = true;
    break;
  case CFG_LOG_FILE:
    rcfg->log_target_file = strdup(argstr);
    break;

  case 's':                    /* SourceIpMode (string) */
    rcfg->source_ip_mode = (0 == strcasecmp("yes", argstr)) ? 1 : 0;
    OLSR_INFO_NH(LOG_CONFIG, "Source IP mode %s\n", rcfg->source_ip_mode ? "enabled" : "disabled");
    break;
  case 'o':                    /* Originator Address (ip) */
    if (inet_pton(AF_INET, argstr, &rcfg->router_id) <= 0) {
      OLSR_ERROR(LOG_CONFIG, "Failed converting IP address %s for originator address\n", argstr);
      olsr_exit(1);
    }
    break;
  case CFG_OLSRPORT:           /* port (i) */
    {
      int arg = -1;
      sscanf(argstr, "%d", &arg);
      if (0 <= arg && arg < (1 << (8 * sizeof(rcfg->olsr_port))))
        rcfg->olsr_port = arg;
      OLSR_INFO_NH(LOG_CONFIG, "OLSR port: %d\n", rcfg->olsr_port);
    }
    break;
  case CFG_DLPATH:
    rcfg->dlPath = strdup(argstr);
    OLSR_INFO_NH(LOG_CONFIG, "Dynamic library path: %s\n", rcfg->dlPath);
    break;
  case CFG_HTTPPORT:
    {
      int arg = -1;
      sscanf(argstr, "%d", &arg);
      if (0 <= arg && arg < (1 << (8 * sizeof(rcfg->comport_http))))
        rcfg->comport_http = arg;
      OLSR_INFO_NH(LOG_CONFIG, "HTTP port: %d\n", rcfg->comport_http);
    }
    break;
  case CFG_HTTPLIMIT:
    {
      int arg = -1;
      sscanf(argstr, "%d", &arg);
      if (0 <= arg && arg < (1 << (8 * sizeof(rcfg->comport_http_limit))))
        rcfg->comport_http_limit = arg;
      OLSR_INFO_NH(LOG_CONFIG, "HTTP connection limit: %d\n", rcfg->comport_http_limit);
    }
    break;
  case CFG_TXTPORT:
    {
      int arg = -1;
      sscanf(argstr, "%d", &arg);
      if (0 <= arg && arg < (1 << (8 * sizeof(rcfg->comport_txt))))
        rcfg->comport_txt = arg;
      OLSR_INFO_NH(LOG_CONFIG, "TXT port: %d\n", rcfg->comport_txt);
    }
    break;
  case CFG_TXTLIMIT:
    {
      int arg = -1;
      sscanf(argstr, "%d", &arg);
      if (0 <= arg && arg < (1 << (8 * sizeof(rcfg->comport_txt_limit))))
        rcfg->comport_txt_limit = arg;
      OLSR_INFO_NH(LOG_CONFIG, "TXT connection limit: %d\n", rcfg->comport_txt_limit);
    }
    break;
  case CFG_HNA_HTIME:
    rcfg->hna_params.emission_interval = olsr_clock_parse_string(argstr);
    OLSR_INFO_NH(LOG_CONFIG, "HNA interval1: %u ms\n", rcfg->hna_params.emission_interval);
    break;
  case CFG_HNA_VTIME:
    rcfg->hna_params.validity_time = olsr_clock_parse_string(argstr);
    OLSR_INFO_NH(LOG_CONFIG, "HNA validity: %u ms\n", rcfg->hna_params.validity_time);
    break;
  case CFG_MID_HTIME:
    rcfg->mid_params.emission_interval = olsr_clock_parse_string(argstr);
    OLSR_INFO_NH(LOG_CONFIG, "MID interval1: %u ms\n", rcfg->mid_params.emission_interval);
    break;
  case CFG_MID_VTIME:
    rcfg->mid_params.validity_time = olsr_clock_parse_string(argstr);
    OLSR_INFO_NH(LOG_CONFIG, "MID validity: %u ms\n", rcfg->mid_params.validity_time);
    break;
  case CFG_TC_HTIME:
    rcfg->tc_params.emission_interval = olsr_clock_parse_string(argstr);
    OLSR_INFO_NH(LOG_CONFIG, "TC interval1: %u ms\n", rcfg->tc_params.emission_interval);
    break;
  case CFG_TC_VTIME:
    rcfg->tc_params.validity_time = olsr_clock_parse_string(argstr);
    OLSR_INFO_NH(LOG_CONFIG, "TC validity: %u ms\n", rcfg->tc_params.validity_time);
    break;

  default:
    OLSR_ERROR(LOG_CONFIG, "Unknown arg in line %d.\n", line);
    olsr_exit(1);
  }                             /* switch */
}

/*
 * Parses command line options using the getopt() runtime
 * function. May also replace a "-f filename" combination
 * with options read in from a config file.
 *
 * @argc: the argv count from main() - or zero if no argv
 * @argv: the array of strings provided from the enviroment
 * @file: the default config file name (used if argc <= 1)
 * @rmsg: to provide buf[FILENAME_MAX + 256] to sprint err msgs
 * @rcfg: returns a valid config pointer, clean with olsr_free_cfg()
 * @returns a parsing status result code
 */
void
olsr_parse_cfg(int argc, char *argv[], const char *file, struct olsr_config ** rcfg)
{
  int opt;
  int opt_idx = 0;
  char *opt_str = 0;
  int opt_argc = 0;
  char **opt_argv = olsr_malloc(argc * sizeof(opt_argv[0]), "opt_argv");
  int *opt_line = olsr_malloc(argc * sizeof(opt_line[0]), "opt_line");

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
   * -i if0 if1...            (see comment below)
   * -int (WIN32)             (preserved)
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
   * interfaces. For this reason, '-i' is ignored which
   * adds the following non-minus args to the list.
   */

  /* *INDENT-OFF* */
  static struct option long_options[] = {
    {"config",                   required_argument, 0, 'f'}, /* (filename) */
    {"delgw",                    no_argument,       0, 'D'},
    {"help",                     optional_argument, 0, 'h'},
    {"iface",                    no_argument,       0, 'i'}, /* if0 if1... */
#ifdef WIN32
    {"int",                      no_argument,       0, 'l'},
#endif
    {"ipc",                      no_argument,       0, 'P'},
    {"log_debug",                required_argument, 0, CFG_LOG_DEBUG}, /* src1,src2,... */
    {"log_info",                 required_argument, 0, CFG_LOG_INFO}, /* src1,src2,... */
    {"log_warn",                 required_argument, 0, CFG_LOG_WARN}, /* src1,src2,... */
    {"log_error",                required_argument, 0, CFG_LOG_ERROR}, /* src1,src2,... */
    {"log_stderr",               no_argument,       0, CFG_LOG_STDERR},
    {"log_syslog",               no_argument,       0, CFG_LOG_SYSLOG},
    {"log_file",                 required_argument, 0, CFG_LOG_FILE}, /* (filename) */
    {"nofork",                   no_argument,       0, 'n'},
    {"version",                  no_argument,       0, 'v'},
    {"AllowNoInt",               required_argument, 0, 'A'}, /* (yes/no) */
    {"ClearScreen",              required_argument, 0, 'C'}, /* (yes/no) */
    {"DebugLevel",               required_argument, 0, 'd'}, /* (i) */
    {"FIBMetric",                required_argument, 0, 'F'}, /* (str) */
    {"Hna4",                     required_argument, 0, '4'}, /* (4body) */
    {"Hna6",                     required_argument, 0, '6'}, /* (6body) */
    {"Interface",                required_argument, 0, 'I'}, /* (if1 if2 {ifbody}) */
    {"IpVersion",                required_argument, 0, 'V'}, /* (i) */
    {"LinkQualityDijkstraLimit", required_argument, 0, 'J'}, /* (i,f) */
    {"LinkQualityFishEye",       required_argument, 0, 'E'}, /* (i) */
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
    {"RouterId",                 required_argument, 0, 'o'}, /* (ip) */
    {"SourceIpMode",             required_argument, 0, 's'}, /* (yes/no) */
    {"OlsrPort",                 required_argument, 0, CFG_OLSRPORT},  /* (i) */
    {"dlPath",                   required_argument, 0, CFG_DLPATH},    /* (path) */
    {"HttpPort",                 required_argument, 0, CFG_HTTPPORT},  /* (i) */
    {"HttpLimit",                required_argument, 0, CFG_HTTPLIMIT}, /* (i) */
    {"TxtPort",                  required_argument, 0, CFG_TXTPORT},   /* (i) */
    {"TxtLimit",                 required_argument, 0, CFG_TXTLIMIT},  /* (i) */
    {"TcInterval",               required_argument, 0, CFG_TC_HTIME},  /* (f) */
    {"TcValidityTime",           required_argument, 0, CFG_TC_VTIME},  /* (f) */
    {"MidInterval",              required_argument, 0, CFG_MID_HTIME},  /* (f) */
    {"MidValidityTime",          required_argument, 0, CFG_MID_VTIME},  /* (f) */
    {"HnaInterval",              required_argument, 0, CFG_HNA_HTIME},  /* (f) */
    {"HnaValidityTime",          required_argument, 0, CFG_HNA_VTIME},  /* (f) */

    {"IpcConnect",               required_argument, 0,  0 }, /* ignored */
    {"UseHysteresis",            required_argument, 0,  0 }, /* ignored */
    {"HystScaling",              required_argument, 0,  0 }, /* ignored */
    {"HystThrHigh",              required_argument, 0,  0 }, /* ignored */
    {"HystThrLow",               required_argument, 0,  0 }, /* ignored */
    {"LinkQualityLevel",         required_argument, 0,  0 }, /* ignored */
    {"LinkQualityWinsize",       required_argument, 0,  0 }, /* ignored */
    {"LinkQualityAlgorithm",     required_argument, 0,  0 }, /* ignored */
    {"LinkQualityAging",         required_argument, 0,  0 }, /* ignored */
    {"dispout",                  no_argument,       0,  0 }, /* ignored */
    {"dispin",                   no_argument,       0,  0 }, /* ignored */
    {0, 0, 0, 0}
  }, *popt = long_options;
  /* *INDENT-ON* */

  /*
   * olsr_malloc() uses calloc, so opt_line is already filled
   * memset(opt_line, 0, argc * sizeof(opt_line[0]));
   */

  /* cleanup static logsource flags */
  cfg_has_log[SEVERITY_DEBUG] = false;
  cfg_has_log[SEVERITY_INFO] = false;
  cfg_has_log[SEVERITY_WARN] = false;
  cfg_has_log[SEVERITY_ERR] = false;

  /* Copy argv array for safe free'ing later on */
  while (opt_argc < argc) {
    const char *p = argv[opt_argc];
    if (0 == strcasecmp(p, "-nofork"))
      p = "-n";
#ifdef WIN32
    else if (0 == strcasecmp(p, "-int"))
      p = "-l";
#endif
    opt_argv[opt_argc] = olsr_strdup(p);
    opt_argc++;
  }

  /* get option count */
  for (opt_idx = 0; long_options[opt_idx].name; opt_idx++);

  /* Calculate short option string */
  opt_str = olsr_malloc(opt_idx * 3, "create short opt_string");
  opt_idx = 0;
  while (popt->name) {
    if (popt->val > 0 && popt->val < 128) {
      opt_str[opt_idx++] = popt->val;

      switch (popt->has_arg) {
      case optional_argument:
        opt_str[opt_idx++] = ':';
        /* Fall through */
      case required_argument:
        opt_str[opt_idx++] = ':';
        break;
      }
    }
    popt++;
  }

  /*
   * If no arguments, revert to default behaviour
   */

  if (1 >= opt_argc) {
    char *argv0_tmp = opt_argv[0];
    free(opt_argv);
    opt_argv = olsr_malloc(3 * sizeof(opt_argv[0]), "default argv");
    opt_argv[0] = argv0_tmp;
    opt_argv[1] = olsr_strdup("-f");
    opt_argv[2] = olsr_strdup(file);
    opt_argc = 3;
  }

  *rcfg = olsr_get_default_cfg();

  while (0 <= (opt = getopt_long(opt_argc, opt_argv, opt_str, long_options, &opt_idx))) {
    switch (opt) {
    case 0:
      OLSR_WARN(LOG_CONFIG, "Ignored deprecated %s\n", long_options[opt_idx].name);
      break;
    case 'f':                  /* config (filename) */
      OLSR_INFO_NH(LOG_CONFIG, "Read config from %s\n", optarg);
      if (0 > read_cfg(optarg, &opt_argc, &opt_argv, &opt_line)) {
        OLSR_ERROR(LOG_CONFIG, "Could not read specified config file %s!\n%s", optarg, strerror(errno));
        olsr_exit(1);
      }
      break;
#ifdef WIN32
    case 'l':                  /* win32: list ifaces */
      ListInterfaces();
      olsr_exit(1);
      break;
#endif
    case 'v':                  /* version */
      fprintf(stderr,  "*** %s ***\n Build date: %s on %s\n http://www.olsr.org\n\n", olsrd_version, build_date, build_host);
      olsr_exit(0);
      break;
    case 'h':                  /* help */
      popt = long_options;
      fprintf(stderr, "Usage: olsrd [OPTIONS]... [interfaces]...\n");
      while (popt->name) {
        if (popt->val) {
          if (popt->val > 0 && popt->val < 128) {
            fprintf(stderr, "-%c or --%s ", popt->val, popt->name);
          } else {
            fprintf(stderr, "      --%s ", popt->name);
          }
          switch (popt->has_arg) {
          case required_argument:
            fprintf(stderr, "arg");
            break;
          case optional_argument:
            fprintf(stderr, "[arg]");
            break;
          }
          fprintf(stderr, "\n");
        }
        popt++;
      }
      if (optarg == NULL) {
        fprintf(stderr, "Use '--help=log'for help about the available logging sources\n");
      } else if (strcasecmp(optarg, "log") == 0) {
        int i;

        fprintf(stderr, "Log sources for --log_debug, --log_info, --log_warn and --log_error:\n");
        for (i = 0; i < LOG_SOURCE_COUNT; i++) {
          fprintf(stderr, "\t%s\n", LOG_SOURCE_NAMES[i]);
        }
      }
      olsr_exit(0);
      break;
    default:
      parse_cfg_option(opt, optarg, opt_line[optind], *rcfg);
    }                           /* switch(opt) */
  }                             /* while getopt_long() */

  while (optind < opt_argc) {
    OLSR_INFO_NH(LOG_CONFIG, "new interface %s\n", opt_argv[optind]);
    queue_if(opt_argv[optind++], *rcfg);
  }

  /* Some cleanup */
  while (0 < opt_argc) {
    free(opt_argv[--opt_argc]);
  }
  free(opt_argv);
  free(opt_line);
  free(opt_str);

  /* logging option post processing */
  if (!((*rcfg)->log_target_syslog || (*rcfg)->log_target_syslog || (*rcfg)->log_target_file != NULL)) {
    (*rcfg)->log_target_stderr = true;
    OLSR_INFO_NH(LOG_CONFIG, "Log: activate default logging target stderr\n");
  }
  for (opt = SEVERITY_INFO; opt < LOG_SEVERITY_COUNT; opt++) {
    if (!cfg_has_log[opt] && cfg_has_log[opt - 1]) {
      int i;

      OLSR_INFO_NH(LOG_CONFIG, "Log: copy log level %s to %s\n", LOG_SEVERITY_NAMES[opt - 1], LOG_SEVERITY_NAMES[opt]);

      /* copy debug to info, info to warning, warning to error (if neccessary) */
      for (i = 0; i < LOG_SOURCE_COUNT; i++) {
        (*rcfg)->log_event[opt][i] = (*rcfg)->log_event[opt - 1][i];
      }
      cfg_has_log[opt] = true;
    }
  }
  if (!cfg_has_log[SEVERITY_ERR]) {
    /* no logging at all defined ? fall back to default */
    char def[10] = DEF_DEBUGLVL;
    parse_cfg_debug(def, *rcfg);
  }
}

/*
 * Checks a given config for illegal values
 */
int
olsr_sanity_check_cfg(struct olsr_config *cfg)
{
  struct olsr_if_config *in = cfg->if_configs;
  struct olsr_if_options *io;
  struct millitxt_buf tbuf;

  /* rttable */
  if (cfg->rt_table == 0) cfg->rt_table = 254;

  /* rttable_default */
  if (cfg->rt_table_default == 0) cfg->rt_table_default = cfg->rt_table;

  /* IP version */
  if (cfg->ip_version != AF_INET && cfg->ip_version != AF_INET6) {
    fprintf(stderr, "Ipversion %d not allowed!\n", cfg->ip_version);
    return -1;
  }

  /* TOS */
  if (cfg->tos > MAX_TOS) {
    fprintf(stderr, "TOS %d is not allowed\n", cfg->tos);
    return -1;
  }

  /* Willingness */
  if (cfg->willingness_auto == false && (cfg->willingness > MAX_WILLINGNESS)) {
    fprintf(stderr, "Willingness %d is not allowed\n", cfg->willingness);
    return -1;
  }

  /* NIC Changes Pollrate */
  if (cfg->nic_chgs_pollrate < MIN_NICCHGPOLLRT || cfg->nic_chgs_pollrate > MAX_NICCHGPOLLRT) {
    fprintf(stderr, "NIC Changes Pollrate %u ms is not allowed\n", cfg->nic_chgs_pollrate);
    return -1;
  }

  /* TC redundancy */
  if (cfg->tc_redundancy > MAX_TC_REDUNDANCY) {
    fprintf(stderr, "TC redundancy %d is not allowed\n", cfg->tc_redundancy);
    return -1;
  }

  /* MPR coverage */
  if (cfg->mpr_coverage < MIN_MPR_COVERAGE || cfg->mpr_coverage > MAX_MPR_COVERAGE) {
    fprintf(stderr, "MPR coverage %d is not allowed\n", cfg->mpr_coverage);
    return -1;
  }

  /* NAT threshold value */
  if (cfg->lq_nat_thresh < 100 || cfg->lq_nat_thresh > 1000) {
    fprintf(stderr, "NAT threshold %s is not allowed\n", olsr_clock_to_string(&tbuf, cfg->lq_nat_thresh));
    return -1;
  }

  /* Source ip mode need fixed router id */
  if (0 == memcmp(&all_zero, &cfg->router_id, sizeof(cfg->router_id)) && cfg->source_ip_mode) {
    fprintf(stderr, "You cannot use source ip routing without setting a fixed router id\n");
    return -1;
  }

  /* check OLSR port */
  if (cfg->olsr_port == 0) {
    fprintf(stderr, "0 is not a valid UDP port\n");
    return -1;
  }

  /* TC interval */
  if (cfg->tc_params.emission_interval < cfg->pollrate ||
      cfg->tc_params.emission_interval > cfg->tc_params.validity_time) {
    fprintf(stderr, "Bad TC parameters! (em: %u ms, vt: %u ms)\n",
        cfg->tc_params.emission_interval,
        cfg->tc_params.validity_time);
    return -1;
  }

  /* MID interval */
  if (cfg->mid_params.emission_interval < cfg->pollrate ||
      cfg->mid_params.emission_interval > cfg->mid_params.validity_time) {
    fprintf(stderr, "Bad MID parameters! (em: %u ms, vt: %u ms)\n",
        cfg->mid_params.emission_interval,
        cfg->mid_params.validity_time);
    return -1;
  }

  /* HNA interval */
  if (cfg->hna_params.emission_interval < cfg->pollrate ||
      cfg->hna_params.emission_interval > cfg->hna_params.validity_time) {
    fprintf(stderr, "Bad HNA parameters! (em: %u ms, vt: %u ms)\n",
        cfg->hna_params.emission_interval,
        cfg->hna_params.validity_time);
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

    if (io->hello_params.emission_interval < cfg->pollrate ||
        io->hello_params.emission_interval > io->hello_params.validity_time) {
      fprintf(stderr, "Bad HELLO parameters! (em: %u ms, vt: %u ms)\n",
          io->hello_params.emission_interval,
          io->hello_params.validity_time);
      return -1;
    }
    in = in->next;
  }

  return 0;
}

/*
 * Free resources occupied by a configuration
 */
void
olsr_free_cfg(struct olsr_config *cfg)
{
  struct olsr_if_config *ind, *in = cfg->if_configs;
  struct plugin_entry *ped, *pe = cfg->plugins;
  struct olsr_lq_mult *mult, *next_mult;

  /* free logfile string if necessary */
  if (cfg->log_target_file) {
    free(cfg->log_target_file);
  }

  /* free dynamic library path */
  if (cfg->dlPath) {
    free(cfg->dlPath);
  }

  /*
   * Free HNAs.
   */
  ip_prefix_list_flush(&cfg->hna_entries);

  /*
   * Free Interfaces - remove_interface() already called
   */
  while (in) {
    for (mult = in->cnf->lq_mult; mult != NULL; mult = next_mult) {
      next_mult = mult->next;
      free(mult);
    }

    free(in->cnf);
    ind = in;
    in = in->next;
    free(ind->name);
    if (ind->config)
      free(ind->config);
    free(ind);
  }

  /*
   * Free Plugins - olsr_close_plugins() allready called
   */
  while (pe) {
    struct plugin_param *pard, *par = pe->params;
    while (par) {
      pard = par;
      par = par->next;
      free(pard->key);
      free(pard->value);
      free(pard);
    }
    ped = pe;
    pe = pe->next;
    free(ped->name);
    free(ped);
  }

  free(cfg);

  return;
}

/*
 * Get a default config
 */
struct olsr_config *
olsr_get_default_cfg(void)
{
  int i;
  struct olsr_config *cfg = olsr_malloc(sizeof(struct olsr_config), "default config");

  cfg->ip_version = AF_INET;
  cfg->ipsize = sizeof(struct in_addr);

  assert(cfg->no_fork == false);
  cfg->allow_no_interfaces = DEF_ALLOW_NO_INTS;
  cfg->willingness_auto = DEF_WILL_AUTO;
  cfg->willingness = DEF_WILL;
  cfg->clear_screen = DEF_CLEAR_SCREEN;

  cfg->tos = DEF_TOS;
  assert(cfg->rt_proto == 0);
  cfg->rt_table = 254;
  assert(cfg->rt_table_default == 0); /*does this ever fire!*/
  cfg->fib_metric = DEF_FIB_METRIC;

  for (i = 0; i < LOG_SOURCE_COUNT; i++) {
    assert(cfg->log_event[SEVERITY_DEBUG][i] == false);
    assert(cfg->log_event[SEVERITY_INFO][i] == false);
    assert(cfg->log_event[SEVERITY_WARN][i] == false);
    assert(cfg->log_event[SEVERITY_ERR][i] == false);
  }
  cfg->log_target_stderr = true;
  assert(cfg->log_target_file == NULL);
  assert(cfg->log_target_syslog == false);

  assert(cfg->plugins == NULL);
  list_init_head(&cfg->hna_entries);
  assert(cfg->if_configs == NULL);

  cfg->pollrate = DEF_POLLRATE;
  cfg->nic_chgs_pollrate = DEF_NICCHGPOLLRT;
  cfg->lq_nat_thresh = DEF_LQ_NAT_THRESH;
  cfg->tc_redundancy = TC_REDUNDANCY;
  cfg->mpr_coverage = MPR_COVERAGE;
  cfg->lq_fish = DEF_LQ_FISH;

  cfg->olsr_port = OLSRPORT;
  assert(cfg->dlPath == NULL);

  cfg->comport_http       = DEF_HTTPPORT;
  cfg->comport_http_limit = DEF_HTTPLIMIT;
  cfg->comport_txt        = DEF_TXTPORT;
  cfg->comport_txt_limit  = DEF_TXTLIMIT;


  cfg->tc_params.emission_interval = TC_INTERVAL;
  cfg->tc_params.validity_time = TOP_HOLD_TIME;
  cfg->mid_params.emission_interval = MID_INTERVAL;
  cfg->mid_params.validity_time = MID_HOLD_TIME;
  cfg->hna_params.emission_interval = HNA_INTERVAL;
  cfg->hna_params.validity_time = HNA_HOLD_TIME;

  assert(0 == memcmp(&all_zero, &cfg->router_id, sizeof(cfg->router_id)));
  assert(0 == cfg->source_ip_mode);
  cfg->will_int = 10 * HELLO_INTERVAL;
  cfg->exit_value = EXIT_SUCCESS;

  assert(cfg->ioctl_s == 0);
#if defined linux
  assert(cfg->rtnl_s == 0);
#endif
#if defined __FreeBSD__ || defined __MacOSX__ || defined __NetBSD__ || defined __OpenBSD__
  assert(cfg->rts_bsd == 0);
#endif

  return cfg;
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

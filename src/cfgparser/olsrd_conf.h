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


#ifndef _OLSRD_CONF_H
#define _OLSRD_CONF_H

#include "olsr_types.h"
#include "olsr_cfg.h"
#include "../common/autobuf.h"

/* fixme: kann weg */
#define PARSER_VERSION "0.1.2"


extern int current_line;

struct conf_token {
  uint32_t integer;
  float      floating;
  bool  boolean;
  char       *string;
};

#if defined __cplusplus
extern "C" {
#endif

void cfgparser_set_default_cnf(struct olsrd_config *);
struct olsrd_config *cfgparser_olsrd_parse_cnf(const char *filename);
void cfgparser_olsrd_free_cnf(struct olsrd_config *cnf);
int cfgparser_olsrd_sanity_check_cnf(struct olsrd_config *cnf);
struct olsrd_config *cfgparser_olsrd_get_default_cnf(void);
struct if_config_options *cfgparser_get_default_if_config(void);
void cfgparser_olsrd_print_cnf(const struct olsrd_config *cnf);
int cfgparser_olsrd_write_cnf(const struct olsrd_config *cnf, const char *fname);
void cfgparser_olsrd_write_cnf_buf(struct autobuf *abuf, const struct olsrd_config *cnf, bool write_more_comments);
int cfgparser_check_pollrate(float *pollrate);

void cfgparser_ip_prefix_list_add(struct ip_prefix_list **, const union olsr_ip_addr *, uint8_t);
int cfgparser_ip_prefix_list_remove(struct ip_prefix_list **, const union olsr_ip_addr *, uint8_t);
struct ip_prefix_list *cfgparser_ip_prefix_list_find(struct ip_prefix_list *, const union olsr_ip_addr *net, uint8_t prefix_len);

#ifdef WIN32
void win32_stdio_hack(unsigned int);
void *win32_olsrd_malloc(size_t size);
void win32_olsrd_free(void *ptr);
#endif

#if defined __cplusplus
}
#endif

#endif /* _OLSRD_CONF_H */

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

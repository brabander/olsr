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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "olsr.h"
#include "olsr_cfg.h"
#include "olsr_cfg_gen.h"

bool disp_pack_out = false;

#ifndef NODEBUG
FILE *debug_handle = NULL;
struct olsr_config *olsr_cnf;
#endif

void *olsr_malloc(size_t size, const char *id __attribute__ ((unused)))
{
  return calloc(1, size);
}

char *olsr_strdup(const char *s)
{
  char *ret = olsr_malloc(1 + strlen(s), "olsr_strdup");
  strcpy(ret, s);
  return ret;
}

char *olsr_strndup(const char *s, size_t n)
{
  size_t len = n < strlen(s) ? n : strlen(s);
  char *ret = olsr_malloc(1 + len, "olsr_strndup");
  strncpy(ret, s, len);
  ret[len] = 0;
  return ret;
}

void parser_set_disp_pack_in(bool val __attribute__ ((unused)))
{
}

static int write_cnf(struct olsr_config *cnf, const char *fname)
{
  FILE *fd;
  struct autobuf abuf;
  if (0 != strcmp(fname, "-")) {
    fd = fopen(fname, "w");
    if (fd == NULL) {
      fprintf(stderr, "Could not open file %s for writing\n%s\n", fname, strerror(errno));
      return -1;
    }
  }
  else {
    fd = stdout;
  }

  printf("Writing config to file \"%s\".... ", fname);

  abuf_init(&abuf, 0);
  olsr_write_cnf_buf(&abuf, cnf, false);
  fputs(abuf.buf, fd);

  abuf_free(&abuf);
  if (0 != strcmp(fname, "-")) {
    fclose(fd);
  }
  printf("DONE\n");

  return 1;
}

int main(int argc, char *argv[])
{
  int i, ret = 0;
#ifndef NODEBUG
  debug_handle = stdout;
#endif
  for (i = 1; i < argc; i++) {
    struct olsr_config *cfg_tmp;
    char cfg_msg[FILENAME_MAX + 256];

    printf("Verifying argv[%d]=%s\n", i, argv[i]);
    if (CFG_ERROR != olsr_parse_cfg(0, NULL, argv[i], cfg_msg, &cfg_tmp)) {
      printf("%s verified: %s\n", argv[i], 0 <= olsr_sanity_check_cfg(cfg_tmp) ? "yes" : "no");
      if (&write_cnf != NULL) write_cnf(cfg_tmp, "-");
    }
    else {
      fprintf(stderr, "%s not verified. %s\n", argv[i], cfg_msg);
      ret = 1;
    }
    olsr_free_cfg(cfg_tmp);
  }
  return ret;
}

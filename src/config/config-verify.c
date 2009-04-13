
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

/************ GLOBALS(begin) ***********/

void *
olsr_malloc(size_t size, const char *id)
{
  void *ptr = calloc(1, size);
  if (ptr == NULL) {
    fprintf(stderr, "error, no memory left for '%s'\n", id);
    exit(1);
  }
  return ptr;
}

char *
olsr_strdup(const char *s)
{
  char *ret = olsr_malloc(1 + strlen(s), "olsr_strdup");
  strcpy(ret, s);
  return ret;
}

char *
olsr_strndup(const char *s, size_t n)
{
  size_t len = n < strlen(s) ? n : strlen(s);
  char *ret = olsr_malloc(1 + len, "olsr_strndup");
  strncpy(ret, s, len);
  ret[len] = 0;
  return ret;
}

/************ GLOBALS(end) ***********/

int
main(int argc, char *argv[])
{
  int i, ret = EXIT_SUCCESS;

  for (i = 1; i < argc; i++) {
    const char *sres;
    struct olsr_config *cfg_tmp;
    char cfg_msg[FILENAME_MAX + 256] = { 0 };
    olsr_parse_cfg_result res = olsr_parse_cfg(0, NULL, argv[i], cfg_msg, &cfg_tmp);

    switch (res) {
    case CFG_ERROR:
      sres = "ERROR";
      break;
    case CFG_WARN:
      sres = "WARN";
      break;
    case CFG_EXIT:
      sres = "EXIT";
      break;
    default:
      sres = "OK";
    }

    fprintf(stderr, "Reading %s returns %s\n", argv[i], sres);

    if (CFG_OK == res) {
      struct autobuf abuf;

      fprintf(stderr, "Verifying %s returns %s\n", argv[i], 0 <= olsr_sanity_check_cfg(cfg_tmp) ? "OK" : "ERROR");

      abuf_init(&abuf, 0);
      olsr_write_cnf_buf(&abuf, cfg_tmp, false);
      fputs(abuf.buf, stdout);
      abuf_free(&abuf);

    } else {
      fprintf(stderr, "Error message: %s\n", cfg_msg);
      ret = EXIT_FAILURE;
    }
    olsr_free_cfg(cfg_tmp);
  }
  return ret;
}

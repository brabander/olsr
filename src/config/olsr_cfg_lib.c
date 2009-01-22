
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

#include "olsr_cfg_lib.h"

int
cfgparser_olsrd_write_cnf(const struct olsr_config *cnf, const char *fname)
{
  struct autobuf abuf;
  FILE *fd = fopen(fname, "w");
  if (fd == NULL) {
    fprintf(stderr, "Could not open file %s for writing\n%s\n", fname, strerror(errno));
    return -1;
  }
  printf("Writing config to file \"%s\".... ", fname);
  abuf_init(&abuf, 0);
  cfgparser_olsrd_write_cnf_buf(&abuf, cnf, false);
  fputs(abuf.buf, fd);
  abuf_free(&abuf);
  fclose(fd);
  printf("DONE\n");
  return 1;
}

#ifdef WIN32

struct ioinfo {
  unsigned int handle;
  unsigned char attr;
  char buff;
  int flag;
  CRITICAL_SECTION lock;
};

void
win32_stdio_hack(unsigned int handle)
{
  HMODULE lib;
  struct ioinfo **info;

  lib = LoadLibrary("msvcrt.dll");

  info = (struct ioinfo **)GetProcAddress(lib, "__pioinfo");

  // (*info)[1].handle = handle;
  // (*info)[1].attr = 0x89; // FOPEN | FTEXT | FPIPE;

  (*info)[2].handle = handle;
  (*info)[2].attr = 0x89;

  // stdout->_file = 1;
  stderr->_file = 2;

  // setbuf(stdout, NULL);
  setbuf(stderr, NULL);
}

void *
win32_olsrd_malloc(size_t size)
{
  return malloc(size);
}

void
win32_olsrd_free(void *ptr)
{
  free(ptr);
}

#endif

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

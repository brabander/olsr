
/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2008, Bernd Petrovitsch <bernd-at-firmix.at>
 * Copyright (c) 2008, Sven-Ola Tuecke <sven-ola-at-gmx.de>
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

#include <assert.h>
#include <ctype.h>
#include <string.h>

#include "common/string.h"

/**
 * A safer version of strncpy that ensures that the
 * destination string will be null-terminated if its
 * length is greater than 0.
 * @param dest target string buffer
 * @param src source string buffer
 * @param size size of target buffer
 * @return pointer to target buffer
 */
char *
strscpy(char *dest, const char *src, size_t size)
{
  assert(dest != NULL);
  assert(src != NULL);

  /* src does not need to be null terminated */
  if (size > 0) {
    strncpy(dest, src, size-1);
    dest[size-1] = 0;
  }

  return dest;
}

/**
 * A safer version of strncat that ensures that
 * the target buffer will be null-terminated if
 * its size is greater than zero.
 *
 * If the target buffer is already full, it will
 * not be changed.
 * @param dest target string buffer
 * @param src source string buffer
 * @param size size of target buffer
 * @return pointer to target buffer
 */
char *
strscat(char *dest, const char *src, size_t size)
{
  size_t l;

  assert(dest != NULL);
  assert(src != NULL);

  l = strlen(dest);
  if (l < size) {
    strscpy(dest + l, src, size - l);
  }
  return dest;
}

/**
 * Removes leading and trailing whitespaces from a string.
 * Instead of moving characters around, it will change the
 * pointer to the beginning of the buffer.
 * @param ptr pointer to string-pointer
 */
void
str_trim (char **ptr) {
  char *string, *end;

  assert (ptr);
  assert (*ptr);

  string = *ptr;

  /* skip leading whitespaces */
  while (isspace(*string)) {
    string++;
  }

  /* get end of string */
  end = string;
  while (*end) {
    end++;
  }
  end--;

  /* remove trailing whitespaces */
  while (end > string && isspace(*end)) {
    *end-- = 0;
  }

  *ptr = string;
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

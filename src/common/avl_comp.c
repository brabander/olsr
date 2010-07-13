/*
 * PacketBB handler library (see RFC 5444)
 * Copyright (c) 2010 Henning Rogge <hrogge@googlemail.com>
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
 * Visit http://www.olsr.org/git for more information.
 *
 * If you find this software useful feel free to make a donation
 * to the project. For more information see the website or contact
 * the copyright holders.
 */

#include <stdint.h>
#include <string.h>

#include "common/avl_comp.h"

/**
 * AVL tree comparator for unsigned 32 bit integers
 * Custom pointer is not used.
 * @param k1 pointer to key 1
 * @param k2 pointer to key 2
 * @param ptr custom pointer for avl comparater (unused)
 * @return +1 if k1>k2, -1 if k1<k2, 0 if k1==k2
 */
int
avl_comp_uint32(const void *k1, const void *k2, void *ptr __attribute__ ((unused)))
{
  const uint32_t *key1 = (const uint32_t *)k1;
  const uint32_t *key2 = (const uint32_t *)k2;
  if (*key1 > *key2) {
    return 1;
  }
  if (*key1 < *key2) {
    return -1;
  }
  return 0;
}

/**
 * AVL tree comparator for unsigned 16 bit integers
 * Custom pointer is not used.
 * @param k1 pointer to key 1
 * @param k2 pointer to key 2
 * @param ptr custom pointer for avl comparater (unused)
 * @return +1 if k1>k2, -1 if k1<k2, 0 if k1==k2
 */
int
avl_comp_uint16(const void *k1, const void *k2, void *ptr __attribute__ ((unused)))
{
  const uint16_t *key1 = (const uint16_t *)k1;
  const uint16_t *key2 = (const uint16_t *)k2;
  if (*key1 > *key2) {
    return 1;
  }
  if (*key1 < *key2) {
    return -1;
  }
  return 0;
}

/**
 * AVL tree comparator for unsigned 8 bit integers
 * Custom pointer is not used
 * @param p1 pointer to key 1
 * @param p2 pointer to key 2
 * @param ptr custom pointer for avl comparater (unused)
 * @return +1 if k1>k2, -1 if k1<k2, 0 if k1==k2
 */
int
avl_comp_uint8(const void *p1, const void *p2, void *ptr __attribute__ ((unused)))
{
  const uint8_t *i1 = p1;
  const uint8_t *i2 = p2;
  if (*i1 > *i2)
    return 1;
  if (*i1 < *i2)
    return -1;
  return 0;
}

/**
 * AVL tree comparator for arbitrary memory blocks.
 * Custom pointer is the length of the memory to compare.
 * @param k1 pointer to key 1
 * @param k2 pointer to key 2
 * @param ptr length of memory blocks to compare (size_t)
 * @return +1 if k1>k2, -1 if k1<k2, 0 if k1==k2
 */
int
avl_comp_mem(const void *k1, const void *k2, void *ptr) {
  const size_t length = (const size_t)ptr;
  return memcmp(k1, k2, length);
}


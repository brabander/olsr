
/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2007, Bernd Petrovitsch <bernd-at-firmix.at>
 * Copyright (c) 2007, Hannes Gredler <hannes-at-gredler.at>
 * Copyright (c) 2008, Alina Friedrichsen <x-alina-at-gmx.net>
 * Copyright (c) 2008, Bernd Petrovitsch <bernd-at-firmix.at>
 * Copyright (c) 2008, John Hay <jhay-at-meraka.org.za>
 * Copyright (c) 2008, Sven-Ola Tuecke <sven-ola-at-gmx.de>
 * Copyright (c) 2009, Henning Rogge <rogge-at-fgan.de>
 * Copyright (c) 2009, Sven-Ola Tuecke <sven-ola-at-gmx.de>
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

#include "ipcalc.h"

#define IN6ADDR_V4MAPPED_LOOPBACK_INIT \
        { { { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
              0x00, 0x00, 0xff, 0xff, 0x7f, 0x00, 0x00, 0x01 } } }

const struct in6_addr in6addr_v4mapped_loopback = IN6ADDR_V4MAPPED_LOOPBACK_INIT;

/* initialize it with all zeroes */
const union olsr_ip_addr all_zero = {.v6 = IN6ADDR_ANY_INIT };

int
prefix_to_netmask(uint8_t * a, int len, uint8_t prefixlen)
{
  int i = 0;
  const int end = MIN(len, prefixlen / 8);

  while (i < end) {
    a[i++] = 0xff;
  }
  if (i < len) {
    a[i++] = 0xff << (8 - (prefixlen % 8));
    while (i < len) {
      a[i++] = 0;
    }
  }

  return (prefixlen <= len * 8);
}

uint8_t
netmask_to_prefix(const uint8_t * adr, int len)
{
  const uint8_t *const a_end = adr + len;
  uint16_t prefix = 0;
  const uint8_t *a;
#if 0
  struct ipaddr_str buf;
#endif

  for (a = adr; a < a_end && *a == 0xff; a++) {
    prefix += 8;
  }
  if (a < a_end) {
    /* handle the last byte */
    switch (*a) {
    case 0:
      prefix += 0;
      break;
    case 128:
      prefix += 1;
      break;
    case 192:
      prefix += 2;
      break;
    case 224:
      prefix += 3;
      break;
    case 240:
      prefix += 4;
      break;
    case 248:
      prefix += 5;
      break;
    case 252:
      prefix += 6;
      break;
    case 254:
      prefix += 7;
      break;
    case 255:
      prefix += 8;
      break;                    /* Shouldn't happen */
    default:
      // removed because of cfg-checker
      // OLSR_WARN(LOG_??, "Got bogus netmask %s\n", ip_to_string(len == 4 ? AF_INET : AF_INET6, &buf, (const union olsr_ip_addr *)adr));
      prefix = UCHAR_MAX;
      break;
    }
  }
  return prefix;
}

const char *
ip_prefix_to_string(int af, struct ipprefix_str *const buf, const struct olsr_ip_prefix *prefix)
{
  int len;
  inet_ntop(af, &prefix->prefix, buf->buf, sizeof(buf->buf));
  len = strlen(buf->buf);
  snprintf(buf->buf + len, sizeof(buf->buf) - len, "/%d", prefix->prefix_len);
  return buf->buf;
}


/* see if the ipaddr is in the net. That is equivalent to the fact that the net part
 * of both are equal. So we must compare the first <prefixlen> bits.
 */
int
ip_in_net(const union olsr_ip_addr *ipaddr, const struct olsr_ip_prefix *net, int ip_version)
{
  int rv;
  if (ip_version == AF_INET) {
    uint32_t netmask = ntohl(prefix_to_netmask4(net->prefix_len));
    rv = (ipaddr->v4.s_addr & netmask) == (net->prefix.v4.s_addr & netmask);
  } else {
    /* IPv6 */
    uint32_t netmask;
    const uint32_t *i = (const uint32_t *)&ipaddr->v6.s6_addr;
    const uint32_t *n = (const uint32_t *)&net->prefix.v6.s6_addr;
    unsigned int prefix_len;
    /* First we compare whole unsigned int's */
    for (prefix_len = net->prefix_len; prefix_len > 32; prefix_len -= 32) {
      if (*i != *n) {
        return false;
      }
      i++;
      n++;
    }
    /* And the remaining is the same as in the IPv4 case */
    netmask = ntohl(prefix_to_netmask4(prefix_len));
    rv = (*i & netmask) == (*n & netmask);
  }
  return rv;
}

static const char *
sockaddr4_to_string(char *const buf, int bufsize, const struct sockaddr *const addr)
{
  char addrbuf[INET6_ADDRSTRLEN];
  const struct sockaddr_in *const sin4 = (const struct sockaddr_in *)addr;
  snprintf(buf, bufsize, "IPv4/%s:%d", inet_ntop(AF_INET, &sin4->sin_addr, addrbuf, sizeof(addrbuf)), sin4->sin_port);
  return buf;
}

static const char *
sockaddr6_to_string(char *const buf, int bufsize, const struct sockaddr *const addr)
{
  char addrbuf[INET6_ADDRSTRLEN];
  const struct sockaddr_in6 *const sin6 = (const struct sockaddr_in6 *)addr;
  snprintf(buf, bufsize,
           "IPv6/[%s]:%d/%x/%x",
           inet_ntop(AF_INET6, &sin6->sin6_addr, addrbuf, sizeof(addrbuf)),
           sin6->sin6_port, (unsigned)sin6->sin6_flowinfo, (unsigned)sin6->sin6_scope_id);
  return buf;
}

const char *
sockaddr_to_string(char *const buf, int bufsize, const struct sockaddr *const addr, unsigned int addrsize)
{
  switch (addr->sa_family) {
  case AF_INET:
    return sockaddr4_to_string(buf, bufsize, addr);
  case AF_INET6:
    return sockaddr6_to_string(buf, bufsize, addr);
  default:
    {
      const int size = MIN(addrsize - sizeof(addr->sa_family), sizeof(addr->sa_data));
      char sep = '/';
      int i;
      int len = snprintf(buf, bufsize, "%d", addr->sa_family);
      for (i = 0; i < size; i++) {
        len += snprintf(buf + len, bufsize - len, "%c%02x", sep, addr->sa_data[i]);
        sep = ' ';
      }
    }
    break;
  }
  return buf;
}


/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

/*
 * Functions for the Windows port
 * Copyright (C) 2004 Thomas Lopatic (thomas@lopatic.de)
 *
 * This file is part of olsrd-unik.
 *
 * olsrd-unik is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * olsrd-unik is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with olsrd-unik; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*
 * Copyright (c) 1996,1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#include <stdio.h>
#include <sys/time.h>
#include <ctype.h>

void gettimeofday(struct timeval *TVal, void *TZone)
{
  SYSTEMTIME SysTime;
  FILETIME FileTime;
  unsigned __int64 Ticks;

  GetSystemTime(&SysTime);
  SystemTimeToFileTime(&SysTime, &FileTime);

  Ticks = ((__int64)FileTime.dwHighDateTime << 32) |
    (__int64)FileTime.dwLowDateTime;

  Ticks -= 116444736000000000LL;

  TVal->tv_sec = (unsigned int)(Ticks / 10000000);
  TVal->tv_usec = (unsigned int)(Ticks % 10000000) / 10;
}

char *StrError(unsigned int ErrNo)
{
  static char Msg[1000];
  
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, ErrNo,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), Msg,
		sizeof (Msg), NULL);
	
  return Msg;
}

void PError(char *Str)
{
  char Msg[1000];
  int Len;

  sprintf(Msg, "ERROR - %s: ", Str);

  Len = strlen(Msg);

  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(),
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), Msg + Len,
                sizeof (Msg) - Len, NULL);

  fprintf(stderr, "%s\n", Msg);
}

void WinSockPError(char *Str)
{
  char Msg[1000];
  int Len;

  sprintf(Msg, "ERROR - %s: ", Str);

  Len = strlen(Msg);

  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, WSAGetLastError(),
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), Msg + Len,
                sizeof (Msg) - Len, NULL);

  fprintf(stderr, "%s\n", Msg);
}

#define NS_INADDRSZ 4
#define NS_IN6ADDRSZ 16
#define NS_INT16SZ 2

static char *inet_ntop4(const unsigned char *src, char *dst, int size)
{
  static const char fmt[] = "%u.%u.%u.%u";
  char tmp[sizeof "255.255.255.255"];

  if (sprintf(tmp, fmt, src[0], src[1], src[2], src[3]) > size)
    return (NULL);

  return strcpy(dst, tmp);
}

static char *inet_ntop6(const unsigned char *src, char *dst, int size)
{
  char tmp[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"], *tp;
  struct { int base, len; } best, cur;
  u_int words[NS_IN6ADDRSZ / NS_INT16SZ];
  int i;

  memset(words, '\0', sizeof words);

  for (i = 0; i < NS_IN6ADDRSZ; i += 2)
    words[i / 2] = (src[i] << 8) | src[i + 1];

  best.base = -1;
  cur.base = -1;

  for (i = 0; i < (NS_IN6ADDRSZ / NS_INT16SZ); i++)
  {
    if (words[i] == 0)
    {
      if (cur.base == -1)
        cur.base = i, cur.len = 1;

      else
        cur.len++;
    }

    else
    {
      if (cur.base != -1)
      {
        if (best.base == -1 || cur.len > best.len)
          best = cur;

        cur.base = -1;
      }
    }
  }

  if (cur.base != -1)
  {
    if (best.base == -1 || cur.len > best.len)
      best = cur;
  }

  if (best.base != -1 && best.len < 2)
    best.base = -1;

  tp = tmp;

  for (i = 0; i < (NS_IN6ADDRSZ / NS_INT16SZ); i++)
  {
    if (best.base != -1 && i >= best.base && i < (best.base + best.len))
    {
      if (i == best.base)
        *tp++ = ':';

      continue;
    }

    if (i != 0)
      *tp++ = ':';

    
    if (i == 6 && best.base == 0 &&
        (best.len == 6 || (best.len == 5 && words[5] == 0xffff)))
    {
      if (!inet_ntop4(src+12, tp, sizeof tmp - (tp - tmp)))
        return (NULL);

      tp += strlen(tp);

      break;
    }

    tp += sprintf(tp, "%x", words[i]);
  }

  if (best.base != -1 && (best.base + best.len) == (NS_IN6ADDRSZ / NS_INT16SZ))
    *tp++ = ':';

  *tp++ = '\0';

  if ((tp - tmp) > size)
    return (NULL);

  return strcpy(dst, tmp);
}

char *inet_ntop(int af, void *src, char *dst, int size)
{
  switch (af)
  {
  case AF_INET:
    return (inet_ntop4(src, dst, size));

  case AF_INET6:
    return (inet_ntop6(src, dst, size));

  default:
    return (NULL);
  }
}

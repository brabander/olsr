/* 
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2004 Thomas Lopatic (thomas@lopatic.de)
 *
 * This file is part of the olsr.org OLSR daemon.
 *
 * olsr.org is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * olsr.org is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with olsr.org; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: compat.c,v 1.5 2004/11/03 18:19:54 tlopatic Exp $
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

#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <ctype.h>
#include "defs.h"

#undef interface
#undef TRUE
#undef FALSE

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

struct ThreadPara
{
  void *(*Func)(void *);
  void *Arg;
};

static unsigned long __stdcall ThreadWrapper(void *Para)
{
  struct ThreadPara *Cast;
  void *(*Func)(void *);
  void *Arg;

  Cast = (struct ThreadPara *)Para;

  Func = Cast->Func;
  Arg = Cast->Arg;
  
  HeapFree(GetProcessHeap(), 0, Para);

  Func(Arg);

  return 0;
}

int pthread_create(HANDLE *Hand, void *Attr, void *(*Func)(void *), void *Arg)
{
  struct ThreadPara *Para;
  unsigned long ThreadId;

  Para = HeapAlloc(GetProcessHeap(), 0, sizeof (struct ThreadPara));

  if (Para == NULL)
    return -1;

  Para->Func = Func;
  Para->Arg = Arg;

  *Hand = CreateThread(NULL, 0, ThreadWrapper, Para, 0, &ThreadId);

  if (*Hand == NULL)
    return -1;

  return 0;
}

int pthread_kill(HANDLE Hand, int Sig)
{
  if (!TerminateThread(Hand, 0))
    return -1;

  return 0;
}

int pthread_mutex_init(HANDLE *Hand, void *Attr)
{
  *Hand = CreateMutex(NULL, FALSE, NULL);

  if (*Hand == NULL)
    return -1;

  return 0;
}

int pthread_mutex_lock(HANDLE *Hand)
{
  if (WaitForSingleObject(*Hand, INFINITE) == WAIT_FAILED)
    return -1;

  return 0;
}

int pthread_mutex_unlock(HANDLE *Hand)
{
  if (!ReleaseMutex(*Hand))
    return -1;

  return 0;
}

void sleep(unsigned int Sec)
{
  Sleep(Sec * 1000);
}

static unsigned int RandState;

void srandom(unsigned int Seed)
{
  RandState = Seed;
}

unsigned int random(void)
{
  RandState = RandState * 1103515245 + 12345;

  return (RandState ^ (RandState >> 16)) % (RAND_MAX + 1);
}

int getpid(void)
{
  return (int)GetCurrentThread();
}

int nanosleep(struct timespec *Req, struct timespec *Rem)
{
  Sleep(Req->tv_sec * 1000 + Req->tv_nsec / 1000000);

  Rem->tv_sec = 0;
  Rem->tv_nsec = 0;

  return 0;
}

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

int inet_aton(char *AddrStr, struct in_addr *Addr)
{
  Addr->s_addr = inet_addr(AddrStr);

  return 1;
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

// XXX - not thread-safe, which is okay for our purposes
 
void *dlopen(char *Name, int Flags)
{
  return (void *)LoadLibrary(Name);
}

int dlclose(void *Handle)
{
  FreeLibrary((HMODULE)Handle);
  return 0;
}

void *dlsym(void *Handle, char *Name)
{
  return GetProcAddress((HMODULE)Handle, Name);
}

char *dlerror(void)
{
  return StrError(GetLastError());
}

#define NS_INADDRSZ 4
#define NS_IN6ADDRSZ 16
#define NS_INT16SZ 2

static int inet_pton4(const char *src, unsigned char *dst)
{
  int saw_digit, octets, ch;
  u_char tmp[NS_INADDRSZ], *tp;

  saw_digit = 0;
  octets = 0;
  *(tp = tmp) = 0;

  while ((ch = *src++) != '\0')
  {
    if (ch >= '0' && ch <= '9') {
      unsigned int new = *tp * 10 + (ch - '0');

      if (new > 255)
        return (0);

      *tp = new;

      if (!saw_digit)
      {
        if (++octets > 4)
          return (0);

        saw_digit = 1;
      }
    }

    else if (ch == '.' && saw_digit)
    {
      if (octets == 4)
        return (0);

      *++tp = 0;

      saw_digit = 0;
    }

    else
      return (0);
  }

  if (octets < 4)
    return (0);

  memcpy(dst, tmp, NS_INADDRSZ);
  return (1);
}

static int inet_pton6(const char *src, unsigned char *dst)
{
  static const char xdigits[] = "0123456789abcdef";
  u_char tmp[NS_IN6ADDRSZ], *tp, *endp, *colonp;
  const char *curtok;
  int ch, saw_xdigit;
  u_int val;

  tp = memset(tmp, '\0', NS_IN6ADDRSZ);
  endp = tp + NS_IN6ADDRSZ;
  colonp = NULL;

  if (*src == ':')
    if (*++src != ':')
      return (0);

  curtok = src;
  saw_xdigit = 0;
  val = 0;

  while ((ch = tolower (*src++)) != '\0')
  {
    const char *pch;

    pch = strchr(xdigits, ch);

    if (pch != NULL)
    {
      val <<= 4;
      val |= (pch - xdigits);

      if (val > 0xffff)
        return (0);

      saw_xdigit = 1;
      continue;
    }

    if (ch == ':')
    {
      curtok = src;

      if (!saw_xdigit)
      {
        if (colonp)
          return (0);

        colonp = tp;
        continue;
      }

      else if (*src == '\0')
      {
        return (0);
      }

      if (tp + NS_INT16SZ > endp)
        return (0);

      *tp++ = (u_char) (val >> 8) & 0xff;
      *tp++ = (u_char) val & 0xff;
      saw_xdigit = 0;
      val = 0;
      continue;
    }

    if (ch == '.' && ((tp + NS_INADDRSZ) <= endp) &&
        inet_pton4(curtok, tp) > 0)
    {
      tp += NS_INADDRSZ;
      saw_xdigit = 0;
      break;
    }

    return (0);
  }

  if (saw_xdigit)
  {
    if (tp + NS_INT16SZ > endp)
      return (0);

    *tp++ = (u_char) (val >> 8) & 0xff;
    *tp++ = (u_char) val & 0xff;
  }

  if (colonp != NULL)
  {
    const int n = tp - colonp;
    int i;

    if (tp == endp)
      return (0);

    for (i = 1; i <= n; i++)
    {
      endp[- i] = colonp[n - i];
      colonp[n - i] = 0;
    }

    tp = endp;
  }

  if (tp != endp)
    return (0);

  memcpy(dst, tmp, NS_IN6ADDRSZ);
  return (1);
}

int inet_pton(int af, char *src, void *dst)
{
  switch (af)
  {
  case AF_INET:
    return (inet_pton4(src, dst));

  case AF_INET6:
    return (inet_pton6(src, dst));

  default:
    return -1;
  }
}

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

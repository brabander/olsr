/*
 * $Id: time.h,v 1.2 2004/09/15 11:18:42 tlopatic Exp $
 * Copyright (C) 2004 Thomas Lopatic (thomas@lopatic.de)
 *
 * This file is part of olsr.org.
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
 */

#if !defined TL_SYS_TIME_H_INCLUDED

#define TL_SYS_TIME_H_INCLUDED

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#undef interface

#define timeradd(x, y, z)                       \
  do                                            \
  {                                             \
    (z)->tv_sec = (x)->tv_sec + (y)->tv_sec;    \
                                                \
    (z)->tv_usec = (x)->tv_usec + (y)->tv_usec; \
                                                \
    if ((z)->tv_usec >= 1000000)                \
    {                                           \
      (z)->tv_sec++;                            \
      (z)->tv_usec -= 1000000;                  \
    }                                           \
  }                                             \
  while (0)

#define timersub(x, y, z)                       \
  do                                            \
  {                                             \
    (z)->tv_sec = (x)->tv_sec - (y)->tv_sec;    \
                                                \
    (z)->tv_usec = (x)->tv_usec - (y)->tv_usec; \
                                                \
    if ((z)->tv_usec < 0)                       \
    {                                           \
      (z)->tv_sec--;                            \
      (z)->tv_usec += 1000000;                  \
    }                                           \
  }                                             \
  while (0)

struct timespec
{
  unsigned int tv_sec;
  unsigned int tv_nsec;
};

int nanosleep(struct timespec *Req, struct timespec *Rem);
void gettimeofday(struct timeval *TVal, void *TZone);

#endif

/*
 * time.c
 *
 *  Created on: Oct 12, 2010
 *      Author: rogge
 */

#include <sys/time.h>

#include "defs.h"
#include "os_time.h"

void
os_sleep(unsigned int Sec)
{
  Sleep(Sec * 1000);
}

int
os_nanosleep(struct timespec *Req, struct timespec *Rem)
{
  Sleep(Req->tv_sec * 1000 + Req->tv_nsec / 1000000);

  Rem->tv_sec = 0;
  Rem->tv_nsec = 0;

  return 0;
}

int
os_gettimeofday(struct timeval *TVal, void *TZone __attribute__ ((unused)))
{
  SYSTEMTIME SysTime;
  FILETIME FileTime;
  unsigned __int64 Ticks;

  GetSystemTime(&SysTime);
  SystemTimeToFileTime(&SysTime, &FileTime);

  Ticks = ((__int64) FileTime.dwHighDateTime << 32) | (__int64) FileTime.dwLowDateTime;

  Ticks -= 116444736000000000LL;

  TVal->tv_sec = (unsigned int)(Ticks / 10000000);
  TVal->tv_usec = (unsigned int)(Ticks % 10000000) / 10;
  return 0;
}

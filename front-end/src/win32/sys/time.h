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

void gettimeofday(struct timeval *TVal, void *TZone);

#endif

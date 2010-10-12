/*
 * time.c
 *
 *  Created on: Oct 12, 2010
 *      Author: rogge
 */

#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#include "os_time.h"

/**
 * Wrapper function for sleep()
 * @param Sec
 */
void
os_sleep(unsigned int Sec)
{
  sleep(Sec);
}

/**
 * Wrapper function for nanosleep()
 * @param Req
 * @param Rem
 * @return
 */
int
os_nanosleep(struct timespec *Req, struct timespec *Rem) {
  return nanosleep(Req, Rem);
}

/**
 * Wrapper function for gettimeofday()
 * @param TVal
 * @param TZone
 * @return
 */
int os_gettimeofday(struct timeval *TVal, void *TZone) {
  return gettimeofday(TVal, TZone);
}

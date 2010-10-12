/*
 * os_time.h
 *
 *  Created on: Oct 12, 2010
 *      Author: rogge
 */

#ifndef OS_TIME_H_
#define OS_TIME_H_

#include <sys/time.h>
#include "defs.h"

int EXPORT(os_gettimeofday)(struct timeval *TVal, void *TZone);
void EXPORT(os_sleep)(unsigned int Sec);
int EXPORT(os_nanosleep)(struct timespec *Req, struct timespec *Rem);

#endif /* OS_TIME_H_ */

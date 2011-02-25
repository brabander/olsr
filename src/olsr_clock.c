
/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004-2009, the olsr.org team - see HISTORY file
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

#include "os_time.h"
#include "olsr_logging.h"
#include "olsr.h"
#include "olsr_clock.h"

/* Timer data */
static uint32_t now_times;             /* relative time compared to startup (in milliseconds */
static struct timeval first_tv;        /* timevalue during startup */
static struct timeval last_tv;         /* timevalue used for last olsr_times() calculation */

void
olsr_clock_init(void) {
  /* Grab initial timestamp */
  if (os_gettimeofday(&first_tv, NULL)) {
    OLSR_ERROR(LOG_TIMER, "OS clock is not working, have to shut down OLSR (%s)\n", strerror(errno));
    olsr_exit(1);
  }
  last_tv = first_tv;
  olsr_timer_updateClock();
}

/**
 * Update the internal clock to current system time
 */
void
olsr_timer_updateClock(void)
{
  struct timeval tv;
  uint32_t t;

  if (os_gettimeofday(&tv, NULL) != 0) {
    OLSR_ERROR(LOG_SCHEDULER, "OS clock is not working, have to shut down OLSR (%s)\n", strerror(errno));
    olsr_exit(1);
  }

  /* test if time jumped backward or more than 60 seconds forward */
  if (tv.tv_sec < last_tv.tv_sec || (tv.tv_sec == last_tv.tv_sec && tv.tv_usec < last_tv.tv_usec)
      || tv.tv_sec - last_tv.tv_sec > 60) {
    OLSR_WARN(LOG_SCHEDULER, "Time jump (%d.%06d to %d.%06d)\n",
              (int32_t) (last_tv.tv_sec), (int32_t) (last_tv.tv_usec), (int32_t) (tv.tv_sec), (int32_t) (tv.tv_usec));

    t = (last_tv.tv_sec - first_tv.tv_sec) * 1000 + (last_tv.tv_usec - first_tv.tv_usec) / 1000;
    t++;                        /* advance time by one millisecond */

    first_tv = tv;
    first_tv.tv_sec -= (t / 1000);
    first_tv.tv_usec -= ((t % 1000) * 1000);

    if (first_tv.tv_usec < 0) {
      first_tv.tv_sec--;
      first_tv.tv_usec += 1000000;
    }
    last_tv = tv;
    now_times =  t;
  }
  last_tv = tv;
  now_times = (tv.tv_sec - first_tv.tv_sec) * 1000 + (tv.tv_usec - first_tv.tv_usec) / 1000;
}

/**
 * Calculates the current time in the internal OLSR representation
 * @return current time
 */
uint32_t
olsr_timer_getNow(void) {
  return now_times;
}

/**
 * Returns the number of milliseconds until the timestamp will happen
 * @param absolute timestamp
 * @return milliseconds until event will happen, negative if it already
 *   happened.
 */
int32_t
olsr_timer_getRelative(uint32_t absolute)
{
  uint32_t diff;
  if (absolute > now_times) {
    diff = absolute - now_times;

    /* overflow ? */
    if (diff > (1u << 31)) {
      return -(int32_t) (0xffffffff - diff);
    }
    return (int32_t) (diff);
  }

  diff = now_times - absolute;
  /* overflow ? */
  if (diff > (1u << 31)) {
    return (int32_t) (0xffffffff - diff);
  }
  return -(int32_t) (diff);
}

/**
 * Checks if a timestamp has already happened
 * @param absolute timestamp
 * @return true if the event already happened, false otherwise
 */
bool
olsr_timer_isTimedOut(uint32_t absolute)
{
  if (absolute > now_times) {
    return absolute - now_times > (1u << 31);
  }

  return now_times - absolute <= (1u << 31);
}

/**
 *Function that converts a double to a mantissa/exponent
 *product as described in RFC3626:
 *
 * value = C*(1+a/16)*2^b [in seconds]
 *
 *  where a is the integer represented by the four highest bits of the
 *  field and b the integer represented by the four lowest bits of the
 *  field.
 *
 *@param interval the time interval to process
 *
 *@return a 8-bit mantissa/exponent product
 */
uint8_t
reltime_to_me(const uint32_t interval)
{
  uint8_t a = 0, b = 0;                /* Underflow defaults */

  /* It is sufficent to compare the integer part since we test on >=.
   * So we have now only a floating point division and the rest of the loop
   * are only integer operations.
   *
   * const unsigned int unscaled_interval = interval / VTIME_SCALE_FACTOR;
   *
   * VTIME_SCALE_FACTOR = 1/16
   *
   * => unscaled_interval = interval(ms) / 1000 * 16
   *                      = interval(ms) / 125 * 2
   */

  unsigned unscaled_interval = interval;
  while (unscaled_interval >= 62) {
    unscaled_interval >>= 1;
    b++;
  }

  if (0 < b) {
    if (15 < --b) {
      a = 15;                   /* Overflow defaults */
      b = 15;
    } else {
      a = (interval >> (b + 2)) - 15;
    }
  }

  return (a << 4) + b;
}

/**
 * Function for converting a mantissa/exponent 8bit value back
 * to double as described in RFC3626:
 *
 * value = C*(1+a/16)*2^b [in seconds]
 *
 *  where a is the integer represented by the four highest bits of the
 *  field and b the integer represented by the four lowest bits of the
 *  field.
 *
 * me is the 8 bit mantissa/exponent value
 *
 * To avoid expensive floating maths, we transform the equation:
 *     value = C * (1 + a / 16) * 2^b
 * first, we make an int addition from the floating point addition:
 *     value = C * ((16 + a) / 16) * 2^b
 * then we get rid of a pair of parentheses
 *     value = C * (16 + a) / 16 * 2^b
 * and now we make an int multiplication from the floating point one
 *     value = C * (16 + a) * 2^b / 16
 * so that we can make a shift from the multiplication
 *     value = C * ((16 + a) << b) / 16
 * and sionce C and 16 are constants
 *     value = ((16 + a) << b) * C / 16
 *
 * VTIME_SCALE_FACTOR = 1/16
 *
 * =>  value(ms) = ((16 + a) << b) / 256 * 1000
 *
 * 1. case: b >= 8
 *           = ((16 + a) << (b-8)) * 1000
 *
 * 2. case: b <= 8
 *           = ((16 + a) * 1000) >> (8-b)
 */
uint32_t
me_to_reltime(const uint8_t me)
{
  const uint8_t a = me >> 4;
  const uint8_t b = me & 0x0F;

  if (b >= 8) {
    return ((16 + a) << (b - 8)) * 1000;
  }
  assert(me == reltime_to_me(((16 + a) * 1000) >> (8 - b)));
  return ((16 + a) * 1000) >> (8 - b);
}

/**
 * converts an unsigned integer value into a string representation
 * (divided by 1000)
 */
char *
olsr_milli_to_txt(struct millitxt_buf *buffer, uint32_t t) {
  sprintf(buffer->buf, "%u.%03u", t/1000, t%1000);
  return buffer->buf;
}

/**
 * converts a floating point text into a unsigned integer representation
 * (multiplied by 1000)
 */
uint32_t olsr_txt_to_milli(char *txt) {
  uint32_t t = 0;
  int fractionDigits = 0;
  bool frac = false;

  while (fractionDigits < 3 && *txt) {
    if (*txt == '.' && !frac) {
      frac = true;
      txt++;
    }

    if (!isdigit(*txt)) {
      break;
    }

    t = t * 10 + (*txt++ - '0');
    if (frac) {
      fractionDigits++;
    }
  }

  while (fractionDigits++ < 3) {
    t *= 10;
  }
  return t;
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2004 Andreas Tønnesen (andreto@ifi.uio.no)
 *
 * This file is part of olsrd-unik.
 *
 * UniK olsrd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * UniK olsrd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with olsrd-unik; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */


#include "mantissa.h"
#include "math.h"

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

olsr_u8_t
double_to_me(double interval)
{
  int a, b;

  b = 0;

  while(interval / VTIME_SCALE_FACTOR >= pow((double)2, (double)b))
    b++;

  b--;
  if(b < 0)
    {
      a = 1;
      b = 0;
    } 
  else 
    if (b > 15)
      {
	a = 15;
	b = 15;
      } 
    else 
      { 
	a = (int)(16*((double)interval/(VTIME_SCALE_FACTOR*(double)pow(2,b))-1));
	while(a >= 16)
	  {
	    a -= 16;
	    b++;
	  }
      }

  //printf("Generated mantissa/exponent(%d/%d): %d from %f\n", a, b, (olsr_u8_t) (a*16+b), interval);
  //printf("Resolves back to: %f\n", me_to_double((olsr_u8_t) (a*16+b)));
  return (olsr_u8_t) (a*16+b);
}




/**
 *Function that converts a mantissa/exponent 8bit value back
 *to double as described in RFC3626:
 *
 * value = C*(1+a/16)*2^b [in seconds]
 *
 *  where a is the integer represented by the four highest bits of the
 *  field and b the integer represented by the four lowest bits of the
 *  field.
 *
 *@param me the 8 bit mantissa/exponen value
 *
 *@return a double value
 */
double
me_to_double(olsr_u8_t me)
{
  int a, b;

  a = me>>4;
  b = me - a*16;

  return (double)(VTIME_SCALE_FACTOR*(1+(double)a/16)*(double)pow(2,b));
}

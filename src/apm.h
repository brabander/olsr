/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2003 Andreas Tønnesen (andreto@ifi.uio.no)
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
 */


#ifndef _OLSR_APM
#define _OLSR_APM

/*
 * Interface to OS dependent power management information
 */

#define OLSR_BATTERY_POWERED  0
#define OLSR_AC_POWERED       1

struct olsr_apm_info
{
  int ac_line_status;
  int battery_percentage;
};


int apm_init();

void apm_printinfo(struct olsr_apm_info *);

/* 
 * This function should return 0 if no powerinfo
 * is available. If returning 1 the function must
 * fill the provided olsr_apm_info struct with
 * the current power status.
 */

int apm_read(struct olsr_apm_info *);

#endif

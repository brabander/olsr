/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2003 Andreas Tønnesen (andreto@ifi.uio.no)
 *
 * This file is part of uolsrd.
 *
 * uolsrd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * uolsrd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with uolsrd; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */


#ifndef _OLSR_APM
#define _OLSR_APM


struct olsr_apm_info
{
  char driver_version[10];
  int apm_version_major;
  int apm_version_minor;
  int apm_flags;
  int ac_line_status;
  int battery_status;
  int battery_flags;
  int battery_percentage;
  int battery_time;
  int using_minutes;
};


int apm_init();

int apm_printinfo(struct olsr_apm_info *);

int apm_read(struct olsr_apm_info *);

#endif

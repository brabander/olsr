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
 * 
 * $Id: apm.c,v 1.5 2004/09/21 19:08:58 kattemat Exp $
 *
 */

#include "../apm.h"
#include <stdio.h>
#include <string.h>

#define APM_PROC "/proc/apm"

struct linux_apm_info
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


extern int
olsr_printf(int, char *, ...);


int 
apm_init()
{
  struct olsr_apm_info ainfo;

  olsr_printf(3, "Initializing APM\n");

  if(apm_read(&ainfo))
    apm_printinfo(&ainfo);

  return 1;
}

void
apm_printinfo(struct olsr_apm_info *ainfo)
{
  olsr_printf(5, "APM info:\n\tAC status %d\n\tBattery percentage %d%%\n\n",
	      ainfo->ac_line_status,
	      ainfo->battery_percentage);

  return;
}


int
apm_read(struct olsr_apm_info *ainfo)
{
  char buffer[100];
  char units[10];
  FILE *apm_procfile;
  struct linux_apm_info lainfo;

  /* Open procfile */
  if((apm_procfile = fopen(APM_PROC, "r")) == NULL)
    return 0;


  fgets(buffer, sizeof(buffer) - 1, apm_procfile);
  if(buffer == NULL)
    {
      /* Try re-opening the file */
      if((apm_procfile = fopen(APM_PROC, "r")) < 0)
	return 0;
      fgets(buffer, sizeof(buffer) - 1, apm_procfile);
      if(buffer == NULL)
	{
	  /* Giving up */
	  fprintf(stderr, "OLSRD: Could not read APM info - setting willingness to default");
	  fclose(apm_procfile);
	  return 0;
	}
    }

  buffer[sizeof(buffer) - 1] = '\0';

  //printf("READ: %s\n", buffer);

  /* Get the info */
  sscanf(buffer, "%s %d.%d %x %x %x %x %d%% %d %s\n",
	 lainfo.driver_version,
	 &lainfo.apm_version_major,
	 &lainfo.apm_version_minor,
	 &lainfo.apm_flags,
	 &lainfo.ac_line_status,
	 &lainfo.battery_status,
	 &lainfo.battery_flags,
	 &lainfo.battery_percentage,
	 &lainfo.battery_time,
	 units);

  lainfo.using_minutes = !strncmp(units, "min", 3) ? 1 : 0;

  /*
   * Should take care of old APM type info here
   */

  /*
   * Fix possible percentage error
   */
  if(lainfo.battery_percentage > 100)
    lainfo.battery_percentage = -1;

  /* Fill the provided struct */

  if(lainfo.ac_line_status)
    ainfo->ac_line_status = OLSR_AC_POWERED;
  else
    ainfo->ac_line_status = OLSR_BATTERY_POWERED;
  
  ainfo->battery_percentage = lainfo.battery_percentage;
  
  fclose(apm_procfile);

  return 1;
}

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

#include "../apm.h"
#include <stdio.h>
#include <string.h>

#define APM_PROC "/proc/apm"

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

int
apm_printinfo(struct olsr_apm_info *ainfo)
{
  olsr_printf(5, "APM info:\n\tKernel driver %s(APM BIOS %d.%d)\n\tFlags:0x%08d\n\tAC status %d\n\tBattery status %d\n\tBattery flags 0x%08x\n\tBattery percentage %d%%\n\tBattery time left: %d min\n\n",
	      ainfo->driver_version,
	      ainfo->apm_version_major,
	      ainfo->apm_version_minor,
	      ainfo->apm_flags,
	      ainfo->ac_line_status,
	      ainfo->battery_status,
	      ainfo->battery_flags,
	      ainfo->battery_percentage,
	      ainfo->battery_time);
	 
  if((ainfo->battery_status == 255) && (ainfo->ac_line_status))
    {
      olsr_printf(2, "No batterys detected\n");
    }
  else
    {
      if((ainfo->battery_status != 255) && (ainfo->ac_line_status))
	{
	  olsr_printf(3, "Battery powered system detected - currently running on AC power\n");
	}
      else
	if(!ainfo->ac_line_status)
	  {
	    olsr_printf(3, "System running on batterys\n");
	  }
    }


  return 0;
}


int
apm_read(struct olsr_apm_info *ainfo)
{
  char buffer[100];
  char units[10];
  FILE *apm_procfile;

  /* Open procfile */
  if((apm_procfile = fopen(APM_PROC, "r")) == NULL)
    return -1;


  fgets(buffer, sizeof(buffer) - 1, apm_procfile);
  if(buffer == NULL)
    {
      /* Try re-opening the file */
      if((apm_procfile = fopen(APM_PROC, "r")) < 0)
	return -1;
      fgets(buffer, sizeof(buffer) - 1, apm_procfile);
      if(buffer == NULL)
	{
	  /* Giving up */
	  fprintf(stderr, "OLSRD: Could not read APM info - setting willingness to default");
	  return -1;
	}
    }

  buffer[sizeof(buffer) - 1] = '\0';

  //printf("READ: %s\n", buffer);

  /* Get the info */
  sscanf(buffer, "%s %d.%d %x %x %x %x %d%% %d %s\n",
	 ainfo->driver_version,
	 &ainfo->apm_version_major,
	 &ainfo->apm_version_minor,
	 &ainfo->apm_flags,
	 &ainfo->ac_line_status,
	 &ainfo->battery_status,
	 &ainfo->battery_flags,
	 &ainfo->battery_percentage,
	 &ainfo->battery_time,
	 units);

  ainfo->using_minutes = !strncmp(units, "min", 3) ? 1 : 0;

  /*
   * Should take care of old APM type info here
   */

  /*
   * Fix possible percentage error
   */
  if(ainfo->battery_percentage > 100)
    ainfo->battery_percentage = -1;

  fclose(apm_procfile);

  return 0;
}

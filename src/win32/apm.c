/*
 * Functions for the Windows port
 * Copyright (C) 2004 Thomas Lopatic (thomas@lopatic.de)
 *
 * Derived from their Linux counterparts
 * Copyright (C) 2003 Andreas Tønnesen (andreto@ifi.uio.no)
 *
 * This file is part of olsrd-unik.
 *
 * olsrd-unik is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * olsrd-unik is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with olsrd-unik; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "../apm.h"
#include <stdio.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef interface

extern int olsr_printf(int, char *, ...);

int apm_init()
{
  struct olsr_apm_info ApmInfo;

  olsr_printf(3, "Initializing APM\n");

  if(apm_read(&ApmInfo) < 0)
    return -1;

  apm_printinfo(&ApmInfo);

  return 0;
}

int apm_printinfo(struct olsr_apm_info *ApmInfo)
{
  olsr_printf(5, "APM info:\n\tAC status %d\n\tBattery status %d\n\tBattery percentage %d%%\n\tBattery time left: %d min\n\n",
	      ApmInfo->ac_line_status,
	      ApmInfo->battery_status,
	      ApmInfo->battery_percentage,
	      ApmInfo->battery_time);
	 
  if(ApmInfo->battery_status >= 128)
    olsr_printf(2, "No batteries detected\n");

  else if (ApmInfo->ac_line_status)
    olsr_printf(3, "Battery powered system detected - currently running on AC power\n");

  else
    olsr_printf(3, "System running on batteries\n");

  return 0;
}

int apm_read(struct olsr_apm_info *ApmInfo)
{
  SYSTEM_POWER_STATUS PowerStat;

  memset(ApmInfo, 0, sizeof (struct olsr_apm_info));

  if (!GetSystemPowerStatus(&PowerStat))
    return -1;

  ApmInfo->ac_line_status = PowerStat.ACLineStatus;
  ApmInfo->battery_status = PowerStat.BatteryFlag;
  ApmInfo->battery_percentage = PowerStat.BatteryLifePercent;
  ApmInfo->battery_time = PowerStat.BatteryLifeTime;

  return 0;
}

/* 
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2004 Thomas Lopatic (thomas@lopatic.de)
 *
 * Derived from its Linux counterpart.
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
 * $Id: apm.c,v 1.6 2004/11/03 18:19:54 tlopatic Exp $
 *
 */

#include "../apm.h"
#include <stdio.h>
#include <string.h>

#undef interface
#undef TRUE
#undef FALSE

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

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

void apm_printinfo(struct olsr_apm_info *ApmInfo)
{
  olsr_printf(5, "APM info:\n\tAC status %d\n\tBattery percentage %d%%\n\n",
	      ApmInfo->ac_line_status,
	      ApmInfo->battery_percentage);
}

int apm_read(struct olsr_apm_info *ApmInfo)
{
  SYSTEM_POWER_STATUS PowerStat;

  memset(ApmInfo, 0, sizeof (struct olsr_apm_info));

  if (!GetSystemPowerStatus(&PowerStat))
    return 0;

  ApmInfo->ac_line_status = (PowerStat.ACLineStatus == 1) ?
    OLSR_AC_POWERED : OLSR_BATTERY_POWERED;
    
  ApmInfo->battery_percentage = (PowerStat.BatteryLifePercent <= 100) ?
    PowerStat.BatteryLifePercent : 0;

  return 1;
}


/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2004 Andreas Tønnesen (andreto@ifi.uio.no)
 *
 * This file is part of olsr.org.
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
 * $Id: log.c,v 1.3 2004/09/21 19:08:58 kattemat Exp $
 *
 */

/* 
 * System logging interface for GNU/Linux systems
 */

#include "../log.h"
#include <syslog.h>
#include <stdarg.h>

void
olsr_openlog(const char *ident)
{
  openlog(ident, LOG_PID | LOG_ODELAY, LOG_DAEMON);
  setlogmask(LOG_UPTO(LOG_INFO));

  return;
}


void
olsr_syslog(int level, char *format, ...)
{

  int linux_level;
  va_list arglist;

  switch(level)
    {
    case(OLSR_LOG_INFO):
      linux_level = LOG_INFO;
      break;
    case(OLSR_LOG_ERR):
      linux_level = LOG_ERR;
      break;
    default:
      return;
    }

  va_start(arglist, format);
  vsyslog(linux_level, format, arglist);
  va_end(arglist);

  return;
}

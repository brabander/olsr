
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
 */

/* 
 * System logging interface
 * Platform independent - the implementations
 * reside in <OS>/log.c(e.g. linux/log.c)
 */

#ifndef _OLSR_SYSLOG_H
#define _OLSR_SYSLOG_H

#define OLSR_LOG_INFO            1
#define OLSR_LOG_ERR             2

void
olsr_openlog(const char *ident);

void
olsr_syslog(int level, char *format, ...);


#endif

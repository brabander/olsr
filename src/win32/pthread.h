/* 
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2004 Thomas Lopatic (thomas@lopatic.de)
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
 * $Id: pthread.h,v 1.4 2004/11/03 18:19:54 tlopatic Exp $
 *
 */

#if !defined TL_PTHREAD_H_INCLUDED

#define TL_PTHREAD_H_INCLUDED

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef interface
#undef TRUE
#undef FALSE

typedef HANDLE pthread_mutex_t;
typedef HANDLE pthread_t;

int pthread_create(HANDLE *Hand, void *Attr, void *(*Func)(void *), void *Arg);
int pthread_kill(HANDLE Hand, int Sig);
int pthread_mutex_init(HANDLE *Hand, void *Attr);
int pthread_mutex_lock(HANDLE *Hand);
int pthread_mutex_unlock(HANDLE *Hand);

#endif

/*
 * $Id: dlfcn.h,v 1.2 2004/09/15 11:18:42 tlopatic Exp $
 * Copyright (C) 2004 Thomas Lopatic (thomas@lopatic.de)
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

#if !defined TL_DLFCN_H_INCLUDED

#define TL_DLFCN_H_INCLUDED

#define RTLD_NOW 0

void *dlopen(char *Name, int Flags);
int dlclose(void *Handle);
void *dlsym(void *Handle, char *Name);
char *dlerror(void);

#endif

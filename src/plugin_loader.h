/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2004 Andreas Tønnesen (andreto@ifi.uio.no)
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
 * $Id: plugin_loader.h,v 1.6 2004/10/09 22:32:47 kattemat Exp $
 *
 */

#ifndef _OLSR_PLUGIN_LOADER
#define _OLSR_PLUGIN_LOADER

#include <dlfcn.h>
#include <stdio.h>
#include "olsr_protocol.h"

#define MAX_LIBS 10

#define PLUGIN_INTERFACE_VERSION 1

/* Data to sent to the plugin with the register_olsr_function call */
struct olsr_plugin_data
{
  int ipversion;
  union olsr_ip_addr *main_addr;
  int (*olsr_plugin_io)(int, void *, size_t);
};


struct olsr_plugin
{
  /* The handle */
  void *dlhandle;

  int (*register_olsr_data)(struct olsr_plugin_data *);

  /* Multi - purpose function */
  int (*plugin_io)(int, void *, size_t);

  struct olsr_plugin *next;
};


struct olsr_plugin *olsr_plugins;


struct plugin_to_load
{
  char name[FILENAME_MAX];
  struct plugin_to_load *next;
};

struct plugin_to_load *plugins_to_load;

void
olsr_init_plugin_loader(void);

void
olsr_add_plugin(char *);

int
olsr_load_plugins(void);

void
init_olsr_plugin(struct olsr_plugin *);

int
olsr_load_dl(char *);

void
olsr_close_plugins(void);

#endif

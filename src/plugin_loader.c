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
 * $Id: plugin_loader.c,v 1.5 2004/09/21 19:08:57 kattemat Exp $
 *
 */

#include "plugin_loader.h"
#include "defs.h"
#include "olsr.h"
#include "scheduler.h"
#include "parser.h"
#include "duplicate_set.h"
#include "plugin.h"
#include "link_set.h"

/**
 *Initializes the plugin loader engine
 *
 */

void
olsr_init_plugin_loader()
{
  olsr_plugins = NULL;
  plugins_to_load = NULL;
}


/**
 *Function that loads all registered plugins
 *
 *@return the number of plugins loaded
 */
int
olsr_load_plugins()
{
  struct plugin_to_load *entry, *old;
  int loaded;

  entry = plugins_to_load;
  loaded = 0;

  olsr_printf(1, "Loading plugins...\n\n");

  while(entry)
    {  
      if(olsr_load_dl(entry->name) < 0)
	olsr_printf(1, "-- PLUGIN LOADING FAILED! --\n\n");
      else
	loaded ++; /* I'm loaded! */

      old = entry;
      entry = entry->next;
      free(old);
    }
  return loaded;
}


/**
 *Function to add a plugin to the set of
 *plugins to be loaded
 *
 *@param name filename of the lib. Must include
 *full path if the file is not located in the standard
 *lib directories
 */
void
olsr_add_plugin(char *name)
{
  struct plugin_to_load *entry;

  olsr_printf(3, "Adding plugin: %s\n", name);

  entry = olsr_malloc(sizeof(struct plugin_to_load), "Add plugin entry");

  strcpy(entry->name, name);
  entry->next = plugins_to_load;
  plugins_to_load = entry;
}


/**
 *Try to load a shared library and extract
 *the required information
 *
 *@param libname the name of the library(file)
 *
 *@return negative on error
 */
int
olsr_load_dl(char *libname)
{
  struct olsr_plugin new_entry, *entry;
  int *interface_version;

  olsr_printf(1, "---------- Plugin loader ----------\nLibrary: %s\n", libname);

  if((new_entry.dlhandle = dlopen(libname, RTLD_NOW)) == NULL)
    {
      olsr_printf(1, "DL loading failed: %s!\n", dlerror());
      return -1;
    }


  /* Fetch the multipurpose function */
  olsr_printf(1, "Checking plugin interface version....");
  /* Register mp function */
  if((interface_version = dlsym(new_entry.dlhandle, "plugin_interface_version")) == NULL)
    {
      olsr_printf(1, "\nPlug-in interface version location failed!\n%s\n", dlerror());
      dlclose(new_entry.dlhandle);
      return -1;
    }
  else
    {
      olsr_printf(1, " %d - ", *interface_version);
      if(*interface_version != PLUGIN_INTERFACE_VERSION)
	{
	  olsr_printf(1, "VERSION MISSMATCH!\n");
	  dlclose(new_entry.dlhandle);
	  return -1;
	}
      else
	olsr_printf(1, "OK\n");
    }


  /* Fetch the multipurpose function */
  olsr_printf(1, "Trying to fetch plugin IO function....");
  /* Register mp function */
  if((new_entry.plugin_io = dlsym(new_entry.dlhandle, "plugin_io")) == NULL)
    {
      olsr_printf(1, "\nPlug-in IO function location %s failed!\n%s\n", libname, dlerror());
      dlclose(new_entry.dlhandle);
      return -1;
    }
  olsr_printf(1, "OK\n");


  olsr_printf(1, "Trying to fetch register function....");

  if((new_entry.register_olsr_data = dlsym(new_entry.dlhandle, "register_olsr_data")) == NULL)
    {
      olsr_printf(1, "\nCould not find function registration function in plugin!\n", dlerror());
      dlclose(new_entry.dlhandle);
      return -1;
    }
  olsr_printf(1, "OK\n");

  entry = olsr_malloc(sizeof(struct olsr_plugin), "Plugin entry");

  memcpy(entry, &new_entry, sizeof(struct olsr_plugin));

  /* Initialize the plugin */
  init_olsr_plugin(entry);

  /* queue */
  entry->next = olsr_plugins;
  olsr_plugins = entry;

  olsr_printf(1, "---------- LIBRARY LOADED ----------\n\n");

  return 0;
}



/**
 *Initialize a loaded plugin
 *This includes sending information
 *from olsrd to the plugin and
 *register the functions from the flugin with olsrd
 *
 *@param entry the plugin to initialize
 *
 *@return nada
 */
void
init_olsr_plugin(struct olsr_plugin *entry)
{
  struct olsr_plugin_data plugin_data;

  olsr_printf(1, "Running registration function...\n");
  /* Fill struct */
  plugin_data.ipversion = ipversion;
  plugin_data.main_addr = &main_addr;

  plugin_data.olsr_plugin_io = &olsr_plugin_io;

  /* Register data with plugin */
  entry->register_olsr_data(&plugin_data);

}



/**
 *Close all loaded plugins
 */
void
olsr_close_plugins()
{
  struct olsr_plugin *entry;
  
  for(entry = olsr_plugins; 
      entry != NULL ; 
      entry = entry->next)
    {
      dlclose(entry->dlhandle);
    }

}

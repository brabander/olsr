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
 * $Id: plugin_loader.c,v 1.10 2004/11/06 09:20:09 kattemat Exp $
 *
 */

#include "plugin_loader.h"
#include "defs.h"
#include "plugin.h"

/* Local functions */

static void
init_olsr_plugin(struct olsr_plugin *);

static int
olsr_load_dl(char *, struct plugin_param *);




/**
 *Function that loads all registered plugins
 *
 *@return the number of plugins loaded
 */
int
olsr_load_plugins()
{
  struct plugin_entry *entry;
  int loaded;

  entry = olsr_cnf->plugins;
  loaded = 0;

  olsr_printf(1, "Loading plugins...\n\n");

  while(entry)
    {  
      if(olsr_load_dl(entry->name, entry->params) < 0)
	olsr_printf(1, "-- PLUGIN LOADING FAILED! --\n\n");
      else
	loaded ++;

      entry = entry->next;
    }
  return loaded;
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
olsr_load_dl(char *libname, struct plugin_param *params)
{
  struct olsr_plugin new_entry, *entry;
  int *interface_version;


  olsr_printf(1, "---------- Plugin loader ----------\nLibrary: %s\n", libname);

  if((new_entry.dlhandle = dlopen(libname, RTLD_NOW)) == NULL)
    {
      olsr_printf(1, "DL loading failed: \"%s\"!\n", dlerror());
      return -1;
    }

  /* Fetch the multipurpose function */
  olsr_printf(1, "Checking plugin interface version....");
  /* Register mp function */
  if((interface_version = dlsym(new_entry.dlhandle, "plugin_interface_version")) == NULL)
    {
      olsr_printf(1, "FAILED: \"%s\"\n", dlerror());
      dlclose(new_entry.dlhandle);
      return -1;
    }
  else
    {
      olsr_printf(1, " %d - ", *interface_version);
      if(*interface_version != PLUGIN_INTERFACE_VERSION)
	olsr_printf(1, "WARNING: VERSION MISSMATCH!\n");
      else
	olsr_printf(1, "OK\n");
    }

  olsr_printf(1, "Trying to fetch register function....");
  
  if((new_entry.register_olsr_data = dlsym(new_entry.dlhandle, "register_olsr_data")) == NULL)
    {
      /* This function must be present */
      olsr_printf(1, "\nCould not find function registration function in plugin!\n%s\nCRITICAL ERROR - aborting!\n", dlerror());
      dlclose(new_entry.dlhandle);
      return -1;
    }
  olsr_printf(1, "OK\n");


  /* Fetch the multipurpose function */
  olsr_printf(1, "Trying to fetch plugin IO function....");
  if((new_entry.plugin_io = dlsym(new_entry.dlhandle, "plugin_io")) == NULL)
    olsr_printf(1, "FAILED: \"%s\"\n", dlerror());
  else
    olsr_printf(1, "OK\n");

  /* Fetch the parameter function */
  olsr_printf(1, "Trying to fetch param function....");
  if((new_entry.register_param = dlsym(new_entry.dlhandle, "register_olsr_param")) == NULL)
    olsr_printf(1, "FAILED: \"%s\"\n", dlerror());
  else
    olsr_printf(1, "OK\n");


  entry = olsr_malloc(sizeof(struct olsr_plugin), "Plugin entry");

  memcpy(entry, &new_entry, sizeof(struct olsr_plugin));

  entry->params = params;

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
  struct plugin_param *params = entry->params;
  int retval;

  if(entry->register_param)
    {
      olsr_printf(1, "Sending parameters...\n");
      while(params)
	{
	  olsr_printf(1, "\"%s\"/\"%s\".... ", params->key, params->value);
	  if((retval = entry->register_param(params->key, params->value)) < 0)
	    {
	      fprintf(stderr, "\nFatal error in plugin parameter \"%s\"/\"%s\"\n", params->key, params->value);
	      exit(EXIT_FAILURE);
	    }
	  retval == 0 ? olsr_printf(1, "FAILED\n") : olsr_printf(1, "OK\n");

	  params = params->next;
	}
    }

  olsr_printf(1, "Running registration function...\n");
  /* Fill struct */
  plugin_data.ipversion = olsr_cnf->ip_version;
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

  olsr_printf(1, "Closing plugins...\n");
  for(entry = olsr_plugins; 
      entry != NULL ; 
      entry = entry->next)
    {
      dlclose(&entry->dlhandle);
    }

}

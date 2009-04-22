
/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004-2009, the olsr.org team - see HISTORY file
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of olsr.org, olsrd nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Visit http://www.olsr.org for more information.
 *
 * If you find this software useful feel free to make a donation
 * to the project. For more information see the website or contact
 * the copyright holders.
 *
 */

#include "plugin_loader.h"
#include "plugin.h"
#include "plugin_util.h"
#include "defs.h"
#include "olsr.h"
#include "olsr_logging.h"

#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>


/* Local functions */
static int init_olsr_plugin(struct olsr_plugin *);
static int olsr_load_dl(char *, struct plugin_param *);
static int olsr_add_dl(struct olsr_plugin *);

static struct olsr_plugin *olsr_plugins = NULL;


/**
 *Function that loads all registered plugins
 *
 *@return the number of plugins loaded
 */
void
olsr_load_plugins(void)
{
  struct plugin_entry *entry = olsr_cnf->plugins;
  int rv = 0;
  OLSR_INFO(LOG_PLUGINS, "-- LOADING PLUGINS --\n");
  for (entry = olsr_cnf->plugins; entry != NULL; entry = entry->next) {
    if (olsr_load_dl(entry->name, entry->params) < 0) {
      rv = 1;
    }
  }
  if (rv != 0) {
    OLSR_ERROR(LOG_PLUGINS, "-- PLUGIN LOADING FAILED! --\n");
    olsr_exit(1);
  }
  OLSR_INFO(LOG_PLUGINS, "-- ALL PLUGINS LOADED! --\n\n");
}

/**
 *Try to load a shared library and extract
 *the required information
 *
 *@param libname the name of the library(file)
 *
 *@return negative on error
 */
static int
olsr_load_dl(char *libname, struct plugin_param *params)
{
  struct olsr_plugin *plugin = olsr_malloc(sizeof(struct olsr_plugin), "Plugin entry");
  int rv;

  if (olsr_cnf->dlPath) {
    char *path = olsr_malloc(strlen(olsr_cnf->dlPath) + strlen(libname) + 1, "Memory for absolute library path");
    strcpy(path, olsr_cnf->dlPath);
    strcat(path, libname);
    OLSR_INFO(LOG_PLUGINS, "---------- LOADING LIBRARY %s from %s (%s)----------\n", libname, olsr_cnf->dlPath, path);
    plugin->dlhandle = dlopen(path, RTLD_NOW);
    free(path);
  } else {
    OLSR_INFO(LOG_PLUGINS, "---------- LOADING LIBRARY %s ----------\n", libname);
    plugin->dlhandle = dlopen(libname, RTLD_NOW);
  }
  if (plugin->dlhandle == NULL) {
    const int save_errno = errno;
    OLSR_ERROR(LOG_PLUGINS, "DL loading failed: \"%s\"!\n", dlerror());
    free(plugin);
    errno = save_errno;
    return -1;
  }

  rv = olsr_add_dl(plugin);
  if (rv == -1) {
    const int save_errno = errno;
    dlclose(plugin->dlhandle);
    free(plugin);
    errno = save_errno;
    OLSR_ERROR(LOG_PLUGINS, "---------- LIBRARY %s FAILED ----------\n\n", libname);
    return -1;
  }

  plugin->params = params;

  /* Initialize the plugin */
  if (init_olsr_plugin(plugin) != 0) {
    const int save_errno = errno;
    dlclose(plugin->dlhandle);
    free(plugin);
    errno = save_errno;
    OLSR_ERROR(LOG_PLUGINS, "---------- LIBRARY %s FAILED ----------\n\n", libname);
    return -1;
  }

  /* queue */
  plugin->next = olsr_plugins;
  olsr_plugins = plugin;

  OLSR_INFO(LOG_PLUGINS, "---------- LIBRARY %s LOADED ----------\n\n", libname);
  return rv;
}

static int
olsr_add_dl(struct olsr_plugin *plugin)
{
  get_interface_version_func get_interface_version;
  get_plugin_parameters_func get_plugin_parameters;
  int plugin_interface_version = -1;

  /* Fetch the interface version function, 3 different ways */
  get_interface_version = dlsym(plugin->dlhandle, "olsrd_plugin_interface_version");
  if (NULL != get_interface_version) {
    plugin_interface_version = get_interface_version();
  }
  OLSR_DEBUG(LOG_PLUGINS, "Checking plugin interface version: %d - OK\n", plugin_interface_version);

  if (plugin_interface_version < 5) {
    /* old plugin interface */
    OLSR_ERROR(LOG_PLUGINS, "YOU ARE USING AN OLD DEPRECATED PLUGIN INTERFACE!\n"
               "DETECTED VERSION %d AND THE CURRENT VERSION IS %d\n"
               "PLEASE UPGRADE YOUR PLUGIN!\n", plugin_interface_version, MOST_RECENT_PLUGIN_INTERFACE_VERSION);
    return -1;
  }

  /* Fetch the init function */
  plugin->plugin_init = dlsym(plugin->dlhandle, "olsrd_plugin_init");
  if (plugin->plugin_init == NULL) {
    OLSR_ERROR(LOG_PLUGINS, "Trying to fetch plugin init function: FAILED: \"%s\"\n", dlerror());
    return -1;
  }


  get_plugin_parameters = dlsym(plugin->dlhandle, "olsrd_get_plugin_parameters");
  if (get_plugin_parameters != NULL) {
    (*get_plugin_parameters) (&plugin->plugin_parameters, &plugin->plugin_parameters_size);
  } else {
    OLSR_ERROR(LOG_PLUGINS, "Trying to fetch parameter table and it's size: FAILED\n");
    return -1;
  }
  return 0;
}


/**
 *Initialize a loaded plugin
 *This includes sending information
 *from olsrd to the plugin and
 *register the functions from the plugin with olsrd
 *
 *@param entry the plugin to initialize
 *
 *@return -1 if there was an error
 */
static int
init_olsr_plugin(struct olsr_plugin *entry)
{
  int rv = 0;
  struct plugin_param *params;
  OLSR_INFO(LOG_PLUGINS, "Setting parameters of plugin...\n");
  for (params = entry->params; params != NULL; params = params->next) {
    OLSR_INFO(LOG_PLUGINS, "\"%s\"/\"%s\"... ", params->key, params->value);
    if (entry->plugin_parameters_size != 0) {
      unsigned int i;
      int rc = 0;
      for (i = 0; i < entry->plugin_parameters_size; i++) {
        if (0 == entry->plugin_parameters[i].name[0] || 0 == strcasecmp(entry->plugin_parameters[i].name, params->key)) {
          /* we have found it! */
          rc = entry->plugin_parameters[i].set_plugin_parameter(params->value, entry->plugin_parameters[i].data,
                                                                0 ==
                                                                entry->plugin_parameters[i].
                                                                name[0] ? (set_plugin_parameter_addon) params->key : entry->
                                                                plugin_parameters[i].addon);
          if (rc != 0) {
            OLSR_ERROR(LOG_PLUGINS, "\nFatal error in plugin parameter \"%s\"/\"%s\"\n", params->key, params->value);
            rv = -1;
          }
          break;
        }
      }
      if (i >= entry->plugin_parameters_size) {
        OLSR_INFO(LOG_PLUGINS, "Ignored parameter \"%s\"\n", params->key);
      } else if (rc == 0) {
        OLSR_INFO(LOG_PLUGINS, "%s: OK\n", params->key);
      } else {
        OLSR_ERROR(LOG_PLUGINS, "%s: FAILED\n", params->key);
        rv = -1;
      }
    } else {
      OLSR_ERROR(LOG_PLUGINS, "I don't know what to do with \"%s\"!\n", params->key);
      rv = -1;
    }
  }

  OLSR_INFO(LOG_PLUGINS, "Running plugin_init function...\n");
  entry->plugin_init();
  return rv;
}


/**
 *Close all loaded plugins
 */
void
olsr_close_plugins(void)
{
  struct olsr_plugin *entry;

  OLSR_INFO(LOG_PLUGINS, "Closing plugins...\n");
  while (olsr_plugins) {
    entry = olsr_plugins;
    olsr_plugins = entry->next;
    dlclose(entry->dlhandle);
    free(entry);
  }
}

/*
 * Local Variables:
 * mode: c
 * style: linux
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */

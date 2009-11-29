
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
#include "common/avl.h"
#include "common/list.h"
#include "olsr_cookie.h"

#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>

/* Local functions */
static struct olsr_plugin *olsr_load_legacy_plugin(const char *, void *);

struct avl_tree plugin_tree;
static bool plugin_tree_initialized = false;

static struct olsr_cookie_info *plugin_mem_cookie = NULL;

static bool olsr_internal_unload_plugin(struct olsr_plugin *plugin, bool cleanup);

/**
 * This function is called by the constructor of a plugin.
 * because of this the first call has to initialize the list
 * head.
 *
 * @param pl_def pointer to plugin definition
 */
void
olsr_hookup_plugin(struct olsr_plugin *pl_def) {
  assert (pl_def->name);
  fprintf(stdout, "hookup %s\n", pl_def->name);
  if (!plugin_tree_initialized) {
    avl_init(&plugin_tree, avl_comp_strcasecmp);
    plugin_tree_initialized = true;
  }
  pl_def->p_node.key = strdup(pl_def->name);
  avl_insert(&plugin_tree, &pl_def->p_node, false);
}

struct olsr_plugin *olsr_get_plugin(const char *libname) {
  struct avl_node *node;
  /* SOT: Hacked away the funny plugin check which fails if pathname is included */
  if (strrchr(libname, '/')) libname = strrchr(libname, '/') + 1;
  if ((node = avl_find(&plugin_tree, libname)) != NULL) {
    return plugin_node2tree(node);
  }
  return NULL;
}

void
olsr_init_pluginsystem(void) {
  plugin_mem_cookie = olsr_alloc_cookie("Plugin handle", OLSR_COOKIE_TYPE_MEMORY);
  olsr_cookie_set_memory_size(plugin_mem_cookie, sizeof(struct olsr_plugin));

  /* could already be initialized */
  if (!plugin_tree_initialized) {
    avl_init(&plugin_tree, avl_comp_strcasecmp);
    plugin_tree_initialized = true;
  }
}

void olsr_plugins_init(bool fail_fast) {
  struct plugin_entry *entry;
  struct olsr_plugin *plugin;

  OLSR_INFO(LOG_PLUGINS, "Loading configured plugins...\n");
  /* first load anything requested but not already loaded */
  for (entry = olsr_cnf->plugins; entry != NULL; entry = entry->next) {
    if (olsr_init_plugin(entry->name) == NULL) {
      if (fail_fast) {
        OLSR_ERROR(LOG_PLUGINS, "Cannot load plugin %s.\n", entry->name);
        olsr_exit(1);
      }
      OLSR_WARN(LOG_PLUGINS, "Cannot load plugin %s.\n", entry->name);
    }
  }

  /* now hookup parameters to plugins */
  for (entry = olsr_cnf->plugins; entry != NULL; entry = entry->next) {
    plugin = olsr_get_plugin(entry->name);
    if (plugin == NULL) {
      if (fail_fast) {
        OLSR_ERROR(LOG_PLUGINS, "Internal error in plugin storage tree, cannot find plugin %s\n", entry->name);
        olsr_exit(1);
      }
      OLSR_WARN(LOG_PLUGINS, "Internal error in plugin storage tree, cannot find plugin %s\n", entry->name);
    }

    plugin->internal_params = entry->params;
  }
}

void olsr_plugins_enable(enum plugin_type type, bool fail_fast) {
  struct olsr_plugin *plugin;

  /* activate all plugins (configured and static linked ones) */
  OLSR_FOR_ALL_PLUGIN_ENTRIES(plugin) {
    if ((type != PLUGIN_TYPE_ALL && type != plugin->type) || plugin->internal_active) {
      continue;
    }

    if (olsr_enable_plugin(plugin)) {
      if (fail_fast) {
        OLSR_ERROR(LOG_PLUGINS, "Error, cannot activate plugin %s.\n", plugin->name);
        olsr_exit(1);
      }
      OLSR_WARN(LOG_PLUGINS, "Error, cannot activate plugin %s.\n", plugin->name);
    }
  } OLSR_FOR_ALL_PLUGIN_ENTRIES_END(plugin)
  OLSR_INFO(LOG_PLUGINS, "All plugins loaded.\n");
}

void
olsr_destroy_pluginsystem(void) {
  struct olsr_plugin *plugin;

  OLSR_FOR_ALL_PLUGIN_ENTRIES(plugin) {
    olsr_disable_plugin(plugin);
    olsr_internal_unload_plugin(plugin, true);
  } OLSR_FOR_ALL_PLUGIN_ENTRIES_END(plugin)
}

static struct olsr_plugin *
olsr_load_legacy_plugin(const char *libname, void *dlhandle) {
  get_interface_version_func get_interface_version;
  get_plugin_parameters_func get_plugin_parameters;
  plugin_init_func init_plugin;

  int plugin_interface_version = -1;
  struct olsr_plugin *plugin = NULL;

  /* Fetch the interface version function, 3 different ways */
  get_interface_version = dlsym(dlhandle, "olsrd_plugin_interface_version");
  if (get_interface_version == NULL) {
    OLSR_WARN(LOG_PLUGINS, "Warning, cannot determine plugin version of '%s'\n", libname);
    return NULL;
  }

  plugin_interface_version = get_interface_version();

  if (plugin_interface_version != 5) {
    /* old plugin interface */
    OLSR_ERROR(LOG_PLUGINS, "Failed to load plugin, version %d is too old\n",
               plugin_interface_version);
    return NULL;
  }

  /* Fetch the init function */
  init_plugin = dlsym(dlhandle, "olsrd_plugin_init");
  if (init_plugin == NULL) {
    OLSR_WARN(LOG_PLUGINS, "Failed to fetch plugin init function: %s\n", dlerror());
    return NULL;
  }

  get_plugin_parameters = dlsym(dlhandle, "olsrd_get_plugin_parameters");
  if (get_plugin_parameters == NULL) {
    OLSR_WARN(LOG_PLUGINS, "Failed to fetch plugin parameters: %s\n", dlerror());
    return NULL;
  }

  OLSR_DEBUG(LOG_PLUGINS, "Got plugin %s, version: %d - OK\n", libname, plugin_interface_version);

  /* initialize plugin structure */
  plugin = (struct olsr_plugin *)olsr_cookie_malloc(plugin_mem_cookie);
  /* SOT: Hacked away the funny plugin check which fails if pathname is included */
  if (strrchr(libname, '/')) libname = strrchr(libname, '/') + 1;
  plugin->name = libname;
  plugin->internal_version = plugin_interface_version;
  plugin->internal_legacy_init = init_plugin;

  plugin->p_node.key = strdup(plugin->name);
  plugin->internal_dlhandle = dlhandle;

  /* get parameters */
  get_plugin_parameters(&plugin->internal_param, &plugin->internal_param_cnt);

  avl_insert(&plugin_tree, &plugin->p_node, false);
  return plugin;
}

/**
 *Try to load a shared library
 *
 *@param libname the name of the library(file)
 *
 *@return dlhandle
 */
struct olsr_plugin *
olsr_init_plugin(const char *libname)
{
  void *dlhandle;
  struct olsr_plugin *plugin;

  /* see if the plugin is there */
  if ((plugin = olsr_get_plugin(libname)) != NULL) {
    return plugin;
  }

  /* attempt to load the plugin */
  if (olsr_cnf->dlPath) {
    char *path = olsr_malloc(strlen(olsr_cnf->dlPath) + strlen(libname) + 1, "Memory for absolute library path");
    strcpy(path, olsr_cnf->dlPath);
    strcat(path, libname);
    OLSR_INFO(LOG_PLUGINS, "Loading plugin %s from %s\n", libname, path);
    dlhandle = dlopen(path, RTLD_NOW);
    free(path);
  } else {
    OLSR_INFO(LOG_PLUGINS, "Loading plugin %s\n", libname);
    dlhandle = dlopen(libname, RTLD_NOW);
  }
  if (dlhandle == NULL) {
    OLSR_ERROR(LOG_PLUGINS, "DL loading failed: \"%s\"!\n", dlerror());
    return NULL;
  }

  /* version 6 plugins should be in the tree now*/
  if ((plugin = olsr_get_plugin(libname)) != NULL) {
    plugin->internal_dlhandle = dlhandle;
    return plugin;
  }

  /* try to load a legacy plugin */
  return olsr_load_legacy_plugin(libname, dlhandle);
}

static bool
olsr_internal_unload_plugin(struct olsr_plugin *plugin, bool cleanup) {
  bool legacy = false;

  if (plugin->internal_active) {
    olsr_disable_plugin(plugin);
  }

  if (plugin->exit != NULL) {
    plugin->exit();
  }

  if (plugin->internal_dlhandle == NULL && !cleanup) {
    /* this is a static plugin, it cannot be unloaded */
    return true;
  }

  OLSR_INFO(LOG_PLUGINS, "Unloading plugin %s\n", plugin->name);

  /* remove first from tree */
  avl_delete(&plugin_tree, &plugin->p_node);
  free(plugin->p_node.key);

  legacy = plugin->internal_version == 5;

  /* cleanup */
  if (plugin->internal_dlhandle) {
    dlclose(plugin->internal_dlhandle);
  }

  /*
   * legacy must be cached because it plugin memory will be gone after dlclose() for
   * modern plugins
   */
  if (legacy) {
    olsr_cookie_free(plugin_mem_cookie, plugin);
  }
  return false;
}

bool
olsr_exit_plugin(struct olsr_plugin *plugin) {
  return olsr_internal_unload_plugin(plugin, false);
}

bool olsr_enable_plugin(struct olsr_plugin *plugin) {
  struct plugin_param *params;
  unsigned int i;

  if (plugin->internal_active) {
    OLSR_DEBUG(LOG_PLUGINS, "Plugin %s is already active.\n", plugin->name);
    return false;
  }

  if (plugin->init != NULL) {
    if (plugin->init()) {
      OLSR_WARN(LOG_PLUGINS, "Error, pre init failed for plugin %s\n", plugin->name);
      return true;
    }
    OLSR_DEBUG(LOG_PLUGINS, "Pre initialization of plugin %s successful\n", plugin->name);
  }

  /* initialize parameters */
  OLSR_INFO(LOG_PLUGINS, "Activating plugin %s\n", plugin->name);
  for (params = plugin->internal_params; params != NULL; params = params->next) {
    OLSR_INFO_NH(LOG_PLUGINS, "    \"%s\" = \"%s\"... ", params->key, params->value);

    for (i = 0; i < plugin->internal_param_cnt; i++) {
      if (0 == plugin->internal_param[i].name[0] || 0 == strcasecmp(plugin->internal_param[i].name, params->key)) {
        /* we have found it! */
        if (plugin->internal_param[i].set_plugin_parameter(params->value, plugin->internal_param[i].data,
            0 == plugin->internal_param[i].name[0] ? (set_plugin_parameter_addon) params->key : plugin->internal_param[i].addon)) {
          OLSR_DEBUG(LOG_PLUGINS, "Bad plugin parameter \"%s\" = \"%s\"... ", params->key, params->value);
          return true;
        }
        break;
      }
    }

    if (i == plugin->internal_param_cnt) {
      OLSR_INFO_NH(LOG_PLUGINS, "    Ignored parameter \"%s\"\n", params->key);
    }
  }

  if (plugin->enable != NULL) {
    if (plugin->enable()) {
      OLSR_WARN(LOG_PLUGINS, "Error, post init failed for plugin %s\n", plugin->name);
      return true;
    }
    OLSR_DEBUG(LOG_PLUGINS, "Post initialization of plugin %s successful\n", plugin->name);
  }
  if (plugin->internal_legacy_init != NULL) {
    if (plugin->internal_legacy_init() != 1) {
      OLSR_WARN(LOG_PLUGINS, "Error, legacy init failed for plugin %s\n", plugin->name);
      return true;
    }
    OLSR_DEBUG(LOG_PLUGINS, "Post initialization of plugin %s successful\n", plugin->name);
  }
  plugin->internal_active = true;

  if (plugin->author != NULL && plugin->descr != NULL) {
    OLSR_INFO(LOG_PLUGINS, "Plugin '%s' (%s) by %s activated sucessfully\n",
        plugin->descr, plugin->name, plugin->author);
  }
  else {
    OLSR_INFO(LOG_PLUGINS, "%sPlugin '%s' activated sucessfully\n",
        plugin->internal_version != 6 ? "Legacy " : "", plugin->name);
  }

  return false;
}

bool olsr_disable_plugin(struct olsr_plugin *plugin) {
  if (!plugin->internal_active) {
    OLSR_DEBUG(LOG_PLUGINS, "Plugin %s is not active.\n", plugin->name);
    return false;
  }

  OLSR_INFO(LOG_PLUGINS, "Deactivating plugin %s\n", plugin->name);
  if (plugin->disable != NULL) {
    if (plugin->disable()) {
      OLSR_DEBUG(LOG_PLUGINS, "Plugin %s cannot be deactivated, error in pre cleanup\n", plugin->name);
      return true;
    }
    OLSR_DEBUG(LOG_PLUGINS, "Pre cleanup of plugin %s successful\n", plugin->name);
  }

  plugin->internal_active = false;
  return false;
}

/*
 * Local Variables:
 * mode: c
 * style: linux
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */

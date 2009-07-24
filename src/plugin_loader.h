
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

#ifndef _OLSR_PLUGIN_LOADER
#define _OLSR_PLUGIN_LOADER

#include "plugin.h"
#include "olsr_types.h"

#include "common/avl.h"
#include "common/list.h"

#define DEFINE_PLUGIN6(descr, author, pre_init, post_init, pre_cleanup, post_cleanup, deactivate, parameter) \
static struct olsr_plugin olsr_internal_plugin_definition = { \
  .p_name = (char*) PLUGIN_FULLNAME , .p_descr = (char*)descr, .p_author = (char*)author, \
  .p_pre_init = pre_init, .p_post_init = post_init, .p_pre_cleanup = pre_cleanup, .p_post_cleanup = post_cleanup, \
  .p_legacy_init = NULL, .p_deactivate = deactivate, .p_version = 6, .p_param = parameter, .p_param_cnt = ARRAYSIZE(parameter) \
}; \
static void hookup_plugin_definition (void) __attribute__ ((constructor)); \
static void hookup_plugin_definition (void) { \
  olsr_hookup_plugin(&olsr_internal_plugin_definition); \
}

#define DEFINE_PLUGIN6_NP(descr, author, pre_init, post_init, pre_cleanup, post_cleanup, deactivate) \
static struct olsr_plugin olsr_internal_plugin_definition = { \
  .p_name = (char*) PLUGIN_FULLNAME , .p_descr = (char*)descr, .p_author = (char*)author, \
  .p_pre_init = pre_init, .p_post_init = post_init, .p_pre_cleanup = pre_cleanup, .p_post_cleanup = post_cleanup, \
  .p_deactivate = deactivate, .p_version = 6, .p_param = NULL, .p_param_cnt = 0 \
}; \
static void hookup_plugin_definition (void) __attribute__ ((constructor)); \
static void hookup_plugin_definition (void) { \
  olsr_hookup_plugin(&olsr_internal_plugin_definition); \
}

/* version 5 */
typedef int (*plugin_init_func) (void);
typedef int (*get_interface_version_func) (void);
typedef void (*get_plugin_parameters_func) (const struct olsrd_plugin_parameters ** params, unsigned int *size);

struct olsr_plugin {
  struct avl_node p_node;

  /* plugin information */
  char *p_name;
  char *p_descr;
  char *p_author;
  bool p_deactivate;    /* plugin can be deactivated */

  /* function pointers */
  bool (*p_pre_init) (void);
  bool (*p_post_init) (void);
  bool (*p_pre_cleanup) (void);
  bool (*p_post_cleanup) (void);
  int  (*p_legacy_init) (void);

  /* plugin interface version */
  int p_version;

  /* plugin list of possible arguments */
  const struct olsrd_plugin_parameters *p_param;

  /* number of arguments */
  unsigned int p_param_cnt;

  /* internal olsr data */
  void *dlhandle;
  struct plugin_param *params;
  bool active;
};

AVLNODE2STRUCT(plugin_node2tree, struct olsr_plugin, p_node)

#define OLSR_FOR_ALL_PLUGIN_ENTRIES(plugin) \
{ \
  struct avl_node *plugin_node, *next_plugin_node; \
  for (plugin_node = avl_walk_first(&plugin_tree); \
    plugin_node; plugin_node = next_plugin_node) { \
    next_plugin_node = avl_walk_next(plugin_node); \
    plugin = plugin_node2tree(plugin_node);
#define OLSR_FOR_ALL_PLUGIN_ENTRIES_END(plugin) }}

struct olsr_plugin *EXPORT(olsr_get_plugin)(char *libname);

void EXPORT(olsr_hookup_plugin) (struct olsr_plugin *plugin);
void EXPORT(olsr_unhookup_plugin) (struct olsr_plugin *plugin);

void EXPORT(olsr_init_pluginsystem)(bool);
void EXPORT(olsr_destroy_pluginsystem)(void);

struct olsr_plugin *EXPORT(olsr_load_plugin)(char *);
bool EXPORT(olsr_unload_plugin)(struct olsr_plugin *);

bool EXPORT(olsr_activate_plugin)(struct olsr_plugin *);
bool EXPORT(olsr_deactivate_plugin)(struct olsr_plugin *);

extern struct avl_tree EXPORT(plugin_tree);

#endif

/*
 * Local Variables:
 * mode: c
 * style: linux
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */

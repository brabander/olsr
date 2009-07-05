
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
#include <string.h>

#include "olsr_cfg.h"
#include "olsr_logging.h"
#include "olsr_cookie.h"
#include "olsr_ip_acl.h"
#include "olsr.h"
#include "scheduler.h"
#include "olsr_comport.h"
#include "olsr_comport_txt.h"
#include "plugin_loader.h"

struct txt_repeat_data {
  struct timer_entry *timer;
  struct autobuf *buf;
  char *cmd;
  char *param;
  bool csv;
};

static struct avl_tree txt_normal_tree, txt_csv_tree, txt_help_tree;
static struct olsr_cookie_info *txtcommand_cookie, *txt_repeattimer_cookie;

static enum olsr_txtcommand_result olsr_txtcmd_quit(
    struct comport_connection *con, char *cmd, char *param);
static enum olsr_txtcommand_result olsr_txtcmd_help(
    struct comport_connection *con, char *cmd, char *param);
static enum olsr_txtcommand_result olsr_txtcmd_csv(
    struct comport_connection *con, char *cmd, char *param);
static enum olsr_txtcommand_result olsr_txtcmd_csvoff(
    struct comport_connection *con, char *cmd, char *param);
static enum olsr_txtcommand_result olsr_txtcmd_repeat(
    struct comport_connection *con, char *cmd, char *param);
static enum olsr_txtcommand_result olsr_txtcmd_timeout(
    struct comport_connection *con, char *cmd, char *param);
static enum olsr_txtcommand_result olsr_txtcmd_version(
    struct comport_connection *con, char *cmd, char *param);
static enum olsr_txtcommand_result olsr_txtcmd_plugin(
    struct comport_connection *con, char *cmd, char *param);
static enum olsr_txtcommand_result olsr_txtcmd_displayhelp(
    struct comport_connection *con, char *cmd, char *param);


static const char *txt_internal_names[] = {
  "quit",
  "exit",
  "help",
  "csv",
  "csvoff",
  "repeat",
  "timeout",
  "version",
  "plugin"
};

static const char *txt_internal_help[] = {
  "shuts down the terminal connection\n",
  "shuts down the terminal connection\n",
  "display the online help text\n",
  "activates the csv (comma separated value) flag\n",
  "deactivates the csv (comma separated value) flag\n",
  "repeat <interval> <command>: repeats a command every <interval> seconds\n",
  "timeout <interval>: set the timeout interval to <interval> seconds, 0 means no timeout\n"
  "displays the version of the olsrd\n"
  "control olsr plugins dynamically, parameters are 'list', 'activate <plugin>', 'deactivate <plugin>', "
    "'load <plugin>' and 'unload <plugin>'"
};

static olsr_txthandler txt_internal_handlers[] = {
  olsr_txtcmd_quit,
  olsr_txtcmd_quit,
  olsr_txtcmd_help,
  olsr_txtcmd_csv,
  olsr_txtcmd_csvoff,
  olsr_txtcmd_repeat,
  olsr_txtcmd_timeout,
  olsr_txtcmd_version,
  olsr_txtcmd_plugin
};

void
olsr_com_init_txt(void) {
  size_t i;

  avl_init(&txt_normal_tree, &avl_comp_strcasecmp);
  avl_init(&txt_csv_tree, &avl_comp_strcasecmp);
  avl_init(&txt_help_tree, &avl_comp_strcasecmp);

  txtcommand_cookie = olsr_alloc_cookie("comport txt commands", OLSR_COOKIE_TYPE_MEMORY);
  olsr_cookie_set_memory_size(txtcommand_cookie, sizeof(struct olsr_txtcommand));

  txt_repeattimer_cookie = olsr_alloc_cookie("txt repeat timer", OLSR_COOKIE_TYPE_TIMER);

  for (i=0; i < ARRAYSIZE(txt_internal_names); i++) {
    olsr_com_add_normal_txtcommand(txt_internal_names[i], txt_internal_handlers[i]);
    olsr_com_add_csv_txtcommand(txt_internal_names[i], txt_internal_handlers[i]);
    olsr_com_add_help_txtcommand(txt_internal_names[i], olsr_txtcmd_displayhelp);
  }
}

void
olsr_com_destroy_txt(void) {
  struct olsr_txtcommand *cmd;
  struct avl_node *node;

  while ((node = avl_walk_first(&txt_normal_tree)) != NULL) {
    cmd = txt_tree2cmd(node);
    olsr_com_remove_normal_txtcommand(cmd);
  }
  while ((node = avl_walk_first(&txt_csv_tree)) != NULL) {
    cmd = txt_tree2cmd(node);
    olsr_com_remove_csv_txtcommand(cmd);
  }
  while ((node = avl_walk_first(&txt_help_tree)) != NULL) {
    cmd = txt_tree2cmd(node);
    olsr_com_remove_help_txtcommand(cmd);
  }
}
struct olsr_txtcommand *
olsr_com_add_normal_txtcommand (const char *command, olsr_txthandler handler) {
  struct olsr_txtcommand *txt;

  txt = olsr_cookie_malloc(txtcommand_cookie);
  txt->node.key = strdup(command);
  txt->handler = handler;

  avl_insert(&txt_normal_tree, &txt->node, AVL_DUP_NO);
  return txt;
}

struct olsr_txtcommand *
olsr_com_add_csv_txtcommand (const char *command, olsr_txthandler handler) {
  struct olsr_txtcommand *txt;

  txt = olsr_cookie_malloc(txtcommand_cookie);
  txt->node.key = strdup(command);
  txt->handler = handler;

  avl_insert(&txt_csv_tree, &txt->node, AVL_DUP_NO);
  return txt;
}

struct olsr_txtcommand *
olsr_com_add_help_txtcommand (const char *command, olsr_txthandler handler) {
  struct olsr_txtcommand *txt;

  txt = olsr_cookie_malloc(txtcommand_cookie);
  txt->node.key = strdup(command);
  txt->handler = handler;

  avl_insert(&txt_help_tree, &txt->node, AVL_DUP_NO);
  return txt;
}

void olsr_com_remove_normal_txtcommand (struct olsr_txtcommand *cmd) {
  avl_delete(&txt_normal_tree, &cmd->node);
  free(cmd->node.key);
  olsr_cookie_free(txtcommand_cookie, cmd);
}

void olsr_com_remove_csv_txtcommand (struct olsr_txtcommand *cmd) {
  avl_delete(&txt_csv_tree, &cmd->node);
  free(cmd->node.key);
  olsr_cookie_free(txtcommand_cookie, cmd);
}

void olsr_com_remove_help_txtcommand (struct olsr_txtcommand *cmd) {
  avl_delete(&txt_help_tree, &cmd->node);
  free(cmd->node.key);
  olsr_cookie_free(txtcommand_cookie, cmd);
}

enum olsr_txtcommand_result
olsr_com_handle_txtcommand(struct comport_connection *con, char *cmd, char *param) {
  struct olsr_txtcommand *ptr;

  ptr = (struct olsr_txtcommand *) avl_find(con->is_csv ? (&txt_csv_tree) : (&txt_normal_tree), cmd);

  OLSR_DEBUG(LOG_COMPORT, "Looking for command '%s' (%s): %s\n",
    cmd, con->is_csv ? "csv" : "normal", ptr ? "unknown" : "available");
  if (ptr == NULL) {
    return UNKNOWN;
  }

  if (ptr->acl) {
    if (!ip_acl_acceptable(ptr->acl, &con->addr, olsr_cnf->ip_version)) {
      return UNKNOWN;
    }
  }

  return ptr->handler(con, cmd, param);
}

static enum olsr_txtcommand_result
olsr_txtcmd_quit(struct comport_connection *con __attribute__ ((unused)),
    char *cmd __attribute__ ((unused)), char *param __attribute__ ((unused))) {
  return QUIT;
}

static enum olsr_txtcommand_result
olsr_txtcmd_displayhelp(struct comport_connection *con,
    char *cmd __attribute__ ((unused)), char *param __attribute__ ((unused))) {
  size_t i;

  for (i=0; i<ARRAYSIZE(txt_internal_names); i++) {
    if (strcasecmp(txt_internal_names[i], cmd) == 0) {
      abuf_puts(&con->out, txt_internal_help[i]);
      return CONTINUE;
    }
  }
  return UNKNOWN;
}

static enum olsr_txtcommand_result
olsr_txtcmd_help(struct comport_connection *con,
    char *cmd __attribute__ ((unused)), char *param) {
  struct olsr_txtcommand *ptr;

  if (param != NULL) {
    ptr = (struct olsr_txtcommand *)avl_find(&txt_help_tree, cmd);
    if (ptr != NULL) {
      return ptr->handler(con, param, NULL);
    }
    return UNKNOWN;
  }

  if (!con->is_csv) {
    abuf_puts(&con->out, "Known commands:\n");
  }

  ptr = (struct olsr_txtcommand *)avl_walk_first(con->is_csv ? (&txt_csv_tree) : (&txt_normal_tree));
  while (ptr) {
    abuf_appendf(&con->out, con->is_csv ? ",%s" : "  %s\n", (char *)ptr->node.key);
    ptr = (struct olsr_txtcommand *)avl_walk_next(&ptr->node);
  }

  abuf_puts(&con->out, con->is_csv ? "\n" : "Use 'help <command> to see a help text for a certain command\n");
  return CONTINUE;
}

static enum olsr_txtcommand_result
olsr_txtcmd_csv(struct comport_connection *con,
    char *cmd __attribute__ ((unused)), char *param __attribute__ ((unused))) {
  con->is_csv = true;
  return CONTINUE;
}

static enum olsr_txtcommand_result
olsr_txtcmd_csvoff(struct comport_connection *con,
    char *cmd __attribute__ ((unused)), char *param __attribute__ ((unused))) {
  con->is_csv = false;
  return CONTINUE;
}

static enum olsr_txtcommand_result
olsr_txtcmd_timeout(struct comport_connection *con,
    char *cmd __attribute__ ((unused)), char *param) {
  con->timeout_value = (uint32_t)strtoul(param, NULL, 10);
  return CONTINUE;
}

static void olsr_txt_repeat_stophandler(struct comport_connection *con) {
  olsr_stop_timer((struct timer_entry *)con->stop_data[0]);
  free(con->stop_data[1]);

  con->stop_handler = NULL;
  con->stop_data[0] = NULL;
  con->stop_data[1] = NULL;
  con->stop_data[2] = NULL;
}

static void olsr_txt_repeat_timer(void *data) {
  struct comport_connection *con = data;

  if (olsr_com_handle_txtcommand(con, con->stop_data[1], con->stop_data[2]) != CONTINUE) {
    con->stop_handler(con);
  }
  olsr_com_activate_output(con);
}

static enum olsr_txtcommand_result
olsr_txtcmd_repeat(struct comport_connection *con, char *cmd __attribute__ ((unused)), char *param) {
  int interval = 0;
  char *ptr;
  struct timer_entry *timer;

  if (con->stop_handler) {
    abuf_puts(&con->out, "Error, you cannot stack continous output commands\n");
    return CONTINUE;
  }

  if (param == NULL || (ptr = strchr(param, ' ')) == NULL) {
    abuf_puts(&con->out, "Missing parameters for repeat\n");
    return CONTINUE;
  }

  ptr++;

  interval = atoi(param);

  timer = olsr_start_timer(interval * 1000, 0, true, &olsr_txt_repeat_timer, con, txt_repeattimer_cookie);
  con->stop_handler = olsr_txt_repeat_stophandler;
  con->stop_data[0] = timer;
  con->stop_data[1] = strdup(ptr);
  con->stop_data[2] = NULL;

  /* split command/parameter and remember it */
  ptr = strchr(con->stop_data[1], ' ');
  if (ptr != NULL) {
    /* found a parameter */
    *ptr++ = 0;
    con->stop_data[2] = ptr;
  }

  /* start command the first time */
  if (olsr_com_handle_txtcommand(con, con->stop_data[1], con->stop_data[2]) != CONTINUE) {
    con->stop_handler(con);
  }
  return CONTINOUS;
}

static enum olsr_txtcommand_result
olsr_txtcmd_version(struct comport_connection *con, char *cmd __attribute__ ((unused)), char *param __attribute__ ((unused))) {
  abuf_appendf(&con->out,
      con->is_csv ? "version,%s,%s,%s\n" : " *** %s ***\n Build date: %s on %s\n http://www.olsr.org\n\n",
      olsrd_version, build_date, build_host);
  return CONTINUE;
}

static enum olsr_txtcommand_result
olsr_txtcmd_plugin(struct comport_connection *con, char *cmd, char *param) {
  struct olsr_plugin *plugin;
  char *para2 = NULL;
  if (param == NULL || strcasecmp(param, "list") == 0) {
    if (!con->is_csv && abuf_puts(&con->out, "Table:\n") < 0) {
      return ABUF_ERROR;
    }
    OLSR_FOR_ALL_PLUGIN_ENTRIES(plugin) {
      if (abuf_appendf(&con->out, con->is_csv ? "%s,%s,%s": " %-30s\t%s\t%s\n",
          plugin->p_name, plugin->active ? "active" : "", plugin->dlhandle == NULL ? "static" : "") < 0) {
        return ABUF_ERROR;
      }
    } OLSR_FOR_ALL_PLUGIN_ENTRIES_END(plugin)
    return CONTINUE;
  }

  para2 = strchr(param, ' ');
  if (para2 == NULL) {
    if (con->is_csv) {
      abuf_appendf(&con->out, "Error, missing or unknown parameter\n");
    }
    return CONTINUE;
  }
  *para2++ = 0;

  plugin = olsr_get_plugin(para2);
  if (strcasecmp(param, "load") == 0) {
    if (plugin != NULL) {
      abuf_appendf(&con->out, "Plugin %s already loaded\n", para2);
      return CONTINUE;
    }
    plugin = olsr_load_plugin(para2);
    if (plugin != NULL) {
      abuf_appendf(&con->out, "Plugin %s successfully loaded\n", para2);
    }
    else {
      abuf_appendf(&con->out, "Could not load plugin %s\n", para2);
    }
    return CONTINUE;
  }

  if (plugin == NULL) {
    if (con->is_csv) {
      abuf_appendf(&con->out, "Error, could not find plugin '%s'.\n", para2);
    }
    return CONTINUE;
  }
  if (strcasecmp(param, "activate") == 0) {
    if (plugin->active) {
      abuf_appendf(&con->out, "Plugin %s already active\n", para2);
    }
    else if (olsr_activate_plugin(plugin)) {
      abuf_appendf(&con->out, "Could not activate plugin %s\n", para2);
    }
    else {
      abuf_appendf(&con->out, "Plugin %s successfully activated\n", para2);
    }
  }
  else if (strcasecmp(param, "deactivate") == 0) {
    if (!plugin->active) {
      abuf_appendf(&con->out, "Plugin %s is not active\n", para2);
    }
    else if (olsr_deactivate_plugin(plugin)) {
      abuf_appendf(&con->out, "Could not deactivate plugin %s\n", para2);
    }
    else {
      abuf_appendf(&con->out, "Plugin %s successfully deactivated\n", para2);
    }
  }
  else if (strcasecmp(param, "unload") == 0) {
    if (plugin->dlhandle == NULL) {
      abuf_appendf(&con->out, "Plugin %s is static and cannot be unloaded\n", para2);
    }
    else if (olsr_unload_plugin(plugin)) {
      abuf_appendf(&con->out, "Could not unload plugin %s\n", para2);
    }
    else {
      abuf_appendf(&con->out, "Plugin %s successfully unloaded\n", para2);
    }
  }
  else {
    abuf_appendf(&con->out, "Unknown command '%s %s %s'.\n", cmd, param, para2);
  }
  return CONTINUE;
}


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



/* System includes */
#include <assert.h>             /* assert() */
#include <stddef.h>             /* NULL */

/* OLSRD includes */
#include "plugin.h"
#include "plugin_loader.h"
#include "plugin_util.h"
#include "defs.h"               /* uint8_t, olsr_cnf */
#include "scheduler.h"          /* olsr_start_timer() */
#include "olsr_cfg.h"           /* olsr_cnf() */
#include "olsr_cookie.h"        /* olsr_alloc_cookie() */

/* OBAMP includes */
#include "obamp.h"
#include "list.h"

static void __attribute__ ((constructor)) my_init(void);
static void __attribute__ ((destructor)) my_fini(void);

static bool obamp_pre_init(void);
static bool obamp_post_init(void);
static bool obamp_pre_cleanup(void);

static const struct olsrd_plugin_parameters plugin_parameters[] = {
  {.name = "NonOlsrIf",.set_plugin_parameter = &AddObampSniffingIf,.data = NULL},
};

OLSR_PLUGIN6(plugin_parameters)
{
.descr = PLUGIN_DESCR,.author = PLUGIN_AUTHOR,.pre_init = obamp_pre_init,.post_init = obamp_post_init,.pre_cleanup =
    obamp_pre_cleanup,};

/**
 * Constructor of plugin, called before parameters are initialized
 */
static bool
obamp_pre_init(void)
{
  PreInitOBAMP();

  //return 0;
  return false;
}

static bool
obamp_post_init(void)
{
  olsrd_plugin_init();

  //return 0;
  return false;
}



/**
 * Destructor of plugin
 */
static bool
obamp_pre_cleanup(void)
{
  //return 0;
  return false;
}


void olsr_plugin_exit(void);

/* -------------------------------------------------------------------------
 * Function   : olsrd_plugin_interface_version
 * Description: Plugin interface version
 * Input      : none
 * Output     : none
 * Return     : OBAMP plugin interface version number
 * Data Used  : none
 * Notes      : Called by main OLSRD (olsr_load_dl) to check plugin interface
 *              version
 * ------------------------------------------------------------------------- */
int
olsrd_plugin_interface_version(void)
{
  return PLUGIN_INTERFACE_VERSION;

}

/* -------------------------------------------------------------------------
 * Function   : olsrd_plugin_init
 * Description: Plugin initialisation
 * Input      : none
 * Output     : none
 * Return     : fail (0) or success (1)
 * Data Used  : olsr_cnf
 * Notes      : Called by main OLSRD (init_olsr_plugin) to initialize plugin
 * ------------------------------------------------------------------------- */
int
olsrd_plugin_init(void)
{
  return InitOBAMP();
}

/* -------------------------------------------------------------------------
 * Function   : olsr_plugin_exit
 * Description: Plugin cleanup
 * Input      : none
 * Output     : none
 * Return     : none
 * Data Used  : none
 * Notes      : Called by my_fini() at unload of shared object
 * ------------------------------------------------------------------------- */
void
olsr_plugin_exit(void)
{
  CloseOBAMP();
}



/* -------------------------------------------------------------------------
 * Function   : olsrd_get_plugin_parameters
 * Description: Return the parameter table and its size
 * Input      : none
 * Output     : params - the parameter table
 *              size - its size in no. of entries
 * Return     : none
 * Data Used  : plugin_parameters
 * Notes      : Called by main OLSR (init_olsr_plugin) for all plugins
 * ------------------------------------------------------------------------- */
void
olsrd_get_plugin_parameters(const struct olsrd_plugin_parameters **params, int *size)
{
  *params = plugin_parameters;
  *size = ARRAYSIZE(plugin_parameters);
}

/* -------------------------------------------------------------------------
 * Function   : my_init
 * Description: Plugin constructor
 * Input      : none
 * Output     : none
 * Return     : none
 * Data Used  : none
 * Notes      : Called at load of shared object
 * ------------------------------------------------------------------------- */
static void
my_init(void)
{
  /* Print plugin info to stdout */
  printf("%s\n", MOD_DESC);

  return;
}

/* -------------------------------------------------------------------------
 * Function   : my_fini
 * Description: Plugin destructor
 * Input      : none
 * Output     : none
 * Return     : none
 * Data Used  : none
 * Notes      : Called at unload of shared object
 * ------------------------------------------------------------------------- */
static void
my_fini(void)
{
  olsr_plugin_exit();
}



/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

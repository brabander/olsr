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


 /*
 * Example plugin for olsrd.org OLSR daemon
 * Only the bare minimum
 */


#include "olsrd_plugin.h"
#include "olsr.h"
#include "defs.h"
#include "plugin.h"
#include "olsr_logging.h"

#include <stdio.h>
#include <string.h>

#define PLUGIN_INTERFACE_VERSION 5


/****************************************************************************
 *                Functions that the plugin MUST provide                    *
 ****************************************************************************/

/**
 * Plugin interface version
 * Used by main olsrd to check plugin interface version
 */
int olsrd_plugin_interface_version(void)
{
    return PLUGIN_INTERFACE_VERSION;
}


static int set_plugin_test(const char *value __attribute__((unused)), void *data __attribute__((unused)), set_plugin_parameter_addon addon __attribute__((unused)))
{
    OLSR_INFO(LOG_PLUGINS, "\n*** MINI: parameter test: %s\n", value);
    return 0;
}

/**
 * Register parameters from config file
 * Called for all plugin parameters
 */
static const struct olsrd_plugin_parameters plugin_parameters[] = {
    { .name = "test",   .set_plugin_parameter = &set_plugin_test,      .data = NULL },
};

void olsrd_get_plugin_parameters(const struct olsrd_plugin_parameters **params, int *size)
{
    *params = plugin_parameters;
    *size = ARRAYSIZE(plugin_parameters);
}


/**
 * Initialize plugin
 * Called after all parameters are passed
 */
int
olsrd_plugin_init(void)
{
	OLSR_INFO(LOG_PLUGINS, "*** MINI: plugin_init\n");

	return 1;
}


/****************************************************************************
 *       Optional private constructor and destructor functions              *
 ****************************************************************************/

/* attention: make static to avoid name clashes */

static void my_init(void) __attribute__ ((constructor));
static void my_fini(void) __attribute__ ((destructor));


/**
 * Optional Private Constructor
 */
static void my_init(void)
{
    // MINI: constructor
}


/**
 * Optional Private Destructor
 */
static void my_fini(void)
{
    // MINI: destructor
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

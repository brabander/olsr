
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

#include "olsrd_plugin.h"
#include "plugin_util.h"
#include "lq_plugin_rfc.h"
#include "olsr.h"
#include "defs.h"
#include "plugin.h"

#include <stdio.h>
#include <string.h>

#define PLUGIN_NAME    "OLSRD lq_etx_rfc plugin"
#define PLUGIN_VERSION "0.1"
#define PLUGIN_AUTHOR   "Henning Rogge and others"
#define MOD_DESC PLUGIN_NAME " " PLUGIN_VERSION " by " PLUGIN_AUTHOR
#define PLUGIN_INTERFACE_VERSION 5

/****************************************************************************
 *                Functions that the plugin MUST provide                    *
 ****************************************************************************/

/**
 * Plugin interface version
 * Used by main olsrd to check plugin interface version
 */
int
olsrd_plugin_interface_version(void)
{
  return PLUGIN_INTERFACE_VERSION;
}

static int
set_plugin_float(const char *value, void *data, set_plugin_parameter_addon addon __attribute__ ((unused)))
{
  if (data != NULL) {
    sscanf(value, "%f", (float *)data);
    OLSR_PRINTF(1, "%s float %f\n", "Got", *(float *)data);
  } else {
    OLSR_PRINTF(0, "%s float %s\n", "Ignored", value);
  }
  return 0;
}

bool use_hysteresis = DEF_USE_HYST;
float scaling = HYST_SCALING;
float thr_high = HYST_THRESHOLD_HIGH;
float thr_low = HYST_THRESHOLD_LOW;

/**
 * Register parameters from config file
 * Called for all plugin parameters
 */
static const struct olsrd_plugin_parameters plugin_parameters[] = {
  {.name = "UseHysteresis",.set_plugin_parameter = &set_plugin_boolean,.data = &use_hysteresis},
  {.name = "HystScaling",.set_plugin_parameter = &set_plugin_float,.data = &scaling},
  {.name = "HystThrHigh",.set_plugin_parameter = &set_plugin_float,.data = &thr_high},
  {.name = "HystThrLow",.set_plugin_parameter = &set_plugin_float,.data = &thr_low},
};

void
olsrd_get_plugin_parameters(const struct olsrd_plugin_parameters **params, int *size)
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
  active_lq_handler = &lq_rfc_handler;
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
static void
my_init(void)
{
  /* Print plugin info to stdout */
  printf("%s\n", MOD_DESC);
}

/**
 * Optional Private Destructor
 */
static void
my_fini(void)
{
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

/*
 * OLSR ARPROAMING PLUGIN
 * http://www.olsr.org
 *
 * Copyright (c) 2004, Andreas Tonnesen(andreto@olsr.org)
 * Copyright (c) 2010, amadeus (amadeus@chemnitz.freifunk.net)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of olsrd, olsr.org nor the names of its
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
 */

#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include "olsr_types.h"
#include "plugin_util.h"
#include "scheduler.h"
#include "olsrd_plugin.h"
#include "olsrd_arproaming.h"

#define PLUGIN_NAME "Arproaming olsrd plugin"
#define PLUGIN_VERSION "0.1"
#define PLUGIN_AUTHOR "amadeus"
#define MOD_DESC PLUGIN_NAME " " PLUGIN_VERSION " by " PLUGIN_AUTHOR
#define PLUGIN_INTERFACE_VERSION 5

static void my_init(void) __attribute__ ((constructor));
static void my_fini(void) __attribute__ ((destructor));

static const struct olsrd_plugin_parameters plugin_parameters[] = {
	{ .name = "Interface", .set_plugin_parameter = &arproaming_parameter_set, .data = &arproaming_parameter_interface },
	{ .name = "Timeout", .set_plugin_parameter = &arproaming_parameter_set, .data = &arproaming_parameter_timeout }
};

int olsrd_plugin_interface_version(void)
{
	return PLUGIN_INTERFACE_VERSION;
}

static void my_init(void)
{
	printf("%s\n", MOD_DESC);
}

static void my_fini(void)
{
	arproaming_plugin_exit();
}

void olsrd_get_plugin_parameters(const struct olsrd_plugin_parameters **params, int *size)
{
	*params = plugin_parameters;
	*size = sizeof(plugin_parameters) / sizeof(*plugin_parameters);
}

int olsrd_plugin_init(void)
{
	if (arproaming_plugin_init() < 0) {
		printf("*** ARPROAMING: Could not initialize plugin!\n");
		return 0;
	}
	else {
		olsr_start_timer(MSEC_PER_SEC/3, 0, OLSR_TIMER_PERIODIC, &arproaming_schedule_event, NULL, 0);
		return 1;
	}
}

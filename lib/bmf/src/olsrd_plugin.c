/*
 * OLSR Basic Multicast Forwarding (BMF) plugin.
 * Copyright (c) 2005, 2006, Thales Communications, Huizen, The Netherlands.
 * Written by Erik Tromp.
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
 * * Neither the name of Thales, BMF nor the names of its 
 *   contributors may be used to endorse or promote products derived 
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED 
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* $Id: olsrd_plugin.c,v 1.1 2006/05/03 08:59:04 kattemat Exp $ */

/*
 * Dynamic linked library for olsr.org olsrd
 */

/* System includes */
#include <assert.h> /* assert() */
#include <stdio.h>

/* OLSRD includes */
#include "olsrd_plugin.h"

/* BMF includes */
#include "Bmf.h" /* InitBmf(), CloseBmf(), RegisterBmfParameter() */

static void __attribute__ ((constructor)) my_init(void);
static void __attribute__ ((destructor)) my_fini(void);

void olsr_plugin_exit(void);

/* Plugin interface version
 * Used by main olsrd to check plugin interface version */
int olsrd_plugin_interface_version()
{
  return OLSRD_PLUGIN_INTERFACE_VERSION;
}

int olsrd_plugin_init()
{
  return InitBmf();
}

/* destructor - called at unload */
void olsr_plugin_exit()
{
  CloseBmf();
}

/* Register parameters from config file
 * Called for all plugin parameters */
int olsrd_plugin_register_param(char* key, char* value)
{
  assert(key != NULL && value != NULL);

  return RegisterBmfParameter(key, value);
}
 
static void my_init()
{
  /* Print plugin info to stdout */
  printf("%s\n", MOD_DESC);

  return;
}

static void my_fini()
{
  olsr_plugin_exit();
}

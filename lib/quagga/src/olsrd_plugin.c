/*
 * OLSRd Quagga plugin
 *
 * Copyright (C) 2006-2008 Immo 'FaUl' Wehrenberg <immo@chaostreff-dortmund.de>
 * Copyright (C) 2007-2010 Vasilis Tsiligiannis <acinonyxs@yahoo.gr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation or - at your option - under
 * the terms of the GNU General Public Licence version 2 but can be
 * linked to any BSD-Licenced Software with public available sourcecode
 *
 */

/* -------------------------------------------------------------------------
 * File               : olsrd_plugin.c
 * Description        : functions to setup plugin
 * ------------------------------------------------------------------------- */

#include "olsrd_plugin.h"
#include "plugin_util.h"
#include "olsr.h"
#include "scheduler.h"
#include "defs.h"
#include "net_olsr.h"

#include "quagga.h"
#include "plugin.h"
#include "parse.h"

#define PLUGIN_NAME    "OLSRD quagga plugin"
#define PLUGIN_VERSION "0.2.2"
#define PLUGIN_AUTHOR  "Immo 'FaUl' Wehrenberg"
#define MOD_DESC PLUGIN_NAME " " PLUGIN_VERSION " by " PLUGIN_AUTHOR
#define PLUGIN_INTERFACE_VERSION 5

static void __attribute__ ((constructor)) my_init(void);
static void __attribute__ ((destructor)) my_fini(void);

static set_plugin_parameter set_redistribute;
static set_plugin_parameter set_exportroutes;
static set_plugin_parameter set_distance;
static set_plugin_parameter set_localpref;
static set_plugin_parameter set_sockpath;
static set_plugin_parameter set_port;
static set_plugin_parameter set_version;

int
olsrd_plugin_interface_version(void)
{
  return PLUGIN_INTERFACE_VERSION;
}

static const struct olsrd_plugin_parameters plugin_parameters[] = {
  {.name = "Redistribute",.set_plugin_parameter = &set_redistribute,},
  {.name = "ExportRoutes",.set_plugin_parameter = &set_exportroutes,},
  {.name = "Distance",.set_plugin_parameter = &set_distance,},
  {.name = "LocalPref",.set_plugin_parameter = &set_localpref,},
  {.name = "SockPath",.set_plugin_parameter = &set_sockpath,.addon = {PATH_MAX},},
  {.name = "Port",.set_plugin_parameter = &set_port,},
  {.name = "Version",.set_plugin_parameter = &set_version,},
};

void
olsrd_get_plugin_parameters(const struct olsrd_plugin_parameters **params, int *size)
{
  *params = plugin_parameters;
  *size = ARRAYSIZE(plugin_parameters);
}

static int
set_redistribute(const char *value, void *data __attribute__ ((unused)), set_plugin_parameter_addon addon __attribute__ ((unused)))
{
  const char *zebra_route_types[] = { "system", "kernel", "connect",
    "static", "rip", "ripng", "ospf",
    "ospf6", "isis", "bgp", "hsls"
  };
  unsigned int i;

  for (i = 0; i < ARRAYSIZE(zebra_route_types); i++) {
    if (!strcmp(value, zebra_route_types[i]))
      if (zebra_redistribute (i)) return 1;
  }

  return 0;
}

static int
set_exportroutes(const char *value, void *data __attribute__ ((unused)), set_plugin_parameter_addon addon __attribute__ ((unused)))
{
  if (!strcmp(value, "only")) {
    olsr_addroute_function = zebra_add_route;
    olsr_delroute_function = zebra_del_route;
    olsr_addroute6_function = zebra_add_route;
    olsr_delroute6_function = zebra_del_route;
    zebra_export_routes(1);
  } else if (!strcmp(value, "additional")) {
    olsr_addroute_function = zebra_add_route;
    olsr_delroute_function = zebra_del_route;
    olsr_addroute6_function = zebra_add_route;
    olsr_delroute6_function = zebra_del_route;
    zebra_export_routes(1);
  } else
    zebra_export_routes(0);
  return 0;
}

static int
set_distance(const char *value, void *data __attribute__ ((unused)), set_plugin_parameter_addon addon __attribute__ ((unused)))
{
  int distance;

  if (set_plugin_int(value, &distance, addon))
    return 1;
  if (distance < 0 || distance > 255)
    return 1;
  zebra_olsr_distance(distance);
  return 0;
}

static int
set_localpref(const char *value, void *data __attribute__ ((unused)), set_plugin_parameter_addon addon __attribute__ ((unused)))
{
  int b;

  if (set_plugin_boolean(value, &b, addon))
    return 1;
  if (b)
    zebra_olsr_localpref();
  return 0;
}

static int
set_sockpath(const char *value, void *data __attribute__ ((unused)), set_plugin_parameter_addon addon)
{
  char sockpath[PATH_MAX];

  if (set_plugin_string(value, &sockpath, addon))
    return 1;
  zebra_sockpath(sockpath);
  return 0;
}

static int
set_port(const char *value, void *data __attribute__ ((unused)), set_plugin_parameter_addon addon __attribute__ ((unused)))
{
  unsigned int port;

  if (set_plugin_port(value, &port, addon))
    return 1;
  zebra_port(port);

  return 0;
}

static int
set_version(const char *value, void *data __attribute__ ((unused)), set_plugin_parameter_addon addon __attribute__ ((unused)))
{
  int version;

  if (set_plugin_int(value, &version, addon))
    return 1;
  if (version < 0 || version > 1)
    return 1;
  zebra_version(version);

  return 0;
}

int
olsrd_plugin_init(void)
{

  olsr_start_timer(1 * MSEC_PER_SEC, 0, OLSR_TIMER_PERIODIC, &zebra_parse, NULL, 0);

  return 0;
}

static void
my_init(void)
{
  init_zebra();
}

static void
my_fini(void)
{
  zebra_cleanup();
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

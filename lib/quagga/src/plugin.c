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
 * File               : plugin.c
 * Description        : functions to set zebra plugin parameters
 * ------------------------------------------------------------------------- */

#include "defs.h"
#include "olsr.h"
#include "log.h"

#include "common.h"
#include "packet.h"
#include "plugin.h"

static void *my_realloc(void *, size_t, const char *);

static void
*my_realloc(void *buf, size_t s, const char *c)
{
  buf = realloc(buf, s);
  if (!buf) {
    OLSR_PRINTF(1, "(QUAGGA) OUT OF MEMORY: %s\n", strerror(errno));
    olsr_syslog(OLSR_LOG_ERR, "olsrd: out of memory!: %m\n");
    olsr_exit(c, EXIT_FAILURE);
  }
  return buf;
}

int
zplugin_redistribute(unsigned char type)
{

  if (type > ZEBRA_ROUTE_MAX - 1)
    return -1;
  zebra.redistribute[type] = 1;

  return 0;

}

void
zplugin_exportroutes(unsigned char t)
{
  if (t)
    zebra.options |= OPTION_EXPORT;
  else
    zebra.options &= ~OPTION_EXPORT;
}

void
zplugin_distance(unsigned char dist)
{
  zebra.distance = dist;
}

void
zplugin_localpref(void)
{
  zebra.flags &= ZEBRA_FLAG_SELECTED;
}

void
zplugin_sockpath(char *sockpath)
{
  size_t len;

  len = strlen(sockpath) + 1;
  zebra.sockpath = my_realloc(zebra.sockpath, len, "zebra_sockpath");
  memcpy(zebra.sockpath, sockpath, len);

}

void
zplugin_port(unsigned int port)
{

  zebra.port = port;

}

void
zplugin_version(char version)
{

  zebra.version = version;

}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

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
 * File               : common.h
 * Description        : common header file
 * ------------------------------------------------------------------------- */

#define OPTION_EXPORT 1

/* Zebra route types */
#define ZEBRA_ROUTE_OLSR		11
#define ZEBRA_ROUTE_MAX			13

struct zebra {
  char status;                         // internal status
  char options;                        // internal options
  int sock;                            // Socket to zebra...
  char redistribute[ZEBRA_ROUTE_MAX];
  char distance;
  char flags;
  char *sockpath;
  unsigned int port;
  char version;
};

extern struct zebra zebra;

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

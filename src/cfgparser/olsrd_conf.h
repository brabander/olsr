/*
 * OLSR ad-hoc routing table management protocol config parser
 * Copyright (C) 2004 Andreas Tønnesen (andreto@olsr.org)
 *
 * This file is part of the olsr.org OLSR daemon.
 *
 * olsr.org is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * olsr.org is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with olsr.org; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 * 
 * $Id: olsrd_conf.h,v 1.4 2004/10/18 13:13:37 kattemat Exp $
 *
 */


#ifndef _OLSRD_CONF_H
#define _OLSRD_CONF_H

#include "olsr_protocol.h"
#include "../olsrd_cfgparser.h"

#define SOFTWARE_VERSION "0.1.1"


int current_line;

struct olsrd_config *cnf;

struct conf_token
{
  olsr_u32_t integer;
  float      floating;
  olsr_u8_t  boolean;
  char       *string;
};

void
set_default_cnf(struct olsrd_config *);

struct if_config_options *
find_if_rule_by_name(struct if_config_options *, char *);

struct if_config_options *
get_default_if_config(void);

#endif

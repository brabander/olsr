
/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2004 Andreas Tønnesen (andreto@ifi.uio.no)
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
 * $Id: configfile.c,v 1.10 2004/10/19 20:55:41 kattemat Exp $
 *
 */
 

#include "defs.h"
#include "configfile.h"
#include "local_hna_set.h"
#include "olsr.h"
#include "plugin_loader.h"
#include "interfaces.h"
#include <string.h>
#include <stdlib.h>

#include "olsr_cfg.h"

/**
 *Funtion that tries to read and parse the config
 *file "filename"
 *@param filename the name(full path) of the config file
 *@return negative on error
 */
void
get_config(char *filename)
{

  /*
   * NB - CHECK IPv6 MULTICAST!
   */
  if((olsr_cnf = olsrd_parse_cnf(filename)) != NULL)
    {
      olsrd_print_cnf(olsr_cnf);  
    }
  else
    {
      printf("Using default config values(no configfile)\n");
      olsr_cnf = olsrd_get_default_cnf();
    }

  /* Add plugins */

}



struct if_config_options *
get_default_ifcnf(struct olsrd_config *cnf)
{
  struct if_config_options *ifc = cnf->if_options;

  while(ifc)
    {
      if(!strcmp(ifc->name, DEFAULT_IF_CONFIG_NAME))
        return ifc;
      ifc = ifc->next;
    }
  return NULL;
}

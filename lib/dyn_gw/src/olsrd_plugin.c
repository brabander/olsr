/*
 * OLSR plugin
 * Copyright (C) 2004 Andreas Tønnesen (andreto@olsr.org)
 *
 * This file is part of the olsrd dynamic gateway detection.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This plugin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with olsrd-unik; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 * 
 * $Id: olsrd_plugin.c,v 1.8 2004/11/06 12:23:46 kattemat Exp $
 *
 */

/*
 * Dynamic linked library example for UniK OLSRd
 */


#include "olsrd_plugin.h"
#include <stdio.h>


/* Data to sent to the plugin with the register_olsr_function call 
 * THIS STRUCT MUST MATCH ITS SIBLING IN plugin_loader.h IN OLSRD
 */
struct olsr_plugin_data
{
  int ipversion;
  union olsr_ip_addr *main_addr;
  int (*olsr_plugin_io)(int, void *, size_t);
};


/**
 * "Private" declarations
 */

void __attribute__ ((constructor)) 
my_init(void);

void __attribute__ ((destructor)) 
my_fini(void);

int
register_olsr_data(struct olsr_plugin_data *);

int
fetch_olsrd_data();


/*
 * Defines the version of the plugin interface that is used
 * THIS IS NOT THE VERSION OF YOUR PLUGIN!
 * Do not alter unless you know what you are doing!
 */
int 
get_plugin_interface_version()
{
  return PLUGIN_INTERFACE_VERSION;
}


/**
 *Constructor
 */
void
my_init()
{
  /* Print plugin info to stdout */
  printf("%s\n", MOD_DESC);

  return;
}

/**
 *Destructor
 */
void
my_fini()
{

  /* Calls the destruction function
   * olsr_plugin_exit()
   * This function should be present in your
   * sourcefile and all data destruction
   * should happen there - NOT HERE!
   */
  olsr_plugin_exit();

  return;
}


int
register_olsr_param(char *key, char *value)
{
  //if(!strcmp(key, "Ip6Net"
  return 1;
}


/**
 *Register needed functions and pointers
 *
 *This function should not be changed!
 *
 */
int
register_olsr_data(struct olsr_plugin_data *data)
{
  /* IPversion */
  ipversion = data->ipversion;
  /* Main address */
  main_addr = data->main_addr;

  /* Multi-purpose function */
  olsr_plugin_io = data->olsr_plugin_io;

  /* Set size of IP address */
  if(ipversion == AF_INET)
    {
      ipsize = sizeof(olsr_u32_t);
    }
  else
    {
      ipsize = sizeof(struct in6_addr);
    }

  if(!fetch_olsrd_data())
    {
      fprintf(stderr, "Could not fetch the neccessary functions from olsrd!\n");
      return 0;
    }

  /* Calls the initialization function
   * olsr_plugin_init()
   * This function should be present in your
   * sourcefile and all data initialization
   * should happen there - NOT HERE!
   */
  if(!olsr_plugin_init())
    {
      fprintf(stderr, "Could not initialize plugin!\n");
      return 0;
    }

  return 1;

}



int
fetch_olsrd_data()
{
  int retval = 1;


  /* Olsr debug output function */
  if(!olsr_plugin_io(GETF__OLSR_PRINTF, 
		     &olsr_printf, 
		     sizeof(olsr_printf)))
  {
    olsr_printf = NULL;
    retval = 0;
  }

  /* Olsr malloc wrapper */
  if(!olsr_plugin_io(GETF__OLSR_MALLOC, 
		     &olsr_malloc, 
		     sizeof(olsr_malloc)))
  {
    olsr_malloc = NULL;
    retval = 0;
  }

  /* Scheduler event registration */
  if(!olsr_plugin_io(GETF__OLSR_REGISTER_SCHEDULER_EVENT, 
		     &olsr_register_scheduler_event, 
		     sizeof(olsr_register_scheduler_event)))
  {
    olsr_register_scheduler_event = NULL;
    retval = 0;
  }


  /* Add hna net IPv4 */
  if(!olsr_plugin_io(GETF__ADD_LOCAL_HNA4_ENTRY, &add_local_hna4_entry, sizeof(add_local_hna4_entry)))
  {
    add_local_hna4_entry = NULL;
    retval = 0;
  }

  /* Remove hna net IPv4 */
  if(!olsr_plugin_io(GETF__REMOVE_LOCAL_HNA4_ENTRY, &remove_local_hna4_entry, sizeof(remove_local_hna4_entry)))
  {
    remove_local_hna4_entry = NULL;
    retval = 0;
  }

  return retval;

}

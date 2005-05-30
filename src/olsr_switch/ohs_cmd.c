
/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2005, Andreas Tønnesen(andreto@olsr.org)
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
 * $Id: ohs_cmd.c,v 1.1 2005/05/30 19:17:20 kattemat Exp $
 */

#include "olsr_host_switch.h"
#include "commands.h"
#include <string.h>

int
ohs_cmd_help(FILE *handle)
{
  int i;

  printf("Olsrd host switch version %s\n", OHS_VERSION);
  printf("Available commands:\n");

  for(i = 0; ohs_commands[i].cmd; i++)
    {
      if(ohs_commands[i].helptext_brief)
	printf("%s - %s\n", 
	       ohs_commands[i].cmd,
	       ohs_commands[i].helptext_brief);
    }
  return i;
}

int
ohs_cmd_exit(FILE *handle)
{

  printf("Exitting... bye-bye!\n");

#ifdef WIN32
  SignalHandler(0);
#else
  ohs_close(0);
#endif

  return 0;
}

int
ohs_parse_command(FILE *handle)
{
  char input_data[100];
  int i;

  fscanf(handle, "%s", input_data);

  printf("ohs_parse_command: %s\n", input_data);
  for(i = 0; ohs_commands[i].cmd; i++)
    {
      if(!strcmp(input_data, ohs_commands[i].cmd))
	{
	  if(ohs_commands[i].cmd_cb)
	    {
	      ohs_commands[i].cmd_cb(handle);
	    }
	  else
	    {
	      printf("No action registered on cmd %s!\n", input_data);
	    }
	  break;
	}
    }
  
  if(!ohs_commands[i].cmd)
    {
      printf("%s: no such cmd!\n", input_data);
    }

  i = 0;
  /* Drain */
  while(fgetc(handle) != '\n')
    {
      i++;
    }

  return i;
}

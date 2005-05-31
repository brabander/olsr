
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
 * $Id: ohs_cmd.c,v 1.8 2005/05/31 14:04:21 kattemat Exp $
 */

#include "olsr_host_switch.h"
#include "olsr_types.h"
#include "commands.h"
#include "link_rules.h"
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#define ARG_BUF_SIZE 500
static char arg_buf[ARG_BUF_SIZE];

#define TOK_BUF_SIZE 500
static char tok_buf[TOK_BUF_SIZE];

static void
get_arg_buf(FILE *handle, char *buf, size_t size)
{
  char c = 0;
  int pos = 0;

  while(((c = fgetc(handle)) != '\n') &&
	pos < (size - 2))
    {
      buf[pos] = c;
      pos++;
    }

  buf[pos] = 0;

  printf("Args: %s\n", buf);
}

static int
get_next_token(char *src, char *dst, size_t buflen)
{
  int i = 0, j = 0;

  dst[0] = 0;
  /* Skip leading spaces */
  while(src[j] == ' ' && src[j] != 0)
    {
      j++;
    }

  src += j;
  i = 0;
  while((src[i] != ' ') && (src[i] != 0) && (i < (buflen - 1)))
    {
      dst[i] = src[i];
      i++;
    }
  dst[i] = 0;

  printf("Extracted token: %s\n", dst);
  return i + j;
}

int
ohs_cmd_link(FILE *handle, char *args)
{
  olsr_u8_t bi = 0;
  struct ohs_connection *src, *dst;
  struct in_addr iaddr;
  int qual;
  struct ohs_ip_link *link, *inv_link;

  if(strlen(args) < strlen("bi"))
    goto print_usage;

  args += get_next_token(args, tok_buf, TOK_BUF_SIZE);

  if(!strlen(tok_buf))
    goto print_usage;

  if(!strncmp(tok_buf, "bi", strlen("bi")))
    {
      bi = 1;
      args += get_next_token(args, tok_buf, TOK_BUF_SIZE);

      if(!strlen(tok_buf))
	goto print_usage;
    }

  if(!inet_aton(tok_buf, &iaddr))
    {
      printf("Invalid src IP %s\n", tok_buf);
      return -1;
    }

  src = get_client_by_addr((union olsr_ip_addr *)&iaddr.s_addr);

  if(!src)
    {
      printf("No such client: %s!\n", tok_buf);
      return -1;
    }

  args += get_next_token(args, tok_buf, TOK_BUF_SIZE);
  
  if(!strlen(tok_buf))
    goto print_usage;

  if(!inet_aton(tok_buf, &iaddr))
    {
      printf("Invalid src IP %s\n", tok_buf);
      return -1;
    }

  dst = get_client_by_addr((union olsr_ip_addr *)&iaddr.s_addr);

  if(!dst)
    {
      printf("No such client: %s!\n", tok_buf);
      return -1;
    }

  args += get_next_token(args, tok_buf, TOK_BUF_SIZE);
  
  if(!strlen(tok_buf))
    goto print_usage;

  qual = atoi(tok_buf);

  if(qual < 0 || qual > 100)
    {
      printf("Link quality out of range(0-100)\n");
      return -1;
    }
  
  printf("%s %sdirectional link %s %c=> %s quality %d\n", 
         (qual == 100) ? "Removing" : "Setting", bi ? "bi" : "uni",
	 olsr_ip_to_string(&src->ip_addr), bi ? '<' : '=', 
         olsr_ip_to_string(&dst->ip_addr), qual);

  link = get_link(src, &dst->ip_addr);
  if(bi)
    inv_link = get_link(dst, &src->ip_addr);
  else
    inv_link = NULL;

  if(qual == 100)
    {
      /* Remove link entry */
      if(link)
        remove_link(src, link);
      if(inv_link)
        remove_link(dst, inv_link);
    }
  else 
    {
      if(!link)
        {
          /* Create new link */
            link = malloc(sizeof(link));
            if(!link)
              OHS_OUT_OF_MEMORY("New link");
            /* Queue */
            link->next = src->links;
            src->links = link;
            COPY_IP(&link->dst, &dst->ip_addr);
            src->linkcnt++;
        }

      link->quality = qual;

      if(bi)
        {
          if(!inv_link)
            {
              /* Create new link */
              inv_link = malloc(sizeof(link));
              if(!inv_link)
                OHS_OUT_OF_MEMORY("New link");
              /* Queue */
              inv_link->next = dst->links;
              dst->links = inv_link;
              COPY_IP(&inv_link->dst, &src->ip_addr);
              dst->linkcnt++;
            }
          inv_link->quality = qual;
        }
    }

  return 1;
 print_usage:
  printf("link <bi> srcIP dstIP [0-100]");
  return -1;
}

int
ohs_cmd_list(FILE *handle, char *args)
{
  struct ohs_connection *oc = ohs_conns;

  printf("All connected clients:\n");

  while(oc)
    {
      printf("\t%s - Rx: %d Tx: %d LinkCnt: %d\n", olsr_ip_to_string(&oc->ip_addr), 
             oc->rx, oc->tx, oc->linkcnt);
      oc = oc->next;
    }

  return 1;
}

int
ohs_cmd_help(FILE *handle, char *args)
{
  int i;

  printf("Olsrd host switch version %s\n", OHS_VERSION);
  printf("Available commands:\n");

  for(i = 0; ohs_commands[i].cmd; i++)
    {
      if(ohs_commands[i].helptext_brief)
	printf("\t%s - %s\n", 
	       ohs_commands[i].cmd,
	       ohs_commands[i].helptext_brief);
    }
  printf("\nType 'help cmd' for help on a specific command(NIY)\n");
  return i;
}

int
ohs_cmd_log(FILE *handle, char *args)
{
  olsr_u8_t set = 0;

  args += get_next_token(args, tok_buf, TOK_BUF_SIZE);
  
  if(strlen(tok_buf) &&
     ((set = !strncmp(tok_buf, "set", strlen("set"))) || 
      !strncmp(tok_buf, "unset", strlen("unset"))))
    {
        olsr_u32_t new_bit = 0;
        
        args += get_next_token(args, tok_buf, TOK_BUF_SIZE);
  
        if(!strlen(tok_buf))
            goto print_usage;
        
        
        if(!strncmp(tok_buf, "CON", strlen("CON")))
	    new_bit = LOG_CONNECT;
        else if(!strncmp(tok_buf, "FOR", strlen("FOR")))
	    new_bit = LOG_FORWARD;
        else if(!strncmp(tok_buf, "LIN", strlen("LIN")))
	    new_bit = LOG_LINK;
	  
        if(!new_bit)
	    goto print_usage;

        if(set)
	    logbits |= new_bit;
        else
	    logbits &= ~new_bit;

        printf("%s log bit: 0x%08x, new log: 0x%08x\n", set ? "Setting" : "Removing",
               new_bit, logbits);

    }
  else
    {
      if(strlen(tok_buf))
	goto print_usage;

      printf("Log: (0x%08x) ", logbits);
      if(logbits & LOG_CONNECT)
	printf("CONNECT ");
      if(logbits & LOG_FORWARD)
	printf("FORWARD ");
      if(logbits & LOG_LINK)
	printf("LINK ");

      printf("\n");
    }
  return 1;

 print_usage:
  printf("Usage: log <[set|unset] [CONNECT|FORWARD|LINK]>\n");
  return 0;

}

int
ohs_cmd_exit(FILE *handle, char *args)
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

  get_arg_buf(handle, arg_buf, ARG_BUF_SIZE);

  printf("ohs_parse_command: %s\n", input_data);
  for(i = 0; ohs_commands[i].cmd; i++)
    {
      if(!strcmp(input_data, ohs_commands[i].cmd))
	{
	  if(ohs_commands[i].cmd_cb)
	    {
	      ohs_commands[i].cmd_cb(handle, arg_buf);
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


  return 0;
}

%{

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
 * $Id: oparse.y,v 1.3 2004/10/17 11:52:41 kattemat Exp $
 *
 */


#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include "olsrd_conf.h"

#define PARSER_DEBUG 0

#define YYSTYPE struct conf_token *

void yyerror(char *);
int yylex(void);

struct if_config_options *
get_default_if_config(void);


struct if_config_options *
get_default_if_config()
{
  struct if_config_options *io = malloc(sizeof(struct if_config_options));
  struct in6_addr in6;
 
  memset(io, 0, sizeof(struct if_config_options));

  io->ipv6_addrtype = 1;

  if(inet_pton(AF_INET6, OLSR_IPV6_MCAST_SITE_LOCAL, &in6) < 0)
    {
      fprintf(stderr, "Failed converting IP address %s\n", OLSR_IPV6_MCAST_SITE_LOCAL);
      exit(EXIT_FAILURE);
    }
  memcpy(&io->ipv6_multi_site.v6, &in6, sizeof(struct in6_addr));

  if(inet_pton(AF_INET6, OLSR_IPV6_MCAST_GLOBAL, &in6) < 0)
    {
      fprintf(stderr, "Failed converting IP address %s\n", OLSR_IPV6_MCAST_GLOBAL);
      exit(EXIT_FAILURE);
    }
  memcpy(&io->ipv6_multi_glbl.v6, &in6, sizeof(struct in6_addr));


  io->hello_params.emission_interval = HELLO_INTERVAL;
  io->hello_params.validity_time = NEIGHB_HOLD_TIME;
  io->tc_params.emission_interval = TC_INTERVAL;
  io->tc_params.validity_time = TOP_HOLD_TIME;
  io->mid_params.emission_interval = MID_INTERVAL;
  io->mid_params.validity_time = MID_HOLD_TIME;
  io->hna_params.emission_interval = HNA_INTERVAL;
  io->hna_params.validity_time = HNA_HOLD_TIME;

  return io;

}




%}

%token TOK_OPEN
%token TOK_CLOSE
%token TOK_SEMI

%token TOK_STRING
%token TOK_INTEGER
%token TOK_FLOAT
%token TOK_BOOLEAN

%token TOK_IP6TYPE

%token TOK_DEBUGLEVEL
%token TOK_IPVERSION
%token TOK_HNA4
%token TOK_HNA6
%token TOK_PLUGIN
%token TOK_INTERFACES
%token TOK_IFSETUP
%token TOK_NOINT
%token TOK_TOS
%token TOK_WILLINGNESS
%token TOK_IPCCON
%token TOK_USEHYST
%token TOK_HYSTSCALE
%token TOK_HYSTUPPER
%token TOK_HYSTLOWER
%token TOK_POLLRATE
%token TOK_TCREDUNDANCY
%token TOK_MPRCOVERAGE
%token TOK_PLNAME
%token TOK_PLPARAM

%token TOK_IP4BROADCAST
%token TOK_IP6ADDRTYPE
%token TOK_IP6MULTISITE
%token TOK_IP6MULTIGLOBAL
%token TOK_HELLOINT
%token TOK_HELLOVAL
%token TOK_TCINT
%token TOK_TCVAL
%token TOK_MIDINT
%token TOK_MIDVAL
%token TOK_HNAINT
%token TOK_HNAVAL

%token TOK_IP4_ADDR
%token TOK_IP6_ADDR

%token TOK_COMMENT

%%

conf:
          | conf block
          | conf stmt
;

stmt:       idebug
          | iipversion
          | bnoint
          | atos
          | awillingness
          | bipccon
          | busehyst
          | fhystscale
          | fhystupper
          | fhystlower
          | fpollrate
          | atcredundancy
          | amprcoverage
          | vcomment
;

block:      TOK_HNA4 hna4body
          | TOK_HNA6 hna6body
          | TOK_INTERFACES ifbody
          | TOK_PLUGIN plbody
          | isetblock isetbody
;

hna4body:       TOK_OPEN hna4stmts TOK_CLOSE
;

hna4stmts: | hna4stmts ihna4entry
;

hna6body:       TOK_OPEN hna6stmts TOK_CLOSE
;

hna6stmts: | hna6stmts ihna6entry
;

ifbody:     TOK_OPEN ifstmts TOK_CLOSE
;

ifstmts:   | ifstmts ifstmt
;

ifstmt:     ifentry
          | vcomment
;

isetbody:   TOK_OPEN isetstmts TOK_CLOSE
;

isetstmts:   | isetstmts isetstmt
;

isetstmt:      vcomment
             | isetip4br
             | isetip6addrt
             | isetip6mults
             | isetip6multg
             | isethelloint
             | isethelloval
             | isettcint
             | isettcval
             | isetmidint
             | isetmidval
             | isethnaint
             | isethnaval
;

plbody:     TOK_OPEN plstmts TOK_CLOSE
;

plstmts:   | plstmts plstmt
;

plstmt:     plname
          | plparam
          | vcomment
;




isetblock:    TOK_IFSETUP TOK_STRING
{
  struct if_config_options *io = get_default_if_config();
  if(io == NULL)
    {
      fprintf(stderr, "Out of memory(ADD IFRULE)\n");
      YYABORT;
    }

  if(PARSER_DEBUG) printf("Interface setup: \"%s\"\n", $2->string);
  
  io->name = $2->string;
  
  
  /* Queue */
  io->next = cnf->if_options;
  cnf->if_options = io;

  free($2);
}
;


isetip4br: TOK_IP4BROADCAST TOK_IP4_ADDR
{
  struct in_addr in;

  if(PARSER_DEBUG) printf("\tIPv4 broadcast: %s\n", $2->string);

  if(inet_aton($2->string, &in) == 0)
    {
      fprintf(stderr, "Failed converting IP address %s\n", $1->string);
      exit(EXIT_FAILURE);
    }

  cnf->if_options->ipv4_broadcast.v4 = in.s_addr;

  free($2->string);
  free($2);
}
;

isetip6addrt: TOK_IP6ADDRTYPE TOK_IP6TYPE
{
  cnf->if_options->ipv6_addrtype = $2->boolean;
  
  free($2);
}
;

isetip6mults: TOK_IP6MULTISITE TOK_IP6_ADDR
{
  struct in6_addr in6;

  if(PARSER_DEBUG) printf("\tIPv6 site-local multicast: %s\n", $2->string);

  if(inet_pton(AF_INET6, $2->string, &in6) < 0)
    {
      fprintf(stderr, "Failed converting IP address %s\n", $2->string);
      exit(EXIT_FAILURE);
    }
  memcpy(&cnf->if_options->ipv6_multi_site.v6, &in6, sizeof(struct in6_addr));


  free($2->string);
  free($2);
}
;


isetip6multg: TOK_IP6MULTIGLOBAL TOK_IP6_ADDR
{
  struct in6_addr in6;

  if(PARSER_DEBUG) printf("\tIPv6 global multicast: %s\n", $2->string);

  if(inet_pton(AF_INET6, $2->string, &in6) < 0)
    {
      fprintf(stderr, "Failed converting IP address %s\n", $2->string);
      exit(EXIT_FAILURE);
    }
  memcpy(&cnf->if_options->ipv6_multi_glbl.v6, &in6, sizeof(struct in6_addr));


  free($2->string);
  free($2);
}
;
isethelloint: TOK_HELLOINT TOK_FLOAT
{
    if(PARSER_DEBUG) printf("\tHELLO interval: %0.2f\n", $2->floating);
    cnf->if_options->hello_params.emission_interval = $2->floating;
    free($2);
}
;
isethelloval: TOK_HELLOVAL TOK_FLOAT
{
    if(PARSER_DEBUG) printf("\tHELLO validity: %0.2f\n", $2->floating);
    cnf->if_options->hello_params.validity_time = $2->floating;
    free($2);
}
;
isettcint: TOK_TCINT TOK_FLOAT
{
    if(PARSER_DEBUG) printf("\tTC interval: %0.2f\n", $2->floating);
    cnf->if_options->tc_params.emission_interval = $2->floating;
    free($2);
}
;
isettcval: TOK_TCVAL TOK_FLOAT
{
    if(PARSER_DEBUG) printf("\tTC validity: %0.2f\n", $2->floating);
    cnf->if_options->tc_params.validity_time = $2->floating;
    free($2);
}
;
isetmidint: TOK_MIDINT TOK_FLOAT
{
    if(PARSER_DEBUG) printf("\tMID interval: %0.2f\n", $2->floating);
    cnf->if_options->mid_params.emission_interval = $2->floating;
    free($2);
}
;
isetmidval: TOK_MIDVAL TOK_FLOAT
{
    if(PARSER_DEBUG) printf("\tMID validity: %0.2f\n", $2->floating);
    cnf->if_options->mid_params.validity_time = $2->floating;
    free($2);
}
;
isethnaint: TOK_HNAINT TOK_FLOAT
{
    if(PARSER_DEBUG) printf("\tHNA interval: %0.2f\n", $2->floating);
    cnf->if_options->hna_params.emission_interval = $2->floating;
    free($2);
}
;
isethnaval: TOK_HNAVAL TOK_FLOAT
{
    if(PARSER_DEBUG) printf("\tHNA validity: %0.2f\n", $2->floating);
    cnf->if_options->hna_params.validity_time = $2->floating;
    free($2);
}
;


idebug:       TOK_DEBUGLEVEL TOK_INTEGER
{

  if($2->boolean == 1)
    {
      if(PARSER_DEBUG) printf("Debug levl AUTO\n");
    }
  else
    {
      cnf->debug_level = $2->integer;
      if(PARSER_DEBUG) printf("Debug level: %d\n", cnf->debug_level);
    }

  free($2);
}
;


iipversion:    TOK_IPVERSION TOK_INTEGER
{
  if(($2->integer != 4) && ($2->integer != 6))
    {
      fprintf(stderr, "IPversion must be 4 or 6!\n");
      YYABORT;
    }
  cnf->ip_version = $2->integer;
  if(PARSER_DEBUG) printf("IpVersion: %d\n", cnf->ip_version);
  free($2);
}
;


ihna4entry:     TOK_IP4_ADDR TOK_IP4_ADDR
{
  struct hna4_entry *h = malloc(sizeof(struct hna4_entry));
  struct in_addr in;

  if(PARSER_DEBUG) printf("HNA IPv4 entry: %s/%s\n", $1->string, $2->string);

  if(h == NULL)
    {
      fprintf(stderr, "Out of memory(HNA4)\n");
      YYABORT;
    }

  if(inet_aton($1->string, &in) == 0)
    {
      fprintf(stderr, "Failed converting IP address %s\n", $1->string);
      exit(EXIT_FAILURE);
    }
  h->net = in.s_addr;
  if(inet_aton($2->string, &in) == 0)
    {
      fprintf(stderr, "Failed converting IP address %s\n", $1->string);
      exit(EXIT_FAILURE);
    }
  h->netmask = in.s_addr;
  /* Queue */
  h->next = cnf->hna4_entries;
  cnf->hna4_entries = h;

  free($1->string);
  free($1);
  free($2->string);
  free($2);

}

ihna6entry:     TOK_IP6_ADDR TOK_INTEGER
{
  struct hna6_entry *h = malloc(sizeof(struct hna6_entry));
  struct in6_addr in6;

  if(PARSER_DEBUG) printf("HNA IPv6 entry: %s/%d\n", $1->string, $2->integer);

  if(h == NULL)
    {
      fprintf(stderr, "Out of memory(HNA6)\n");
      YYABORT;
    }

  if(inet_pton(AF_INET6, $1->string, &in6) < 0)
    {
      fprintf(stderr, "Failed converting IP address %s\n", $1->string);
      exit(EXIT_FAILURE);
    }
  memcpy(&h->net, &in6, sizeof(struct in6_addr));

  if(($2->integer < 0) || ($2->integer > 128))
    {
      fprintf(stderr, "Illegal IPv6 prefix length %d\n", $2->integer);
      exit(EXIT_FAILURE);
    }

  h->prefix_len = $2->integer;
  /* Queue */
  h->next = cnf->hna6_entries;
  cnf->hna6_entries = h;

  free($1->string);
  free($1);
  free($2);

}

ifentry: TOK_STRING TOK_STRING
{
  struct olsr_if *in = malloc(sizeof(struct olsr_if));
  
  if(in == NULL)
    {
      fprintf(stderr, "Out of memory(ADD IF)\n");
      YYABORT;
    }

  in->name = $1->string;
  in->config = $2->string;

  if(PARSER_DEBUG) printf("Interface: %s Ruleset: %s\n", $1->string, $2->string);

  /* Queue */
  in->next = cnf->interfaces;
  cnf->interfaces = in;

  free($1);
  free($2);
}
;

bnoint: TOK_NOINT TOK_BOOLEAN
{
  if(PARSER_DEBUG) printf("Noint set to %d\n", $2->boolean);
  free($2);
}
;

atos: TOK_TOS TOK_INTEGER
{
  if($2->boolean == 1)
    {
      if(PARSER_DEBUG) printf("Tos AUTO\n");
    }
  else
    {
      if(PARSER_DEBUG) printf("TOS: %d\n", $2->integer);
    }
  free($2);

}
;

awillingness: TOK_WILLINGNESS TOK_INTEGER
{
  if($2->boolean == 1)
    {
      if(PARSER_DEBUG) printf("Willingness AUTO\n");
    }
  else
    {
      if(PARSER_DEBUG) printf("Willingness: %d\n", $2->integer);
    }
  free($2);

}
;

bipccon: TOK_IPCCON TOK_BOOLEAN
{
  if($2->boolean == 1)
    {
      if(PARSER_DEBUG) printf("IPC allowed\n");
    }
  else
    {
      if(PARSER_DEBUG) printf("IPC blocked\n");
    }
  free($2);

}
;


busehyst: TOK_USEHYST TOK_BOOLEAN
{
  if($2->boolean == 1)
    {
      if(PARSER_DEBUG) printf("Hysteresis enabled\n");
    }
  else
    {
      if(PARSER_DEBUG) printf("Hysteresis disabled\n");
    }
  free($2);

}
;


fhystscale: TOK_HYSTSCALE TOK_FLOAT
{
  cnf->hysteresis_param.scaling = $2->floating;
  if(PARSER_DEBUG) printf("Hysteresis Scaling: %0.2f\n", $2->floating);
  free($2);
}
;


fhystupper: TOK_HYSTUPPER TOK_FLOAT
{
  cnf->hysteresis_param.thr_high = $2->floating;
  if(PARSER_DEBUG) printf("Hysteresis UpperThr: %0.2f\n", $2->floating);
  free($2);
}
;


fhystlower: TOK_HYSTLOWER TOK_FLOAT
{
  cnf->hysteresis_param.thr_low = $2->floating;
  if(PARSER_DEBUG) printf("Hysteresis LowerThr: %0.2f\n", $2->floating);
  free($2);
}
;

fpollrate: TOK_POLLRATE TOK_FLOAT
{
  if(PARSER_DEBUG) printf("Pollrate %0.2f\n", $2->floating);
  cnf->pollrate = $2->floating;

  free($2);
}
;


atcredundancy: TOK_TCREDUNDANCY TOK_INTEGER
{
  if($2->boolean == 1)
    {
      if(PARSER_DEBUG) printf("TC redundancy AUTO\n");
    }
  else
    {
      if(PARSER_DEBUG) printf("TC redundancy %d\n", $2->integer);
      cnf->tc_redundancy = $2->integer;
    }
  free($2);

}
;

amprcoverage: TOK_MPRCOVERAGE TOK_INTEGER
{
  if($2->boolean == 1)
    {
      if(PARSER_DEBUG) printf("MPR coverage AUTO\n");
    }
  else
    {
      if(PARSER_DEBUG) printf("MPR coverage %d\n", $2->integer);
      cnf->mpr_coverage = $2->integer;
    }
  free($2);
}
;


plname: TOK_PLNAME TOK_STRING
{
  struct plugin_entry *pe = malloc(sizeof(struct plugin_entry));
  
  if(pe == NULL)
    {
      fprintf(stderr, "Out of memory(ADD PL)\n");
      YYABORT;
    }

  pe->name = $2->string;
  
  if(PARSER_DEBUG) printf("Plugin: %s\n", $2->string);

  /* Queue */
  pe->next = cnf->plugins;
  cnf->plugins = pe;

  free($2);
}
;

plparam: TOK_PLPARAM TOK_STRING TOK_STRING
{

    if(PARSER_DEBUG) printf("Plugin param key:\"%s\" val: \"%s\"\n", $2->string, $3->string);

    free($2->string);
    free($2);
    free($3->string);
    free($3);
}
;

vcomment:       TOK_COMMENT
{
    //if(PARSER_DEBUG) printf("Comment\n");
}
;



%%

void yyerror (char *string)
{
  fprintf(stderr, "Config line %d: %s\n", current_line, string);
}

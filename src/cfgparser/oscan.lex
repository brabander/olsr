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
 * $Id: oscan.lex,v 1.3 2004/10/17 11:52:41 kattemat Exp $
 *
 */


#define YYSTYPE struct conf_token *

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "olsrd_conf.h"

#include "oparse.h"

/* Prototypes */
int yyget_lineno(void);
FILE * yyget_in(void);
FILE* yyget_out(void);
int yyget_leng(void);
char *yyget_text(void);
void yyset_lineno(int);
void yyset_in(FILE *);
void yyset_out(FILE *);
int yyget_debug(void);
void yyset_debug(int);
int yylex_destroy(void);

struct conf_token *
get_conf_token(void);

struct conf_token *
get_conf_token()
{
  struct conf_token *t = malloc(sizeof(struct conf_token));

  if (t == NULL)
    {
      fprintf(stderr, "Cannot allocate %d bytes for an configuration token.\n",
	      sizeof (struct conf_token));
      exit(EXIT_FAILURE);
    }

  memset(t, 0, sizeof(struct conf_token));

  return t;
}



%}

%option noyywrap

DECDIGIT [0-9]
FLOAT {DECDIGIT}+\.{DECDIGIT}+
HEXDIGIT [a-f][A-F][0-9]

IPV4ADDR ({DECDIGIT}){1,3}\.({DECDIGIT}){1,3}\.({DECDIGIT}){1,3}\.({DECDIGIT}){1,3}

HEXBYTE ([a-f]|[A-F]|[0-9]){1,4}

IP6PAT1 ({HEXBYTE}:){7}{HEXBYTE}
IP6PAT2 {HEXBYTE}::({HEXBYTE}:){0,5}{HEXBYTE}
IP6PAT3 ({HEXBYTE}:){2}:({HEXBYTE}:){0,4}{HEXBYTE}
IP6PAT4 ({HEXBYTE}:){3}:({HEXBYTE}:){0,3}{HEXBYTE}
IP6PAT5 ({HEXBYTE}:){4}:({HEXBYTE}:){0,2}{HEXBYTE}
IP6PAT6 ({HEXBYTE}:){5}:({HEXBYTE}:){0,1}{HEXBYTE}
IP6PAT7 ({HEXBYTE}:){6}:{HEXBYTE}
IP6PAT8 ({HEXBYTE}:){1,7}:
IP6PAT9 ::

IPV6ADDR {IP6PAT1}|{IP6PAT2}|{IP6PAT3}|{IP6PAT4}|{IP6PAT5}|{IP6PAT6}|{IP6PAT7}|{IP6PAT8}|{IP6PAT9}


%%

\s*"#".*\n {

  current_line++;
  return TOK_COMMENT;
}

\{ {
  yylval = NULL;
  return TOK_OPEN;
}

\} {
  yylval = NULL;
  return TOK_CLOSE;
}

\; {
  yylval = NULL;
  return TOK_SEMI;
}

\"[^\"]*\" {
  yylval = get_conf_token();

  yylval->string = malloc(yyleng - 1);

  if (yylval->string == NULL)
  {
    fprintf(stderr,
            "Cannot allocate %d bytes for string token data.\n", yyleng - 1);
    yyterminate();
  }

  strncpy(yylval->string, yytext + 1, yyleng - 2);
  yylval->string[yyleng - 2] = 0;

  return TOK_STRING;
}

0x{HEXDIGIT}+ {
  yylval = get_conf_token();

  yylval->integer = strtol(yytext, NULL, 0);

  return TOK_INTEGER;
}

{FLOAT} {
  yylval = get_conf_token();

  sscanf(yytext, "%f", &yylval->floating);
  return TOK_FLOAT;
}

{IPV4ADDR} {
  yylval = get_conf_token();
  
  yylval->string = malloc(yyleng + 1);
  
  if (yylval->string == NULL)
    {
      fprintf(stderr,
	      "Cannot allocate %d bytes for string token data.\n", yyleng + 1);
      yyterminate();
    }
  
  strncpy(yylval->string, yytext, yyleng+1);

  return TOK_IP4_ADDR;
}



{IPV6ADDR} {

  yylval = get_conf_token();
  
  yylval->string = malloc(yyleng+1);
  
  if (yylval->string == NULL)
    {
      fprintf(stderr,
	      "Cannot allocate %d bytes for string token data.\n", yyleng + 1);
      yyterminate();
    }
  
  strncpy(yylval->string, yytext, yyleng+1);
  
  return TOK_IP6_ADDR;
}


"auto"|{DECDIGIT}+ {

  yylval = get_conf_token();

  if (strncmp(yytext, "auto", 4) == 0)
    {
      yylval->boolean = 1;
    }
  else
    {
      yylval->boolean = 0;
      yylval->integer = atoi(yytext);
    }

  return TOK_INTEGER;

}


"yes"|"no" {
  yylval = get_conf_token();

  if (strncmp(yytext, "yes", 3) == 0)
    yylval->boolean = 1;

  else
    yylval->boolean = 0;

  return TOK_BOOLEAN;
}



"site-local"|"global" {
  yylval = get_conf_token();

  if (strncmp(yytext, "site-local", 10) == 0)
    yylval->boolean = 1;

  else
    yylval->boolean = 0;

  return TOK_IP6TYPE;
}


"DebugLevel" {
  yylval = NULL;
  return TOK_DEBUGLEVEL;
}

"IpVersion" {
  yylval = NULL;
  return TOK_IPVERSION;
}

"Hna4" {
  yylval = NULL;
  return TOK_HNA4;
}

"Hna6" {
  yylval = NULL;
  return TOK_HNA6;
}

"LoadPlugin" {
  yylval = NULL;
  return TOK_PLUGIN;
}

"PlName" {
  yylval = NULL;
  return TOK_PLNAME;
}

"PlParam" {
  yylval = NULL;
  return TOK_PLPARAM;
}

"Interfaces" {
  yylval = NULL;
  return TOK_INTERFACES;
}

"AllowNoInt" {
  yylval = NULL;
  return TOK_NOINT;
}

"TosValue" {
  yylval = NULL;
  return TOK_TOS;
}

"Willingness" {
  yylval = NULL;
  return TOK_WILLINGNESS;
}

"IpcConnect" {
  yylval = NULL;
  return TOK_IPCCON;
}

"UseHysteresis" {
  yylval = NULL;
  return TOK_USEHYST;
}

"HystScaling" {
  yylval = NULL;
  return TOK_HYSTSCALE;
}

"HystThrHigh" {
  yylval = NULL;
  return TOK_HYSTUPPER;
}

"HystThrLow" {
  yylval = NULL;
  return TOK_HYSTLOWER;
}

"Pollrate" {
  yylval = NULL;
  return TOK_POLLRATE;
}


"TcRedundancy" {
  yylval = NULL;
  return TOK_TCREDUNDANCY;
}

"MprCoverage" {
  yylval = NULL;
  return TOK_MPRCOVERAGE;
}


"IfSetup" {
  yylval = NULL;
  return TOK_IFSETUP;
}


"Ip4Broadcast" {
  yylval = NULL;
  return TOK_IP4BROADCAST;
}
"Ip6AddrType" {
  yylval = NULL;
  return TOK_IP6ADDRTYPE;
}
"Ip6MulticastSite" {
  yylval = NULL;
  return TOK_IP6MULTISITE;
}
"Ip6MulticastGlobal" {
  yylval = NULL;
  return TOK_IP6MULTIGLOBAL;
}
"HelloInterval" {
  yylval = NULL;
  return TOK_HELLOINT;
}
"HelloValidityTime" {
  yylval = NULL;
  return TOK_HELLOVAL;
}
"TcInterval" {
  yylval = NULL;
  return TOK_TCINT;
}
"TcValidityTime" {
  yylval = NULL;
  return TOK_TCVAL;
}
"MidInterval" {
  yylval = NULL;
  return TOK_MIDINT;
}
"MidValidityTime" {
  yylval = NULL;
  return TOK_MIDVAL;
}
"HnaInterval" {
  yylval = NULL;
  return TOK_HNAINT;
}
"HnaValidityTime" {
  yylval = NULL;
  return TOK_HNAVAL;
}



\n|\r\n {
  current_line++;
}

\ |\t

. {
  /* Do nothing */
  //fprintf(stderr, "Failed to parse line %d of configuration file.\n",
  //      current_line);
  //yyterminate();
  //yy_fatal_error("Parsing failed.\n");

  /* To avoid compiler warning (stupid...) */
  if(0)
    yyunput(0, NULL);
}

%%

/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004, Andreas Tønnesen(andreto@olsr.org)
 *                     includes code by Bruno Randolf
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
 * $Id: olsrd_httpinfo.h,v 1.5 2004/12/17 11:44:30 kattemat Exp $
 */

/*
 * Dynamic linked library for the olsr.org olsr daemon
 */

#ifndef _OLSRD_HTTP_INFO
#define _OLSRD_HTTP_INFO

#include "olsrd_plugin.h"


#define HTTP_VERSION "HTTP/1.1"

/**Response types */
#define HTTP_200 HTTP_VERSION " 200 OK\r\n"
#define HTTP_400 HTTP_VERSION " 400 Bad Request\r\n"
#define HTTP_404 HTTP_VERSION " 404 Not Found\r\n"


#define HTTP_400_MSG "<html><h1>400 - ERROR</h1><hr><i>" PLUGIN_NAME " version " PLUGIN_VERSION  "</i></html>"
#define HTTP_404_MSG "<html><h1>404 - ERROR, no such file</h1><hr>This server does not support file requests!<br><br><i>" PLUGIN_NAME " version " PLUGIN_VERSION  "</i></html>"


static const char *http_ok_head[] =
{
    "<HEAD>\n",
    "<META http-equiv=\"Content-type\" content=\"text/html; charset=ISO-8859-1\">\n",
    "<TITLE>olsr.org</TITLE>\n",
    "</HEAD>\n",
    "<STYLE>\n",
    "<!--\n",
    "A {text-decoration: none}\n",
    "H1, H2, H3, TD, TH, B {font-family: Helvetica}\n",
    "-->\n",
    "</STYLE>\n\n",
    "<BODY BGCOLOR=\"#FFFFFF\" TEXT=\"#0000000\">\n",
    "<TABLE WIDTH=800 BORDER=0 CELLSPACING=0 CELLPADDING=0 ALIGN=center>\n",
    "<TR BGCOLOR=\"#000000\">\n",
    "<TD WIDTH=30 HEIGHT=\"69\">",
    "</TD>\n",
    "<TD WIDTH=345 VALIGN=center ALIGN=center>\n",
    "<FONT COLOR=white SIZE=\"6\" FACE=\"timesroman\"></b>olsr.org OLSR daemon<b></TD>\n",
    "<TD ALIGN=center VALIGN=bottom><FONT COLOR=white>\n"
    "</FONT></TD>\n",
    "</TR><TR BGCOLOR=\"#888888\">\n",
    "<TD COLSPAN=2 HEIGHT=30 ALIGN=left VALIGN=center>\n",
    "<B><FONT SIZE=+1 COLOR=\"#FFFFFF\">&nbsp;Version 0.4.8\n",
    "</FONT></B></TD>\n",
    "<TD COLSPAN=2 ALIGN=right VALIGN=center>\n",
    "<TABLE WIDTH=\"100\%\" VALIGN=center>\n",
    "<TR>\n",
    "<TD ALIGN=left>\n",
    "</TD>\n",
    "<TD ALIGN=right>\n",
    "<FONT COLOR=\"#FFFFFF\">\n",
    "<A HREF=\"\"><B>Link1</B></A>&nbsp;|&nbsp;\n",
    "<A HREF=\"\"><B>Link2</B></A>&nbsp;|&nbsp;\n",
    "<A HREF=\"\"><B>Link3</B></A>&nbsp;|&nbsp;\n",
    "<A HREF=\"/\"><B>Refresh</B></A>&nbsp;&nbsp;\n",
    "</FONT>\n",
    "</TD>\n",
    "</TR>\n",
    "</TABLE>\n",
    "</TD>\n",
    "</TR>\n",
    "</TABLE>\n",
    "<!-- END HEAD -->\n\n",
    NULL
};





static const char *http_ok_tail[] =
{
    "\n<!-- START TAIL -->\n\n",
    "<P>\n",
    "<P>\n",
    "<HR ALIGN=center WIDTH=800>\n",
    "<P>\n",
    "<P>     \n",
    "<TABLE WIDTH=800 BGCOLOR=\"#ECECEC\" BORDER=0 CELLPADDING=2 ALIGN=center>\n",
    "<TR>    \n",
    "<TD ALIGN=center VALIGN=center>\n",
    "<TABLE BORDER=0 CELLSPACING=3 CELLPADDING=2>\n",
    "<TR>\n",
    "<TD WIDTH=\"50\%\" ALIGN=\"center\" VALIGN=\"center\">\n",
    "</CENTER>\n",
    "<CENTER>For questions or comments on this plugin send mail to<br>\n",
    "<A HREF=\"mailto:olsr-users@olsr.org\">olsr-users@olsr.org</A> or \n",
    "<A HREF=\"mailto:andreto-at-olsr.org\">andreto-at-olsr.org</A></CENTER></TD>\n",
    "<TD WIDTH=\"50\%\" ALIGN=\"center\" VALIGN=\"center\">\n",
    "<CENTER><FONT-=2>Official olsrd homepage:</FONT><br><A HREF=\"http://www.olsr.org/\">http://www.olsr.org</A>\n",
    "</FONT></CENTER>\n",
    "</TD>\n",
    "</TR>\n",
    "</TABLE>\n",
    "</TD>\n",
    "</TR>\n",
    "</TABLE>\n",
    "</BODY></HTML>\n",
    NULL
};



static const char *http_frame[] =
{
  "<P>\n<TABLE WIDTH=800 CELLSPACING=0 CELLPADDING=3 BORDER=1 ALIGN=center>\n",
  "<TR BGCOLOR=\"#CFCFCF\">\n<TH ALIGN=left><B>%s</B></TH>\n",
  "</TR><TR BGCOLOR=\"#ECECEC\">\n",
  "<TD>\n",
  "<P>\n",
  "<pre>\n",
  "<!-- BODY -->",
  "</pre>\n",
  "</TD>\n",
  "</TR>\n",
  "</TABLE>\n",
  NULL
};



typedef enum
  {
    HTTP_BAD_REQ,
    HTTP_BAD_FILE,
    HTTP_OK
  }http_header_type;

char netmask[5];

char *
olsr_ip_to_string(union olsr_ip_addr *);

char *
olsr_netmask_to_string(union hna_netmask *);


#endif

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
 * $Id: olsrd_httpinfo.h,v 1.12 2004/12/19 15:04:30 kattemat Exp $
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
  "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n",
  "<HEAD>\n",
  "<META http-equiv=\"Content-type\" content=\"text/html; charset=ISO-8859-1\">\n",
  "<TITLE>olsr.org httpinfo plugin</TITLE>\n",
  "</HEAD>\n",
  "<STYLE>\n",
  "<!--\n",
  "A {text-decoration: none}\n",
  "TH{text-align: left}\n",
  "H1, H2, H3, TD, TH, B {font-family: Helvetica; font-size: 80%}\n",
  "-->\n",
  "</STYLE>\n\n",
  "<BODY BGCOLOR=\"#FFFFFF\" TEXT=\"#0000000\">\n",
  "<TABLE WIDTH=800 BORDER=0 CELLSPACING=0 CELLPADDING=0 ALIGN=center>\n",
  "<TR BGCOLOR=\"#000044\">\n",
  "<TD HEIGHT=\"69\" WIDTH=\"100%\" VALIGN=center ALIGN=left>\n",
  "<FONT COLOR=white SIZE=\"6\" FACE=\"timesroman\">&nbsp;&nbsp;&nbsp;olsr.org OLSR daemon</TD>\n",
  "</FONT></TD>\n",
  "</TR>\n<TR BGCOLOR=\"#8888cc\">\n",
  "<TD HEIGHT=\"25\" ALIGN=right VALIGN=center>\n",
  "<FONT COLOR=\"#FFFFFF\">\n",
  "<A HREF=\"#status\"><B>Status</B></A>&nbsp;|&nbsp;\n",
  "<A HREF=\"#routes\"><B>Routes</B></A>&nbsp;|&nbsp;\n",
  "<A HREF=\"#neighbors\"><B>Neighbors</B></A>&nbsp;|&nbsp;\n",
  "<A HREF=\"#topology\"><B>Topology</B></A>&nbsp;|&nbsp;\n",
  "<A HREF=\"#hna\"><B>HNA</B></A>&nbsp;|&nbsp;\n",
  "<A HREF=\"#mid\"><B>MID</B></A>&nbsp;|&nbsp;\n",
  "<A HREF=\"/\"><B>Refresh</B></A>&nbsp;&nbsp;\n",
  "</FONT>\n",
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
    "<TABLE WIDTH=800 BGCOLOR=\"#E0E0FF\" BORDER=0 CELLPADDING=2 ALIGN=center>\n",
    "<TR>    \n",
    "<TD ALIGN=center VALIGN=center>\n",
    "<TABLE BORDER=0 CELLSPACING=3 CELLPADDING=2>\n",
    "<TR>\n",
    "<TD WIDTH=\"50\%\" ALIGN=\"center\" VALIGN=\"center\">\n",
    "</CENTER>\n",
    "<CENTER>Plugin by Andreas T&oslash;nnesen.<br> Send questions or comments to<br>\n",
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
  "<TR BGCOLOR=\"#E0E0FF\">\n<TH ALIGN=left><a name=\"%s\">%s</a></TH>\n",
  "</TR><TR BGCOLOR=\"#ECECEC\">\n",
  "<TD>\n",
  "<P>\n",
  "<!-- BODY -->",
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

char *
olsr_ip_to_string(union olsr_ip_addr *);

char *
olsr_netmask_to_string(union hna_netmask *);


#endif

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
 * $Id: olsrd_httpinfo.h,v 1.22 2005/01/02 14:21:11 kattemat Exp $
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



static const char *httpinfo_css[] =
{
  "A {text-decoration: none}\n",
  "TH{text-align: left}\n",
  "H1, H2, H3, TD, TH {font-family: Helvetica; font-size: 80%%}\n",
  "hr\n{\nborder: none;\npadding: 1px;\nbackground: url(grayline.gif) repeat-x bottom;\n}\n",
  "#maintable\n{\nmargin: 0px;\npadding: 5px;\nborder-left: 1px solid #ccc;\n",
  "border-right: 1px solid #ccc;\nborder-bottom: 1px solid #ccc;\n}\n",
  "#footer\n{\nfont-size: 10px;\nline-height: 14px;\ntext-decoration: none;\ncolor: #666;\n}\n",
  "#hdr\n{\nfont-size: 14px;\ntext-align: center;\n\nline-height: 16px;\n",
  "text-decoration: none;\nborder: 1px solid #ccc;\n",
  "margin: 5px;\nbackground: #ececec;\n}\n",
  "#container\n{\nwidth: 500px;\npadding: 30px;\nborder: 1px solid #ccc;\nbackground: #fff;\n}\n",
  "#tabnav\n{\nheight: 20px;\nmargin: 0;\npadding-left: 10px;\n",
  "background: url(grayline.gif) repeat-x bottom;\n}\n",
  "#tabnav li\n{\nmargin: 0;\npadding: 0;\ndisplay: inline;\nlist-style-type: none;\n}\n",
  "#tabnav a:link, #tabnav a:visited\n{\nfloat: left;\nbackground: #ececec;\n",
  "font-size: 12px;\nline-height: 14px;\nfont-weight: bold;\npadding: 2px 10px 2px 10px;\n",
  "margin-right: 4px;\nborder: 1px solid #ccc;\ntext-decoration: none;\ncolor: #666;\n}",
  "#tabnav a:link.active, #tabnav a:visited.active\n{\nborder-bottom: 1px solid #fff;\n",
  "background: #ffffff;\ncolor: #000;\n}\n",
  "#tabnav a:hover\n{\nbackground: #777777;\ncolor: #ffffff;\n}\n",
  NULL
};



static const char *http_ok_head[] =
{
  "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n",
  "<HEAD>\n",
  "<META http-equiv=\"Content-type\" content=\"text/html; charset=ISO-8859-1\">\n",
  "<TITLE>olsr.org httpinfo plugin</TITLE>\n",
  "<link rel=\"icon\" href=\"favicon.ico\" type=\"image/x-icon\">\n",
  "<link rel=\"shortcut icon\" href=\"favicon.ico\" type=\"image/x-icon\">\n",
  "<link rel=\"stylesheet\" type=\"text/css\" href=\"httpinfo.css\">\n",
  "</HEAD>\n",
  "<body bgcolor=\"#ffffff\" text=\"#000000\">\n",
  "<table align=\"center\" border=\"0\" cellpadding=\"0\" cellspacing=\"0\" width=\"800\">"
  "<tbody><tr bgcolor=\"#ffffff\">",
  "<td align=\"left\" height=\"69\" valign=\"middle\" width=\"80%\">",
  "<font color=\"black\" face=\"timesroman\" size=\"6\">&nbsp;&nbsp;&nbsp;olsr.org OLSR daemon</font></td>",
  "<td align=\"right\" height=\"69\" valign=\"middle\" width=\"20%\">",
  "<img src=\"/logo.gif\"></td>",
  "</tr>",
  "<p>",
  "</table>",
  "<!-- END HEAD -->\n\n",
  NULL
};



static const char *html_tabs[] =
{
  "<table align=\"center\" border=\"0\" cellpadding=\"0\" cellspacing=\"0\" width=\"800\">\n",
  "<tr bgcolor=\"#ffffff\"><td>\n",
  "<ul id=\"tabnav\">\n",
  "<!-- TAB ELEMENTS -->",
  "<li><a href=\"%s\" %s>%s</a></li>\n",
  "</ul>\n",
  "</td></tr>\n",
  "<tr><td>\n",
  NULL
};





static const char *http_ok_tail[] =
{
    "\n<!-- START TAIL -->\n\n",
    "<div id=\"footer\">\n\n",
    "<p><center>\n",
    "(C)2005 Andreas T&oslash;nnesen<br>\n",
    "<a href=\"http://www.olsr.org/\">http://www.olsr.org</a>\n",
    "</center>\n",
    "</div>\n",
    "</body></html>\n",
    NULL
};


static const char *about_frame[] =
{
    "Plugin by Andreas T&oslash;nnesen.<br> Send questions or comments to\n",
    "<a href=\"mailto:olsr-users@olsr.org\">olsr-users@olsr.org</a> or\n",
    "<a href=\"mailto:andreto-at-olsr.org\">andreto-at-olsr.org</a><br>\n"
    "Official olsrd homepage: <a href=\"http://www.olsr.org/\">http://www.olsr.org</a><br>\n",
    NULL
};



static const char *http_frame[] =
{
  "<div id=\"maintable\">\n",
  "<!-- BODY -->",
  "</div>\n",
  NULL
};



typedef enum
  {
    HTTP_BAD_REQ,
    HTTP_BAD_FILE,
    HTTP_OK
  }http_header_type;


struct http_stats
{
  olsr_u32_t ok_hits;
  olsr_u32_t err_hits;
  olsr_u32_t ill_hits;
};

char *
olsr_ip_to_string(union olsr_ip_addr *);

char *
olsr_netmask_to_string(union hna_netmask *);


#endif

/*
 * The olsr.org Optimized Link-State Routing daemon (olsrd)
 *
 * Copyright (c) 2004, Thomas Lopatic (thomas@olsr.org)
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
 * $Id: plugin.c,v 1.4 2005/04/13 22:53:13 tlopatic Exp $
 */

#include <string.h>
#include <time.h> // clock_t required by olsrd includes

#include "link.h"
#include "plugin.h"
#include "lib.h"
#include "os_unix.h"
#include "http.h"
#include "glua.h"
#include "glua_ext.h"

#include <olsr_plugin_io.h>
#include <plugin_loader.h>
#include <link_set.h>
#include <neighbor_table.h>
#include <two_hop_neighbor_table.h>
#include <mid_set.h>
#include <tc_set.h>
#include <hna_set.h>
#include <routing_table.h>
#include <olsr_protocol.h>
#include <lq_route.h>

#define MESSAGE_TYPE 129

int get_plugin_interface_version(void);
int plugin_io(int cmd, void *data, int len);
int register_olsr_data(struct olsr_plugin_data *data);
int register_olsr_param(char *name, char *value);

static int ipAddrLen;
static union olsr_ip_addr *mainAddr;

static struct interface *intTab = NULL;
static struct neighbor_entry *neighTab = NULL;
static struct mid_entry *midTab = NULL;
static struct tc_entry *tcTab = NULL;
static struct hna_entry *hnaTab = NULL;
static struct rt_entry *routeTab = NULL;
static struct olsrd_config *config = NULL;

static int (*pluginIo)(int which, void *data, int len);

static int (*regTimeout)(void (*timeoutFunc)(void));
static int (*regParser)(void (*parserFunc)(unsigned char *mess,
                                           struct interface *inInt,
                                           union olsr_ip_addr *neighIntAddr),
                        int type, int forward);
static int (*checkLink)(union olsr_ip_addr *neighIntAddr);
static int (*checkDup)(union olsr_ip_addr *origAddr, unsigned short seqNo);
static int (*forward)(unsigned char *mess, union olsr_ip_addr *origAddr,
                      unsigned short seqNo, struct interface *inInt,
                      union olsr_ip_addr *neighIntAddr);

static unsigned short (*getSeqNo)(void);
static int (*netPush)(struct interface *outInt, void *mess,
                      unsigned short size);
static int (*netOutput)(struct interface *outInt);

static void *(*lookupMprs)(union olsr_ip_addr *neighAddr);

static int iterIndex;
static struct interface *iterIntTab = NULL;
static struct link_entry *iterLinkTab = NULL;
static struct neighbor_entry *iterNeighTab = NULL;
static struct mid_entry *iterMidTab = NULL;
static struct tc_entry *iterTcTab = NULL;
static struct hna_entry *iterHnaTab = NULL;
static struct rt_entry *iterRouteTab = NULL;

static void __attribute__((constructor)) banner(void)
{
  printf("Tiny Application Server 0.1 by olsr.org\n");
}

static double lqToEtx(double lq, double nlq)
{
  if (lq < MIN_LINK_QUALITY || nlq < MIN_LINK_QUALITY)
    return 0.0;

  else
    return 1.0 / (lq * nlq);
}

int iterLinkTabNext(char *buff, int len)
{
  double etx;

  if (iterLinkTab == NULL)
    return -1;

  etx = lqToEtx(iterLinkTab->loss_link_quality,
                iterLinkTab->neigh_link_quality);

  snprintf(buff, len, "local~%s~remote~%s~main~%s~hysteresis~%f~lq~%f~nlq~%f~etx~%f~",
           rawIpAddrToString(&iterLinkTab->local_iface_addr, ipAddrLen),
           rawIpAddrToString(&iterLinkTab->neighbor_iface_addr, ipAddrLen),
           rawIpAddrToString(&iterLinkTab->neighbor->neighbor_main_addr, ipAddrLen),
           iterLinkTab->L_link_quality, iterLinkTab->loss_link_quality,
           iterLinkTab->neigh_link_quality, etx);

  iterLinkTab = iterLinkTab->next;

  return 0;
}

void iterLinkTabInit(void)
{
  if (pluginIo == NULL)
  {
    iterLinkTab = NULL;
    return;
  }

  pluginIo(GETD__LINK_SET, &iterLinkTab, sizeof (iterLinkTab));
}

int iterNeighTabNext(char *buff, int len)
{
  int res;
  int i;
  struct neighbor_2_list_entry *neigh2;
  
  if (iterNeighTab == NULL)
    return -1;

  res = snprintf(buff, len,
                 "main~%s~symmetric~%s~mpr~%s~mprs~%s~willingness~%d~[~neighbors2~",
                 rawIpAddrToString(&iterNeighTab->neighbor_main_addr, ipAddrLen),
                 iterNeighTab->status == SYM ? "true" : "false",
                 iterNeighTab->is_mpr != 0 ? "true" : "false",
                 lookupMprs(&iterNeighTab->neighbor_main_addr) != NULL ?
                 "true" : "false",
                 iterNeighTab->willingness);

  i = 0;

  len -= res;
  buff += res;

  len -= 2;

  for (neigh2 = iterNeighTab->neighbor_2_list.next;
       neigh2 != &iterNeighTab->neighbor_2_list;
       neigh2 = neigh2->next)
  {
    res = snprintf(buff, len, "%d~%s~", i,
                   rawIpAddrToString(&neigh2->neighbor_2->neighbor_2_addr,
                                     ipAddrLen));

    if (res < len)
      buff += res;

    len -= res;

    if (len <= 0)
      break;

    i++;
  }

  strcpy(buff, "]~");

  iterNeighTab = iterNeighTab->next;

  if (iterNeighTab == &neighTab[iterIndex])
  {
    iterNeighTab = NULL;
    
    while (++iterIndex < HASHSIZE)
      if (neighTab[iterIndex].next != &neighTab[iterIndex])
      {
        iterNeighTab = neighTab[iterIndex].next;
        break;
      }
  }

  return 0;
}

void iterNeighTabInit(void)
{
  iterNeighTab = NULL;

  if (neighTab == NULL)
    return;

  for (iterIndex = 0; iterIndex < HASHSIZE; iterIndex++)
    if (neighTab[iterIndex].next != &neighTab[iterIndex])
    {
      iterNeighTab = neighTab[iterIndex].next;
      break;
    }
}

int iterRouteTabNext(char *buff, int len)
{
  if (iterRouteTab == NULL)
    return -1;

  snprintf(buff, len, "destination~%s~gateway~%s~interface~%s~metric~%d~",
           rawIpAddrToString(&iterRouteTab->rt_dst, ipAddrLen),
           rawIpAddrToString(&iterRouteTab->rt_router, ipAddrLen),
           iterRouteTab->rt_if->int_name, iterRouteTab->rt_metric);

  iterRouteTab = iterRouteTab->next;

  if (iterRouteTab == &routeTab[iterIndex])
  {
    iterRouteTab = NULL;
    
    while (++iterIndex < HASHSIZE)
      if (routeTab[iterIndex].next != &routeTab[iterIndex])
      {
        iterRouteTab = routeTab[iterIndex].next;
        break;
      }
  }

  return 0;
}

void iterRouteTabInit(void)
{
  iterRouteTab = NULL;

  if (routeTab == NULL)
    return;

  for (iterIndex = 0; iterIndex < HASHSIZE; iterIndex++)
    if (routeTab[iterIndex].next != &routeTab[iterIndex])
    {
      iterRouteTab = routeTab[iterIndex].next;
      break;
    }
}

int iterTcTabNext(char *buff, int len)
{
  int res;
  int i;
  struct topo_dst *dest;
  
  if (iterTcTab == NULL)
    return -1;

  res = snprintf(buff, len,
                 "main~%s~[~destinations~",
                 rawIpAddrToString(&iterTcTab->T_last_addr, ipAddrLen));

  i = 0;

  len -= res;
  buff += res;

  len -= 2;

  for (dest = iterTcTab->destinations.next; dest != &iterTcTab->destinations;
       dest = dest->next)
  {
    res = snprintf(buff, len, "[~%d~address~%s~etx~%f~]~", i,
                   rawIpAddrToString(&dest->T_dest_addr, ipAddrLen),
                   lqToEtx(dest->link_quality, dest->inverse_link_quality));

    if (res < len)
      buff += res;

    len -= res;

    if (len <= 0)
      break;

    i++;
  }

  strcpy(buff, "]~");

  iterTcTab = iterTcTab->next;

  if (iterTcTab == &tcTab[iterIndex])
  {
    iterTcTab = NULL;
    
    while (++iterIndex < HASHSIZE)
      if (tcTab[iterIndex].next != &tcTab[iterIndex])
      {
        iterTcTab = tcTab[iterIndex].next;
        break;
      }
  }

  return 0;
}

void iterTcTabInit(void)
{
  iterTcTab = NULL;

  if (tcTab == NULL)
    return;

  for (iterIndex = 0; iterIndex < HASHSIZE; iterIndex++)
    if (tcTab[iterIndex].next != &tcTab[iterIndex])
    {
      iterTcTab = tcTab[iterIndex].next;
      break;
    }
}

static void parserFunc(unsigned char *mess, struct interface *inInt,
                       union olsr_ip_addr *neighIntAddr)
{
  union olsr_ip_addr *orig = (union olsr_ip_addr *)(mess + 4);
  unsigned short seqNo = (mess[ipAddrLen + 6] << 8) | mess[ipAddrLen + 7];
  int len = (mess[2] << 8) | mess[3];
  char *service, *string;
  int i;

  if (memcmp(orig, mainAddr, ipAddrLen) == 0)
    return;

  if (checkLink(neighIntAddr) != SYM_LINK)
  {
    error("TAS message not from symmetric neighbour\n");
    return;
  }

  if (len < ipAddrLen + 8 + 2)
  {
    error("short TAS message received (%d bytes)\n", len);
    return;
  }

  if (checkDup(orig, seqNo) != 0)
  {
    len -= ipAddrLen + 8;
    service = mess + ipAddrLen + 8;

    for (i = 0; i < len && service[i] != 0; i++);

    if (i++ == len)
    {
      error("TAS message has unterminated service string\n");
      return;
    }

    if (i == len)
    {
      error("TAS message lacks payload string\n");
      return;
    }

    string = service + i;
    len -= i;

    for (i = 0; i < len && string[i] != 0; i++);

    if (i == len)
    {
      error("TAS message has unterminated payload string\n");
      return;
    }

    httpAddTasMessage(service, string, rawIpAddrToString(orig, ipAddrLen));
  }

  forward(mess, orig, seqNo, inInt, neighIntAddr);
}

void sendMessage(const char *service, const char *string)
{
  unsigned char *mess, *walker;
  int len, pad;
  unsigned short seqNo;
  struct interface *inter;

  pad = len = ipAddrLen + 8 + strlen(service) + 1 + strlen(string) + 1;

  len = 1 + ((len - 1) | 3);

  pad = len - pad;

  walker = mess = allocMem(len);

  seqNo = getSeqNo();

  *walker++ = MESSAGE_TYPE;
  *walker++ = 0;
  *walker++ = (unsigned char)(len >> 8);
  *walker++ = (unsigned char)len;

  memcpy(walker, mainAddr, ipAddrLen);
  walker += ipAddrLen;

  *walker++ = 255;
  *walker++ = 0;
  *walker++ = (unsigned char)(seqNo >> 8);
  *walker++ = (unsigned char)seqNo;

  while (*service != 0)
    *walker++ = *service++;

  *walker++ = 0;

  while (*string != 0)
    *walker++ = *string++;

  *walker++ = 0;

  while (pad-- > 0)
    *walker++ = 0;

  for (inter = intTab; inter != NULL; inter = inter->int_next)
  {
    if (netPush(inter, mess, len) != len)
    {
      netOutput(inter);
      netPush(inter, mess, len);
    }
  }
}

static void serviceFunc(void)
{
  static int up = 0;

  if (up == 0)
  {
    if (httpSetup() < 0)
      return;

    up = 1;
  }

  if (up != 0)
    httpService((int)(1.0 / config->pollrate));
}

int get_plugin_interface_version(void)
{
  httpInit();

  return 3;
}

int plugin_io(int cmd, void *data, int len)
{
  return 0;
}

int register_olsr_data(struct olsr_plugin_data *data)
{
  ipAddrLen = addrLen(data->ipversion);
  mainAddr = data->main_addr;

  pluginIo = (int (*)(int, void *, int))data->olsr_plugin_io;

  pluginIo(GETD__IFNET, &intTab, sizeof (intTab));
  pluginIo(GETD__NEIGHBORTABLE, &neighTab, sizeof (neighTab));
  pluginIo(GETD__MID_SET, &midTab, sizeof (midTab));
  pluginIo(GETD__TC_TABLE, &tcTab, sizeof (tcTab));
  pluginIo(GETD__HNA_SET, &hnaTab, sizeof (hnaTab));
  pluginIo(GETD__ROUTINGTABLE, &routeTab, sizeof (routeTab));
  pluginIo(GETD__OLSR_CNF, &config, sizeof (config));

  pluginIo(GETF__OLSR_REGISTER_TIMEOUT_FUNCTION, &regTimeout,
           sizeof (regTimeout));
  pluginIo(GETF__OLSR_PARSER_ADD_FUNCTION, &regParser, sizeof (regParser));
  pluginIo(GETF__CHECK_NEIGHBOR_LINK, &checkLink, sizeof (checkLink));
  pluginIo(GETF__OLSR_CHECK_DUP_TABLE_PROC, &checkDup, sizeof (checkDup));
  pluginIo(GETF__OLSR_FORWARD_MESSAGE, &forward, sizeof (forward));

  pluginIo(GETF__GET_MSG_SEQNO, &getSeqNo, sizeof (getSeqNo));
  pluginIo(GETF__NET_OUTBUFFER_PUSH, &netPush, sizeof (netPush));
  pluginIo(GETF__NET_OUTPUT, &netOutput, sizeof (netOutput));

  pluginIo(GETF__OLSR_LOOKUP_MPRS_SET, &lookupMprs, sizeof (lookupMprs));

  regTimeout(serviceFunc);
  regParser(parserFunc, MESSAGE_TYPE, 1);

  return 0;
}

int register_olsr_param(char *name, char *value)
{
  if (strcmp(name, "address") == 0)
  {
    if (httpSetAddress(value) < 0)
      return 0;

    return 1;
  }

  if (strcmp(name, "port") == 0)
  {
    if (httpSetPort(value) < 0)
      return 0;

    return 1;
  }

  if (strcmp(name, "rootdir") == 0)
  {
    if (httpSetRootDir(value) < 0)
      return 0;

    return 1;
  }

  if (strcmp(name, "workdir") == 0)
  {
    if (httpSetWorkDir(value) < 0)
      return 0;

    return 1;
  }

  if (strcmp(name, "indexfile") == 0)
  {
    httpSetIndexFile(value);
    return 1;
  }

  if (strcmp(name, "user") == 0)
  {
    httpSetUser(value);
    return 1;
  }

  if (strcmp(name, "password") == 0)
  {
    httpSetPassword(value);
    return 1;
  }

  if (strcmp(name, "sesstime") == 0)
  {
    if (httpSetSessTime(value) < 0)
      return 0;

    return 1;
  }

  if (strcmp(name, "pubdir") == 0)
  {
    httpSetPubDir(value);
    return 1;
  }

  if (strcmp(name, "quantum") == 0)
  {
    if (httpSetQuantum(value) < 0)
      return 0;

    return 1;
  }

  if (strcmp(name, "messtime") == 0)
  {
    if (httpSetMessTime(value) < 0)
      return 0;

    return 1;
  }

  if (strcmp(name, "messlimit") == 0)
  {
    if (httpSetMessLimit(value) < 0)
      return 0;

    return 1;
  }

  return 0;
}

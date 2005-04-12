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
 * $Id: glua_ext.c,v 1.1 2005/04/12 17:17:25 tlopatic Exp $
 */

#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "lua/lualib.h"

#include "link.h"
#include "plugin.h"
#include "lib.h"
#include "os_unix.h"
#include "http.h"
#include "glua.h"
#include "glua_ext.h"

static char *getToken(char **point)
{
  char *localPoint = *point;
  char *start;

  start = localPoint;

  while (*localPoint != '~' && *localPoint != 0)
    localPoint++;

  if (*localPoint == 0)
    return NULL;

  *localPoint++ = 0;

  *point = localPoint;

  return start;
}

int tasOlsrSendMessage(lua_State *lua)
{
  const char *service;
  const char *string;

  service = luaL_checkstring(lua, 1);
  string = luaL_checkstring(lua, 2);

  sendMessage(service, string);
  httpAddTasMessage(service, string, "localhost");

  return 0;
}

int tasOlsrGetMessage(lua_State *lua)
{
  const char *service;
  char *string;
  char *from;

  service = luaL_checkstring(lua, 1);

  if (httpGetTasMessage(service, &string, &from) < 0)
  {
    lua_pushnil(lua);
    lua_pushnil(lua);
  }

  else
  {
    lua_pushstring(lua, string);
    lua_pushstring(lua, from);

    freeMem(string);
    freeMem(from);
  }

  return 2;
}

static void addTable(lua_State *lua, char *name, void (*init)(void),
                     int (*next)(char *buff, int len))
{
  int i;
  char buff[1024], *walker;

  lua_pushstring(lua, name);
  lua_newtable(lua);

  init();

  i = 0;

  while (next(buff, sizeof (buff)) >= 0)
  {
    walker = buff;

    lua_pushnumber(lua, i++);
    lua_newtable(lua);

    while (*walker != 0)
    {
      lua_pushstring(lua, getToken(&walker));
      lua_pushstring(lua, getToken(&walker));

      lua_settable(lua, -3);
    }

    lua_settable(lua, -3);
  }

  lua_settable(lua, -3);
}

int tasOlsrGetInfo(lua_State *lua)
{
  lua_newtable(lua);

  addTable(lua, "routes", iterRouteTabInit, iterRouteTabNext);
  addTable(lua, "links", iterLinkTabInit, iterLinkTabNext);
  addTable(lua, "neighbors", iterNeighTabInit, iterNeighTabNext);

  return 1;
}

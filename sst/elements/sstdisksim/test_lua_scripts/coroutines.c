// Copyright 2009-2013 Sandia Corporation. Under the terms
// of Contract DE-AC04-94AL85000 with Sandia Corporation, the U.S.
// Government retains certain rights in this software.
// 
// Copyright (c) 2009-2013, Sandia Corporation
// All rights reserved.
// 
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.


extern "C"
{
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include <cassert>
#include <iostream>

static int anint = 0;

int yielding(lua_State* L);

int yielding2(lua_State* L)
{
  anint += 13;
  lua_pushnumber(L, anint); 
  return lua_yieldk(L, 1, 0, yielding);
}

int yielding(lua_State* L)
{
  anint++;
  lua_pushnumber(L, anint); 
  return lua_yieldk(L, 1, 0, yielding2);
}

int main()
{
  int test;
  lua_State* L = luaL_newstate();
  luaL_openlibs(L);
  
  lua_State* t = lua_newthread(L);
  
  lua_pushcclosure(L, &yielding, 0);
  lua_setglobal(L, "yielding");
  
  lua_pushcclosure(L, &yielding2, 0);
  lua_setglobal(L, "yielding2");
  
  luaL_dofile(t, "coroutines.lua");
  
  lua_getglobal(t, "f");
  
  test = lua_resume(t, 0);
  assert(lua_gettop(t) == 1);
  test = lua_tonumber(t, -1);
  lua_pop(t, 1);
  
  lua_pushnumber(t, 1);
  test = lua_resume(t, 0);
  assert(lua_gettop(t) == 1);
  test = lua_tonumber(t, -1);
  lua_pop(t, 1);
  
  lua_pushnumber(t, 2);
  test = LUA_YIELD;
  test = lua_resume(t, 0);
  assert(lua_gettop(t) == 1);
  test = lua_tonumber(t, -1);
  lua_pop(t, 1);
  
  lua_close(L); /* Only need to close the top thread-
		   all threads underneath get closed */
}

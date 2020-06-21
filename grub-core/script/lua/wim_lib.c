/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2009,2018,2019,2020  Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "grub_lib.h"

#include <grub/misc.h>
#include <grub/file.h>
#include <grub/wimtools.h>

static int
wim_file_exist (lua_State *state)
{
  const char *wim;
  const char *path;
  unsigned int index = 0;
  grub_file_t file = 0;

  wim = luaL_checkstring (state, 1);
  path = luaL_checkstring (state, 2);
  if (lua_gettop (state) > 2)
    index = luaL_checkinteger (state, 3);

  file = grub_file_open (wim, GRUB_FILE_TYPE_LOOPBACK);
  if (file)
  {
    lua_pushboolean (state, grub_wim_file_exist (file, index, path));
    grub_file_close (file);
  }
  else
    lua_pushboolean (state, 0);
  return 1;
}

static int
wim_is64 (lua_State *state)
{
  const char *wim;
  unsigned int index = 0;
  grub_file_t file = 0;

  wim = luaL_checkstring (state, 1);
  if (lua_gettop (state) > 1)
    index = luaL_checkinteger (state, 2);

  file = grub_file_open (wim, GRUB_FILE_TYPE_LOOPBACK);
  if (file)
  {
    lua_pushboolean (state, grub_wim_is64 (file, index));
    grub_file_close (file);
  }
  else
    lua_pushboolean (state, 0);
  return 1;
}

luaL_Reg wimlib[] =
{
  {"file_exist", wim_file_exist},
  {"is64", wim_is64},
  {0, 0}
};

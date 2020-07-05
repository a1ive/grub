/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2020  Free Software Foundation, Inc.
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

#include <grub/dl.h>
#include <grub/mm.h>
#include <grub/misc.h>
#include <grub/types.h>
#include <grub/file.h>
#include <grub/command.h>
#include <grub/term.h>
#include <grub/i18n.h>
#include <grub/charset.h>
#include <grub/lua.h>

#include "reg.h"

GRUB_MOD_LICENSE ("GPLv3+");

#define MAX_NAME 255

static grub_err_t
grub_cmd_reg (grub_command_t cmd __attribute__ ((unused)),
              int argc, char **args)

{
  HKEY root = 0;
  grub_err_t errno;
  grub_file_t file = 0;
  grub_reg_hive_t *hive = NULL;
  if (argc < 1)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "bad argument.");
  file = grub_file_open (args[0], GRUB_FILE_TYPE_CAT);
  if (!file)
    return grub_error (GRUB_ERR_FILE_NOT_FOUND, "file not found.");
  errno = grub_open_hive (file, &hive);
  if (errno || !hive)
    return grub_error (GRUB_ERR_BAD_OS, "load error.");
  hive->find_root (hive, &root);
  grub_printf ("root: %d\n", root);
  grub_refresh ();
  uint32_t i = 0;
  uint16_t wname[MAX_NAME];
  unsigned char name[MAX_NAME];
  while (1)
  {
    errno = hive->enum_keys (hive, root, i, wname, MAX_NAME);
    if (errno == GRUB_ERR_FILE_NOT_FOUND)
      break;
    if (errno)
    {
      grub_printf ("ERROR: %d %d", i, errno);
      break;
    }
    grub_utf16_to_utf8 (name, wname, MAX_NAME);
    grub_printf ("%d %s\n", i, name);
    grub_refresh ();
    i++;
  }
  hive->close (hive);
  grub_file_close (file);
  return GRUB_ERR_NONE;
}

static grub_command_t cmd;

static int
reg_open (lua_State *state)
{
  grub_reg_hive_t *hive = NULL;
  grub_file_t file;

  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  file = lua_touserdata (state, 1);
  grub_open_hive (file, &hive);
  lua_pushlightuserdata (state, hive);
  return 1;
}

static int
reg_close (lua_State *state)
{
  grub_reg_hive_t *hive;

  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  hive = lua_touserdata (state, 1);
  hive->close (hive);
  return 0;
}

static int
reg_find_root (lua_State *state)
{
  HKEY root = 0;
  grub_reg_hive_t *hive = NULL;

  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  hive = lua_touserdata (state, 1);
  hive->find_root (hive, &root);
  lua_pushinteger (state, root);
  return 1;
}

static int
reg_find_key (lua_State *state)
{
  HKEY root, key;
  grub_reg_hive_t *hive = NULL;
  const char *path8;
  uint16_t path16[MAX_NAME];

  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  hive = lua_touserdata (state, 1);
  root = luaL_checkinteger (state, 2);
  path8 = luaL_checkstring (state, 3);

  grub_memset (&path16, 0, MAX_NAME);
  grub_utf8_to_utf16 (path16, MAX_NAME * sizeof (uint16_t),
                      (grub_uint8_t *)path8, -1, NULL);

  hive->find_key (hive, root, path16, &key);
  lua_pushinteger (state, key);
  return 1;
}

static void
enum_reg (struct grub_reg_hive *hive, HKEY key)
{
  grub_err_t errno;
  HKEY subkey;
  uint32_t i, type, data_len;
  uint16_t wname[MAX_NAME];
  unsigned char name[MAX_NAME];
  uint8_t *data;
  for (i = 0; ; i++)
  {
    errno = hive->enum_keys (hive, key, i, wname, MAX_NAME);
    if (errno)
      break;
    subkey = 0;
    hive->find_key (hive, key, wname, &subkey);
    if (!subkey)
      break;
    grub_utf16_to_utf8 (name, wname, MAX_NAME);
    grub_printf ("%u [%s]\n", subkey, name);
    grub_refresh ();
    grub_getkey ();
    enum_reg (hive, subkey);
  }
  for (i = 0; ; i++)
  {
    errno = hive->enum_values (hive, key, i, wname, MAX_NAME, &type);
    if (errno)
      break;
    data_len = 0;
    errno = hive->query_value_no_copy (hive, key,
                                       wname, (void **)&data, &data_len, &type);
    for (i = 0; i < data_len; i++)
      grub_printf ("%02x ", data[i]);
    grub_printf ("len=%d type=0x%x\n", data_len, type);
    grub_getkey ();
  }
}

static int
reg_query_value (lua_State *state)
{
  HKEY root = 0;
  grub_reg_hive_t *hive = NULL;

  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  hive = lua_touserdata (state, 1);
  hive->find_root (hive, &root);
  enum_reg (hive, root);
  return 0;
}

static luaL_Reg reglib[] =
{
  {"open", reg_open},
  {"close", reg_close},
  {"find_root", reg_find_root},
  {"find_key", reg_find_key},
  {"query_value", reg_query_value},
  {0, 0}
};

GRUB_MOD_INIT(libreg)
{
  cmd = grub_register_command ("regtool", grub_cmd_reg, 0,
                               N_("Load Windows Registry."));
  if (grub_lua_global_state)
  {
    lua_gc (grub_lua_global_state, LUA_GCSTOP, 0);
    luaL_register (grub_lua_global_state, "reg", reglib);
    lua_gc (grub_lua_global_state, LUA_GCRESTART, 0);
  }
}

GRUB_MOD_FINI(libreg)
{
  grub_unregister_command (cmd);
}

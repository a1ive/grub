/*
 *  BURG - Brand-new Universal loadeR from GRUB
 *  Copyright 2009 Bean Lee - All Rights Reserved
 *
 *  BURG is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  BURG is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with BURG.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "grub_lib.h"

#include <grub/env.h>
#include <grub/lib.h>
#include <grub/parser.h>
#include <grub/command.h>
#include <grub/file.h>
#include <grub/menu.h>
#include <grub/device.h>
#include <grub/term.h>

static int
save_errno (lua_State *state)
{
  int saved_errno;

  saved_errno = grub_errno;
  grub_errno = 0;

  lua_pushinteger (state, saved_errno);
  lua_setfield (state, LUA_GLOBALSINDEX, "grub_errno");

  if (saved_errno)
    lua_pushstring (state, grub_errmsg);
  else
    lua_pushnil (state);

  lua_setfield (state, LUA_GLOBALSINDEX, "grub_errmsg");

  return saved_errno;
}

static int
push_result (lua_State *state)
{
  lua_pushinteger (state, save_errno (state));
  return 1;
}

static int
grub_lua_run (lua_State *state)
{
  int n;
  char **args;
  const char *s;

  s = luaL_checkstring (state, 1);
  if ((! grub_parser_split_cmdline (s, 0, 0, &n, &args))
      && (n > 0))
    {
      grub_command_t cmd;

      cmd = grub_command_find (args[0]);
      if (cmd)
	(cmd->func) (cmd, n - 1, &args[1]);
      else
	grub_error (GRUB_ERR_FILE_NOT_FOUND, "command not found");

      grub_free (args[0]);
      grub_free (args);
    }

  return push_result (state);
}

static int
grub_lua_getenv (lua_State *state)
{
  int n, i;

  n = lua_gettop (state);
  for (i = 1; i <= n; i++)
    {
      const char *name, *value;

      name = luaL_checkstring (state, i);
      value = grub_env_get (name);
      if (value)
	lua_pushstring (state, value);
      else
	lua_pushnil (state);
    }

  return n;
}

static int
grub_lua_setenv (lua_State *state)
{
  const char *name, *value;

  name = luaL_checkstring (state, 1);
  value = luaL_checkstring (state, 2);

  if (name[0])
    grub_env_set (name, value);

  return 0;
}

static int
enum_device (const char *name, void *closure)
{
  lua_State *state = closure;
  int result;
  grub_device_t dev;

  result = 0;
  dev = grub_device_open (name);
  if (dev)
    {
      grub_fs_t fs;

      fs = grub_fs_probe (dev);
      if (fs)
	{
	  lua_pushvalue (state, 1);
	  lua_pushstring (state, name);
	  lua_pushstring (state, fs->name);
	  if (! fs->uuid)
	    lua_pushnil (state);
	  else
	    {
	      int err;
	      char *uuid;

	      err = fs->uuid (dev, &uuid);
	      if (err)
		{
		  grub_errno = 0;
		  lua_pushnil (state);
		}
	      else
		{
		  lua_pushstring (state, uuid);
		  grub_free (uuid);
		}
	    }

	  lua_call (state, 3, 1);
	  result = lua_tointeger (state, -1);
	  lua_pop (state, 1);
	}
      else
	grub_errno = 0;
      grub_device_close (dev);
    }
  else
    grub_errno = 0;

  return result;
}

static int
grub_lua_enum_device (lua_State *state)
{
  luaL_checktype (state, 1, LUA_TFUNCTION);
  grub_device_iterate (enum_device, state);
  return push_result (state);
}

static int
enum_file (const char *name, const struct grub_dirhook_info *info,
	   void *closure)
{
  lua_State *state = closure;
  int result;

  lua_pushvalue (state, 1);
  lua_pushstring (state, name);
  lua_pushinteger (state, info->dir != 0);
  lua_call (state, 2, 1);
  result = lua_tointeger (state, -1);
  lua_pop (state, 1);

  return result;
}

static int
grub_lua_enum_file (lua_State *state)
{
  char *device_name;
  const char *arg;
  grub_device_t dev;

  luaL_checktype (state, 1, LUA_TFUNCTION);
  arg = luaL_checkstring (state, 2);
  device_name = grub_file_get_device_name (arg);
  dev = grub_device_open (device_name);
  if (dev)
    {
      grub_fs_t fs;
      const char *path;

      fs = grub_fs_probe (dev);
      path = grub_strchr (arg, ')');
      if (! path)
	path = arg;
      else
	path++;

      if (fs)
	{
	  (fs->dir) (dev, path, enum_file, state);
	}

      grub_device_close (dev);
    }

  grub_free (device_name);

  return push_result (state);
}

static int
grub_lua_file_open (lua_State *state)
{
  grub_file_t file;
  const char *name;
  const char *flag;

  name = luaL_checkstring (state, 1);
  flag = (lua_gettop (state) > 1) ? luaL_checkstring (state, 2) : 0;
  file = grub_file_open (name);
  save_errno (state);

  if (! file)
    return 0;

  if (grub_strchr (flag, 'w'))
    grub_blocklist_convert (file);

  lua_pushlightuserdata (state, file);
  return 1;
}

static int
grub_lua_file_close (lua_State *state)
{
  grub_file_t file;

  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  file = lua_touserdata (state, 1);
  grub_file_close (file);

  return push_result (state);
}

static int
grub_lua_file_seek (lua_State *state)
{
  grub_file_t file;
  grub_off_t offset;

  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  file = lua_touserdata (state, 1);
  offset = luaL_checkinteger (state, 2);

  offset = grub_file_seek (file, offset);
  save_errno (state);

  lua_pushinteger (state, offset);
  return 1;
}

static int
grub_lua_file_read (lua_State *state)
{
  grub_file_t file;
  luaL_Buffer b;
  int n;

  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  file = lua_touserdata (state, 1);
  n = luaL_checkinteger (state, 2);

  luaL_buffinit (state, &b);
  while (n)
    {
      char *p;
      int nr;

      nr = (n > LUAL_BUFFERSIZE) ? LUAL_BUFFERSIZE : n;
      p = luaL_prepbuffer (&b);

      nr = grub_file_read (file, p, nr);
      if (nr <= 0)
	break;

      luaL_addsize (&b, nr);
      n -= nr;
    }

  save_errno (state);
  luaL_pushresult (&b);
  return 1;
}

static int
grub_lua_file_write (lua_State *state)
{
  grub_file_t file;
  grub_ssize_t ret;
  grub_size_t len;
  const char *buf;

  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  file = lua_touserdata (state, 1);
  buf = lua_tolstring (state, 2, &len);
  ret = grub_blocklist_write (file, buf, len);
  if (ret > 0)
    file->offset += ret;

  save_errno (state);
  lua_pushinteger (state, ret);
  return 1;
}

static int
grub_lua_file_getline (lua_State *state)
{
  grub_file_t file;
  char *line;

  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  file = lua_touserdata (state, 1);

  line = grub_getline (file);
  save_errno (state);

  if (! line)
    return 0;

  lua_pushstring (state, line);
  grub_free (line);
  return 1;
}

static int
grub_lua_file_getsize (lua_State *state)
{
  grub_file_t file;

  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  file = lua_touserdata (state, 1);

  lua_pushinteger (state, file->size);
  return 1;
}

static int
grub_lua_file_getpos (lua_State *state)
{
  grub_file_t file;

  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  file = lua_touserdata (state, 1);

  lua_pushinteger (state, file->offset);
  return 1;
}

static int
grub_lua_file_eof (lua_State *state)
{
  grub_file_t file;

  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  file = lua_touserdata (state, 1);

  lua_pushboolean (state, file->offset >= file->size);
  return 1;
}

static int
grub_lua_file_exist (lua_State *state)
{
  grub_file_t file;
  const char *name;
  int result;

  result = 0;
  name = luaL_checkstring (state, 1);
  file = grub_file_open (name);
  if (file)
    {
      result++;
      grub_file_close (file);
    }
  else
    grub_errno = 0;

  lua_pushboolean (state, result);
  return 1;
}

static int
grub_lua_add_menu (lua_State *state)
{
  int n;
  const char *source;

  source = luaL_checklstring (state, 1, 0);
  n = lua_gettop (state) - 1;
  if (n > 0)
    {
      const char *args[sizeof (char *) * n];
      char *p;
      int i;

      for (i = 0; i < n; i++)
	args[i] = luaL_checkstring (state, 2 + i);

      p = grub_strdup (source);
      if (! p)
	return push_result (state);

      grub_menu_entry_add (n, args, p);
    }
  else
    {
      lua_pushstring (state, "not enough parameter");
      lua_error (state);
    }

  return push_result (state);
}

static int
grub_lua_read_byte (lua_State *state)
{
  grub_addr_t addr;

  addr = luaL_checkinteger (state, 1);
  lua_pushinteger (state, *((grub_uint8_t *) addr));
  return 1;
}

static int
grub_lua_read_word (lua_State *state)
{
  grub_addr_t addr;

  addr = luaL_checkinteger (state, 1);
  lua_pushinteger (state, *((grub_uint16_t *) addr));
  return 1;
}

static int
grub_lua_read_dword (lua_State *state)
{
  grub_addr_t addr;

  addr = luaL_checkinteger (state, 1);
  lua_pushinteger (state, *((grub_uint32_t *) addr));
  return 1;
}

static int
grub_lua_write_byte (lua_State *state)
{
  grub_addr_t addr;

  addr = luaL_checkinteger (state, 1);
  *((grub_uint8_t *) addr) = luaL_checkinteger (state, 2);
  return 1;
}

static int
grub_lua_write_word (lua_State *state)
{
  grub_addr_t addr;

  addr = luaL_checkinteger (state, 1);
  *((grub_uint16_t *) addr) = luaL_checkinteger (state, 2);
  return 1;
}

static int
grub_lua_write_dword (lua_State *state)
{
  grub_addr_t addr;

  addr = luaL_checkinteger (state, 1);
  *((grub_uint32_t *) addr) = luaL_checkinteger (state, 2);
  return 1;
}

static int
grub_lua_getkey (lua_State *state)
{
  lua_pushinteger (state, grub_getkey ());
  return 1;
}

static int
grub_lua_checkkey (lua_State *state)
{
  lua_pushboolean (state, grub_checkkey () >= 0);
  return 1;
}

static int
grub_lua_getkeystatus (lua_State *state)
{
  lua_pushinteger (state, grub_getkeystatus ());
  return 1;
}

static int
grub_lua_cls (lua_State *state __attribute__ ((unused)))
{
  grub_cls ();
  return 0;
}

static int
grub_lua_setcolorstate (lua_State *state)
{
  grub_setcolorstate (luaL_checkinteger (state, 1));
  return 0;
}

static int
grub_lua_refresh (lua_State *state __attribute__ ((unused)))
{
  grub_refresh ();
  return 0;
}

static int
grub_lua_name2key (lua_State *state)
{
  const char *s;

  s = luaL_checkstring (state, 1);
  lua_pushinteger (state, grub_menu_name2key (s));
  return 1;
}

static int
grub_lua_key2name (lua_State *state)
{
  int key;

  key = luaL_checkinteger (state, 1);
  lua_pushstring (state, grub_menu_key2name (GRUB_TERM_ASCII_CHAR (key)));
  return 1;
}

static int
grub_lua_enum_term (lua_State *state)
{
  struct grub_term_output *term;

  luaL_checktype (state, 1, LUA_TFUNCTION);
  FOR_ACTIVE_TERM_OUTPUTS(term)
  {
    lua_pushvalue (state, 1);
    lua_pushlightuserdata (state, term);
    lua_pushstring (state, term->name);
    lua_call (state, 2, 0);
    lua_pop (state, 1);
  }

  return 0;
}

static int
grub_lua_term_width (lua_State *state)
{
  struct grub_term_output *term;

  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  term = lua_touserdata (state, 1);
  lua_pushinteger (state, grub_term_width (term));
  return 1;
}

static int
grub_lua_term_height (lua_State *state)
{
  struct grub_term_output *term;

  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  term = lua_touserdata (state, 1);
  lua_pushinteger (state, grub_term_height (term));
  return 1;
}

static int
grub_lua_term_getxy (lua_State *state)
{
  struct grub_term_output *term;
  grub_uint16_t xy;

  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  term = lua_touserdata (state, 1);

  xy = grub_term_getxy (term);
  lua_pushinteger (state, xy >> 8);
  lua_pushinteger (state, xy & 0xff);
  return 2;
}

static int
grub_lua_term_gotoxy (lua_State *state)
{
  struct grub_term_output *term;

  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  term = lua_touserdata (state, 1);
  grub_term_gotoxy (term, luaL_checkinteger (state, 2),
		    luaL_checkinteger (state, 3));
  return 0;
}

static int
grub_lua_term_setcolorstate (lua_State *state)
{
  struct grub_term_output *term;

  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  term = lua_touserdata (state, 1);
  grub_term_setcolorstate (term, luaL_checkinteger (state, 2));
  return 0;
}

static int
grub_lua_term_setcolor (lua_State *state)
{
  struct grub_term_output *term;

  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  term = lua_touserdata (state, 1);
  grub_term_setcolor (term, luaL_checkinteger (state, 2),
		      luaL_checkinteger (state, 3));
  return 0;
}

static int
grub_lua_term_setcursor (lua_State *state)
{
  struct grub_term_output *term;

  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  term = lua_touserdata (state, 1);
  grub_term_setcursor (term, lua_toboolean (state, 2));
  return 0;
}

static int
grub_lua_term_getcolor (lua_State *state)
{
  struct grub_term_output *term;
  grub_uint8_t normal_color, highlight_color;

  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  term = lua_touserdata (state, 1);
  grub_term_getcolor (term, &normal_color, &highlight_color);
  lua_pushinteger (state, normal_color);
  lua_pushinteger (state, highlight_color);
  return 2;
}

luaL_Reg grub_lua_lib[] =
  {
    {"run", grub_lua_run},
    {"getenv", grub_lua_getenv},
    {"setenv", grub_lua_setenv},
    {"enum_device", grub_lua_enum_device},
    {"enum_file", grub_lua_enum_file},
    {"file_open", grub_lua_file_open},
    {"file_close", grub_lua_file_close},
    {"file_seek", grub_lua_file_seek},
    {"file_read", grub_lua_file_read},
    {"file_write", grub_lua_file_write},
    {"file_getline", grub_lua_file_getline},
    {"file_getsize", grub_lua_file_getsize},
    {"file_getpos", grub_lua_file_getpos},
    {"file_eof", grub_lua_file_eof},
    {"file_exist", grub_lua_file_exist},
    {"add_menu", grub_lua_add_menu},
    {"read_byte", grub_lua_read_byte},
    {"read_word", grub_lua_read_word},
    {"read_dword", grub_lua_read_dword},
    {"write_byte", grub_lua_write_byte},
    {"write_word", grub_lua_write_word},
    {"write_dword", grub_lua_write_dword},
    {"getkey", grub_lua_getkey},
    {"checkkey", grub_lua_checkkey},
    {"getkeystatus", grub_lua_getkeystatus},
    {"cls", grub_lua_cls},
    {"setcolorstate", grub_lua_setcolorstate},
    {"refresh", grub_lua_refresh},
    {"name2key", grub_lua_name2key},
    {"key2name", grub_lua_key2name},
    {"enum_term", grub_lua_enum_term},
    {"term_width", grub_lua_term_width},
    {"term_height", grub_lua_term_height},
    {"term_getxy", grub_lua_term_getxy},
    {"term_gotoxy", grub_lua_term_gotoxy},
    {"term_setcolorstate", grub_lua_term_setcolorstate},
    {"term_setcolor", grub_lua_term_setcolor},
    {"term_setcursor", grub_lua_term_setcursor},
    {"term_getcolor", grub_lua_term_getcolor},
    {0, 0}
  };

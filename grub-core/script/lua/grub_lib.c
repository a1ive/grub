/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2009,2018  Free Software Foundation, Inc.
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
#include "gbk.h"

#include <grub/dl.h>
#include <grub/env.h>
#include <grub/parser.h>
#include <grub/command.h>
#include <grub/normal.h>
#include <grub/term.h>
#include <grub/file.h>
#include <grub/menu.h>
#include <grub/misc.h>
#include <grub/device.h>
#include <grub/i18n.h>
#include <grub/lib/crc.h>
#include <grub/lib/hexdump.h>
#include <grub/video.h>
#include <grub/bitmap.h>
#include <grub/time.h>
#include <grub/gfxmenu_view.h>

#ifdef ENABLE_LUA_PCI
#include <grub/pci.h>
#endif

#define UTF_MAX     8
#define DBCS_MAX    2

/* Updates the globals grub_errno and grub_msg, leaving their values on the 
   top of the stack, and clears grub_errno. When grub_errno is zero, grub_msg
   is not left on the stack. The value returned is the number of values left on
   the stack. */
static int
push_result (lua_State *state)
{
  int saved_errno;
  int num_results;

  saved_errno = grub_errno;
  grub_errno = 0;

  /* Push once for setfield, and again to leave on the stack */
  lua_pushinteger (state, saved_errno);
  lua_pushinteger (state, saved_errno);
  lua_setfield (state, LUA_GLOBALSINDEX, "grub_errno");

  if (saved_errno)
  {
    /* Push once for setfield, and again to leave on the stack */
    lua_pushstring (state, grub_errmsg);
    lua_pushstring (state, grub_errmsg);
    num_results = 2;
  }
  else
  {
    lua_pushnil (state);
    num_results = 1;
  }

  lua_setfield (state, LUA_GLOBALSINDEX, "grub_errmsg");

  return num_results;
}

/* Updates the globals grub_errno and grub_msg ( without leaving them on the
   stack ), clears grub_errno,  and returns the value of grub_errno before it
   was cleared. */
static int
save_errno (lua_State *state)
{
  int saved_errno;

  saved_errno = grub_errno;
  lua_pop(state, push_result(state));

  return saved_errno;
}

static unsigned from_utf8(unsigned uni_code) {
    const unsigned short *page = from_uni[(uni_code >> 8) & 0xFF];
    return page == NULL ? DBCS_DEFAULT_CODE : page[uni_code & 0xFF];
}

static unsigned to_utf8(unsigned cp_code) {
    const unsigned short *page = to_uni[(cp_code >> 8) & 0xFF];
    return page == NULL ? UNI_INVALID_CODE : page[cp_code & 0xFF];
}

static size_t utf8_encode(char *s, unsigned ch) {
    if (ch < 0x80) {
        s[0] = (char)ch;
        return 1;
    }
    if (ch <= 0x7FF) {
        s[1] = (char) ((ch | 0x80) & 0xBF);
        s[0] = (char) ((ch >> 6) | 0xC0);
        return 2;
    }
    if (ch <= 0xFFFF) {
three:
        s[2] = (char) ((ch | 0x80) & 0xBF);
        s[1] = (char) (((ch >> 6) | 0x80) & 0xBF);
        s[0] = (char) ((ch >> 12) | 0xE0);
        return 3;
    }
    if (ch <= 0x1FFFFF) {
        s[3] = (char) ((ch | 0x80) & 0xBF);
        s[2] = (char) (((ch >> 6) | 0x80) & 0xBF);
        s[1] = (char) (((ch >> 12) | 0x80) & 0xBF);
        s[0] = (char) ((ch >> 18) | 0xF0);
        return 4;
    }
    if (ch <= 0x3FFFFFF) {
        s[4] = (char) ((ch | 0x80) & 0xBF);
        s[3] = (char) (((ch >> 6) | 0x80) & 0xBF);
        s[2] = (char) (((ch >> 12) | 0x80) & 0xBF);
        s[1] = (char) (((ch >> 18) | 0x80) & 0xBF);
        s[0] = (char) ((ch >> 24) | 0xF8);
        return 5;
    }
    if (ch <= 0x7FFFFFFF) {
        s[5] = (char) ((ch | 0x80) & 0xBF);
        s[4] = (char) (((ch >> 6) | 0x80) & 0xBF);
        s[3] = (char) (((ch >> 12) | 0x80) & 0xBF);
        s[2] = (char) (((ch >> 18) | 0x80) & 0xBF);
        s[1] = (char) (((ch >> 24) | 0x80) & 0xBF);
        s[0] = (char) ((ch >> 30) | 0xFC);
        return 6;
    }

    /* fallback */
    ch = 0xFFFD;
    goto three;
}

static size_t utf8_decode(const char *s, const char *e, unsigned *pch) {
    unsigned ch;

    if (s >= e) {
        *pch = 0;
        return 0;   
    }

    ch = (unsigned char)s[0];
    if (ch < 0xC0) goto fallback;
    if (ch < 0xE0) {
        if (s+1 >= e || (s[1] & 0xC0) != 0x80)
            goto fallback;
        *pch = ((ch   & 0x1F) << 6) | (s[1] & 0x3F);
        return 2;
    }
    if (ch < 0xF0) {
        if (s+2 >= e || (s[1] & 0xC0) != 0x80
                || (s[2] & 0xC0) != 0x80)
            goto fallback;
        *pch = ((ch   & 0x0F) << 12) | ((s[1] & 0x3F) <<  6) | (s[2] & 0x3F);
        return 3;
    }
    {
        int count = 0; /* to count number of continuation bytes */
        unsigned res = 0;
        while ((ch & 0x40) != 0) { /* still have continuation bytes? */
            int cc = (unsigned char)s[++count];
            if ((cc & 0xC0) != 0x80) /* not a continuation byte? */
                goto fallback; /* invalid byte sequence, fallback */
            res = (res << 6) | (cc & 0x3F); /* add lower 6 bits from cont. byte */
            ch <<= 1; /* to test next bit */
        }
        if (count > 5)
            goto fallback; /* invalid byte sequence */
        res |= ((ch & 0x7F) << (count * 5)); /* add first byte */
        *pch = res;
        return count+1;
    }

fallback:
    *pch = ch;
    return 1;
}

static void add_utf8char(luaL_Buffer *b, unsigned ch) {
    char buff[UTF_MAX];
    size_t n = utf8_encode(buff, ch);
    luaL_addlstring(b, buff, n);
}

static size_t dbcs_decode(const char *s, const char *e, unsigned *pch) {
    unsigned ch;
    if (s >= e) {
        *pch = 0;
        return 0;
    }

    ch = s[0] & 0xFF;
    if (to_uni_00[ch] != UNI_INVALID_CODE) {
        *pch = ch;
        return 1;
    }

    *pch = (ch << 8) | (s[1] & 0xFF);
    return 2;
}

static void add_dbcschar(luaL_Buffer *b, unsigned ch) {
    if (ch < 0x7F)
        luaL_addchar(b, ch);
    else {
        luaL_addchar(b, (ch >> 8) & 0xFF);
        luaL_addchar(b, ch & 0xFF);
    }
}

static size_t dbcs_length(const char *s, const char *e) {
    size_t dbcslen = 0;
    while (s < e) {
        if ((unsigned char)(*s++) > 0x7F)
            ++s;
        ++dbcslen;
    }
    return dbcslen;
}


/* dbcs module interface */

static const char *check_dbcs(lua_State *L, int idx, const char **pe) {
    size_t len;
    const char *s = luaL_checklstring(L, idx, &len);
    if (pe != NULL) *pe = s + len;
    return s;
}

static int posrelat(int pos, size_t len) {
    if (pos >= 0) return (size_t)pos;
    else if (0u - (size_t)pos > len) return 0;
    else return len - ((size_t)-pos) + 1;
}

static int string_from_utf8(lua_State *L) {
    const char *e, *s = check_dbcs(L, 1, &e);
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    while (s < e) {
        unsigned ch;
        s += utf8_decode(s, e, &ch);
        add_dbcschar(&b, from_utf8(ch));
    }
    luaL_pushresult(&b);
    return 1;
}

static int string_to_utf8(lua_State *L) {
    const char *e, *s = check_dbcs(L, 1, &e);
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    while (s < e) {
        unsigned ch;
        s += dbcs_decode(s, e, &ch);
        add_utf8char(&b, to_utf8(ch));
    }
    luaL_pushresult(&b);
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
      && (n >= 0))
    {
      grub_command_t cmd;

      cmd = grub_command_find (args[0]);
      if (cmd)
	(cmd->func) (cmd, n-1, &args[1]);
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
grub_lua_exportenv (lua_State *state)
{
  const char *name, *value;

  name = luaL_checkstring (state, 1);
  value = luaL_checkstring (state, 2);

  if (name[0])
    {
    grub_env_export (name);
    if (value[0])
      grub_env_set (name, value);
    }

  return 0;
}
/* Helper for grub_lua_enum_device.  */
static int
grub_lua_enum_device_iter (const char *name, void *data)
{
  lua_State *state = data;
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

	  if (! fs->label)
	    lua_pushnil (state);
	  else
	    {
	      int err;
	      char *label = NULL;

	      err = fs->label (dev, &label);
	      if (err)
		{
		  grub_errno = 0;
		  lua_pushnil (state);
		}
	      else
		{
		  if (label == NULL)
		    {
		      lua_pushnil (state);
		    }
		  else
		    {
		      lua_pushstring (state, label);
		    }
		  grub_free (label);
		}
	    }

	  lua_call (state, 4, 1);
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
  grub_device_iterate (grub_lua_enum_device_iter, state);
  return push_result (state);
}

static int
enum_file (const char *name, const struct grub_dirhook_info *info,
	   void *data)
{
  int result;
  lua_State *state = data;

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

#ifdef ENABLE_LUA_PCI
/* Helper for grub_lua_enum_pci.  */
static int
grub_lua_enum_pci_iter (grub_pci_device_t dev, grub_pci_id_t pciid, void *data)
{
  lua_State *state = data;
  int result;
  grub_pci_address_t addr;
  grub_uint32_t class;

  lua_pushvalue (state, 1);
  lua_pushinteger (state, grub_pci_get_bus (dev));
  lua_pushinteger (state, grub_pci_get_device (dev));
  lua_pushinteger (state, grub_pci_get_function (dev));
  lua_pushinteger (state, pciid);

  addr = grub_pci_make_address (dev, GRUB_PCI_REG_CLASS);
  class = grub_pci_read (addr);
  lua_pushinteger (state, class);

  lua_call (state, 5, 1);
  result = lua_tointeger (state, -1);
  lua_pop (state, 1);

  return result;
}

static int
grub_lua_enum_pci (lua_State *state)
{
  luaL_checktype (state, 1, LUA_TFUNCTION);
  grub_pci_iterate (grub_lua_enum_pci_iter, state);
  return push_result (state);
}
#endif

static int
grub_lua_file_open (lua_State *state)
{
  grub_file_t file;
  const char *name;

  name = luaL_checkstring (state, 1);
  file = grub_file_open (name, GRUB_FILE_TYPE_NO_DECOMPRESS);
  save_errno (state);

  if (! file)
    return 0;

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
grub_lua_file_getline (lua_State *state)
{
  grub_file_t file;
  char *line;

  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  file = lua_touserdata (state, 1);

  line = grub_file_getline (file);
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
  file = grub_file_open (name, GRUB_FILE_TYPE_FS_SEARCH);
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
grub_lua_file_crc32 (lua_State *state)
{
  grub_file_t file;
  const char *name;
  int crc;
  char crcstr[10];
  char buf[GRUB_DISK_SECTOR_SIZE];
  grub_ssize_t size;
  name = luaL_checkstring (state, 1);
  file = grub_file_open (name, GRUB_FILE_TYPE_TO_HASH);
  if (file)
    {
      crc = 0;
      while ((size = grub_file_read (file, buf, sizeof (buf))) > 0)
        crc = grub_getcrc32c (crc, buf, size);
      grub_snprintf (crcstr, 10, "%08x", crc);
      lua_pushstring (state, crcstr);
    }
  return 1;
}

//file, skip, length
static int
grub_lua_hexdump (lua_State *state)
{
  grub_file_t file;
  char buf[GRUB_DISK_SECTOR_SIZE * 4];
  grub_ssize_t size, length;
  grub_disk_addr_t skip;
  grub_size_t var_len;
  char *var_buf = NULL;
  char *var_hex = NULL;
  char *p = NULL;
  char *s = NULL;

  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  file = lua_touserdata (state, 1);
  skip = luaL_checkinteger (state, 2);
  length = luaL_checkinteger (state, 3);

  var_len = length + 1;
  var_buf = (char *) grub_malloc (var_len);
  var_hex = (char *) grub_malloc (3 * var_len);
  if (var_buf)
    p = var_buf;
  if (var_hex)
    s = var_hex;
  if (file)
    {
      file->offset = skip;
      while ((size = grub_file_read (file, buf, sizeof (buf))) > 0)
	{
	  unsigned long len;
	  len = ((length) && (size > length)) ? length : size;
          grub_memcpy (p, buf, len);
          p += len;
	  skip += len;
	  if (length)
	    {
	      length -= len;
	      if (!length)
		break;
	    }
	}
      grub_size_t i;
      *p = 0;
      *s = 0;
      for (i = 0; i < var_len - 1; i++)
             {
	     var_hex = grub_xasprintf ("%s %02x", var_hex, (unsigned char) var_buf[i]);
	     var_buf[i] = ((var_buf[i] >= 32) && (var_buf[i] < 127)) ? var_buf[i] : '.';
             }
      lua_pushstring (state, var_buf);
      lua_pushstring (state, var_hex);
      return 2;
    }
  return 0;
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
      const char **args;
      char *p;
      int i;

      args = grub_malloc (n * sizeof (args[0]));
      if (!args)
	return push_result (state);
      for (i = 0; i < n; i++)
	args[i] = luaL_checkstring (state, 2 + i);

      p = grub_strdup (source);
      if (! p)
	return push_result (state);

      grub_normal_add_menu_entry (n, args, NULL, NULL, NULL, NULL, NULL, p, 0, 0);
    }
  else
    {
      lua_pushstring (state, "not enough parameter");
      lua_error (state);
    }

  return push_result (state);
}

static int
grub_lua_clear_menu (lua_State *state __attribute__ ((unused)))
{
  grub_menu_t menu = grub_env_get_menu();

  menu->entry_list = NULL;
  menu->size=0;
  return 0;
}

static int
grub_lua_add_icon_menu (lua_State *state)
{
  int n;
  const char *source;
  source = luaL_checklstring (state, 2, 0);
  n = lua_gettop (state) - 2;
  if (n > 0)
    {
      const char **args;
      char *p;
      int i;
      char **class = NULL;
      class = grub_malloc (sizeof (class[0]));
      class[0] = grub_strdup (luaL_checklstring (state, 1, 0));
      class[1] = NULL;
      args = grub_malloc (n * sizeof (args[0]));
      if (!args)
	return push_result (state);
      for (i = 0; i < n; i++)
	args[i] = luaL_checkstring (state, 3 + i);

      p = grub_strdup (source);
      if (! p)
	return push_result (state);
      grub_normal_add_menu_entry (n, args, class, NULL, NULL, NULL, NULL, p, 0, 0);
    }
  else
    {
      lua_pushstring (state, "not enough parameter");
      lua_error (state);
    }
  return push_result (state);
}

static int
grub_lua_add_hidden_menu (lua_State *state)
{
  int n;
  const char *source;
  source = luaL_checklstring (state, 2, 0);
  n = lua_gettop (state) - 2;
  if (n > 0)
    {
      const char **args;
      char *p;
      int i;
      const char *hotkey;
      args = grub_malloc (n * sizeof (args[0]));
      if (!args)
	return push_result (state);
      for (i = 0; i < n; i++)
	args[i] = luaL_checkstring (state, 3 + i);

      p = grub_strdup (source);
      if (! p)
	return push_result (state);
	  hotkey = grub_strdup (luaL_checklstring (state, 1, 0));
      grub_normal_add_menu_entry (n, args, NULL, NULL, NULL, hotkey, NULL, p, 0, 1);
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
grub_lua_read (lua_State *state __attribute__ ((unused)))
{
  int i;
  char *line;
  char *tmp;
  char c;
  i = 0;
  line = grub_malloc (1 + i + sizeof('\0'));
  if (! line)
    return 0;

  while (1)
    {
      c = grub_getkey ();
      if ((c == '\n') || (c == '\r'))
	break;

      line[i] = c;
      if (grub_isprint (c))
	grub_printf ("%c", c);
      i++;
      tmp = grub_realloc (line, 1 + i + sizeof('\0'));
      if (! tmp)
	{
	  grub_free (line);
	  return 0;
	}
      line = tmp;
    }
  line[i] = '\0';

  lua_pushstring (state, line);
  return 1;
}

static int
grub_lua_gettext (lua_State *state)
{
  const char *translation;
  translation = luaL_checkstring (state, 1);
  lua_pushstring (state, grub_gettext (translation));
  return 1;
}

luaL_Reg grub_lua_lib[] =
  {
    {"run", grub_lua_run},
    {"getenv", grub_lua_getenv},
    {"setenv", grub_lua_setenv},
    {"exportenv", grub_lua_exportenv},
    {"enum_device", grub_lua_enum_device},
    {"enum_file", grub_lua_enum_file},
#ifdef ENABLE_LUA_PCI
    {"enum_pci", grub_lua_enum_pci},
#endif
    {"file_open", grub_lua_file_open},
    {"file_close", grub_lua_file_close},
    {"file_seek", grub_lua_file_seek},
    {"file_read", grub_lua_file_read},
    {"file_getline", grub_lua_file_getline},
    {"file_getsize", grub_lua_file_getsize},
    {"file_getpos", grub_lua_file_getpos},
    {"file_eof", grub_lua_file_eof},
    {"file_exist", grub_lua_file_exist},
    {"file_crc32", grub_lua_file_crc32},
    {"hexdump", grub_lua_hexdump},
    {"add_menu", grub_lua_add_menu},
    {"add_icon_menu", grub_lua_add_icon_menu},
    {"add_hidden_menu", grub_lua_add_hidden_menu},
    {"clear_menu", grub_lua_clear_menu},
    {"read_byte", grub_lua_read_byte},
    {"read_word", grub_lua_read_word},
    {"read_dword", grub_lua_read_dword},
    {"write_byte", grub_lua_write_byte},
    {"write_word", grub_lua_write_word},
    {"write_dword", grub_lua_write_dword},
    {"cls", grub_lua_cls},
    {"setcolorstate", grub_lua_setcolorstate},
    {"refresh", grub_lua_refresh},
    {"read", grub_lua_read},
    {"gettext", grub_lua_gettext},
    {0, 0}
  };

/* Lua function: get_time_ms() : returns the time in milliseconds.  */
static int
lua_sys_get_time_ms (lua_State *state)
{
  lua_pushinteger (state, grub_get_time_ms ());
  return 1;
}

static int
lua_sys_random (lua_State *state)
{
  uint16_t r = grub_get_time_ms ();
  uint16_t m;
  m = luaL_checkinteger (state, 1);
  r = ((r * 7621) + 1) % 32768;
  lua_pushinteger (state, r % m);    
  return 1;
}

static int
lua_input_read (lua_State *state)
{
  char *line = grub_getline ();
  if (! line)
    lua_pushnil(state);
  else
    lua_pushstring (state, line);

  grub_free (line);
  return 1;
}

/* Lua function: input.getkey() : returns { ASCII char, scan code }.  */
static int
lua_input_getkey (lua_State *state)
{
  int c = grub_getkey();
  lua_pushinteger (state, c & 0xFF);          /* Push ASCII character code.  */
  lua_pushinteger (state, (c >> 8) & 0xFF);   /* Push the scan code.  */
  return 2;
}
static int
lua_input_getkey_noblock (lua_State *state __attribute__ ((unused)))
{
  int c = grub_getkey_noblock ();
  lua_pushinteger (state, c & 0xFF);          /* Push ASCII character code.  */
  lua_pushinteger (state, (c >> 8) & 0xFF);   /* Push the scan code.  */
  return 2;
}

/* Lua function: video.swap_buffers().  */
static int
lua_video_swap_buffers (lua_State *state)
{
  if (grub_video_swap_buffers () != GRUB_ERR_NONE)
    return luaL_error (state, "Error swapping video buffers: %s", grub_errmsg);
  return 0;
}

static grub_video_color_t
check_grub_color (lua_State *state, int narg)
{
  /* Get the color components.  */
  luaL_argcheck (state, lua_istable (state, narg), narg, "should be a color");
  lua_getfield (state, narg, "r");
  lua_getfield (state, narg, "g");
  lua_getfield (state, narg, "b");
  lua_getfield (state, narg, "a");
  grub_video_color_t color;
  color = grub_video_map_rgba (luaL_checkint (state, -4),
                               luaL_checkint (state, -3),
                               luaL_checkint (state, -2),
                               luaL_optint (state, -1, 255));
  lua_pop (state, 4);
  return color;
}

/* Lua function: video.fill_rect(color, x, y, width, height). */
static int
lua_video_fill_rect (lua_State *state)
{
  grub_video_color_t color = check_grub_color (state, 1);
  int x = luaL_checkint (state, 2);
  int y = luaL_checkint (state, 3);
  int w = luaL_checkint (state, 4);
  int h = luaL_checkint (state, 5);
  if (grub_video_fill_rect (color, x, y, w, h) != GRUB_ERR_NONE)
    return luaL_error (state, "Error filling rectangle: %s", grub_errmsg);
  return 0;
}

/* Lua function: video.draw_string(text, font, color, x, y). */
static int
lua_video_draw_string (lua_State *state)
{
  grub_font_draw_string (luaL_checkstring (state, 1),
                          grub_font_get (luaL_checkstring (state, 2)),
                          check_grub_color (state, 3),
                          luaL_checkint (state, 4),
                          luaL_checkint (state, 5));
  return 0;
}

struct hook_ctx
{
  unsigned int len;
  char* data;
};

static int
hook (const struct grub_video_mode_info *info, void *hook_arg)
{
  unsigned int len;
  struct hook_ctx *ctx = hook_arg;
  char buf[24];
  if (info->mode_type & GRUB_VIDEO_MODE_TYPE_PURE_TEXT)
    return 0;
  grub_snprintf (buf, sizeof(buf), "%dx%dx%d ", info->width, info->height, info->bpp);
  len = grub_strlen(buf);
  if (ctx->data)
    {
      grub_strcpy(ctx->data + ctx->len, buf);
    }
  ctx->len += len;
  return 0;
}

/* Lua function: video.info(). */
static int
lua_video_info (lua_State *state __attribute__ ((unused)))
{
  grub_video_adapter_t adapter;
  grub_video_driver_id_t id;
  struct hook_ctx ctx;
#ifdef GRUB_MACHINE_PCBIOS
  grub_dl_load ("vbe");
#endif
  id = grub_video_get_driver_id ();
  FOR_VIDEO_ADAPTERS (adapter)
  {
    if (! adapter->iterate || (adapter->id != id && (id != GRUB_VIDEO_DRIVER_NONE || adapter->init() != GRUB_ERR_NONE)))
      {
        continue;
      }
    ctx.data = NULL;
    ctx.len = 0;
    adapter->iterate (hook, &ctx);
    ctx.data = grub_malloc(ctx.len+1);
    ctx.data[0] = '\0'; ctx.len = 0;
    adapter->iterate (hook, &ctx);
    if (adapter->id != id)
      adapter->fini();
    if (id != GRUB_VIDEO_DRIVER_NONE || ctx.len)
      {
        lua_pushstring (state, ctx.data);
        grub_free(ctx.data);
	break;
      }
    else
      grub_free(ctx.data);
  }
  return 1;
}

static int lua_gbk_len (lua_State *state) {
    const char *e, *s = check_dbcs(state, 1, &e);
    lua_pushinteger(state, dbcs_length(s, e));
    return 1;
}

static int lua_gbk_byte(lua_State *state) {
    const char *e, *s = check_dbcs(state, 1, &e);
    size_t len = dbcs_length(s, e);
    int posi = posrelat((int)luaL_optinteger(state, 2, 1), len);
    int pose = posrelat((int)luaL_optinteger(state, 3, posi), len);
    const char *start = s;
    int i, n;
    if (posi < 1) posi = 1;
    if (pose > (int)len) pose = len;
    n = (int)(pose - posi + 1);
    if (posi + n <= pose) /* (size_t -> int) overflow? */
        return luaL_error(state, "string slice too long");
    luaL_checkstack(state, n, "string slice too long");
    for (i = 0; i < posi; ++i) {
        unsigned ch;
        start += dbcs_decode(start, e, &ch);
    }
    for (i = 0; i < n; ++i) {
        unsigned ch;
        start += dbcs_decode(start, e, &ch);
        lua_pushinteger(state, ch);
    }
    return n;
}

static int lua_gbk_char(lua_State *state) {
    luaL_Buffer b;
    int i, top = lua_gettop(state);
    luaL_buffinit(state, &b);
    for (i = 1; i <= top; ++i)
        add_dbcschar(&b, (unsigned)luaL_checkinteger(state, i));
    luaL_pushresult(&b);
    return 1;
}

static int lua_gbk_fromutf8(lua_State *state) {
    int i, top;
    switch (lua_type(state, 1)) {
    case LUA_TSTRING:
        return string_from_utf8(state);
    case LUA_TNUMBER:
        top = lua_gettop(state);
        for (i = 1; i <= top; ++i) {
            unsigned code = (unsigned)luaL_checkinteger(state, i);
            lua_pushinteger(state, (lua_Integer)from_utf8(code));
            lua_replace(state, i);
        }
        return top;
    }
    return luaL_error(state, "string/number expected, got %s",
            luaL_typename(state, 1));
}

static int lua_gbk_toutf8(lua_State *state) {
    int i, top;
    switch (lua_type(state, 1)) {
    case LUA_TSTRING:
        return string_to_utf8(state);
    case LUA_TNUMBER:
        top = lua_gettop(state);
        for (i = 1; i <= top; ++i) {
            unsigned code = (unsigned)luaL_checkinteger(state, i);
            lua_pushinteger(state, to_utf8(code));
            lua_replace(state, i);
        }
        return top;
    }
    return luaL_error(state, "string/number expected, got %s",
            luaL_typename(state, 1));
}

luaL_Reg syslib[] = {
    {"get_time_ms", lua_sys_get_time_ms},
    {"random", lua_sys_random},
    {0, 0}
};

luaL_Reg inputlib[] = {
    {"getkey", lua_input_getkey},
    {"getkey_noblock", lua_input_getkey_noblock},
    {"read", lua_input_read},
    {0, 0}
};

luaL_Reg videolib[] = {
    {"swap_buffers", lua_video_swap_buffers},
    {"fill_rect", lua_video_fill_rect},
    {"draw_string", lua_video_draw_string},
    {"info", lua_video_info},
    {0, 0}
};

luaL_Reg gbklib[] = {
    {"len", lua_gbk_len},
    {"byte", lua_gbk_byte},
    {"char", lua_gbk_char},
    {"fromutf8", lua_gbk_fromutf8},
    {"toutf8", lua_gbk_toutf8},
    {0, 0}
};

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

#include <grub/dl.h>
#include <grub/misc.h>
#include <grub/disk.h>
#include <grub/file.h>
#include <grub/partition.h>
#include <grub/datetime.h>
#include <ff.h>
#include <diskio.h>

static FATFS fatfs_list[10];

/* fat.mount hdx,y disknum */
static int
fat_mount (lua_State *state)
{
  int num = 0;
  char dev[3] = "1:";
  grub_disk_t disk = 0;
  const char *name = NULL;

  name = luaL_checkstring (state, 1);
  num = luaL_checkinteger (state, 2);
  if (num > 9 || num <= 0)
    return 0;
  disk = grub_disk_open (name);
  if (!disk)
    return 0;
  if (fat_stat[num].disk)
  {
    grub_disk_close (disk);
    printf ("disk number in use\n");
    return 0;
  }
  fat_stat[num].present = 1;
  grub_snprintf (fat_stat[num].name, 2, "%d", num);
  fat_stat[num].disk = disk;
  fat_stat[num].total_sectors = disk->total_sectors;
  /* f_mount */
  grub_snprintf (dev, 3, "%d:", num);
  f_mount (&fatfs_list[num], dev, 0);
  return 0;
}

/* fat.umount disknum */
static int
fat_umount (lua_State *state)
{
  int num = 0;
  char dev[3] = "1:";
  num = luaL_checkinteger (state, 1);
  if (num > 9 || num <= 0)
    return 0;
  if (fat_stat[num].disk)
    grub_disk_close (fat_stat[num].disk);
  fat_stat[num].disk = 0;
  fat_stat[num].present = 0;
  fat_stat[num].total_sectors = 0;
  /* f_mount */
  grub_snprintf (dev, 3, "%d:", num);
  f_mount(0, dev, 0);
  grub_memset (&fatfs_list[num], 0, sizeof (FATFS));
  return 0;
}

/* fat.disk_status disknum @ret: grub_disk_t */
static int
fat_disk_status (lua_State *state)
{
  int num = 0;
  num = luaL_checkinteger (state, 1);
  if (num > 9 || num <= 0)
    return 0;
  if (!fat_stat[num].disk)
    return 0;
  lua_pushlightuserdata (state, fat_stat[num].disk);
  return 1;
}

/* fat.get_label disknum @ret:label */
static int
fat_get_label (lua_State *state)
{
  char label[35];
  int num = 0;
  char dev[3] = "1:";
  num = luaL_checkinteger (state, 1);
  if (num > 9 || num <= 0)
    return 0;
  grub_snprintf (dev, 3, "%d:", num);
  f_getlabel(dev, label, 0);
  lua_pushstring (state, label);
  return 1;
}

/* fat.set_label disknum label */
static int
fat_set_label (lua_State *state)
{
  const char *label;
  int num = 0;
  char dev[40] = "1:";
  num = luaL_checkinteger (state, 1);
  if (num > 9 || num <= 0)
    return 0;
  label = luaL_checkstring (state, 2);
  if (grub_strlen (label) > 34)
    return 0;
  grub_snprintf (dev, 40, "%d:%s", num, label);
  f_setlabel(dev);
  return 0;
}

/* fat.mkdir path */
static int
fat_mkdir (lua_State *state)
{
  const char *path;
  path = luaL_checkstring (state, 1);
  f_mkdir (path);
  return 0;
}

/* fat.rename path1 path2 */
static int
fat_rename (lua_State *state)
{
  const char *path1, *path2;
  path1 = luaL_checkstring (state, 1);
  path2 = luaL_checkstring (state, 2);
  f_rename (path1, path2);
  return 0;
}

/* fat.unlink path */
static int
fat_unlink (lua_State *state)
{
  const char *path;
  path = luaL_checkstring (state, 1);
  f_unlink (path);
  return 0;
}

/* fat.open path mode @ret:FIL */
static int
fat_open (lua_State *state)
{
  FIL *file = NULL;
  const char *name;
  BYTE flag = 0;

  file = grub_malloc (sizeof (FIL));
  if (! file)
    return 0;
  name = luaL_checkstring (state, 1);
  flag = (lua_gettop (state) > 1) ? luaL_checkinteger (state, 2) : 0;
  f_open (file, name, flag);
  lua_pushlightuserdata (state, file);
  return 1;
}

/* fat.close FIL */
static int
fat_close (lua_State *state)
{
  FIL *file = NULL;

  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  file = lua_touserdata (state, 1);
  f_close (file);
  grub_free (file);
  return 0;
}

/* fat.read FIL size @ret:buf */
static int
fat_read (lua_State *state)
{
  FIL *file = NULL;
  luaL_Buffer b;
  int n;
  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  file = lua_touserdata (state, 1);
  n = luaL_checkinteger (state, 2);
  luaL_buffinit (state, &b);
  while (n)
  {
    char *p;
    UINT nr;

    nr = (n > LUAL_BUFFERSIZE) ? LUAL_BUFFERSIZE : n;
    p = luaL_prepbuffer (&b);

    f_read (file, p, nr, &nr);
    if (nr == 0)
      break;

    luaL_addsize (&b, nr);
    n -= nr;
  }
  luaL_pushresult (&b);
  return 1;
}

/* fat.write FIL buf @ret:size */
static int
fat_write (lua_State *state)
{
  FIL *file;
  UINT ret;
  grub_size_t len;
  const char *buf;

  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  file = lua_touserdata (state, 1);
  buf = lua_tolstring (state, 2, &len);
  f_write (file, buf, len, &ret);

  lua_pushinteger (state, ret);
  return 1;
}

/* fat.lseek FIL ofs */
static int
fat_lseek (lua_State *state)
{
  FIL *file;
  int ofs;
  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  file = lua_touserdata (state, 1);
  ofs = luaL_checkinteger (state, 2);
  f_lseek (file, ofs);
  return 0;
}

/* fat.tell FIL @ret:ofs */
static int
fat_tell (lua_State *state)
{
  FIL *file;
  int ret;
  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  file = lua_touserdata (state, 1);
  ret = f_tell (file);
  lua_pushinteger (state, ret);
  return 1;
}

/* fat.eof FIL @ret:num */
static int
fat_eof (lua_State *state)
{
  FIL *file;
  int ret;
  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  file = lua_touserdata (state, 1);
  ret = f_eof (file);
  lua_pushinteger (state, ret);
  return 1;
}

/* fat.size FIL @ret:size */
static int
fat_size (lua_State *state)
{
  FIL *file;
  int ret;
  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  file = lua_touserdata (state, 1);
  ret = f_size (file);
  lua_pushinteger (state, ret);
  return 1;
}

/* fat.truncate FIL */
static int
fat_truncate (lua_State *state)
{
  FIL *file;
  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  file = lua_touserdata (state, 1);
  f_truncate (file);
  return 0;
}

luaL_Reg fatlib[] =
{
  {"mount", fat_mount},
  {"umount", fat_umount},
  {"disk_status", fat_disk_status},
  {"get_label", fat_get_label},
  {"set_label", fat_set_label},
  {"mkdir", fat_mkdir},
  {"rename", fat_rename},
  {"unlink", fat_unlink},
  {"open", fat_open},
  {"close", fat_close},
  {"read", fat_read},
  {"write", fat_write},
  {"lseek", fat_lseek},
  {"tell", fat_tell},
  {"eof", fat_eof},
  {"size", fat_size},
  {"truncate", fat_truncate},
  {0, 0}
};

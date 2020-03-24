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
#include <grub/normal.h>
#include <grub/term.h>
#include <grub/file.h>
#include <grub/menu.h>
#include <grub/misc.h>
#include <grub/video.h>
#include <grub/bitmap.h>
#include <grub/gfxmenu_view.h>

static int
lua_input_read (lua_State *state)
{
  int hide;
  hide = (lua_gettop (state) > 0) ? luaL_checkinteger (state, 1) : 0;
  char *line = grub_getline (hide);
  if (! line)
    lua_pushnil(state);
  else
    lua_pushstring (state, line);

  grub_free (line);
  grub_printf ("\n");
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

static int
lua_video_draw_pixel (lua_State *state)
{
  grub_video_color_t color = check_grub_color (state, 1);
  int x = luaL_checkint (state, 2);
  int y = luaL_checkint (state, 3);
  if (grub_video_fill_rect (color, x, y, 1, 1) != GRUB_ERR_NONE)
    return luaL_error (state, "Error filling rectangle: %s", grub_errmsg);
  return 0;
}

static int
lua_video_get_info (lua_State *state)
{
  struct grub_video_mode_info info;
  unsigned int width = 0, height = 0;
  if (grub_video_get_info (&info) == GRUB_ERR_NONE)
  {
    width = info.width;
    height = info.height;
  }
  lua_pushinteger (state, width);
  lua_pushinteger (state, height);
  return 2;
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

static int
lua_video_bitmap_load (lua_State *state)
{
  struct grub_video_bitmap *bitmap = NULL;
  const char *filename;

  filename = luaL_checkstring (state, 1);
  grub_video_bitmap_load (&bitmap, filename);
  save_errno (state);

  if (! bitmap)
    return 0;

  lua_pushlightuserdata (state, bitmap);
  return 1;
}

static int
lua_video_bitmap_close (lua_State *state)
{
  struct grub_video_bitmap *bitmap;

  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  bitmap = lua_touserdata (state, 1);
  grub_video_bitmap_destroy (bitmap);

  return 0;
}

static int
lua_video_bitmap_get_info (lua_State *state)
{
  unsigned int width, height;
  struct grub_video_bitmap *bitmap;

  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  bitmap = lua_touserdata (state, 1);
  width = grub_video_bitmap_get_width (bitmap);
  height = grub_video_bitmap_get_height (bitmap);
  lua_pushinteger (state, width);
  lua_pushinteger (state, height);
  return 2;
}

static int
lua_video_bitmap_blit (lua_State *state)
{
  struct grub_video_bitmap *bitmap;
  int x, y, offset_x, offset_y, width, height;

  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  bitmap = lua_touserdata (state, 1);
  x = luaL_checkint (state, 2);
  y = luaL_checkint (state, 3);
  offset_x = luaL_checkint (state, 4);
  offset_y = luaL_checkint (state, 5);
  width = luaL_checkint (state, 6);
  height = luaL_checkint (state, 7);
  grub_video_blit_bitmap (bitmap, GRUB_VIDEO_BLIT_BLEND,
                          x, y, offset_x, offset_y,
                          width, height);
  return 0;
}

/* src, w, h */
static int
lua_video_bitmap_rescale (lua_State *state)
{
  int w, h;
  struct grub_video_bitmap *bitmap;
  struct grub_video_bitmap *new_bitmap = NULL;

  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  bitmap = lua_touserdata (state, 1);
  w = luaL_checkint (state, 2);
  h = luaL_checkint (state, 3);
  grub_video_bitmap_create_scaled (&new_bitmap, w, h, bitmap,
                                   GRUB_VIDEO_BITMAP_SCALE_METHOD_BEST);
  if (! new_bitmap)
    return 0;
  lua_pushlightuserdata (state, new_bitmap);
  return 1;
}

luaL_Reg inputlib[] = {
    {"getkey", lua_input_getkey},
    {"getkey_noblock", lua_input_getkey_noblock},
    {"read", lua_input_read},
    {0, 0}
};

luaL_Reg videolib[] = {
    {"swap_buffers", lua_video_swap_buffers},
    {"fill_rect", lua_video_fill_rect},
    {"draw_pixel", lua_video_draw_pixel},
    {"get_info", lua_video_get_info},
    {"draw_string", lua_video_draw_string},
    {"info", lua_video_info},
    {"bitmap_load", lua_video_bitmap_load},
    {"bitmap_close", lua_video_bitmap_close},
    {"bitmap_info", lua_video_bitmap_get_info},
    {"bitmap_blit", lua_video_bitmap_blit},
    {"bitmap_rescale", lua_video_bitmap_rescale},
    {0, 0}
};

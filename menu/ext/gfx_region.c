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

#include <grub/dl.h>
#include <grub/mm.h>
#include <grub/env.h>
#include <grub/misc.h>
#include <grub/video.h>
#include <grub/bitmap.h>
#include <grub/bitmap_scale.h>
#include <grub/menu_region.h>
#include <grub/fbutil.h>
#include <grub/fbblit.h>

#define DEFAULT_VIDEO_MODE "auto"

static int screen_width;
static int screen_height;
static grub_font_t default_font;
static struct grub_menu_region grub_gfx_region;

#define REFERENCE_STRING	"m"

static grub_err_t
grub_gfx_region_init (void)
{
  const char *modevar;
  struct grub_video_mode_info mode_info;
  grub_err_t err;
  char *fn;

  modevar = grub_env_get ("gfxmode");
  if (! modevar || *modevar == 0)
    err = grub_video_set_mode (DEFAULT_VIDEO_MODE,
			       GRUB_VIDEO_MODE_TYPE_PURE_TEXT |
			       GRUB_VIDEO_MODE_TYPE_DOUBLE_BUFFERED, 0);
  else
    {
      char *tmp;
      tmp = grub_xasprintf ("%s;" DEFAULT_VIDEO_MODE, modevar);
      if (!tmp)
	return grub_errno;
      err = grub_video_set_mode (tmp,
				 GRUB_VIDEO_MODE_TYPE_PURE_TEXT |
				 GRUB_VIDEO_MODE_TYPE_DOUBLE_BUFFERED, 0);
      grub_free (tmp);
    }

  if (err)
    return err;

  err = grub_video_get_info (&mode_info);
  if (err)
    return err;

  screen_width = mode_info.width;
  screen_height = mode_info.height;

  fn = grub_env_get ("gfxfont");
  if (! fn)
    fn = "";

  default_font = grub_font_get (fn);
  grub_gfx_region.char_width = grub_font_get_string_width (default_font,
							   REFERENCE_STRING);
  grub_gfx_region.char_height = grub_font_get_ascent (default_font) +
    grub_font_get_descent (default_font);

  if ((! grub_gfx_region.char_width) || (! grub_gfx_region.char_height))
    return grub_error (GRUB_ERR_UNKNOWN_DEVICE, "invalid font");

  return grub_errno;
}

static grub_err_t
grub_gfx_region_fini (void)
{
  grub_video_restore ();
  return (grub_errno = GRUB_ERR_NONE);
}

static void
grub_gfx_region_get_screen_size (int *width, int *height)
{
  *width = screen_width;
  *height = screen_height;
}

static grub_font_t
grub_gfx_region_get_font (const char *name)
{
  return (name) ? grub_font_get (name) : default_font;
}

static int
grub_gfx_region_get_text_width (grub_font_t font, const char *str,
				int count, int *chars)
{
  int width, w;
  const char *p;

  if (! count)
    {
      if (chars)
	*chars = grub_strlen (str);

      return grub_font_get_string_width (font, str);
    }

  width = 0;
  p = str;
  while ((count) && (*p) &&
	 ((w = grub_font_get_code_width (font, p, &p)) > 0))
    {
      width += w;
      count--;
    }

  if (chars)
    *chars = p - str;

  return width;
}

static int
grub_gfx_region_get_text_height (grub_font_t font)
{
  return grub_font_get_height (font);
}

static struct grub_video_bitmap *
grub_gfx_region_get_bitmap (const char *name)
{
  struct grub_video_bitmap *bitmap;

  if (grub_video_bitmap_load (&bitmap, name))
    {
      grub_errno = 0;
      return 0;
    }

  return bitmap;
}

static void
grub_gfx_region_free_bitmap (struct grub_video_bitmap *bitmap)
{
  grub_video_bitmap_destroy (bitmap);
}

static void
grub_gfx_region_scale_bitmap (struct grub_menu_region_bitmap *bitmap)
{
  if ((bitmap->bitmap != bitmap->cache->bitmap) &&
      (bitmap->bitmap != bitmap->cache->scaled_bitmap))
    grub_video_bitmap_destroy (bitmap->bitmap);

  bitmap->bitmap = 0;
  grub_video_bitmap_create_scaled (&bitmap->bitmap, bitmap->common.width,
				   bitmap->common.height,
				   bitmap->cache->bitmap,
				   bitmap->scale, bitmap->color);
}

static struct grub_video_bitmap *
grub_gfx_region_new_bitmap (int width, int height)
{
  struct grub_video_bitmap *dst = 0;
  
  grub_video_bitmap_create (&dst, width, height,
			    GRUB_VIDEO_BLIT_FORMAT_RGBA_8888);
  return dst;
}

static void
grub_gfx_region_blit_bitmap (struct grub_video_bitmap *dst,
			     struct grub_video_bitmap *src,
			     enum grub_video_blit_operators oper,
			     int dst_x, int dst_y, int width, int height,
			     int src_x, int src_y)
{
  struct grub_video_fbblit_info dst_info;
  struct grub_video_fbblit_info src_info;

  dst_info.mode_info = &dst->mode_info;
  dst_info.data = dst->data;
  src_info.mode_info = &src->mode_info;
  src_info.data = src->data;
  grub_video_fbblit (&dst_info, &src_info, oper,
		     dst_x, dst_y, width, height, src_x, src_y);
}

static grub_video_color_t
grub_gfx_region_map_color (int fg_color, int bg_color __attribute__ ((unused)))
{
  return grub_video_map_color (fg_color);
}

static grub_video_color_t
grub_gfx_region_map_rgb (grub_uint8_t red, grub_uint8_t green, grub_uint8_t blue)
{
  return grub_video_map_rgb (red, green, blue);
}

static void
grub_gfx_region_update_rect (struct grub_menu_region_rect *rect,
			     int x, int y, int width, int height,
			     int scn_x, int scn_y)
{
  if (rect->fill)
    {
      struct grub_font_glyph *glyph;
      int w, h, font_ascent, font_width;

      grub_video_set_viewport (scn_x, scn_y, width, height);
      glyph = grub_font_get_glyph_with_fallback (default_font, rect->fill);
      font_ascent = grub_font_get_ascent (default_font);
      font_width = glyph->device_width;
      for (h = -y ; h < height; h += grub_gfx_region.char_height)
	{
	  for (w = -x; w < width; w += font_width)
	    grub_font_draw_glyph (glyph, rect->color, w, h + font_ascent);
	}
      grub_video_set_viewport (0, 0, screen_width, screen_height);
    }
  else
    grub_video_fill_rect (rect->color, scn_x, scn_y, width, height);
  grub_video_update_rect (scn_x, scn_y, width, height);
}

static void
grub_gfx_region_update_text (struct grub_menu_region_text *text,
			     int x, int y, int width, int height,
			     int scn_x, int scn_y)
{
  grub_video_set_viewport (scn_x, scn_y, width, height);
  grub_font_draw_string (text->text, text->font, text->color, - x,
			 - y + grub_font_get_ascent (text->font));
  grub_video_set_viewport (0, 0, screen_width, screen_height);
  grub_video_update_rect (scn_x, scn_y, width, height);
}

static void
grub_gfx_region_update_bitmap (struct grub_menu_region_bitmap *bitmap,
			       int x, int y, int width, int height,
			       int scn_x, int scn_y)
{
  grub_video_blit_bitmap (bitmap->bitmap, GRUB_VIDEO_BLIT_BLEND,
			  scn_x, scn_y, x, y, width, height);
  grub_video_update_rect (scn_x, scn_y, width, height);
}

#define CURSOR_HEIGHT	2

static void
grub_gfx_region_draw_cursor (struct grub_menu_region_text *text,
			     int width, int height, int scn_x, int scn_y)
{
  int y;

  y = grub_font_get_ascent (text->font);
  if (y >= height)
    return;

  height -= y;
  if (height > CURSOR_HEIGHT)
    height = CURSOR_HEIGHT;

  grub_video_fill_rect (text->color, scn_x, scn_y + y, width, height);
  grub_video_update_rect (scn_x, scn_y + y, width, height);
}

static struct grub_menu_region grub_gfx_region =
  {
    .name = "gfx",
    .init = grub_gfx_region_init,
    .fini = grub_gfx_region_fini,
    .get_screen_size = grub_gfx_region_get_screen_size,
    .get_font = grub_gfx_region_get_font,
    .get_text_width = grub_gfx_region_get_text_width,
    .get_text_height = grub_gfx_region_get_text_height,
    .get_bitmap = grub_gfx_region_get_bitmap,
    .free_bitmap = grub_gfx_region_free_bitmap,
    .scale_bitmap = grub_gfx_region_scale_bitmap,
    .map_color = grub_gfx_region_map_color,
    .map_rgb = grub_gfx_region_map_rgb,
    .update_rect = grub_gfx_region_update_rect,
    .update_text = grub_gfx_region_update_text,
    .update_bitmap = grub_gfx_region_update_bitmap,    
    .draw_cursor = grub_gfx_region_draw_cursor,
    .new_bitmap = grub_gfx_region_new_bitmap,
    .blit_bitmap = grub_gfx_region_blit_bitmap
  };

GRUB_MOD_INIT(gfxrgn)
{
  grub_menu_region_register ("gfx", &grub_gfx_region);
}

GRUB_MOD_FINI(gfxrgn)
{
  grub_menu_region_unregister (&grub_gfx_region);
}

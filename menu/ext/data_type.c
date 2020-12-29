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

#include <grub/mm.h>
#include <grub/env.h>
#include <grub/misc.h>
#include <grub/term.h>
#include <grub/menu_data.h>
#include <grub/bitmap_scale.h>

GRUB_EXPORT(grub_menu_next_field);
GRUB_EXPORT(grub_menu_restore_field);
GRUB_EXPORT(grub_menu_parse_bitmap);
GRUB_EXPORT(grub_menu_parse_color);
GRUB_EXPORT(grub_menu_parse_size);

char *
grub_menu_next_field (char *name, char c)
{
  char *p;

  p = grub_strchr (name, c);
  if (p)
    {
      *(p++) = '\0';
    }

  return p;
}

void
grub_menu_restore_field (char *name, char c)
{
  if (name)
    *(name - 1) = c;
}

static char *color_list[16] =
{
  "black",
  "blue",
  "green",
  "cyan",
  "red",
  "magenta",
  "brown",
  "light-gray",
  "dark-gray",
  "light-blue",
  "light-green",
  "light-cyan",
  "light-red",
  "light-magenta",
  "yellow",
  "white"
};

static int
parse_color_name (int *ret, char *name)
{
  grub_uint8_t i;
  for (i = 0; i < sizeof (color_list) / sizeof (*color_list); i++)
    if (! grub_strcmp (name, color_list[i]))
      {
	*ret = i;
	return 0;
      }
  return -1;
}

static int
parse_key (char *name)
{
  int k;

  if (name[1] == 0)
    return name[0];

  if (*name != '#')
    return -1;

  k = grub_strtoul (name + 1, &name, 0);
  return (*name != 0) ? 0 : k;
}

static grub_video_color_t
parse_color (char *name, grub_uint32_t *fill)
{
  int fg, bg;
  char *p, *n;

  n = grub_menu_next_field (name, ',');
  if (n)
    *fill = parse_key (n);

  if (*name == '#')
    {
      grub_uint32_t rgb;

      rgb = grub_strtoul (name + 1, &name, 16);
      if (grub_menu_region_get_current ()->map_rgb)
	{
	  grub_menu_restore_field (n, ',');
	  return grub_menu_region_get_current ()->map_rgb (rgb >> 16, rgb >> 8,
							   rgb);
	}

      if (*name == '/')
	name++;
    }

  p = grub_menu_next_field (name, '/');
  if (parse_color_name (&fg, name) == -1)
    fg = 7;

  grub_menu_restore_field (p, '/');
  if (p)
    {
      if (parse_color_name (&bg, p) == -1)
	bg = 0;
    }
  else
    {
      if ((fill) && (! grub_menu_region_gfx_mode ()))
	{
	  bg = fg;
	  fg = 7;
	}
      else
	bg = 0;
    }

  grub_menu_restore_field (n, ',');
  return grub_menu_region_map_color (fg, bg);
}

grub_video_color_t
grub_menu_parse_color (const char *str, grub_uint32_t *fill,
		       grub_video_color_t *color_selected,
		       grub_uint32_t *fill_selected)
{
  char *name, *n;
  grub_video_color_t color, color1;

  name = (char *) str;
  n = grub_menu_next_field (name, ':');
  color = parse_color (name, fill);
  grub_menu_restore_field (n, ':');
  color1 = (n) ? parse_color (n, fill_selected) : color;

  if (color_selected)
    *color_selected = color1;

  return color;
}

static grub_menu_region_common_t
parse_bitmap (char *name, grub_uint32_t def_fill)
{
  int scale = GRUB_VIDEO_BITMAP_SCALE_TYPE_NORMAL;
  grub_video_color_t color;
  char *ps, *pc;
  grub_uint32_t fill;

  if (name[0] == '(')
    {
      ps = grub_strchr (name, ')');
      if (! ps)
	return 0;
    }
  else
    ps = name;
  
  ps = grub_menu_next_field (ps, ',');
  if (ps)
    {
      pc = grub_menu_next_field (ps, ',');

      if (! grub_strcmp (ps, "center"))
	scale = GRUB_VIDEO_BITMAP_SCALE_TYPE_CENTER;
      else if (! grub_strcmp (ps, "tiling"))
	scale = GRUB_VIDEO_BITMAP_SCALE_TYPE_TILING;
      else if (! grub_strcmp (ps, "minfit"))
	scale = GRUB_VIDEO_BITMAP_SCALE_TYPE_MINFIT;
      else if (! grub_strcmp (ps, "maxfit"))
	scale = GRUB_VIDEO_BITMAP_SCALE_TYPE_MAXFIT;

      grub_menu_restore_field (pc, ',');
    }
  else
    pc = 0;

  fill = def_fill;
  color = (pc) ? parse_color (pc, &fill) : 0;

  if ((*name) && (grub_menu_region_gfx_mode ()))
    {
      grub_menu_region_bitmap_t bitmap;

      if (! grub_strcmp (name, "none"))
	{
	  grub_menu_restore_field (ps, ',');
	  return 0;
	}

      bitmap = grub_menu_region_create_bitmap (name, scale, color);
      if (bitmap)
	{
	  grub_menu_restore_field (ps, ',');
	  return (grub_menu_region_common_t) bitmap;
	}
    }

  grub_menu_restore_field (ps, ',');

  if ((int) fill == -1)
    return 0;

  return (grub_menu_region_common_t)
    grub_menu_region_create_rect (grub_menu_region_get_char_width (),
				  grub_menu_region_get_char_height (),
				  color, fill);
}

grub_menu_region_common_t
grub_menu_parse_bitmap (const char *str, grub_uint32_t def_fill,
			grub_menu_region_common_t *bitmap_selected)
{
  char *name, *n;
  grub_menu_region_common_t bitmap;

  if (bitmap_selected)
    *bitmap_selected = 0;

  name = (str) ? (char *) str : "";
  n = grub_menu_next_field (name, ':');
  bitmap = parse_bitmap (name, def_fill);
  grub_menu_restore_field (n, ':');
  if ((n) && (bitmap_selected))
    *bitmap_selected = parse_bitmap (n, def_fill);

  return bitmap;
}

long
grub_menu_parse_size (const char *str, int parent_size, int horizontal)
{
  char *end;
  long ret;

  ret = grub_strtol (str, &end, 0);
  if (*end == 0)
    ret *= (horizontal) ?
      grub_menu_region_get_char_width () :
      grub_menu_region_get_char_height ();
  else
    {
      if (*end == '%')
	{
	  ret = (ret * parent_size) / 100;
	  end++;
	}

      if ((*end == '/') && (! grub_menu_region_gfx_mode ()))
	{
	  int old;

	  old = ret;
	  ret = grub_strtol (end + 1, &end, 0);
	  if (old < 0)
	    ret = -ret;

	  if (*end == '%')
	    ret = (ret * parent_size) / 100;
	}
    }

  return ret;
}

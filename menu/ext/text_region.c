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
#include <grub/misc.h>
#include <grub/term.h>
#include <grub/menu_region.h>

static struct grub_menu_region grub_text_region;

static grub_err_t
grub_text_region_init (void)
{
  grub_text_region.char_width = grub_text_region.char_height = 1;
  return 0;
}

static void
grub_text_region_get_screen_size (int *width, int *height)
{
  *width = grub_term_width (grub_menu_region_text_term) - 1;
  *height = grub_term_height (grub_menu_region_text_term);
}

static int
grub_text_region_get_text_width (grub_font_t font __attribute__ ((unused)),
				 const char *str, int count, int *chars)
{
  int len;

  len = (count) ? count : (int) grub_strlen (str);
  if (chars)
    *chars = len;

  return len;
}

static int
grub_text_region_get_text_height (grub_font_t font __attribute__ ((unused)))
{
  return 1;
}

static grub_video_color_t
grub_text_region_map_color (int fg_color, int bg_color)
{
  return ((bg_color & 7) << 4) + fg_color;
}

static void
grub_text_region_update_rect (struct grub_menu_region_rect *rect,
			      int x __attribute__ ((unused)),
			      int y __attribute__ ((unused)),
			      int width, int height, int scn_x, int scn_y)
{
  int w, h;
  grub_uint32_t fill;

  fill = (rect->fill) ? rect->fill : ' ';
  grub_term_setcolor (grub_menu_region_text_term, rect->color, 0);
  grub_term_setcolorstate (grub_menu_region_text_term, GRUB_TERM_COLOR_NORMAL);
  for (h = 0; h < height; h++)
    {
      grub_term_gotoxy (grub_menu_region_text_term, scn_x, scn_y);

      for (w = 0; w < width; w++)
	grub_putcode (fill, grub_menu_region_text_term);

      scn_y++;
    }
}

static void
grub_text_region_update_text (struct grub_menu_region_text *text,
			      int x, int y __attribute__ ((unused)),
			      int width, int height __attribute__ ((unused)),
			      int scn_x, int scn_y)
{
  char *s;
  int i;

  grub_term_setcolor (grub_menu_region_text_term, text->color, 0);
  grub_term_setcolorstate (grub_menu_region_text_term, GRUB_TERM_COLOR_NORMAL);
  grub_term_gotoxy (grub_menu_region_text_term, scn_x, scn_y);
  s = text->text + x;
  for (i = 0; i < width; i++, s++)
    grub_putcode (*s, grub_menu_region_text_term);
}

static void
grub_text_region_hide_cursor (void)
{
  grub_term_setcursor (grub_menu_region_text_term, 0);
}

static void
grub_text_region_draw_cursor (struct grub_menu_region_text *text
			      __attribute__ ((unused)),
			      int width __attribute__ ((unused)),
			      int height __attribute__ ((unused)),
			      int scn_x, int scn_y)
{
  grub_term_gotoxy (grub_menu_region_text_term, scn_x, scn_y);
  grub_term_setcursor (grub_menu_region_text_term, 1);
}

static struct grub_menu_region grub_text_region =
  {
    .name = "text",
    .init = grub_text_region_init,
    .get_screen_size = grub_text_region_get_screen_size,
    .get_text_width = grub_text_region_get_text_width,
    .get_text_height = grub_text_region_get_text_height,
    .map_color = grub_text_region_map_color,
    .update_rect = grub_text_region_update_rect,
    .update_text = grub_text_region_update_text,
    .hide_cursor = grub_text_region_hide_cursor,
    .draw_cursor = grub_text_region_draw_cursor,
  };

GRUB_MOD_INIT(txtrgn)
{
  grub_menu_region_register ("text", &grub_text_region);
}

GRUB_MOD_FINI(txtrgn)
{
  grub_menu_region_unregister (&grub_text_region);
}

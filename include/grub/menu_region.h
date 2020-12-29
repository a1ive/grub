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

#ifndef GRUB_MENU_REGION_HEADER
#define GRUB_MENU_REGION_HEADER 1

#include <grub/err.h>
#include <grub/symbol.h>
#include <grub/types.h>
#include <grub/handler.h>
#include <grub/bitmap.h>
#include <grub/font.h>
#include <grub/term.h>

#define GRUB_MENU_REGION_TYPE_TEXT	0
#define GRUB_MENU_REGION_TYPE_RECT	1
#define GRUB_MENU_REGION_TYPE_BITMAP	2

struct grub_menu_region_common
{
  int type;
  int ofs_x;
  int ofs_y;
  int width;
  int height;
};
typedef struct grub_menu_region_common *grub_menu_region_common_t;

struct grub_menu_region_rect
{
  struct grub_menu_region_common common;
  grub_video_color_t color;
  grub_uint32_t fill;
};
typedef struct grub_menu_region_rect *grub_menu_region_rect_t;

struct grub_menu_region_text
{
  struct grub_menu_region_common common;
  grub_font_t font;
  grub_video_color_t color;
  char text[0];
};
typedef struct grub_menu_region_text *grub_menu_region_text_t;

struct grub_bitmap_cache
{
  struct grub_bitmap_cache *next;
  const char *name;
  struct grub_video_bitmap *bitmap;
  struct grub_video_bitmap *scaled_bitmap;
  int count;
};
typedef struct grub_bitmap_cache *grub_bitmap_cache_t;

struct grub_menu_region_bitmap
{
  struct grub_menu_region_common common;
  struct grub_video_bitmap *bitmap;
  struct grub_bitmap_cache *cache;
  int scale;
  grub_video_color_t color;
};
typedef struct grub_menu_region_bitmap *grub_menu_region_bitmap_t;

struct grub_menu_region
{
  struct grub_menu_region *next;
  const char *name;
  grub_err_t (*init) (void);
  grub_err_t (*fini) (void);
  int char_width;
  int char_height;
  grub_font_t (*get_font) (const char *name);
  struct grub_video_bitmap * (*get_bitmap) (const char *name);
  void (*free_bitmap) (struct grub_video_bitmap *bitmap);
  void (*scale_bitmap) (struct grub_menu_region_bitmap *bitmap);
  struct grub_video_bitmap * (*new_bitmap) (int width, int height);
  void (*blit_bitmap) (struct grub_video_bitmap *dst,
		       struct grub_video_bitmap *src,
		       enum grub_video_blit_operators oper,
		       int dst_x, int dst_y, int width, int height,
		       int src_x, int src_y);
  
  grub_video_color_t (*map_color) (int fg_color, int bg_color);
  grub_video_color_t (*map_rgb) (grub_uint8_t red, grub_uint8_t green,
				 grub_uint8_t blue);
  void (*get_screen_size) (int *width, int *height);
  int (*get_text_width) (grub_font_t font, const char *str,
			 int count, int *chars);
  int (*get_text_height) (grub_font_t font);
  void (*update_rect) (struct grub_menu_region_rect *rect,
		       int x, int y, int width, int height,
		       int scn_x, int scn_y);
  void (*update_text) (struct grub_menu_region_text *text,
		       int x, int y, int width, int height,
		       int scn_x, int scn_y);
  void (*update_bitmap) (struct grub_menu_region_bitmap *bitmap,
			 int x, int y, int width, int height,
			 int scn_x, int scn_y);
  void (*hide_cursor) (void);
  void (*draw_cursor) (struct grub_menu_region_text *text,
		       int width, int height, int scn_x, int scn_y);
};
typedef struct grub_menu_region *grub_menu_region_t;

struct grub_menu_region_update_list
{
  struct grub_menu_region_update_list *next;
  grub_menu_region_common_t region;
  int org_x;
  int org_y;
  int x;
  int y;
  int width;
  int height;
};
typedef struct grub_menu_region_update_list *grub_menu_region_update_list_t;

extern struct grub_handler_class grub_menu_region_class;

#define grub_menu_region_register(name, region) \
  grub_menu_region_register_internal (region); \
  GRUB_MODATTR ("handler", "menu_region." name);

static inline void
grub_menu_region_register_internal (grub_menu_region_t region)
{
  grub_handler_register (&grub_menu_region_class, GRUB_AS_HANDLER (region));
}

static inline void
grub_menu_region_unregister (grub_menu_region_t region)
{
  grub_handler_unregister (&grub_menu_region_class, GRUB_AS_HANDLER (region));
}

static inline grub_menu_region_t
grub_menu_region_get_current (void)
{
  return (grub_menu_region_t) grub_menu_region_class.cur_handler;
}

static inline void
grub_menu_region_get_screen_size (int *width, int *height)
{
  grub_menu_region_get_current ()->get_screen_size (width, height);
}

static inline grub_font_t
grub_menu_region_get_font (const char *name)
{
  return ((grub_menu_region_get_current ()->get_font) ?
	  grub_menu_region_get_current ()->get_font (name) : 0);
}

static inline int
grub_menu_region_get_text_width (grub_font_t font, const char *str,
				 int count, int *chars)
{
  return grub_menu_region_get_current ()->get_text_width (font, str, count,
							  chars);
}

static inline int
grub_menu_region_get_text_height (grub_font_t font)
{
  return grub_menu_region_get_current ()->get_text_height (font);
}

static inline int
grub_menu_region_get_char_width (void)
{
  return grub_menu_region_get_current ()->char_width;
}

static inline int
grub_menu_region_get_char_height (void)
{
  return grub_menu_region_get_current ()->char_height;
}

static inline grub_video_color_t
grub_menu_region_map_color (int fg_color, int bg_color)
{
  return grub_menu_region_get_current ()->map_color (fg_color, bg_color);
}

static inline int
grub_menu_region_gfx_mode (void)
{
  return (grub_menu_region_get_current ()->get_font != 0);
}

static inline void
grub_menu_region_draw_cursor (grub_menu_region_text_t text,
			      int width, int height, int scn_x, int scn_y)
{
  grub_menu_region_get_current ()->draw_cursor (text, width, height,
						scn_x, scn_y);
}

static inline void
grub_menu_region_hide_cursor (void)
{
  if (grub_menu_region_get_current ()->hide_cursor)
    grub_menu_region_get_current ()->hide_cursor ();
}

static inline void
grub_menu_region_blit_bitmap (grub_menu_region_common_t dst,
			      grub_menu_region_common_t src,
			      enum grub_video_blit_operators oper,
			      int dst_x, int dst_y, int width, int height,
			      int src_x, int src_y)
{
  if ((grub_menu_region_get_current ()->blit_bitmap) &&
      (dst->type == GRUB_MENU_REGION_TYPE_BITMAP) &&
      (src->type == GRUB_MENU_REGION_TYPE_BITMAP))
    grub_menu_region_get_current ()->blit_bitmap
      (((grub_menu_region_bitmap_t) dst)->bitmap,
       ((grub_menu_region_bitmap_t) src)->bitmap,
       oper, dst_x, dst_y, width, height, src_x, src_y);
}

grub_menu_region_text_t
grub_menu_region_create_text (grub_font_t font, grub_video_color_t color,
			      const char *str);
grub_menu_region_rect_t
grub_menu_region_create_rect (int width, int height, grub_video_color_t color,
			      grub_uint32_t fill);
grub_menu_region_bitmap_t
grub_menu_region_create_bitmap (const char *name, int scale,
				grub_video_color_t color);
grub_menu_region_bitmap_t
grub_menu_region_new_bitmap (int width, int height);
void grub_menu_region_scale (grub_menu_region_common_t region, int width,
			     int height);
void grub_menu_region_free (grub_menu_region_common_t region);

int grub_menu_region_check_rect (int *x1, int *y1, int *w1, int *h1,
				 int x2, int y2, int w2, int h2);
void grub_menu_region_add_update (grub_menu_region_update_list_t *head,
				  grub_menu_region_common_t region,
				  int org_x, int org_y, int x, int y,
				  int width, int height);
void grub_menu_region_apply_update (grub_menu_region_update_list_t head);

extern grub_term_output_t grub_menu_region_text_term;

#endif

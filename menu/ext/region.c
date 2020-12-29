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
#include <grub/misc.h>
#include <grub/tree.h>
#include <grub/menu_region.h>

GRUB_EXPORT(grub_menu_region_class);
GRUB_EXPORT(grub_menu_region_create_text);
GRUB_EXPORT(grub_menu_region_create_rect);
GRUB_EXPORT(grub_menu_region_create_bitmap);
GRUB_EXPORT(grub_menu_region_scale);
GRUB_EXPORT(grub_menu_region_free);

GRUB_EXPORT(grub_menu_region_check_rect);
GRUB_EXPORT(grub_menu_region_add_update);
GRUB_EXPORT(grub_menu_region_apply_update);

GRUB_EXPORT(grub_menu_region_text_term);

grub_term_output_t grub_menu_region_text_term;

grub_bitmap_cache_t cache_head;

struct grub_handler_class grub_menu_region_class =
  {
    .name = "menu_region"
  };

#define grub_cur_menu_region	grub_menu_region_get_current ()

grub_menu_region_text_t
grub_menu_region_create_text (grub_font_t font, grub_uint32_t color,
			      const char *str)
{
  grub_menu_region_text_t region;

  if (! str)
    str = "";

  region = grub_zalloc (sizeof (*region) + grub_strlen (str) + 1);
  if (! region)
    return 0;

  region->common.type = GRUB_MENU_REGION_TYPE_TEXT;
  region->common.width = grub_menu_region_get_text_width (font, str, 0, 0);
  region->common.height = grub_menu_region_get_text_height (font);

  region->font = font;
  region->color = color;
  grub_strcpy (region->text, str);

  return region;
}

grub_menu_region_rect_t
grub_menu_region_create_rect (int width, int height, grub_video_color_t color,
			      grub_uint32_t fill)
{
  grub_menu_region_rect_t region;

  region = grub_zalloc (sizeof (*region));
  if (! region)
    return 0;

  region->common.type = GRUB_MENU_REGION_TYPE_RECT;
  region->common.width = width;
  region->common.height = height;
  region->color = color;
  region->fill = fill;

  return region;
}

grub_menu_region_bitmap_t
grub_menu_region_create_bitmap (const char *name, int scale,
				grub_video_color_t color)
{
  grub_menu_region_bitmap_t region = 0;
  struct grub_video_bitmap *bitmap;
  grub_bitmap_cache_t cache = 0;

  if (! grub_cur_menu_region->get_bitmap)
    return 0;

  region = grub_zalloc (sizeof (*region));
  if (! region)
    return 0;

  cache = grub_named_list_find (GRUB_AS_NAMED_LIST (cache_head), name);
  if (cache)
    bitmap = cache->bitmap;
  else
    {
      cache = grub_zalloc (sizeof (*cache));
      if (! cache)
	goto quit;

      bitmap = grub_cur_menu_region->get_bitmap (name);

      if (! bitmap)
	goto quit;

      cache->bitmap = bitmap;
      cache->name = grub_strdup (name);
      grub_list_push (GRUB_AS_LIST_P (&cache_head), GRUB_AS_LIST (cache));
    }

  cache->count++;
  region->common.type = GRUB_MENU_REGION_TYPE_BITMAP;
  region->common.width = bitmap->mode_info.width;
  region->common.height = bitmap->mode_info.height;
  region->bitmap = bitmap;
  region->cache = cache;
  region->scale = scale;
  region->color = color;
  return region;

 quit:
  grub_free (region);
  grub_free (cache);
  return 0;
}

void
grub_menu_region_scale (grub_menu_region_common_t region, int width,
			int height)
{
  if ((! region) || ((width == region->width) && (height == region->height)))
    return;

  region->width = width;
  region->height = height;

  if ((region->type == GRUB_MENU_REGION_TYPE_BITMAP) &&
      (grub_cur_menu_region->scale_bitmap))
    {
      grub_menu_region_bitmap_t b;

      b = (grub_menu_region_bitmap_t) region;
      if ((b->cache->scaled_bitmap) &&
	  (width == (int) b->cache->scaled_bitmap->mode_info.width) &&
	  (height == (int) b->cache->scaled_bitmap->mode_info.height))
	{
	  b->bitmap = b->cache->scaled_bitmap;
	}
      else
	{
	  grub_cur_menu_region->scale_bitmap (b);
	  if (! b->cache->scaled_bitmap)
	    b->cache->scaled_bitmap = b->bitmap;
	}
    }
}

static void
grub_bitmap_cache_free (grub_bitmap_cache_t cache)
{
  cache->count--;
  if (cache->count == 0)
    {
      grub_list_remove (GRUB_AS_LIST_P (&cache_head), GRUB_AS_LIST (cache));
      grub_cur_menu_region->free_bitmap (cache->bitmap);
      grub_cur_menu_region->free_bitmap (cache->scaled_bitmap);
      grub_free ((char *) cache->name);
      grub_free (cache);
    }
}

void
grub_menu_region_free (grub_menu_region_common_t region)
{
  if (! region)
    return;

  if ((region->type == GRUB_MENU_REGION_TYPE_BITMAP) &&
      (grub_cur_menu_region->free_bitmap))
    {
      grub_menu_region_bitmap_t r = (grub_menu_region_bitmap_t) region;

      if (r->cache)
	{
	  if ((r->bitmap != r->cache->bitmap) &&
	      (r->bitmap != r->cache->scaled_bitmap))
	    grub_cur_menu_region->free_bitmap (r->bitmap);

	  grub_bitmap_cache_free (r->cache);
	}
      else
	grub_cur_menu_region->free_bitmap (r->bitmap);	
    }

  grub_free (region);
}

grub_menu_region_bitmap_t
grub_menu_region_new_bitmap (int width, int height)
{
  grub_menu_region_bitmap_t region = 0;
  struct grub_video_bitmap *bm;  
  
  if (! grub_menu_region_get_current ()->new_bitmap)
    return 0;

  region = grub_zalloc (sizeof (*region));
  if (! region)
    return 0;

  bm = grub_menu_region_get_current ()->new_bitmap (width, height);
  if (! bm)
    {
      grub_free (region);
      return 0;  
    }

  region->common.type = GRUB_MENU_REGION_TYPE_BITMAP;
  region->common.width = width;
  region->common.height = height;
  region->bitmap = bm;
  return region;
}

int
grub_menu_region_check_rect (int *x1, int *y1, int *w1, int *h1,
			     int x2, int y2, int w2, int h2)
{
  *w1 += *x1;
  *h1 += *y1;
  w2 += x2;
  h2 += y2;

  if (*x1 < x2)
    *x1 = x2;

  if (*y1 < y2)
    *y1 = y2;

  if (*w1 > w2)
    *w1 = w2;

  if (*h1 > h2)
    *h1 = h2;

  if ((*w1 <= *x1) || (*h1 <= *y1))
    return 0;

  *w1 -= *x1;
  *h1 -= *y1;

  return 1;
}

void
grub_menu_region_add_update (grub_menu_region_update_list_t *head,
			     grub_menu_region_common_t region,
			     int org_x, int org_y, int x, int y,
			     int width, int height)
{
  grub_menu_region_update_list_t u;

  if (! region)
    return;

  org_x += region->ofs_x;
  org_y += region->ofs_y;
  x -= region->ofs_x;
  y -= region->ofs_y;

  if (! grub_menu_region_check_rect (&x, &y, &width, &height,
				     0, 0, region->width, region->height))
    return;

  u = grub_malloc (sizeof (*u));
  if (! u)
    return;

  u->region = region;
  u->org_x = org_x;
  u->org_y = org_y;
  u->x = x;
  u->y = y;
  u->width = width;
  u->height = height;

  if ((! grub_menu_region_gfx_mode ()) ||
      ((region->type == GRUB_MENU_REGION_TYPE_RECT) &&
       (! ((grub_menu_region_rect_t) region)->fill)) ||
      ((region->type == GRUB_MENU_REGION_TYPE_BITMAP) &&
       (! ((grub_menu_region_bitmap_t) region)->bitmap->transparent)))
    {
      grub_menu_region_update_list_t *p, q;

      for (p = head, q = *p; q;)
	{
	  int x1, y1, w1, h1;

	  x1 = org_x + x - q->org_x;
	  y1 = org_y + y - q->org_y;
	  w1 = width;
	  h1 = height;

	  if (! grub_menu_region_check_rect (&x1, &y1, &w1, &h1,
					     q->x, q->y, q->width, q->height))
	    {
	      p = &(q->next);
	      q = q->next;
	      continue;
	    }

	  if (y1 > q->y)
	    {
	      grub_menu_region_update_list_t n;

	      n = grub_malloc (sizeof (*u));
	      if (n)
		{
		  n->region = q->region;
		  n->org_x = q->org_x;
		  n->org_y = q->org_y;
		  n->x = q->x;
		  n->y = q->y;
		  n->width = q->width;
		  n->height = y1 - q->y;
		  *p = n;
		  p = &(n->next);
		}
	    }

	  if (x1 > q->x)
	    {
	      grub_menu_region_update_list_t n;

	      n = grub_malloc (sizeof (*u));
	      if (n)
		{
		  n->region = q->region;
		  n->org_x = q->org_x;
		  n->org_y = q->org_y;
		  n->x = q->x;
		  n->y = y1;
		  n->width = x1 - q->x;
		  n->height = h1;
		  *p = n;
		  p = &(n->next);
		}
	    }

	  if (x1 + w1 < q->x + q->width)
	    {
	      grub_menu_region_update_list_t n;

	      n = grub_malloc (sizeof (*u));
	      if (n)
		{
		  n->region = q->region;
		  n->org_x = q->org_x;
		  n->org_y = q->org_y;
		  n->x = x1 + w1;
		  n->y = y1;
		  n->width = q->x + q->width - n->x;
		  n->height = h1;
		  *p = n;
		  p = &(n->next);
		}
	    }

	  if (y1 + h1 < q->y + q->height)
	    {
	      grub_menu_region_update_list_t n;

	      n = grub_malloc (sizeof (*u));
	      if (n)
		{
		  n->region = q->region;
		  n->org_x = q->org_x;
		  n->org_y = q->org_y;
		  n->x = q->x;
		  n->y = y1 + h1;
		  n->width = q->width;
		  n->height = q->y + q->height - n->y;
		  *p = n;
		  p = &(n->next);
		}
	    }

	  *p = q->next;
	  grub_free (q);
	  q = *p;
	}
    }

  u->next = *head;
  *head = u;
}

void
grub_menu_region_apply_update (grub_menu_region_update_list_t head)
{
  grub_menu_region_update_list_t prev = 0;

  grub_menu_region_hide_cursor ();
  while (head)
    {
      grub_menu_region_update_list_t temp;

      temp = head->next;
      head->next = prev;
      prev = head;
      head = temp;
    }

  head = prev;
  while (head)
    {
      grub_menu_region_update_list_t c;
      grub_menu_region_common_t r;
      int scn_x, scn_y;

      c = head;
      head = head->next;
      r = c->region;
      scn_x = c->org_x + c->x;
      scn_y = c->org_y + c->y;

#if 0
      grub_printf ("[%d: %d %d %d %d %d %d]  ", r->type, c->x, c->y,
		   c->width, c->height, scn_x, scn_y);
#else
      switch (r->type)
	{
	case GRUB_MENU_REGION_TYPE_RECT:
	  grub_cur_menu_region->update_rect ((grub_menu_region_rect_t ) r,
					     c->x, c->y, c->width, c->height,
					     scn_x, scn_y);
	  break;

	case GRUB_MENU_REGION_TYPE_TEXT:
	  grub_cur_menu_region->update_text ((grub_menu_region_text_t ) r,
					     c->x, c->y, c->width, c->height,
					     scn_x, scn_y);
	  break;

	case GRUB_MENU_REGION_TYPE_BITMAP:
	  grub_cur_menu_region->update_bitmap ((grub_menu_region_bitmap_t ) r,
					       c->x, c->y, c->width, c->height,
					       scn_x, scn_y);
	  break;
	}
#endif
      grub_free (c);
    }
}

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
#include <grub/err.h>
#include <grub/misc.h>
#include <grub/time.h>
#include <grub/widget.h>
#include <grub/dialog.h>
#include <grub/menu_data.h>
#include <grub/term.h>
#include <grub/loader.h>
#include <grub/parser.h>
#include <grub/command.h>

GRUB_EXPORT(grub_widget_class_list);
GRUB_EXPORT(grub_widget_create);
GRUB_EXPORT(grub_widget_init);
GRUB_EXPORT(grub_widget_draw);
GRUB_EXPORT(grub_widget_draw_region);
GRUB_EXPORT(grub_widget_free);
GRUB_EXPORT(grub_widget_select_node);
GRUB_EXPORT(grub_widget_input);
GRUB_EXPORT(grub_widget_get_prop);
GRUB_EXPORT(grub_widget_current_node);
GRUB_EXPORT(grub_widget_refresh);
GRUB_EXPORT(grub_widget_screen);

grub_widget_class_t grub_widget_class_list;
grub_uitree_t grub_widget_current_node;
int grub_widget_refresh;
grub_uitree_t grub_widget_screen;

grub_err_t
grub_widget_create (grub_uitree_t node)
{
  grub_uitree_t child;

  child = node;
  while (child)
    {
      grub_widget_class_t class;
      grub_widget_t widget;
      int size;

      class = grub_widget_class_list;

      while (class)
	{
	  if (! grub_strcmp (child->name, class->name))
	    break;

	  class = class->next;
	}

      if (! class)
	  return grub_error (GRUB_ERR_BAD_ARGUMENT, "class not found");

      size = (class->get_data_size) ? class->get_data_size () : 0;
      widget = grub_zalloc (sizeof (struct grub_widget) + size);
      if (! widget)
	break;

      widget->class = class;
      widget->data = (char *) widget + sizeof (struct grub_widget);
      widget->node = child;
      child->data = widget;

      child = grub_tree_next_node (GRUB_AS_TREE (node), GRUB_AS_TREE (child));
    }

  return grub_errno;
}

static void
init_size (grub_widget_t widget, int size_only)
{
  grub_widget_t parent;
  grub_uitree_t node;
  int pw, ph;
  char *p, *m1, *m2;

  node = widget->node;
  if ((! node->parent) || (! node->parent->data))
    return;

  parent = widget->node->parent->data;
  pw = parent->inner_width;
  ph = parent->inner_height;

  p = grub_widget_get_prop (node, "width");
  if (p)
    {
      node->flags |= GRUB_WIDGET_FLAG_FIXED_WIDTH;
      widget->width = grub_menu_parse_size (p, pw, 1);
    }

  p = grub_widget_get_prop (node, "height");
  if (p)
    {
      node->flags |= GRUB_WIDGET_FLAG_FIXED_HEIGHT;
      widget->height = grub_menu_parse_size (p, ph, 0);
    }

  m1 = grub_widget_get_prop (node, "attach_left");
  m2 = grub_widget_get_prop (node, "attach_right");
  if ((m1) && (m2))
    {
      node->flags |= GRUB_WIDGET_FLAG_FIXED_WIDTH;
      widget->width = pw - grub_menu_parse_size (m1, pw, 1)
	- grub_menu_parse_size (m2, pw, 1);
    }

  m1 = grub_widget_get_prop (node, "attach_top");
  m2 = grub_widget_get_prop (node, "attach_bottom");
  if ((m1) && (m2))
    {
      node->flags |= GRUB_WIDGET_FLAG_FIXED_HEIGHT;
      widget->height = ph - grub_menu_parse_size (m1, ph, 0)
	- grub_menu_parse_size (m2, ph, 0);
    }

  if (size_only)
    return;

  p = grub_widget_get_prop (node, "attach_left");
  if (p)
    {
      node->flags |= GRUB_WIDGET_FLAG_FIXED_X;
      widget->x = grub_menu_parse_size (p, pw, 1);
    }

  p = grub_widget_get_prop (node, "attach_right");
  if (p)
    {
      node->flags |= GRUB_WIDGET_FLAG_FIXED_X;
      widget->x = pw - grub_menu_parse_size (p, pw, 1) - widget->width;
    }

  p = grub_widget_get_prop (node, "attach_hcenter");
  if (p)
    {
      node->flags |= GRUB_WIDGET_FLAG_FIXED_X;
      widget->x = (pw - widget->width) / 2 + grub_menu_parse_size (p, pw, 1);
    }

  p = grub_widget_get_prop (node, "attach_top");
  if (p)
    {
      node->flags |= GRUB_WIDGET_FLAG_FIXED_Y;
      widget->y = grub_menu_parse_size (p, ph, 0);
    }

  p = grub_widget_get_prop (node, "attach_bottom");
  if (p)
    {
      node->flags |= GRUB_WIDGET_FLAG_FIXED_Y;
      widget->y = ph - grub_menu_parse_size (p, ph, 0) - widget->height;
    }

  p = grub_widget_get_prop (node, "attach_vcenter");
  if (p)
    {
      node->flags |= GRUB_WIDGET_FLAG_FIXED_Y;
      widget->y = (ph - widget->height) / 2 + grub_menu_parse_size (p, ph, 0);
    }
}

#define HALIGN_LEFT 0
#define HALIGN_CENTER 1
#define HALIGN_RIGHT 2
#define HALIGN_EXTEND 3

#define VALIGN_TOP 0
#define VALIGN_CENTER 1
#define VALIGN_BOTTOM 2
#define VALIGN_EXTEND 3

static void
align_x (grub_widget_t child, int halign, int width)
{
  int delta;

  delta = width - child->width;
  if (delta <= 0)
    return;

  if (halign == HALIGN_EXTEND)
    child->width += delta;
  else if (halign == HALIGN_CENTER)
    child->x += delta >> 1;
  else if (halign == HALIGN_RIGHT)
    child->x += delta;
}

static void
align_y (grub_widget_t child, int valign, int height)
{
  int delta;

  delta = height - child->height;
  if (delta <= 0)
    return;

  if (valign == VALIGN_EXTEND)
    child->height += delta;
  else if (valign == VALIGN_CENTER)
    child->y += delta >> 1;
  else if (valign == VALIGN_BOTTOM)
    child->y += delta;
}

static void
get_direction (grub_uitree_t node, int *horizontal, int *reverse)
{
  char *p;

  *horizontal = 0;
  *reverse = 0;
  p = grub_widget_get_prop (node, "direction");
  if (p)
    {
      if (! grub_strcmp (p, "left_to_right"))
	*horizontal = 1;
      else if (! grub_strcmp (p, "right_to_left"))
	*reverse = *horizontal = 1;
      else if (! grub_strcmp (p, "bottom_to_top"))
	*reverse = 1;
    }
}

static int screen_width, screen_height;

static void
adjust_layout (grub_widget_t widget, int calc_mode)
{
  grub_uitree_t node;
  int space, extra, count, width, height, max_items, index, max_width, max_height;
  int horizontal, reverse;
  char *p;

  node = widget->node;

  p = grub_widget_get_prop (node, "space");
  space = (p) ? grub_menu_parse_size (p, 0, 1) : 0;

  p = grub_widget_get_prop (node, "max_items");
  max_items = (p) ? grub_strtoul (p, 0, 0) : 0;

  get_direction (node, &horizontal, &reverse);

  width = 0;
  height = 0;
  count = 0;
  index = 0;
  max_width = screen_width;
  max_height = screen_height;
  for (node = node->child; node; node = node->next)
    {
      grub_widget_t child;
      int skip;

      child = node->data;
      if (! child)
	continue;

      if (calc_mode)
	skip = (horizontal) ? (child->width <= 0) : (child->height <= 0);
      else
	{
	  init_size (child, 0);
	  skip = ((child->width <= 0) || (child->height <= 0) ||
		  (node->flags & GRUB_WIDGET_FLAG_FIXED_XY));
	}

      if (skip)
	continue;

      if (horizontal)
	{
	  width += child->width + space;
	  if (child->height > height)
	    height = child->height;
	}
      else
	{
	  height += child->height + space;
	  if (child->width > width)
	    width = child->width;
	}

      if (grub_widget_get_prop (node, "extend"))
	{
	  node->flags |= GRUB_WIDGET_FLAG_EXTEND;
	  count++;
	}

      index++;
      if (index == max_items)
	{
	  if (horizontal)
	    {
	      if (width - space < max_width)
		max_width = width - space;
	    }
	  else
	    {
	      if (height - space < max_height)
		max_height = height - space;
	    }
	}
    }

  if (horizontal)
    {
      if (width > 0)
	width -= space;
    }
  else
    {
      if (height > 0)
	height -= space;
    }

  node = widget->node;
  if (calc_mode)
    {
      init_size (widget, 1);

      if (! (node->flags & GRUB_WIDGET_FLAG_FIXED_WIDTH))
	{
	  p = grub_widget_get_prop (node, "max_width");
	  if (p)
	    {
	      int w;

	      w = grub_menu_parse_size (p, 0, 1);
	      if (width > w)
		width = w;
	    }

	  if (width > max_width)
	    width = max_width;

	  p = grub_widget_get_prop (node, "min_width");
	  if (p)
	    {
	      int w;

	      w = grub_menu_parse_size (p, 0, 1);
	      if (width < w)
		width = w;
	    }

	  widget->width = width;
	}

      if (! (node->flags & GRUB_WIDGET_FLAG_FIXED_HEIGHT))
	{
	  p = grub_widget_get_prop (node, "max_height");
	  if (p)
	    {
	      int h;

	      h = grub_menu_parse_size (p, 0, 0);
	      if (height > h)
		height = h;
	    }

	  if (height > max_height)
	    height = max_height;

	  p = grub_widget_get_prop (node, "min_height");
	  if (p)
	    {
	      int h;

	      h = grub_menu_parse_size (p, 0, 0);
	      if (height < h)
		height = h;
	    }

	  widget->height = height;
	}

      return;
    }

  if (horizontal)
    {
      extra = widget->inner_width - width;
      if (reverse)
	{
	  if ((extra > 0) && (count > 0))
	    width = widget->inner_width;
	}
      else
	width = 0;
      height = 0;
    }
  else
    {
      extra = widget->inner_height - height;
      if (reverse)
	{
	  if ((extra > 0) && (count > 0))
	    height = widget->inner_height;
	}
      else
	height = 0;
      width = 0;
    }

  if (extra < 0)
    extra = 0;

  if (count > 1)
    extra = extra / count;

  for (node = node->child; node; node = node->next)
    {
      grub_widget_t child;
      int flags, valign, halign;

      child = node->data;
      flags = (node->flags & GRUB_WIDGET_FLAG_FIXED_XY);
      if ((! child) || (child->width <= 0) || (child->height <= 0) ||
	  (flags == GRUB_WIDGET_FLAG_FIXED_XY))
	continue;

      halign = HALIGN_EXTEND;
      p = grub_widget_get_prop (node, "halign");
      if (p)
	{
	  if (! grub_strcmp (p, "left"))
	    halign = HALIGN_LEFT;
	  else if (! grub_strcmp (p, "center"))
	    halign = HALIGN_CENTER;
	  else if (! grub_strcmp (p, "right"))
	    halign = HALIGN_RIGHT;
	}

      valign = VALIGN_EXTEND;
      p = grub_widget_get_prop (node, "valign");
      if (p)
	{
	  if (! grub_strcmp (p, "top"))
	    valign = VALIGN_TOP;
	  else if (! grub_strcmp (p, "center"))
	    valign = VALIGN_CENTER;
	  else if (! grub_strcmp (p, "bottom"))
	    valign = VALIGN_BOTTOM;
	}

      if (flags == GRUB_WIDGET_FLAG_FIXED_X)
	align_y (child, valign, widget->inner_height);
      else if (flags == GRUB_WIDGET_FLAG_FIXED_Y)
	align_x (child, halign, widget->inner_width);
      else
	{
	  if (horizontal)
	    {
	      int w;

	      w = child->width;
	      if (node->flags & GRUB_WIDGET_FLAG_EXTEND)
		w += extra;

	      if (reverse)
		{
		  child->x = width - w;
		  width -= w + space;
		}
	      else
		{
		  child->x = width;
		  width += w + space;
		}

	      if (node->flags & GRUB_WIDGET_FLAG_EXTEND)
		align_x (child, halign, w);
	      align_y (child, valign, widget->inner_height);
	    }
	  else
	    {
	      int h;

	      h = child->height;
	      if (node->flags & GRUB_WIDGET_FLAG_EXTEND)
		h += extra;

	      if (reverse)
		{
		  child->y = height - h;
		  height -= h + space;
		}
	      else
		{
		  child->y = height;
		  height += h + space;
		}

	      if (node->flags & GRUB_WIDGET_FLAG_EXTEND)
		align_y (child, valign, h);
	      align_x (child, halign, widget->inner_width);
	    }
	}
    }
}

static void
init_widget (grub_uitree_t node)
{
  grub_uitree_t child;
  grub_widget_t widget;

  child = node->child;
  while (child)
    {
      init_widget (child);
      child = child->next;
    }

  widget = node->data;
  adjust_layout (widget, 1);
  if (widget->class->init_size)
    widget->class->init_size (widget);
}

void
grub_widget_init (grub_uitree_t node)
{
  grub_uitree_t child;

  grub_menu_region_get_screen_size (&screen_width, &screen_height);

  init_widget (node);

  init_size (node->data, 0);
  child = node;
  while (child)
    {
      grub_widget_t widget;

      widget = child->data;
      widget->inner_width = widget->width;
      widget->inner_height = widget->height;

      if (child->parent)
	{
	  grub_widget_t parent = child->parent->data;
	  widget->org_x = parent->org_x + parent->inner_x + widget->x;
	  widget->org_y = parent->org_y + parent->inner_y + widget->y;
	}

      if (widget->class->fini_size)
	widget->class->fini_size (widget);
      adjust_layout (widget, 0);

      if ((widget->width > 0) && (widget->height > 0))
	{
	  if (grub_widget_get_prop (child, "anchor"))
	    child->flags |= GRUB_WIDGET_FLAG_ANCHOR;

	  if (grub_widget_get_prop (child, "command"))
	    child->flags |= GRUB_WIDGET_FLAG_NODE;
	}

      child = grub_tree_next_node (GRUB_AS_TREE (node), GRUB_AS_TREE (child));
    }
}

void
grub_widget_free (grub_uitree_t node)
{
  grub_uitree_t child;

  child = node;
  while (child)
    {
      grub_widget_t widget;

      widget = child->data;
      if (widget)
	{
	  if (widget->class->free)
	    widget->class->free (widget);

	  grub_free (widget);
	  child->data = 0;
	}

      child = grub_tree_next_node (GRUB_AS_TREE (node), GRUB_AS_TREE (child));
    }
}

static void
draw_child (grub_menu_region_update_list_t *head, grub_uitree_t node,
	    int x, int y, int width, int height)
{
  grub_widget_t widget;
  grub_uitree_t child;

  widget = node->data;
  for (child = node->child; child; child = child->next)
    {
      grub_widget_t c;
      int cx, cy, cw, ch;

      if (child->flags & GRUB_WIDGET_FLAG_HIDDEN)
	continue;

      c = child->data;
      cx = widget->org_x + x - c->org_x;
      cy = widget->org_y + y - c->org_y;
      cw = width;
      ch = height;

      if (grub_menu_region_check_rect (&cx, &cy, &cw, &ch, 0, 0,
					c->width, c->height))
	{
	  if (c->class->draw)
	    c->class->draw (c, head, cx, cy, cw, ch);

	  if (grub_menu_region_check_rect (&cx, &cy, &cw, &ch,
					   c->inner_x, c->inner_y,
					   c->inner_width,c->inner_height))
	    draw_child (head, child, cx, cy, cw, ch);
	}
    }
}

static void
draw_parent (grub_menu_region_update_list_t *head, grub_uitree_t node,
	     int x, int y, int width, int height)
{
  grub_widget_t widget;

  widget = node->data;
  if (node->flags & GRUB_WIDGET_FLAG_TRANSPARENT)
    {
      grub_widget_t p;

      p = node->parent->data;
      draw_parent (head, node->parent, x + widget->x + p->inner_x,
		   y + widget->y + p->inner_y, width, height);
    }

  if (widget->class->draw)
    widget->class->draw (widget, head, x, y, width, height);
}

void
grub_widget_draw_region (grub_menu_region_update_list_t *head,
			 grub_uitree_t node, int x, int y,
			 int width, int height)
{
  grub_widget_t w;
  grub_uitree_t c;

  if (node->flags & GRUB_WIDGET_FLAG_HIDDEN)
    return;

  w = node->data;
  if (! grub_menu_region_check_rect (&x, &y, &width, &height,
				     0, 0, w->width, w->height))
    return;

  c = node;
  while (1)
    {
      grub_widget_t p;

      if ((! c->parent) || (! c->parent->data))
	break;

      p = c->parent->data;
      x += ((grub_widget_t) c->data)->x;
      y += ((grub_widget_t) c->data)->y;
      if (! grub_menu_region_check_rect (&x, &y, &width, &height,
					 0, 0,
					 p->inner_width, p->inner_height))
	return;

      x += p->inner_x;
      y += p->inner_y;
      c = c->parent;
    }

  x += ((grub_widget_t) c->data)->org_x - w->org_x;
  y += ((grub_widget_t) c->data)->org_y - w->org_y;

  draw_parent (head, node, x, y, width, height);
  if (! grub_menu_region_check_rect (&x, &y, &width, &height,
				     w->inner_x, w->inner_y,
				     w->inner_width, w->inner_height))
    return;
  draw_child (head, node, x, y, width, height);
}

void
grub_widget_draw (grub_uitree_t node)
{
  grub_widget_t widget;

  widget = node->data;
  if (widget)
    {
      grub_menu_region_update_list_t head;

      head = 0;
      grub_widget_draw_region (&head, node, 0, 0, widget->width,
			       widget->height);
      grub_menu_region_apply_update (head);
    }
}

static void
update_position (grub_uitree_t parent, grub_uitree_t node)
{
  grub_widget_t p, w;

  p = parent->data;
  w = node->data;
  if ((p) && (w))
    {
      grub_uitree_t child;

      w->org_x = p->org_x + p->inner_x + w->x;
      w->org_y = p->org_y + p->inner_y + w->y;
      for (child = node->child; child; child = child->next)
	update_position (node, child);
    }
}

static void
scroll_node (grub_uitree_t node, int dx, int dy)
{
  grub_uitree_t child;

  for (child = node->child; child; child = child->next)
    {
      grub_widget_t widget;

      widget = child->data;
      if ((! widget) || (child->flags & GRUB_WIDGET_FLAG_FIXED_XY))
	continue;

      widget->x += dx;
      widget->y += dy;
      update_position (node, child);
    }
}

static grub_uitree_t
grub_widget_scroll (grub_uitree_t node)
{
  grub_uitree_t save;
  grub_widget_t widget;
  int x, y, width, height;

  save = node;
  widget = node->data;
  x = 0;
  y = 0;
  width = widget->width;
  height = widget->height;
  while (1)
    {
      grub_widget_t parent;
      int dx;
      int dy;

      if (! node->parent)
	break;

      parent = node->parent->data;

      if (widget->width <= parent->inner_width)
	{
	  x = 0;
	  width = widget->width;
	}

      if (widget->height <= parent->inner_height)
	{
	  y = 0;
	  height = widget->height;
	}

      x += widget->x;
      y += widget->y;

      dx = 0;
      dy = 0;
      if (x + width > parent->inner_width)
	{
	  dx = parent->inner_width - width - x;
	  x += dx;
	}

      if (y + height > parent->inner_height)
	{
	  dy = parent->inner_height - height - y;
	  y += dy;
	}

      if (x < 0)
	{
	  dx += -x;
	  x = 0;
	}

      if (y < 0)
	{
	  dy += -y;
	  y = 0;
	}

      if ((dx) || (dy))
	{
	  save = node->parent;
	  if (node->flags & GRUB_WIDGET_FLAG_FIXED_XY)
	    {
	      widget->x += dx;
	      widget->y += dy;
	      update_position (save, node);
	    }
	  else
	    scroll_node (save, dx, dy);
	}

      if (! grub_menu_region_check_rect (&x, &y, &width, &height,
					 0, 0,
					 parent->inner_width,
					 parent->inner_height))
	return node;

      x += parent->inner_x;
      y += parent->inner_y;

      node = node->parent;
      widget = node->data;
    }

  return save;
}

#define DIR_NEXT	1
#define DIR_ANCHOR	2

static char *
get_dir_cmd (grub_uitree_t node, int k)
{
  int horizontal, reverse;
  int dir;

  if (node->parent)
    get_direction (node->parent, &horizontal, &reverse);
  else
    horizontal = reverse = 0;

  if (k == GRUB_TERM_LEFT)
    dir = DIR_ANCHOR;
  else if (k == GRUB_TERM_RIGHT)
    dir = DIR_ANCHOR | DIR_NEXT;
  else if (k == GRUB_TERM_UP)
    dir = 0;
  else if (k == GRUB_TERM_DOWN)
    dir = DIR_NEXT;
  else
    return 0;

  if (horizontal)
    dir ^= DIR_ANCHOR;

  if ((reverse) & (! (dir & DIR_ANCHOR)))
    dir ^= DIR_NEXT;

  if (dir == 0)
    return "ui_prev_node";
  else if (dir == DIR_NEXT)
    return "ui_next_node";
  else if (dir == DIR_ANCHOR)
    return "ui_prev_anchor";
  else
    return "ui_next_anchor";
}

static grub_uitree_t
find_selected_node (grub_uitree_t root)
{
  grub_uitree_t node;

  node = root->child;
  while (node)
    {
      if (node->flags & GRUB_WIDGET_FLAG_SELECTED)
	{
	  if (node->flags & GRUB_WIDGET_FLAG_NODE)
	    return node;
	  else
	    node = node->child;
	}
      else
	node = node->next;
    }

  return 0;
}

static int
map_key (int key)
{
  grub_uitree_t map;
  const char *name;

  map = grub_uitree_find (&grub_uitree_root, "mapkey");
  if (! map)
    return key;

  name = grub_menu_key2name (key);
  if (! name)
    return key;

  name = grub_uitree_get_prop (map, name);
  return (name) ? grub_menu_name2key (name) : key;
}

static char *
onkey (int key)
{
  grub_uitree_t map;
  const char *name;

  map = grub_uitree_find (&grub_uitree_root, "onkey");
  if (! map)
    return 0;

  name = grub_menu_key2name (key);
  if (! name)
    return 0;

  return grub_uitree_get_prop (map, name);
}

void
grub_widget_select_node (grub_uitree_t node, int selected)
{
  grub_uitree_t child;

  child = node->child;
  while (child)
    {
      if (selected)
	child->flags |= GRUB_WIDGET_FLAG_SELECTED;
      else
	child->flags &= ~GRUB_WIDGET_FLAG_SELECTED;
      child = grub_tree_next_node (GRUB_AS_TREE (node), GRUB_AS_TREE (child));
    }

  while (node)
    {
      if (selected)
	node->flags |= GRUB_WIDGET_FLAG_SELECTED;
      else
	node->flags &= ~GRUB_WIDGET_FLAG_SELECTED;
      node = node->parent;
    }
}

static void
change_node (grub_uitree_t prev, grub_uitree_t node)
{
  grub_uitree_t child, parent, scroll;
  grub_uitree_t dyn_prev, dyn_node;

  child = node;
  while (child)
    {
      child->flags |= GRUB_WIDGET_FLAG_MARKED;
      child = child->parent;
    }

  dyn_prev = parent = prev;
  prev = 0;
  while (! (parent->flags & GRUB_WIDGET_FLAG_MARKED))
    {
      prev = parent;
      if (prev->flags & GRUB_WIDGET_FLAG_DYNAMIC)
	dyn_prev = prev;
      parent = parent->parent;
    }

  child = node;
  while (child)
    {
      child->flags &= ~GRUB_WIDGET_FLAG_MARKED;
      child = child->parent;
    }

  dyn_node = node;
  scroll = grub_widget_scroll (node);
  while (1)
    {
      grub_uitree_t p;

      if (node == scroll)
	scroll = 0;
      p = node->parent;
      if (p == parent)
	break;
      node = p;
      if (node->flags & GRUB_WIDGET_FLAG_DYNAMIC)
	dyn_node = node;
    }

  if (scroll)
    grub_widget_draw (scroll);
  else if (! prev)
    grub_widget_draw (parent);
  else
    {
      grub_widget_draw (dyn_prev);
      grub_widget_draw (dyn_node);
    }
}

static grub_uitree_t
find_next_node (grub_uitree_t anchor, grub_uitree_t node)
{
  grub_uitree_t n;

  n = node;
  do
    {
      n = grub_tree_next_node (GRUB_AS_TREE (anchor), GRUB_AS_TREE (n));

      if (! n)
	n = anchor;

      if (n == node)
	return 0;
    }
  while (! (n->flags & GRUB_WIDGET_FLAG_NODE));

  return n;
}

static grub_uitree_t
find_prev_node (grub_uitree_t anchor, grub_uitree_t node)
{
  grub_uitree_t save, n;

  save = 0;
  n = anchor;
  while (n)
    {
      if (n == node)
	{
	  if (save)
	    break;
	}
      else
	{
	  if (n->flags & GRUB_WIDGET_FLAG_NODE)
	    save = n;
	}

      n = grub_tree_next_node (GRUB_AS_TREE (anchor), GRUB_AS_TREE (n));
    }

  return save;
}

static grub_uitree_t
run_dir_cmd (char *name, grub_uitree_t current_node)
{
  grub_uitree_t root, next, node, anchor, save;

  root = current_node;
  anchor = 0;
  while (root)
    {
      if ((root->flags & GRUB_WIDGET_FLAG_ANCHOR) && (! anchor))
	anchor = root;

      if (root->flags & GRUB_WIDGET_FLAG_ROOT)
	break;

      root = root->parent;
    }

  if ((name[8] == 'a') && (root != anchor))
    {
      save = anchor->child;
      anchor->child = 0;
      node = anchor;
      anchor = root;
    }
  else
    {
      save = 0;
      node = current_node;
    }

  next = (name[3] == 'n') ? find_next_node (anchor, node) :
    find_prev_node (anchor, node);

  if (save)
    node->child = save;

  return next;
}

static int
check_timeout (grub_uitree_t root, int *key)
{
  char *p;
  int total, left, has_key;
  grub_uint64_t last_time;

  p = grub_env_get ("timeout");
  if (! p)
    return 0;

  total = grub_strtol (p, 0, 0);
  if (total < 0)
    return 0;

  has_key = (grub_checkkey () >= 0);
  if ((has_key) || (! total))
    {
      *key = (has_key) ? GRUB_TERM_ASCII_CHAR (grub_getkey ()) : '\r';
      return 0;
    }

  grub_widget_draw (root);
  root = grub_uitree_find_id (root, "__timeout__");
  total *= 1000;
  left = total;
  *key = '\r';
  last_time = grub_get_time_ms ();
  while (left > 0)
    {
      grub_uint64_t delta;

      if (grub_checkkey () >= 0)
	{
	  *key = GRUB_TERM_ASCII_CHAR (grub_getkey ());
	  left = 0;
	  break;
	}

      if (root)
	{
	  grub_uitree_t child;
	  grub_menu_region_update_list_t head;

	  head = 0;
	  child = root;
	  while (child)
	    {
	      grub_widget_t widget;

	      widget = child->data;
	      if ((widget) && (widget->class->set_timeout))
		{
		  widget->class->set_timeout (widget, total, left);
		  if (left > 0)
		    widget->class->draw (widget, &head, 0, 0,
					 widget->width, widget->height);
		}

	      child = grub_tree_next_node (GRUB_AS_TREE (root),
					   GRUB_AS_TREE (child));
	    }
	  grub_menu_region_apply_update (head);
	}

      delta = grub_get_time_ms () - last_time;
      last_time += delta;
      left -= delta;
    }

  if (root)
    {
      grub_widget_t widget, parent;
      grub_menu_region_update_list_t head;

      widget = root->data;
      parent = root->parent->data;
      head = 0;
      if (p)
	draw_parent (&head, root->parent, widget->x + parent->inner_x,
		     widget->y + parent->inner_y,
		     widget->width, widget->height);
      grub_menu_region_apply_update (head);
      root->flags |= GRUB_WIDGET_FLAG_HIDDEN;
    }

  return 1;
}

int
grub_widget_input (grub_uitree_t root, int nested)
{
  int init, c;

  root->flags |= (GRUB_WIDGET_FLAG_ROOT | GRUB_WIDGET_FLAG_ANCHOR);

  grub_widget_current_node = find_selected_node (root);
  if (! grub_widget_current_node)
    {
      grub_widget_current_node = find_next_node (root, root);
      if (! grub_widget_current_node)
	grub_widget_current_node = root;
      else
	grub_widget_select_node (grub_widget_current_node, 1);
    }

  init = c = 0;
  if (! nested)
    init = check_timeout (root, &c);

  if (! init)
    {
      grub_uitree_t node;

      node = grub_uitree_find_id (root, "__timeout__");
      if (node)
	node->flags |= GRUB_WIDGET_FLAG_HIDDEN;
    }

  while (1)
    {
      char *cmd, *users;

      users = 0;
      cmd = onkey (c);
      if (! cmd)
	{
	  if (c == '\r')
	    {
	      cmd = grub_widget_get_prop (grub_widget_current_node, "command");
	      users = grub_widget_get_prop (grub_widget_current_node, "users");
	      c = 0;
	    }
	  else if (c == GRUB_TERM_ESC)
	    cmd = "ui_escape";
	  else if (c == GRUB_TERM_TAB)
	    cmd = "ui_next_anchor";
	  else
	    cmd = get_dir_cmd (grub_widget_current_node, c);
	}
      else if (*cmd == '*')
	{
	  cmd++;
	  users = "";
	}

      if ((cmd) && (*cmd))
	{
	  if ((users) && (! grub_dialog_password (users)))
	    {
	      grub_errno = 0;
	    }
	  else if (! grub_strcmp (cmd, "ui_escape"))
	    {
	      if (nested)
		return GRUB_ERR_MENU_ESCAPE;
	    }
	  else if (! grub_strcmp (cmd, "ui_quit"))
	    {
	      return grub_error (GRUB_ERR_MENU_ESCAPE, "quit");
	    }
	  else if ((! grub_strcmp (cmd, "ui_next_node")) ||
		   (! grub_strcmp (cmd, "ui_prev_node")) ||
		   (! grub_strcmp (cmd, "ui_next_anchor")) ||
		   (! grub_strcmp (cmd, "ui_prev_anchor")))
	    {
	      grub_uitree_t next;

	      next = run_dir_cmd (cmd, grub_widget_current_node);
	      if (next)
		{
		  grub_widget_select_node (grub_widget_current_node, 0);
		  grub_widget_select_node (next, 1);

		  if (init)
		    change_node (grub_widget_current_node, next);
		  grub_widget_current_node = next;
		}
	    }
	  else if (! grub_memcmp (cmd, "ui_next_class", 13))
	    {
	      char *class = cmd + 13;

	      while (*class == ' ')
		class++;

	      if (! *class)
		{
		  char *parm;
		  parm = grub_uitree_get_prop (grub_widget_current_node,
					       "parameters");
		  class = grub_dialog_get_parm (grub_widget_current_node,
						parm, "class");
		}
	      if (class)
		{
		  grub_uitree_t cur, next;

		  cur = grub_widget_current_node;
		  while (1)
		    {
		      char *parm, *ncls;

		      next = run_dir_cmd ("ui_next_node", cur);
		      if ((! next) || (next == grub_widget_current_node))
			break;

		      parm = grub_uitree_get_prop (next, "parameters");
		      ncls = grub_dialog_get_parm (next, parm, "class");
		      if ((ncls) && (! grub_strcmp (class, ncls)))
			{
			  grub_widget_select_node (grub_widget_current_node,
						   0);
			  grub_widget_select_node (next, 1);

			  if (init)
			    change_node (grub_widget_current_node, next);
			  grub_widget_current_node = next;
			  break;
			}
		      cur = next;
		    }
		}
	    }
	  else
	    {
	      int r;

	      if ((! c) && (! nested))
		{
		  char *index;

		  index = grub_uitree_get_prop (grub_widget_current_node,
						"index");
		  if (index)
		    grub_env_set ("chosen", index);
		}

	      r = grub_parser_execute (cmd);

	      if (grub_widget_refresh)
		return grub_errno;

	      if (grub_errno == GRUB_ERR_NONE && grub_loader_is_loaded ())
		grub_command_execute ("boot", 0, 0);

	      if ((r >= 0) && (r != GRUB_ERR_MENU_ESCAPE) && (nested))
		return r;

	      grub_errno = 0;
	    }
	}

      if (! init)
	{
	  grub_widget_draw (root);
	  init++;
	}

      while (1)
	{
	  grub_widget_t widget;

	  widget = grub_widget_current_node->data;
	  if (widget->class->draw_cursor)
	    widget->class->draw_cursor (widget);

	  c = map_key (GRUB_TERM_ASCII_CHAR (grub_getkey ()));
	  if (widget->class->onkey)
	    {
	      int r;

	      r = widget->class->onkey (widget, c);
	      if (grub_widget_refresh)
		return r;

	      if ((r >= 0) && (nested))
		return r;
	      else if (r == GRUB_WIDGET_RESULT_SKIP)
		break;
	    }
	  else
	    break;
	}
    }
}

static grub_uitree_t
find_child (grub_uitree_t node, const char *name)
{
  grub_uitree_t child;

  if (! *name)
    return 0;

  child = node->child;
  while (child)
    {
      if (! grub_strcmp (child->name, name))
	break;

      child = child->next;
    }

  return child;
}

char *
grub_widget_get_prop (grub_uitree_t node, const char *name)
{
  grub_uitree_t class_node;
  grub_uitree_t child;
  char *prop, *class;

  prop = grub_uitree_get_prop (node, name);
  if (prop)
    return prop;

  class_node = grub_uitree_find (&grub_uitree_root, "class");
  if (! class_node)
    return 0;

  class = grub_uitree_get_prop (node, "class");
  while (class)
    {
      char *p;

      p = grub_menu_next_field (class, ',');
      child = find_child (class_node, class);
      grub_menu_restore_field (p, ',');
      if (child)
	{
	  prop = grub_uitree_get_prop (child, name);
	  if (prop)
	    return prop;
	}
      class = p;
    }

  prop = 0;
  child = find_child (class_node, node->name);
  if (child)
    prop = grub_uitree_get_prop (child, name);

  return prop;
}

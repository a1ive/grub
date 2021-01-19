/* gui_activity_bar.c - GUI activity bar component, based on GUI progress bar */
/*
 *  Extention for GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2008,2009  Free Software Foundation, Inc.
 *  Copyright (C) 2014 Miray Software <oss@miray.de>
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

#include <grub/mm.h>
#include <grub/misc.h>
#include <grub/gui.h>
#include <grub/font.h>
#include <grub/gui_string_util.h>
#include <grub/gfxmenu_view.h>
#include <grub/gfxwidgets.h>
#include <grub/i18n.h>
#include <grub/color.h>

#include <grub/time.h>
#include "gui_activity_bar.h"
#include "grub/miray_activity.h"
#include "miray_screen.h"
#include "miray_gfx_screen.h"

static const unsigned bar_highlight_width = 31;
static const unsigned bar_step            = 1;

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

struct grub_gui_activity_bar
{
  struct grub_gui_activity activity;

  grub_gui_container_t parent;
  grub_video_rect_t bounds;
  char *id;
  int visible;
  unsigned step;
  int value;
  int end;
  unsigned highlight_width;
  grub_video_rgba_color_t border_color;
  grub_video_rgba_color_t bg_color;
  grub_video_rgba_color_t fg_color;

  char *theme_dir;
  int need_to_recreate_pixmaps;
  int pixmapbar_available;
  char *bar_pattern;
  char *highlight_pattern;
  grub_gfxmenu_box_t bar_box;
  grub_gfxmenu_box_t highlight_box;
  int highlight_overlay;
};

typedef struct grub_gui_activity_bar *grub_gui_activity_bar_t;

static void
activity_bar_destroy (void *vself)
{
  grub_gui_activity_bar_t self = vself;
  grub_free (self->theme_dir);
  grub_free (self->id);
  grub_free (self);
}

static const char *
activity_bar_get_id (void *vself)
{
  grub_gui_activity_bar_t self = vself;
  return self->id;
}

static int
activity_bar_is_instance (void *vself __attribute__((unused)), const char *type)
{
  return grub_strcmp (type, "component") == 0;
}

#if 0
static int
check_pixmaps (grub_gui_activity_bar_t self)
{
  if (!self->pixmapbar_available)
    return 0;
  if (self->need_to_recreate_pixmaps)
    {
      grub_gui_recreate_box (&self->bar_box,
                             self->bar_pattern,
                             self->theme_dir);

      grub_gui_recreate_box (&self->highlight_box,
                             self->highlight_pattern,
                             self->theme_dir);

      self->need_to_recreate_pixmaps = 0;
    }

  return (self->bar_box != 0 && self->highlight_box != 0);
}
#endif

static void
draw_filled_rect_bar (grub_gui_activity_bar_t self, unsigned int barstart, unsigned int barend)
{
  /* Set the progress bar's frame.  */
  grub_video_rect_t f;
  f.x = 1;
  f.y = 1;
  //f.width = self->bounds.width - 2;
  //f.height = self->bounds.height - 2;
   f.width = self->activity.component.w;
   f.height = self->activity.component.h;

  /* Border.  */
  grub_video_fill_rect (grub_video_map_rgba_color (self->border_color),
                        f.x - 1, f.y - 1,
                        f.width + 2, f.height + 2);


  barstart = max(barstart, f.x);
  barend = min((unsigned int)barend, f.width + f.x);
  

  /* Bar background */
  if ((unsigned int)barstart > f.x)
    grub_video_fill_rect (grub_video_map_rgba_color (self->bg_color),
                        f.x, f.y,
                        barstart - f.x, f.height);
  if ((unsigned int)barend < f.width + f.x)
    grub_video_fill_rect (grub_video_map_rgba_color (self->bg_color),
                        barend + 1, f.y,
                        f.width - (barend + 1), f.height);
 
  /* Bar foreground.  */
  grub_video_fill_rect (grub_video_map_rgba_color (self->fg_color),
                        barstart, f.y,
                        barend - barstart + 1, f.height);
}

#if 0
static void
draw_pixmap_bar (grub_gui_activity_bar_t self)
{
  grub_gfxmenu_box_t bar = self->bar_box;
  grub_gfxmenu_box_t hl = self->highlight_box;
  int w = self->bounds.width;
  int h = self->bounds.height;
  int bar_l_pad = bar->get_left_pad (bar);
  int bar_r_pad = bar->get_right_pad (bar);
  int bar_t_pad = bar->get_top_pad (bar);
  int bar_b_pad = bar->get_bottom_pad (bar);
  int bar_h_pad = bar_l_pad + bar_r_pad;
  int bar_v_pad = bar_t_pad + bar_b_pad;
  int hl_l_pad = hl->get_left_pad (hl);
  int hl_r_pad = hl->get_right_pad (hl);
  int hl_t_pad = hl->get_top_pad (hl);
  int hl_b_pad = hl->get_bottom_pad (hl);
  int hl_h_pad = hl_l_pad + hl_r_pad;
  int hl_v_pad = hl_t_pad + hl_b_pad;
  int tracklen = w - bar_h_pad;
  int trackheight = h - bar_v_pad;
  int barwidth;
  int hlheight = trackheight;
  int hlx = bar_l_pad;
  int hly = bar_t_pad;

  bar->set_content_size (bar, tracklen, trackheight);
  bar->draw (bar, 0, 0);

  if (self->highlight_overlay)
    {
      tracklen += hl_h_pad;
      hlx -= hl_l_pad;
      hly -= hl_t_pad;
    }
  else
    hlheight -= hl_v_pad;

  if (self->value <= self->start
      || self->end <= self->start)
    barwidth = 0;
  else
    barwidth = ((unsigned) (tracklen * (self->value - self->start))
		/ ((unsigned) (self->end - self->start)));

  if (barwidth >= hl_h_pad)
    {
      hl->set_content_size (hl, barwidth - hl_h_pad, hlheight);
      hl->draw (hl, hlx, hly);
    }
}
#endif

static void
activity_bar_paint (void *vself, const grub_video_rect_t *region)
{
  grub_gui_activity_bar_t self = vself;
  grub_video_rect_t vpsave;

  if (! self->visible)
    return;
  if (!grub_video_have_common_points (region, &self->bounds))
    return;

  //if (self->end == self->start)
  //  return;

  grub_gui_set_viewport (&self->bounds, &vpsave);

  //if (check_pixmaps (self))
  //  draw_pixmap_bar (self);
  //else

  int barstart = self->value;
  int barend = self->value + self->highlight_width;

  draw_filled_rect_bar (self, barstart, barend);

  grub_gui_restore_viewport (&vpsave);
}


static void
progress_bar_paint (void *vself, const grub_video_rect_t *region)
{
  grub_gui_activity_bar_t self = vself;
  grub_video_rect_t vpsave;

  if (! self->visible)
    return;
  if (!grub_video_have_common_points (region, &self->bounds))
    return;

  if (self->end == 0)
    return;

  //if (self->end == self->start)
  //  return;

  grub_gui_set_viewport (&self->bounds, &vpsave);

  //if (check_pixmaps (self))
  //  draw_pixmap_bar (self);
  //else

  int barstart = (int)0;
  int barend   = self->end != 0 ? grub_divmod64(self->value * (grub_uint64_t)self->activity.component.w, self->end, 0) : 0;
  

  draw_filled_rect_bar (self, barstart, barend);

  grub_gui_restore_viewport (&vpsave);
}


static void
activity_bar_set_parent (void *vself, grub_gui_container_t parent)
{
  grub_gui_activity_bar_t self = vself;
  self->parent = parent;
}

static grub_gui_container_t
activity_bar_get_parent (void *vself)
{
  grub_gui_activity_bar_t self = vself;
  return self->parent;
}

static void
activity_bar_set_bounds (void *vself, const grub_video_rect_t *bounds)
{
  grub_gui_activity_bar_t self = vself;
  self->bounds = *bounds;
}

static void
activity_bar_get_bounds (void *vself, grub_video_rect_t *bounds)
{
  grub_gui_activity_bar_t self = vself;
  *bounds = self->bounds;
}

static void
activity_bar_get_minimal_size (void *vself __attribute__((unused)),
			       unsigned *width, unsigned *height)
{
  unsigned min_width = 0;
  unsigned min_height = 0;
  //grub_gui_activity_bar_t self = vself;

#if 0
  if (check_pixmaps (self))
    {
      grub_gfxmenu_box_t bar = self->bar_box;
      grub_gfxmenu_box_t hl = self->highlight_box;
      min_width += bar->get_left_pad (bar) + bar->get_right_pad (bar);
      min_height += bar->get_top_pad (bar) + bar->get_bottom_pad (bar);
      if (!self->highlight_overlay)
        {
          min_width += hl->get_left_pad (hl) + hl->get_right_pad (hl);
          min_height += hl->get_top_pad (hl) + hl->get_bottom_pad (hl);
        }
    }
  else
#endif
    {
      min_height += 2;
      min_width += 2;
    }
  *width = 200;
  if (*width < min_width)
    *width = min_width;
  *height = 28;
  if (*height < min_height)
    *height = min_height;
}


static void
activity_bar_set_state (void *vself, int visible, int start __attribute__ ((unused)),
			int current, int end __attribute__ ((unused)))
{
  if (current != gui_activity_bar_advance_val)
    return;

  grub_gui_activity_bar_t self = vself;  
  self->visible = visible;
  self->value = self->value + self->step;
  if (self->value >= (int)self->bounds.width - 2)
    self->value = self->step - self->highlight_width;
}



static void
progress_bar_set_state (void *vself, int visible, int start __attribute__ ((unused)),
			int current, int end)
{
  if (current == gui_activity_bar_advance_val)
    return;

  grub_gui_activity_bar_t self = vself;  

  self->visible = visible;
  self->value = current;
  self->end = end;
}

#if 0
static void
activity_bar_advance(void *vself)
{
  grub_gui_activity_bar_t self = vself;  
  self->visible = 1;
  self->value = self->value + self->step;
  if (self->value >= (int)self->bounds.width - 2)
    self->value = self->step - self->highlight_width;

  miray_screen_draw_activity();
}
#endif

static void
activity_bar_finish(void *vself)
{
  grub_gui_activity_bar_t self = vself;  

  while (self->value > 0)
  {
	 grub_millisleep (miray_activity_tick_throttle_ms);
    activity_bar_set_state(vself, 1, 0, gui_activity_bar_advance_val, 0);
    miray_screen_draw_activity();
  }

  while (self->value < 0)
  {
    grub_millisleep (miray_activity_tick_throttle_ms);
    activity_bar_set_state(vself, 1, 0, gui_activity_bar_advance_val, 0);
    miray_screen_draw_activity();
  }

  while (self->highlight_width < self->bounds.width)
  {
    grub_millisleep (miray_activity_tick_throttle_ms);
    self->highlight_width += self->step;
    miray_screen_draw_activity();
  }

  grub_millisleep (miray_activity_tick_throttle_ms);
}

static grub_err_t
activity_bar_set_property (void *vself, const char *name, const char *value)
{
  grub_gui_activity_bar_t self = vself;
  if (grub_strcmp (name, "border_color") == 0)
    {
       grub_video_parse_color (value, &self->border_color);
    }
  else if (grub_strcmp (name, "bg_color") == 0)
    {
       grub_video_parse_color (value, &self->bg_color);
    }
  else if (grub_strcmp (name, "fg_color") == 0)
    {
      grub_video_parse_color (value, &self->fg_color);
    }
  else if (grub_strcmp (name, "bar_style") == 0)
    {
      self->need_to_recreate_pixmaps = 1;
      self->pixmapbar_available = 1;
      grub_free (self->bar_pattern);
      self->bar_pattern = value ? grub_strdup (value) : 0;
    }
  else if (grub_strcmp (name, "highlight_style") == 0)
    {
      self->need_to_recreate_pixmaps = 1;
      self->pixmapbar_available = 1;
      grub_free (self->highlight_pattern);
      self->highlight_pattern = value ? grub_strdup (value) : 0;
    }
  else if (grub_strcmp (name, "highlight_overlay") == 0)
    {
      self->highlight_overlay = grub_strcmp (value, "true") == 0;
    }
  else if (grub_strcmp (name, "theme_dir") == 0)
    {
      self->need_to_recreate_pixmaps = 1;
      grub_free (self->theme_dir);
      self->theme_dir = value ? grub_strdup (value) : 0;
    }
  else if (grub_strcmp (name, "id") == 0)
    {
      grub_gfxmenu_timeout_unregister ((grub_gui_component_t) self);
      grub_free (self->id);
      if (value)
        self->id = grub_strdup (value);
      else
        self->id = 0;
    }
  else if (grub_strcmp (name, "activity_finish") == 0)
  {
    if (self->step != 0) activity_bar_finish(self);
  }
  return grub_errno;
}

static struct grub_gui_component_ops activity_bar_ops =
{
  .destroy = activity_bar_destroy,
  .get_id = activity_bar_get_id,
  .is_instance = activity_bar_is_instance,
  .paint = activity_bar_paint,
  .set_parent = activity_bar_set_parent,
  .get_parent = activity_bar_get_parent,
  .set_bounds = activity_bar_set_bounds,
  .get_bounds = activity_bar_get_bounds,
  .get_minimal_size = activity_bar_get_minimal_size,
  .set_property = activity_bar_set_property
};

static struct grub_gui_component_ops progress_bar_ops =
{
  .destroy = activity_bar_destroy,
  .get_id = activity_bar_get_id,
  .is_instance = activity_bar_is_instance,
  .paint = progress_bar_paint,
  .set_parent = activity_bar_set_parent,
  .get_parent = activity_bar_get_parent,
  .set_bounds = activity_bar_set_bounds,
  .get_bounds = activity_bar_get_bounds,
  .get_minimal_size = activity_bar_get_minimal_size,
  .set_property = activity_bar_set_property
};


static struct grub_gui_progress_ops activity_bar_pg_ops =
{
  .set_state = activity_bar_set_state
};

static struct grub_gui_progress_ops progress_bar_pg_ops =
{
  .set_state = progress_bar_set_state
};


static grub_video_rgba_color_t black = { .red = 0, .green = 0, .blue = 0, .alpha = 255 };
static grub_video_rgba_color_t gray = { .red = 128, .green = 128, .blue = 128, .alpha = 255 };
static grub_video_rgba_color_t lightgray = { .red = 200, .green = 200, .blue = 200, .alpha = 255 };


grub_gui_component_t
grub_miray_gui_activity_bar_new (void)
{
  grub_gui_activity_bar_t self;
  self = grub_zalloc (sizeof (*self));
  if (! self)
    return 0;

  self->activity.ops  = &activity_bar_pg_ops;
  self->activity.component.ops = &activity_bar_ops;
  self->visible = 1;
  self->step = bar_step;
  self->highlight_width = bar_highlight_width;
  self->value = bar_step - bar_highlight_width;
  self->border_color = black;
  self->bg_color = gray;
  self->fg_color = lightgray;
  self->highlight_overlay = 0;

  return (grub_gui_component_t) self;
}



grub_gui_component_t
grub_miray_gui_progress_bar_new (void)
{
  grub_gui_activity_bar_t self;
  self = grub_zalloc (sizeof (*self));
  if (! self)
    return 0;

  self->activity.ops  = &progress_bar_pg_ops;
  self->activity.component.ops = &progress_bar_ops;
  self->visible = 1;
  self->step = 0;
  self->highlight_width = 0;
  self->value = 0;
  self->end = 0;
  self->border_color = black;
  self->bg_color = gray;
  self->fg_color = lightgray;
  self->highlight_overlay = 0;

  return (grub_gui_component_t) self;
}

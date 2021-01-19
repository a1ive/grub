/*
 *  Extention for GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2010-2014 Miray Software <oss@miray.de>
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

#include "miray_screen.h"
#include <grub/video.h>
#include <grub/gui.h>
#include <grub/env.h>
#include <grub/gfxterm.h>
#include "gui_activity_bar.h"
#include "miray_gfx_screen.h"

#define min(x, y) ((x) < (y) ? (x) : (y))

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif


#define MAX_MENU_ENTRIES 10

static const unsigned label_height = 20;

static const char * darkgray_c  = "#202020ff";
static const char * lightgray_c = "#b8b8b8ff";


static const char * message_text_color = "white";


static const unsigned menu_height = 40;
static const char * menu_text_color = "#b8b8b8ff";


enum fg_modes
{
   FG_HIDDEN,
   FG_DEFAULT,
   FG_ACTIVITY
};

struct miray_gfx_screen_data
{
   grub_video_rect_t screen_rect;
   unsigned int v_height;
   unsigned int v_width;
   unsigned int screen_offs_y_text;
   unsigned int screen_double_repaint;
   grub_font_t menu_font;
   grub_font_t text_font;
   grub_font_t timeout_font;

   /* Background */
   grub_video_rgba_color_t bg_color;
   grub_gui_container_t bg_canvas;
   grub_gui_component_t bg_center_image;

   grub_gui_container_t bg_label;
   grub_gui_component_t bg_label_image;

   /* Foreground */
   unsigned int fg_mode;

   grub_gui_container_t fg_timeout;
   grub_gui_component_t label_timeout1;
   grub_gui_component_t label_timeout2;

   grub_gui_container_t fg_default;
   grub_gui_component_t label_default;

   grub_gui_container_t fg_activity;
   grub_gui_component_t label_activity;
   grub_gui_component_t progress1_activity;
   grub_gui_component_t progress2_activity;
   unsigned int activity_cur;
   void * activity_hook;

   grub_video_rgba_color_t bg_menu;
   grub_video_rgba_color_t bg_menuseperator;
   grub_gui_container_t menu;
   unsigned int menu_entry_offset;
};
typedef struct miray_gfx_screen_data mgs_data_t;

struct miray_screen * miray_gfx_screen_new(struct grub_term_output *term);


static inline void _screen_reset_term(mgs_data_t * data)
{
  grub_font_t font;

   grub_video_set_active_render_target (GRUB_VIDEO_RENDER_TARGET_DISPLAY);
   grub_video_set_viewport (0, 0, data->v_width, data->v_height);
   grub_video_set_region(0, 0, data->v_width, data->v_height);

   grub_gfxterm_decorator_hook = 0;

   font = grub_font_get ("");
   if (font != 0)
   {
      grub_gfxterm_set_window (GRUB_VIDEO_RENDER_TARGET_DISPLAY,
				  0, 0, data->v_width, data->v_height,
				  data->screen_double_repaint,
				  font, 10);
   }
}


static inline void _gui_container_add(grub_gui_container_t cont, grub_gui_component_t child)
{
   if (cont != 0 && cont->ops != 0 && cont->ops->add != 0)
      cont->ops->add(cont, child);
}

static inline void _gui_container_remove(grub_gui_container_t cont, grub_gui_component_t child)
{
   if (cont != 0 && cont->ops != 0 && cont->ops->remove != 0)
      cont->ops->remove(cont, child);
}

struct container_clear_state
{
   grub_gui_container_t cont;
};

static void _gui_container_clear_cb(grub_gui_component_t comp, void *userdata)
{
   struct container_clear_state * state = userdata;
   state->cont->ops->remove(state->cont, comp);
}

static inline void _gui_container_clear(grub_gui_container_t cont)
{
   struct container_clear_state state;
   state.cont = cont;

   cont->ops->iterate_children(cont, _gui_container_clear_cb, &state);
}



static inline void _gui_destroy(grub_gui_component_t comp)
{
   if (comp != 0 && comp->ops != 0 && comp->ops->destroy != 0)
      comp->ops->destroy(comp);
}

static inline void _gui_paint(grub_gui_component_t comp, const grub_video_rect_t *bounds)
{
   if (comp != 0 && comp->ops != 0 && comp->ops->paint != 0)
      comp->ops->paint(comp, bounds);

}

static inline grub_err_t _gui_set_property(grub_gui_component_t comp, const char *name, const char *value)
{
   if (comp != 0 && comp->ops != 0 && comp->ops->set_property != 0)
      return comp->ops->set_property(comp, name, value);

   return grub_error(GRUB_ERR_BUG, "Invalid comp");
}

static inline void _gui_set_bounds (grub_gui_component_t comp, const grub_video_rect_t *bounds)
{
   if (comp != 0 && comp->ops != 0 && comp->ops->set_bounds != 0)
      comp->ops->set_bounds(comp, bounds);
}


static inline grub_gui_component_t _menu_component_new(grub_menu_entry_t entry)
{
   char * name = miray_screen_entry_name(entry);

   // Hidden or no shortcut
   if (name == 0)
      return 0;
   
   grub_gui_component_t ret = grub_gui_label_new();
   _gui_set_property(ret, "text", name);
   _gui_set_property(ret, "color", menu_text_color);
   _gui_set_property(ret, "font", screen_data.menu_font ? : "");

   grub_free(name);

   return ret;
}

static inline void _screen_paint_menu(mgs_data_t * data, const grub_video_rect_t * bounds)
{
   if (data->menu == 0)
      return;

   grub_video_rect_t menu_bounds;
   data->menu->component.ops->get_bounds(&data->menu->component, &menu_bounds);

   if (bounds->y + bounds->height >= menu_bounds.y - 1)
   {
      grub_video_set_viewport (0, 0, data->v_width, data->v_height);


      grub_video_fill_rect (grub_video_map_rgba_color (data->bg_menuseperator),
                            0,
                            data->v_height - menu_height - 1,
                            data->v_width,
                            1);      

      grub_video_fill_rect (grub_video_map_rgba_color (data->bg_menu),
                            0,
                            data->v_height - menu_height,
                            data->v_width,
                            menu_height);      


      menu_bounds.y = 0;
      _gui_paint(&data->menu->component, &menu_bounds);

   }
}

static inline void _set_single_progress_bar(mgs_data_t * data);
static inline void _set_dual_progress_bar(mgs_data_t * data);


inline static void _set_label_positions(mgs_data_t * data)
{
   static const unsigned int label_border = 0;
   //static const unsigned label_space  = 2;

   unsigned int ascent = grub_font_get_ascent (data->text_font);

   unsigned int line1 = 363 - ascent - label_border;
   unsigned int line2 = 390 - ascent - label_border;

   data->label_default->y = line1;
   
   data->label_timeout1->y = line1;
   data->label_timeout2->y = line2;

   data->label_activity->y = line1;
}


/*
 *  screen interface
 */

static grub_err_t miray_gfx_screen_destroy(struct miray_screen *scr)
{
   mgs_data_t * data = scr->data;

   /* Containers also free their children
    * So we only call destroy for the parent containers
    */

   if (data->bg_canvas != 0)
      data->bg_canvas->component.ops->destroy(data->bg_canvas);

   if (data->fg_timeout != 0)
      data->fg_timeout->component.ops->destroy(data->fg_timeout);

   if (data->fg_default != 0)
      data->fg_default->component.ops->destroy(data->fg_default);

   if (data->fg_activity != 0)
      data->fg_activity->component.ops->destroy(data->fg_activity);

   if (data->menu != 0)
      data->menu->component.ops->destroy(data->menu);
   
   grub_free(data);
   grub_free(scr);

   return GRUB_ERR_NONE;
};


static void miray_gfx_screen_reset (struct miray_screen * scr)
{
   mgs_data_t * data = scr->data;

   data->fg_mode = FG_DEFAULT;

   struct grub_gui_progress * progress1 = (struct grub_gui_progress *)data->progress1_activity;
   struct grub_gui_progress * progress2 = (struct grub_gui_progress *)data->progress2_activity;
   if (progress1 != 0 || progress2 != 0)
   {
      data->activity_cur = 0;
      
      if (progress1 != 0) progress1->ops->set_state(progress1, 0, 0, 0, 0);
      if (progress2 != 0) progress2->ops->set_state(progress2, 0, 0, 0, 0);
   }

}



static inline void _screen_draw_bg(mgs_data_t * data, const grub_video_rect_t * abs_bounds)
{
   grub_video_fill_rect (grub_video_map_rgba_color (data->bg_color),
                         abs_bounds->x,
                         abs_bounds->y,
                         abs_bounds->width,
                         abs_bounds->height);

}

static void miray_gfx_screen_clear (struct miray_screen * scr)
{
   mgs_data_t * data = scr->data;

   grub_video_rect_t bounds = {
      .x = 0,
      .y = 0,
      .width = data->v_width,
      .height = data->v_height
   };

   _screen_reset_term(data);

   _screen_draw_bg(data, &bounds);
   grub_video_swap_buffers ();
   if (data->screen_double_repaint)
   {
      _screen_draw_bg(data, &bounds);
   }
}


static void miray_gfx_screen_draw_rect_single (struct miray_screen * scr, const grub_video_rect_t * bounds, int clear)
{
   mgs_data_t * data = scr->data;

   if (clear)
   {
      _screen_draw_bg(data, bounds);
   }

   grub_video_rect_t rel_bounds;
   rel_bounds.x = bounds->x > data->screen_rect.x ? bounds->x - data->screen_rect.x : 0;
   rel_bounds.y = bounds->y > data->screen_rect.y ? bounds->y - data->screen_rect.y : 0;
   rel_bounds.width = bounds->x + bounds->width > data->screen_rect.x ? bounds->x + bounds->width - data->screen_rect.x : 0;
   rel_bounds.height = bounds->y + bounds->height > data->screen_rect.y ? bounds->y + bounds->height - data->screen_rect.y : 0;


   _gui_paint(&(data->bg_canvas->component), &rel_bounds);


   int timeout = grub_menu_get_timeout();
   
   switch(data->fg_mode)
   {
      default:
         break;

      case FG_DEFAULT:
         if (timeout > 0)
         {
            if (screen_data.timeout_format != 0 && data->label_timeout2 != 0)
            {
               /* We need to manually set the timeout here, because
                * the internal mode only works with a valid view
                */
               char *timeout_str = 0;
               timeout_str = grub_xasprintf(screen_data.timeout_format, timeout);
               _gui_set_property(data->label_timeout2, "text", timeout_str);
               grub_free(timeout_str);
            }

            _gui_paint(&data->fg_timeout->component, &rel_bounds);
         }
         else
         {
            _gui_paint(&data->fg_default->component, &rel_bounds);
         }


         _screen_paint_menu(data, bounds);
         break;

      case FG_ACTIVITY:
         _gui_paint(&data->fg_activity->component, &rel_bounds);
         break;

      case FG_HIDDEN:
         if (data->bg_label_image != 0)
         {
            _gui_paint(&data->bg_label->component, &rel_bounds);
         }
         else
         {
            // Low memory fallback
            _gui_paint(&data->fg_activity->component, &rel_bounds);
         }
   }
}

static void miray_gfx_screen_redraw_rect (struct miray_screen * scr, const grub_video_rect_t * bounds, int clear)
{
   if (scr->term != 0)
      grub_term_setcursor (scr->term, 0);

   mgs_data_t * data = scr->data;

   miray_gfx_screen_draw_rect_single(scr, bounds, clear);
   
   grub_video_swap_buffers ();
   if (data->screen_double_repaint)
   {
      miray_gfx_screen_draw_rect_single(scr, bounds, clear); 
   }
}


static void miray_gfx_screen_redraw (struct miray_screen * scr)
{
   mgs_data_t * data = scr->data;

   _screen_reset_term(data);

   const grub_video_rect_t bounds = { .x = 0, .y = 0, .width = data->v_width, .height = data->v_height };
   
   miray_gfx_screen_redraw_rect(scr, &bounds, 1);
}

static void miray_gfx_screen_redraw_text (struct miray_screen *scr, int clear __attribute__ ((unused)))
{
   mgs_data_t * data = scr->data;

   const grub_video_rect_t bounds = { .x = 0, .y = data->screen_rect.y + data->screen_offs_y_text, .width = data->v_width, .height = data->v_height - data->screen_rect.y - data->screen_offs_y_text };
   miray_gfx_screen_redraw_rect(scr, &bounds, 1);
}

static void miray_gfx_screen_redraw_timeout (struct miray_screen *scr, int clear __attribute__ ((unused)))
{
   mgs_data_t * data = scr->data;

   const grub_video_rect_t bounds = { .x = 0, .y = data->screen_rect.y + data->screen_offs_y_text, .width = data->v_width, .height = data->v_height - data->screen_rect.y - data->screen_offs_y_text - menu_height - 1 };
   miray_gfx_screen_redraw_rect(scr, &bounds, 1);
}

static void miray_gfx_screen_set_splash_menu(struct miray_screen *scr, grub_menu_t menu)
{
   mgs_data_t * data = scr->data;

   _gui_container_clear(data->menu);
   
   if (menu == 0) return;

   grub_gui_component_t list[MAX_MENU_ENTRIES];

   grub_menu_entry_t entry;
   int list_entries = 0;
   unsigned sumw = 0;
   unsigned menu_width = data->menu->component.w; 
   for (entry = menu->entry_list; entry != 0 && list_entries < MAX_MENU_ENTRIES; entry = entry->next)
   {
      list[list_entries] = _menu_component_new(entry);

      if (list[list_entries] != 0)
      {
         unsigned mh = 0;
         unsigned mw = 0;

         if (list[list_entries]->ops->get_minimal_size)
            list[list_entries]->ops->get_minimal_size (list[list_entries], &mw, &mh);

         sumw += mw;

         list_entries++;
      }
   }

   if (sumw > menu_width)
   {
      int i;
      for (i = 0; i < list_entries; i++)
      {
         list[i]->ops->destroy(list[i]);
      }
      return;
   }

   if (list_entries > 0)
   {
      unsigned spacer_sum = menu_width - sumw;
      int i;
      for (i = 0; i < list_entries; i++)
      {
         unsigned space = spacer_sum / (list_entries - i + 1);
         if (space > 0)
         {
            grub_gui_component_t spacer = grub_gui_label_new();
            spacer->w = space;
            _gui_container_add(data->menu, spacer);
         }
         spacer_sum -= space;
         
          _gui_container_add(data->menu, list[i]);        
      }

      if (spacer_sum > 0)
      {
         grub_gui_component_t spacer = grub_gui_label_new();
         spacer->w = spacer_sum;
         _gui_container_add(data->menu, spacer);
      }
   }
   
}

static int miray_gfx_screen_run_submenu(struct miray_screen *scr __attribute__ ((unused)), grub_menu_t menu)
{
   return miray_run_menu(menu);
}



static grub_err_t miray_gfx_screen_property (struct miray_screen * scr, const char * name, const char * value)
{
   mgs_data_t * data = scr->data;

   if (grub_strcmp(name, "default") == 0)
   {
      _gui_set_property(data->label_default, "text", value);
   }
   else if (grub_strcmp(name, "timeout_message") == 0)
   {
      _gui_set_property(data->label_timeout1, "text", value);
   }
   else if (grub_strcmp(name, "timeout_format") == 0)
   {
      _gui_set_property(data->label_timeout2, "text", value);
   }
   else if (grub_strcmp(name, "activity") == 0)
   {
      _gui_set_property(data->label_activity, "text", value);
      if (value != 0)
         data->fg_mode = FG_ACTIVITY;
      else
         data->fg_mode = FG_DEFAULT;
   }
   else if (grub_strcmp(name, "text_font") == 0 || grub_strcmp(name, "label_font") == 0)
   {
      const char * font = value ? : "";

      data->text_font = grub_font_get(font);
      
      _gui_set_property(data->label_timeout1, "font", font);
      if (data->timeout_font == 0)
      {
         _gui_set_property(data->label_timeout2, "font", font);
      }

      _gui_set_property(data->label_default, "font", font);
      
      _gui_set_property(data->label_activity, "font", font);

      _set_label_positions(data);
   }
   else if (grub_strcmp(name, "timeout_font") == 0)
   {
      if (value == 0 || value[0] == '\0')
      {
         // TODO: reset to text_font?
         return 0;
      }

      data->timeout_font = grub_font_get(value);

      _gui_set_property(data->label_timeout2, "font", value);
      _set_label_positions(data);
      
   }
   else if (grub_strcmp(name, "menu_font") == 0)
   {
      grub_video_rect_t bounds;
      
      const char * font = value ? : "";

      data->menu_font = grub_font_get(font);
      unsigned int ascent = grub_font_get_ascent (data->menu_font);

      data->menu->component.ops->get_bounds(data->menu, &bounds);
      bounds.height = ((menu_height - ascent) / 2) + ascent; 
      bounds.y = data->v_height - bounds.height;
      data->menu->component.y = bounds.y;
      data->menu->component.h = bounds.height;
      data->menu->component.ops->set_bounds(data->menu, &bounds);

      // rebuild menu, currently needed to get the spacing right
      miray_gfx_screen_set_splash_menu(scr, screen_data.menu);
   }
   else if (grub_strcmp(name, "max_stage") == 0)
   {
      if (screen_data.stage_max <= 1)
      {
         _set_single_progress_bar(data);
      }
      else
      {
         _set_dual_progress_bar(data);
      }
   }
   else if (grub_strcmp(name, "stage") == 0)
   {
      if (screen_data.stage > 1 && data->fg_mode == FG_ACTIVITY)
      {
         struct grub_gui_progress * activity1 = (struct grub_gui_progress *)data->progress1_activity;
         if (activity1 != 0) activity1->ops->set_state(data->progress1_activity, 1, 0, 1, 1);
      }
   }

   return GRUB_ERR_NONE;
}

static void miray_gfx_screen_message_box(struct miray_screen *scr, const char ** message __attribute__((unused)), const char * color __attribute__((unused)))
{
   mgs_data_t * data = scr->data;

   static const unsigned border_x = 15;
   static const unsigned border_y= 15;
   static const unsigned spacing = 5;

   if (message == 0) /* Nothing to display */
      return;

   unsigned text_width = 0;
   unsigned text_height = 0;
   unsigned count = 0;

   {
      while(message[count] != 0)
      {
         text_width = max(text_width, (unsigned)grub_font_get_string_width (data->text_font, message[count]));         
         count++;
      }
      
   }

   if (count == 0) // Nothing to display
      return;

   text_height = count * (grub_font_get_ascent (data->text_font) + grub_font_get_descent (data->text_font)) + (count - 1) * spacing;


   grub_video_rgba_color_t bg_color = {.red = 128, .blue = 0, .green = 0, .alpha = 255 };
   grub_video_rgba_color_t fg_color = {.red = 255, .blue = 255, .green = 255, .alpha = 255 };

   unsigned width = 2 * border_x + text_width;
   unsigned height = 2 * border_y + text_height;
   unsigned offs_x = data->screen_rect.x + (data->screen_rect.width - width) / 2;
   unsigned offs_y = data->screen_rect.y + (data->screen_rect.height - height) / 2;

   grub_video_fill_rect (grub_video_map_rgba_color (bg_color),
                        offs_x, offs_y, width, height);
   unsigned i;
   for (i = 0; i < count; i++)
   {
      unsigned y = offs_y + border_y + grub_font_get_ascent (data->text_font) + i * (grub_font_get_ascent (data->text_font) + grub_font_get_descent (data->text_font) + spacing);
      unsigned x = offs_x + border_x + (text_width - grub_font_get_string_width(data->text_font, message[i])) / 2;
   
      grub_font_draw_string (message[i],
                           data->text_font,
                           grub_video_map_rgba_color (fg_color),
                           x,
                           y);
   }
   
   grub_video_swap_buffers ();

}


static char *
miray_gfx_screen_submenu_center_entry(struct miray_screen *scr __attribute__((unused)), const char * string)
{
   int len = grub_strlen(string);
   int pad = (grub_term_width(grub_term_outputs) - len -5) /2;

   {
      const char * val = grub_env_get ("gfx_center_padding");
      if (val)
      {
         pad = (int) grub_strtoul (val, 0, 0);
      }
   }

   char * new_string = grub_malloc(sizeof(char) * (len + pad + 1));
   grub_memset(new_string, ' ', len + pad);
   new_string[len + pad] = '\0';

   grub_memcpy(&new_string[pad], string, len);

   return new_string;
}



static void miray_gfx_screen_set_progress(struct miray_screen *scr, grub_uint64_t cur, grub_uint64_t max)
{
   struct miray_gfx_screen_data *data = scr->data;
   if (data->fg_mode != FG_ACTIVITY)
      return;

   if (max == 0)
      return;

   struct grub_gui_progress * activity1 = (struct grub_gui_progress *)data->progress1_activity;
   struct grub_gui_progress * activity2 = (struct grub_gui_progress *)data->progress2_activity;
   if (activity1 != 0)
   {
      if (screen_data.stage <=1)
      {
         activity1->ops->set_state(data->progress1_activity, 1, 0, (int)cur, (int)max);
      }
      else
      {
         activity1->ops->set_state(data->progress1_activity, 1, 0, 1, 1);
      }
      
   }

   if (activity2 != 0)
   {
      if (screen_data.stage <= 1)
      {
         activity2->ops->set_state(data->progress2_activity, 1, 0, 0, 1);
      }
      else
      {
         activity2->ops->set_state(data->progress2_activity, 1, 0, (int)cur, (int)max);
      }
   }
   



   if (activity1 != 0 || activity2 != 0)
   {
      miray_screen_draw_activity();
   }

}




static void miray_gfx_screen_finish (struct miray_screen * scr)
{
   mgs_data_t * data = scr->data;

   if (data->progress1_activity != 0) data->progress1_activity->ops->set_property(data->progress1_activity, "activity_finish", "true"); 
   if (screen_data.stage_max > 1 && data->progress2_activity != 0) data->progress2_activity->ops->set_property(data->progress2_activity, "activity_finish", "true"); 

   data->fg_mode = FG_HIDDEN;
   miray_gfx_screen_redraw(scr);
}


/*
 * Init method
 */

struct miray_screen *
miray_gfx_screen_new(struct grub_term_output *term)
{
   struct grub_video_mode_info mode_info;
   grub_err_t err = GRUB_ERR_NONE;
   struct miray_screen *ret = 0;

   const char * core_dir  = "/boot/grub/core_data";

   grub_dprintf("miray", "%s\n", __FUNCTION__);

   err = grub_video_get_info (&mode_info);
   if (err != GRUB_ERR_NONE)
   {
      return 0;
   }

   grub_dprintf("miray", "%d\n", __LINE__);


   if (mode_info.width < 640 || mode_info.height < 480)
   {
      return 0;
   }

   grub_dprintf("miray", "%d\n", __LINE__);

   ret = grub_zalloc(sizeof(struct miray_screen));
   if (ret == 0)
   {
      grub_error(GRUB_ERR_OUT_OF_MEMORY, "Out of Memory (gfx screen)");
      return 0;
   }

   ret->destroy = miray_gfx_screen_destroy;
   ret->reset   = miray_gfx_screen_reset;
   ret->clear   = miray_gfx_screen_clear;
   ret->redraw  = miray_gfx_screen_redraw;
   ret->redraw_text = miray_gfx_screen_redraw_text;
   ret->redraw_timeout = miray_gfx_screen_redraw_timeout;

   ret->set_splash_menu = miray_gfx_screen_set_splash_menu;
   ret->run_submenu = miray_gfx_screen_run_submenu;


   ret->property     = miray_gfx_screen_property;
   ret->set_progress = miray_gfx_screen_set_progress;

   ret->message_box = miray_gfx_screen_message_box;
   ret->submenu_center_entry = miray_gfx_screen_submenu_center_entry;

   ret->finish = miray_gfx_screen_finish;

   grub_dprintf("miray", "%d\n", __LINE__);

   struct miray_gfx_screen_data * data = grub_zalloc(sizeof(struct miray_gfx_screen_data));
   if (data == 0)
   {
      grub_free(ret);
      grub_error(GRUB_ERR_OUT_OF_MEMORY, "Out of Memory (gfx screen data)");
      return 0;
   }

   grub_dprintf("miray", "%d\n", __LINE__);

   // Center the boot screen if the screen is bigger than 1024x768
   data->v_width = mode_info.width;
   data->v_height = mode_info.height;
   data->screen_rect.width = 640;
   data->screen_rect.height = 480;
   data->screen_rect.x = (mode_info.width - data->screen_rect.width) / 2;
   data->screen_rect.y = (mode_info.height - data->screen_rect.height) / 2;
   //data->screen_offs_y_text = 300;
   data->screen_offs_y_text = 338;
   data->screen_double_repaint =
      (mode_info.mode_type & GRUB_VIDEO_MODE_TYPE_DOUBLE_BUFFERED)
       && !(mode_info.mode_type & GRUB_VIDEO_MODE_TYPE_UPDATING_SWAP);

   data->bg_color.red   = 0;
   data->bg_color.green = 0;
   data->bg_color.blue  = 0;   
   data->bg_color.alpha = 255;   

   data->bg_menu.red    = 32;
   data->bg_menu.green  = 32;
   data->bg_menu.blue   = 32;
   data->bg_menu.alpha  = 255;

   data->bg_menuseperator.red   = 184;
   data->bg_menuseperator.green = 184;
   data->bg_menuseperator.blue  = 184;
   data->bg_menuseperator.alpha = 255;

   data->menu_font = grub_font_get("");
   data->text_font = grub_font_get("");
   data->fg_mode = FG_DEFAULT;


   ret->data = data;
   ret->term = term;

   data->activity_hook = 0;

   grub_dprintf("miray", "%d\n", __LINE__);


   {
      data->bg_canvas = grub_gui_canvas_new();
      if (data->bg_canvas == 0)
      {
         grub_error(GRUB_ERR_OUT_OF_MEMORY, "Out of Memory (background canvas)");
         goto ret_error;
      }
      data->bg_canvas->component.ops->set_bounds(data->bg_canvas, &data->screen_rect);
      data->bg_canvas->component.x = data->screen_rect.x;
      data->bg_canvas->component.y = data->screen_rect.y;
      data->bg_canvas->component.w = data->screen_rect.width;
      data->bg_canvas->component.h = data->screen_rect.height;

      grub_dprintf("miray", "%d\n", __LINE__);
      {
         data->bg_center_image = grub_gui_image_new();
         if (data->bg_center_image == 0)
         {
            grub_error(GRUB_ERR_OUT_OF_MEMORY, "Out of Memory (center image)");
            goto ret_error;
         }
         
         _gui_set_property(data->bg_center_image, "theme_dir", core_dir);
         if (_gui_set_property(data->bg_center_image, "file", "center_image.png") == GRUB_ERR_NONE)
         {
            unsigned width = 0;
            unsigned height = 0;

            grub_dprintf("miray", "%d\n", __LINE__);

            data->bg_center_image->ops->get_minimal_size(data->bg_center_image, &width, &height);
            data->bg_center_image->w = min(width, data->screen_rect.width);
            data->bg_center_image->h = min(height,data->screen_rect.height);
            data->bg_center_image->x = (data->screen_rect.width - data->bg_center_image->w) / 2;
            data->bg_center_image->y = 109;

            _gui_container_add(data->bg_canvas, data->bg_center_image);
         }
         else
         {
            grub_dprintf("miray", "Could not load image center_image.png\n");

            data->bg_center_image->ops->destroy(data->bg_center_image);
            data->bg_center_image = 0;

            while (grub_error_pop()) {}
         }

      }

   }

   grub_dprintf("miray", "%d\n", __LINE__);

   {
      data->bg_label = grub_gui_canvas_new();
      if (data->bg_label == 0)
      {
         grub_error(GRUB_ERR_OUT_OF_MEMORY, "Out of Memory (gui canvas)");
         goto ret_error;
      }
      data->bg_label->component.ops->set_bounds(data->bg_label, &data->screen_rect);
      data->bg_label->component.x = data->screen_rect.x;
      data->bg_label->component.y = data->screen_rect.y;
      data->bg_label->component.w = data->screen_rect.width;
      data->bg_label->component.h = data->screen_rect.height;

      {
         data->bg_label_image = grub_gui_image_new();
         if (data->bg_label_image == 0)
         {
            grub_error(GRUB_ERR_OUT_OF_MEMORY, "Out of Memory (gui image)");
            goto ret_error;
         }

         _gui_set_property(data->bg_label_image, "theme_dir", core_dir);
         if (_gui_set_property(data->bg_label_image, "file", "center_text.png") == GRUB_ERR_NONE)
         {
            unsigned width = 0;
            unsigned height = 0;

            data->bg_label_image->ops->get_minimal_size(data->bg_label_image, &width, &height);
            data->bg_label_image->w = min(width, data->screen_rect.width);
            data->bg_label_image->h = min(height,data->screen_rect.height);
            data->bg_label_image->x = (data->screen_rect.width - data->bg_label_image->w) / 2;
            data->bg_label_image->y = 339;

            _gui_container_add(data->bg_label, data->bg_label_image);
         }
         else
         {
            grub_dprintf("miray", "Could not load image center_text.png\n");         
            
            data->bg_label_image->ops->destroy(data->bg_label_image);
            data->bg_label_image = 0;

            while (grub_error_pop()) {}
         }

      }

   }

   grub_dprintf("miray", "%d\n", __LINE__);

   {

      data->fg_default = grub_gui_canvas_new();
      if (data->fg_default == 0)
      {
         grub_error(GRUB_ERR_OUT_OF_MEMORY, "Out of Memory (gui text)");
         goto ret_error;
      }

      data->fg_default->component.ops->set_bounds(data->fg_default, &data->screen_rect);
      data->fg_default->component.w = data->screen_rect.width;
      data->fg_default->component.h = data->screen_rect.height;

      {
         data->label_default = grub_gui_label_new();
         if (data->label_default == 0)
         {
            grub_error(GRUB_ERR_OUT_OF_MEMORY, "Out of Memory (gui label)");
            goto ret_error;
         }

         data->label_default->x = 0;
         data->label_default->w = data->screen_rect.width;
         data->label_default->y = data->screen_offs_y_text + 10;
         data->label_default->h = label_height;
         _gui_set_property(data->label_default, "color", message_text_color);
         _gui_set_property(data->label_default, "align", "center");
         //_gui_set_property(data->label_default, "font", default_font);
         _gui_container_add(data->fg_default, data->label_default);
      }

   }

   grub_dprintf("miray", "%d\n", __LINE__);

   {
      data->fg_timeout = grub_gui_canvas_new();
      if (data->fg_timeout == 0)
      {
         grub_error(GRUB_ERR_OUT_OF_MEMORY, "Out of Memory (timeout canvas)");
         goto ret_error;
      }
      data->fg_timeout->component.ops->set_bounds(data->fg_timeout, &data->screen_rect);
      data->fg_timeout->component.w = data->screen_rect.width;
      data->fg_timeout->component.h = data->screen_rect.height;

      {
         data->label_timeout1 = grub_gui_label_new();
         if (data->label_timeout1 == 0)
         {
            grub_error(GRUB_ERR_OUT_OF_MEMORY, "Out of Memory (timeout label 1)");
            goto ret_error;
         }
         data->label_timeout1->x = 0;
         data->label_timeout1->w = data->screen_rect.width;
         data->label_timeout1->y = data->screen_offs_y_text + 10;
         data->label_timeout1->h = label_height;
         _gui_set_property(data->label_timeout1, "color", message_text_color);
         _gui_set_property(data->label_timeout1, "align", "center");
         //_gui_set_property(data->label_timeout1, "font", default_font);
         _gui_container_add(data->fg_timeout, data->label_timeout1);
      }

      {
         data->label_timeout2 = grub_gui_label_new();
         if (data->label_timeout2 == 0)
         {
            grub_error(GRUB_ERR_OUT_OF_MEMORY, "Out of Memory (timeout label 2)");
            goto ret_error;
         }
         data->label_timeout2->x = 0;
         data->label_timeout2->w = data->screen_rect.width;
         data->label_timeout2->y = data->screen_offs_y_text + 10 + label_height + 7;
         data->label_timeout2->h = label_height;
         _gui_set_property(data->label_timeout2, "color", lightgray_c);
         _gui_set_property(data->label_timeout2, "align", "center");
         //_gui_set_property(data->label_timeout2, "font", default_font);
         _gui_container_add(data->fg_timeout, data->label_timeout2);
      }
   }

   grub_dprintf("miray", "%d\n", __LINE__);

   {
      data->fg_activity = grub_gui_canvas_new();
      if (data->fg_activity == 0)
      {
         grub_error(GRUB_ERR_OUT_OF_MEMORY, "Out of Memory");
         goto ret_error;
      }
      data->fg_activity->component.ops->set_bounds(data->fg_activity, &data->screen_rect);
      data->fg_activity->component.w = data->screen_rect.width;
      data->fg_activity->component.h = data->screen_rect.height;
      
      {
         data->label_activity = grub_gui_label_new();
         if (data->label_activity == 0)
         {
            grub_error(GRUB_ERR_OUT_OF_MEMORY, "Out of Memory");
            goto ret_error;
         }
         data->label_activity->x = 0;
         data->label_activity->w = data->screen_rect.width;
         data->label_activity->y = data->screen_offs_y_text + 10;
         data->label_activity->h = label_height;
         _gui_set_property(data->label_activity, "color", message_text_color);
         _gui_set_property(data->label_activity, "align", "center");
         //_gui_set_property(data->label_activity, "font", default_font);
         _gui_container_add(data->fg_activity, data->label_activity);
      }

      {
         // This will be moved up if we need 2 progress bars
         data->progress1_activity = grub_miray_gui_progress_bar_new();
         if (data->progress1_activity == 0)
         {
            grub_error(GRUB_ERR_OUT_OF_MEMORY, "Out of Memory");
            goto ret_error;
         }
         data->progress1_activity->x = 160;
         data->progress1_activity->w = 320;
         data->progress1_activity->y = 380;
         data->progress1_activity->h = 2;
         _gui_set_property(data->progress1_activity, "bg_color", darkgray_c);
         _gui_set_property(data->progress1_activity, "fg_color", "white");
         _gui_container_add(data->fg_activity, data->progress1_activity);
      }

      {
         data->progress2_activity = grub_miray_gui_progress_bar_new();
         if (data->progress2_activity == 0)
         {
            grub_error(GRUB_ERR_OUT_OF_MEMORY, "Out of Memory");
            goto ret_error;
         }
         data->progress2_activity->x = 160;
         data->progress2_activity->w = 320;
         data->progress2_activity->y = 380;
         data->progress2_activity->h = 2;
         _gui_set_property(data->progress2_activity, "bg_color", darkgray_c);
         _gui_set_property(data->progress2_activity, "fg_color", "white");
         _gui_container_add(data->fg_activity, data->progress2_activity);
      }
   }
   
   grub_dprintf("miray", "%d\n", __LINE__);

   {
      const grub_video_rect_t bounds = { .x = 0,
                                         .y = data->v_height - menu_height,
                                         .width = data->v_width,
                                         .height = menu_height };
      
      data->menu = grub_gui_hbox_new();
      if (data->menu == 0)
      {
         grub_error(GRUB_ERR_OUT_OF_MEMORY, "Out of Memory");
         goto ret_error;
      }
      data->menu->component.ops->set_bounds(data->menu, &bounds);
      data->menu->component.x = bounds.x;
      data->menu->component.y = bounds.y;
      data->menu->component.w = bounds.width;
      data->menu->component.h = bounds.height;
   }

   grub_dprintf("miray", "%d\n", __LINE__);

   return ret;


ret_error:

   grub_error(GRUB_ERR_MENU, "error creating gfx screen\n");

   if (ret != 0)
   {
      miray_gfx_screen_destroy(ret);
   }

   grub_dprintf("miray", "%d\n", __LINE__);
   
   return 0;
   
}

static inline void _set_single_progress_bar(mgs_data_t * data)
{
   data->progress1_activity->y = 380;

   data->label_default->y = data->screen_offs_y_text + 10;
   data->label_timeout1->y = data->screen_offs_y_text + 10;
   data->label_timeout2->y = data->screen_offs_y_text + 10 + label_height + 7;
   data->label_activity->y = data->screen_offs_y_text + 10;

   struct grub_gui_progress * activity2 = (struct grub_gui_progress *)data->progress2_activity;
   activity2->ops->set_state(data->progress2_activity, 0, 0, 0, 0); 
}

static inline void _set_dual_progress_bar(mgs_data_t * data)
{
   data->progress1_activity->y = 370;

   data->label_default->y = data->screen_offs_y_text;
   data->label_timeout1->y = data->screen_offs_y_text;
   data->label_timeout2->y = data->screen_offs_y_text + label_height + 7;
   data->label_activity->y = data->screen_offs_y_text;

   struct grub_gui_progress * activity2 = (struct grub_gui_progress *)data->progress2_activity;
   activity2->ops->set_state(data->progress2_activity, 1, 0, 0, 1);    
}


/* This method is also exported to optimize rendering of some elements */
void miray_screen_draw_rect(const grub_video_rect_t * bounds, int clear)
{
   miray_gfx_screen_redraw_rect(_miray_screen, bounds, clear);
}


void miray_screen_draw_activity()
{
   mgs_data_t * data = _miray_screen->data;

   const grub_video_rect_t bounds = { .x = data->screen_rect.x, .y = data->screen_rect.y, .width = data->screen_rect.width, .height = data->screen_rect.height };

   if (data->fg_mode != FG_ACTIVITY)
      return;

   _gui_paint(&data->fg_activity->component, &bounds);
   
   grub_video_swap_buffers ();
}

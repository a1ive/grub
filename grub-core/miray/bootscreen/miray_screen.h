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


#ifndef MIRAY_SCREEN
#define MIRAY_SCREEN

#include <grub/err.h>
#include <grub/menu.h>
#include <grub/term.h>
#include <grub/misc.h>



struct miray_screen
{
   grub_err_t (*destroy) (struct miray_screen *);
  
   void (*clear)  (struct miray_screen *);
   void (*redraw) (struct miray_screen *);  
   void (*redraw_text) (struct miray_screen *, int clear);
   void (*redraw_timeout) (struct miray_screen *, int clear); 
   void (*reset) (struct miray_screen *);

   void (*set_splash_menu) (struct miray_screen *, grub_menu_t menu);
   int  (*run_submenu) (struct miray_screen *, grub_menu_t menu);

   grub_err_t (*property) (struct miray_screen *, const char * name, const char * value);
   void (*set_progress) (struct miray_screen *, grub_uint64_t current, grub_uint64_t max);
   
   void (*message_box) (struct miray_screen *, const char ** message,
                        const char * color);
   char* (*submenu_center_entry) (struct miray_screen *, const char *string);

   void (*finish) (struct miray_screen *);

   grub_term_output_t term;
   void *data;
};




grub_err_t miray_screen_init(void);
grub_err_t miray_screen_fini(void);

/* Set external screen handler. "screen == 0" resets to default */ 
grub_err_t miray_screen_set_screen(struct miray_screen * screen);

/* internal methods*/
char * miray_screen_entry_name(const grub_menu_entry_t entry);

struct miray_screen_common_data
{
   grub_menu_t menu;
   
   char * default_message;
   char * timeout_message;
   char * timeout_format;
   char * timeout_stop;
   char * activity_descr;

   int stage_max;
   int stage;

   /* Needed by gfx menu */
   char * menu_font;
   char * text_font;
};
extern struct miray_screen_common_data screen_data;

void miray_screen_set_splash_menu (grub_menu_t menu);
grub_err_t miray_screen_property(const char * name, const char * value);


extern struct miray_screen * _miray_screen;

static inline grub_err_t miray_screen_destroy(struct miray_screen *scr)
{
   grub_err_t ret = GRUB_ERR_NONE;
   if (scr != 0 && scr->destroy != 0)
   {
      ret = scr->destroy(scr);
   }
   else
   {
      grub_free(scr);
   }

   return ret;
}

static inline void miray_screen_set_term(grub_term_output_t term __attribute__((unused)))
{
  //if (_miray_screen != 0) _miray_screen->term = term;
}

static inline void miray_screen_redraw(void)
{
   if (_miray_screen != 0)
   {
      /* Update the terminal to the current output terminal */
      //_miray_screen->term = grub_term_outputs; // Actually does not work correctly yet....
   
      if (_miray_screen->redraw != 0)
         _miray_screen->redraw (_miray_screen);
   }
}

static inline void miray_screen_redraw_text(int clear)
{
   if (_miray_screen != 0)
   {
      if (_miray_screen->redraw_text != 0 && _miray_screen->term == grub_term_outputs)
         _miray_screen->redraw_text (_miray_screen, clear);
      else
         miray_screen_redraw();
   }
}

static inline void miray_screen_redraw_timeout(int clear)
{
   if (_miray_screen != 0 && _miray_screen->redraw_timeout != 0)
   {
      _miray_screen->redraw_timeout(_miray_screen, clear);
   }
   else
   {
      miray_screen_redraw_text(clear);
   }
}

static inline void miray_screen_reset(void)
{
   if (_miray_screen != 0 && _miray_screen->reset != 0)
      (_miray_screen->reset) (_miray_screen);
}



static inline void miray_screen_run_submenu (grub_menu_t menu)
{
   if (_miray_screen != 0 && _miray_screen->run_submenu != 0)
      (_miray_screen->run_submenu) (_miray_screen, menu);
}


static inline void miray_screen_message_box (const char ** message, const char *color)
{
   if (_miray_screen != 0 && _miray_screen->message_box != 0)
      (_miray_screen->message_box) (_miray_screen, message, color);
}

static inline char * miray_screen_submenu_center_entry(const char * string)
{
   if (_miray_screen != 0 && _miray_screen->submenu_center_entry != 0)
      return _miray_screen->submenu_center_entry(_miray_screen, string);

   return grub_strdup(string);
}


static inline void miray_screen_cls (void)
{
   if (_miray_screen != 0 && _miray_screen->clear != 0)
      _miray_screen->clear(_miray_screen);
}

static inline void miray_screen_finish (void)
{
   if (_miray_screen != 0 && _miray_screen->finish != 0)
      (_miray_screen->finish) (_miray_screen);
}


#endif

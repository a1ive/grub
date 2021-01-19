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


#include <grub/term.h>
#include <grub/mm.h>
#include <grub/dl.h>
#include <grub/command.h>
#include <grub/i18n.h>
#include <grub/file.h>
#include <grub/time.h>

#include "miray_screen.h"

struct miray_screen * _miray_screen = 0;

extern struct miray_screen * miray_text_screen_new(struct grub_term_output *term);

/*
 * Common screen data handling
 */ 

struct miray_screen_common_data screen_data =
{
   .menu            = 0,
   .menu_font       = 0,
   .text_font       = 0,
   
   .default_message = 0,
   .timeout_message = 0,
   .timeout_format  = 0,
   .timeout_stop    = 0,
   .activity_descr  = 0,

   .stage_max = 1,
   .stage     = 0,

};


void miray_screen_set_splash_menu (grub_menu_t menu)
{
   screen_data.menu = menu;

   if (_miray_screen != 0 && _miray_screen->set_splash_menu != 0)
      (_miray_screen->set_splash_menu) (_miray_screen, menu);
}


static void
miray_screen_file_progress_hook (grub_disk_addr_t sector __attribute__ ((unused)),
                                 unsigned offset __attribute__ ((unused)),
                                 unsigned length __attribute__ ((unused)),
                                 void *data)
{
   static unsigned int last_tick = 0;
   grub_file_t file = data;

   if (file->progress_offset >= file->size)
      return;

   if (file->progress_offset < file->offset)
   {
      file->progress_offset = file->offset;
   }

   file->progress_offset += length;
   if (file->progress_offset > file->size)
   {
      file->progress_offset = file->size;
   }
   
   if (_miray_screen == 0 || _miray_screen->set_progress == 0)
      return;

   if (file->size < 100 * 1024) // Avoid some very smal (file system?) reads
      return;

   grub_uint32_t cur = grub_get_time_ms(); // Avoid flicker
   if ((last_tick + 50) > cur && file->progress_offset < file->size) // update every 50 ms at max
      return;

   last_tick = cur;


   //if (file->size == 0)
   //   return

   //_miray_screen->set_progress(_miray_screen, file->offset + length, file->size);
   _miray_screen->set_progress(_miray_screen, file->progress_offset, file->size);
}


grub_err_t miray_screen_property(const char * name, const char * data)
{
   if (data != 0 && data[0] == '\0')
      data = 0;
   
   if (grub_strcmp(name, "default") == 0)
   {
      grub_free(screen_data.default_message);
      screen_data.default_message = 0;

      if (data != 0)
         screen_data.default_message = grub_strdup(data);
   }
   else if (grub_strcmp(name, "activity") == 0)
   {      
      grub_free(screen_data.activity_descr);
      screen_data.activity_descr = 0;

      if (screen_data.stage_max < 1)
      {
         screen_data.stage_max = 1;
      }

      if (screen_data.stage < 1)
      {
         screen_data.stage = 1;
      }
   
      if (data != 0)
      {
         screen_data.activity_descr = grub_strdup(data);
      }
   }
   else if (grub_strcmp(name, "timeout_format") == 0)
   {
      grub_free(screen_data.timeout_format);
      screen_data.timeout_format = 0;
   
      if (data != 0)
         screen_data.timeout_format = grub_strdup(data);
   }
   else if (grub_strcmp(name, "timeout_message") == 0)
   {
      grub_free(screen_data.timeout_message);
      screen_data.timeout_message = 0;
   
      if (data != 0)
         screen_data.timeout_message = grub_strdup(data);
   }
   else if (grub_strcmp(name, "timeout_stop") == 0)
   {
      grub_free(screen_data.timeout_stop);
      screen_data.timeout_stop = 0;
   
      if (data != 0)
         screen_data.timeout_stop = grub_strdup(data);
   }
   else if (grub_strcmp(name, "menu_font") == 0)
   {
      grub_free(screen_data.menu_font);
      screen_data.menu_font = 0;

      if (data != 0)
         screen_data.menu_font = grub_strdup(data);
   }
   else if (grub_strcmp(name, "text_font") == 0)
   {
      grub_free(screen_data.text_font);
      screen_data.text_font = 0;

      if (data != 0)
         screen_data.text_font = grub_strdup(data);
   }
   else if (grub_strcmp(name, "max_stage") == 0)
   {
      int num = (int) grub_strtoul (data, 0, 0);
      screen_data.stage_max = num;
   }
   else if (grub_strcmp(name, "stage") == 0)
   {
      int num = (int) grub_strtoul (data, 0, 0);
      screen_data.stage = num;
   }
      
   if (_miray_screen != 0 && _miray_screen->property != 0)
      return _miray_screen->property(_miray_screen, name, data);

   return GRUB_ERR_NONE;
}


grub_err_t miray_screen_set_screen(struct miray_screen * screen)
{
   grub_err_t ret = GRUB_ERR_NONE;

   grub_dprintf("miray", "%s\n", __FUNCTION__);

   if (screen != 0 && screen == _miray_screen) // Nothing to do in that case
      return GRUB_ERR_NONE; 
   
   if (_miray_screen != 0) 
   {
      grub_file_progress_hook = 0;
      miray_screen_destroy(_miray_screen);
      _miray_screen = 0;
   }   

   if (screen != 0)
   {
      _miray_screen = screen;
   }
   else
   {
      _miray_screen = miray_text_screen_new(grub_term_outputs);
   }

   if (_miray_screen != 0)
   {
      if (_miray_screen->set_splash_menu != 0)
      {
         _miray_screen->set_splash_menu(_miray_screen, screen_data.menu);
      }
      
      if (_miray_screen->property != 0)
      {
         _miray_screen->property(_miray_screen, "default", screen_data.default_message);
         _miray_screen->property(_miray_screen, "activity", screen_data.activity_descr);
         _miray_screen->property(_miray_screen, "timeout_format", screen_data.timeout_format);
         _miray_screen->property(_miray_screen, "timeout_message", screen_data.timeout_message);
         _miray_screen->property(_miray_screen, "timeout_stop", screen_data.timeout_stop);
         
         _miray_screen->property(_miray_screen, "menu_font", screen_data.menu_font);
         _miray_screen->property(_miray_screen, "text_font", screen_data.text_font);
      }

      if (_miray_screen->set_progress != 0)
      {
         grub_file_progress_hook = miray_screen_file_progress_hook;         
      }
   }

   miray_screen_redraw();

   return ret;
}

char * miray_screen_entry_name(const grub_menu_entry_t entry)
{
   // Menu entries without a hotkey cannot be executed
   if (entry->hotkey == 0)
      return 0;
   
   // Hide menu entries without title. 
   if (entry->title == 0 || entry->title[0] == '\0')
      return 0;;

   const char *key_string = 0;
   char key_buf[4] = {'<', ' ', '>', 0};
   switch (entry->hotkey)
   {
      case GRUB_TERM_KEY_F1: key_string = "<F1>"; break;
      case GRUB_TERM_KEY_F2: key_string = "<F2>"; break;
      case GRUB_TERM_KEY_F3: key_string = "<F3>"; break;
      case GRUB_TERM_KEY_F4: key_string = "<F4>"; break;
      case GRUB_TERM_KEY_F5: key_string = "<F5>"; break;
      case GRUB_TERM_KEY_F6: key_string = "<F6>"; break;
      case GRUB_TERM_KEY_F7: key_string = "<F7>"; break;
      case GRUB_TERM_KEY_F8: key_string = "<F8>"; break;
      case GRUB_TERM_KEY_F9: key_string = "<F9>"; break;
      case GRUB_TERM_KEY_F10: key_string = "<F10>"; break;
      case GRUB_TERM_KEY_F11: key_string = "<F11>"; break;
      case GRUB_TERM_KEY_F12: key_string = "<F12>"; break;
      //case GRUB_TERM_KEY_TAB: key_string = "<TAB>"; break;
      default:
         if ((entry->hotkey >= 'A' && entry->hotkey <= 'Z') ||
            (entry->hotkey >= '0' && entry->hotkey <= '9'))
            {
               key_buf[1] = entry->hotkey;
               key_string = key_buf;
            }
         else if (entry->hotkey >= 'a' && entry->hotkey <= 'z')
            {
               key_buf[1] = entry->hotkey + 'A' - 'a';
               key_string = key_buf;
            }
      break;
   }

   // No valid shortcut
   if (key_string == 0)
      return 0;

   return grub_xasprintf("%s %s", key_string, entry->title);
}



/*
 * cmd line commands
 */



 

static grub_err_t
miray_screen_cmd_redraw (struct grub_command *cmd __attribute__ ((unused)),
   int argc __attribute__ ((unused)), char *argv[] __attribute__ ((unused)))
{
   miray_screen_redraw();
   return GRUB_ERR_NONE;
}

static grub_err_t
miray_screen_cmd_redraw_text (struct grub_command *cmd __attribute__ ((unused)),
   int argc __attribute__ ((unused)), char *argv[] __attribute__ ((unused)))
{
   miray_screen_redraw_text(1);
   return GRUB_ERR_NONE;
}

static grub_err_t
miray_screen_cmd_reset (struct grub_command *cmd __attribute__ ((unused)),
   int argc __attribute__ ((unused)), char *argv[] __attribute__ ((unused)))
{
   miray_screen_reset();
   return GRUB_ERR_NONE;
}

static grub_err_t
miray_screen_cmd_finish (struct grub_command *cmd __attribute__ ((unused)),
   int argc __attribute__ ((unused)), char *argv[] __attribute__ ((unused)))
{
   miray_screen_finish();
   return GRUB_ERR_NONE;
}


static grub_err_t
miray_screen_cmd_property(struct grub_command * cmd __attribute__ ((unused)), int argc, char *argv[])
{
   if (argc < 1)
   {
      return grub_error(GRUB_ERR_BAD_ARGUMENT, "Missing name");
   }
   else if (argc < 2)
   {
      return miray_screen_property(argv[0], 0);
   }
   else
   {
      return miray_screen_property(argv[0], argv[1]);
   }
}

static grub_err_t
miray_screen_cmd_msg_default (struct grub_command *cmd __attribute__ ((unused)),
   int argc, char *argv[])
{
   if (argc < 1)
      // Unset message
      miray_screen_property("default", 0);
   else
      miray_screen_property("default", argv[0]);

   return GRUB_ERR_NONE;
}

static grub_err_t
miray_screen_cmd_msg_activity (struct grub_command *cmd __attribute__ ((unused)),
   int argc, char *argv[])
{
   if (argc < 1)
      return grub_error (GRUB_ERR_BAD_ARGUMENT, "Missing argument");

   miray_screen_property("activity", argv[0]);

   return GRUB_ERR_NONE;
}

static grub_err_t
miray_screen_cmd_msg_timeout (struct grub_command *cmd __attribute__ ((unused)),
   int argc, char *argv[])
{
   char * message = 0;
   char * format  = 0;
   char * stop    = 0;
   
   if (argc < 1)
   {
      // Unset message
   }
   else if (argc < 2)
   {
      format = argv[0];
   }
   else if (argc < 3)
   {
      message = argv[0];
      format = argv[1];
   }
   else 
   {
      message = argv[0];
      format = argv[1];
      stop = argv[2];
   } 

   miray_screen_property("timeout_format", format);
   miray_screen_property("timeout_message", message);
   miray_screen_property("timeout_stop", stop);

   return GRUB_ERR_NONE;
}

static grub_err_t
miray_screen_cmd_logo (struct grub_command *cmd __attribute__ ((unused)),
   int argc, char *argv[])
{
   if (argc < 1)
      return grub_error (GRUB_ERR_BAD_ARGUMENT, "Missing argument");

   return miray_screen_property("logo", argv[0]);
}


/*
 * Init code
 */

static grub_command_t _cmd_redraw, _cmd_redraw_text, _cmd_reset, _cmd_finish;
static grub_command_t _cmd_property;
static grub_command_t _cmd_msg_normal, _cmd_msg_activity, _cmd_msg_timeout;
static grub_command_t _cmd_logo;



grub_err_t miray_screen_init(void)
{
   grub_err_t ret = GRUB_ERR_NONE;

   grub_dprintf("miray", "%s\n", __FUNCTION__);

   screen_data.default_message = grub_strdup("Initializing ...");

   if ((ret = miray_screen_set_screen(0)) != GRUB_ERR_NONE)
   {
      return ret;
   }
   
   _cmd_redraw = grub_register_command ("miray_screen_redraw",
      miray_screen_cmd_redraw, 0, N_("redraw screen"));
   _cmd_redraw_text = grub_register_command ("miray_screen_redraw_text",
      miray_screen_cmd_redraw_text, 0, N_("redraw screen"));
   _cmd_reset = grub_register_command ("miray_screen_reset", 
      miray_screen_cmd_reset, 0, N_("reset screen"));
   _cmd_finish = grub_register_command ("miray_screen_finish", 
      miray_screen_cmd_finish, 0, N_("finish screen"));

   _cmd_property = grub_register_command("miray_screen_property",
      miray_screen_cmd_property, 0, N_("set message components"));

   _cmd_msg_normal = grub_register_command ("miray_screen_msg_default",
      miray_screen_cmd_msg_default, 0, N_("set default message"));
   _cmd_msg_activity = grub_register_command ("miray_screen_msg_activity", 
      miray_screen_cmd_msg_activity, 0, N_("set activity message"));
   _cmd_msg_timeout = grub_register_command ("miray_screen_msg_timeout", 
      miray_screen_cmd_msg_timeout, 0, N_("set timeout message"));

   _cmd_logo = grub_register_command ("miray_screen_set_logo", 
      miray_screen_cmd_logo, 0, N_("set bootscreen logo by name"));

   return GRUB_ERR_NONE;
}

grub_err_t miray_screen_fini(void)
{
   grub_unregister_command(_cmd_redraw);
   grub_unregister_command(_cmd_redraw_text);
   grub_unregister_command(_cmd_reset);
   grub_unregister_command(_cmd_finish);
   grub_unregister_command(_cmd_property);
   grub_unregister_command(_cmd_msg_normal);
   grub_unregister_command(_cmd_msg_activity);
   grub_unregister_command(_cmd_msg_timeout);
   grub_unregister_command(_cmd_logo);

   grub_file_progress_hook = 0;

   if (_miray_screen != 0)
   {
      grub_err_t ret = miray_screen_destroy(_miray_screen);
      if (ret != GRUB_ERR_NONE) return ret;

      _miray_screen = 0;
   }

   grub_free(screen_data.default_message);
   grub_free(screen_data.timeout_message);
   grub_free(screen_data.timeout_format);
   grub_free(screen_data.timeout_stop);
   grub_free(screen_data.activity_descr);
   
   grub_free(screen_data.menu_font);
   grub_free(screen_data.text_font);
   
   return GRUB_ERR_NONE;
}

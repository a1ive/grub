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


#include <grub/dl.h>
#include <grub/menu_viewer.h>
#include <grub/normal.h>
#include <grub/term.h>
#include <grub/time.h>
#include <grub/i18n.h>
#include <grub/loader.h>
#include <grub/charset.h>
#include <grub/mm.h>
#include <grub/misc.h>
#include <grub/menu.h>

#include "miray_bootscreen.h"
#include "miray_constants.h"
#include "miray_screen.h"


static const int boot_next_entry = 1;
static const int boot_symobi_entry = 0;

const char * boot_options;

#ifndef GRUB_TERM_DISP_HLINE
#define GRUB_TERM_DISP_HLINE GRUB_UNICODE_HLINE
#endif

static inline char _upcase_ascii(char c)
{
	return (c >= 'a' && c <= 'z') ? c + 'A' - 'a' : c;
}


grub_err_t miray_bootscreen_run_submenu(const char * menufile)
{
   int ret; // unused, needed by grub-code
   grub_menu_t mainmenu;
   grub_menu_t menu;
   grub_menu_entry_t e;
  
   if (menufile == 0)
      menufile = grub_env_get("ext_menu");

   mainmenu = grub_env_get_menu();
   grub_env_unset_menu();

   miray_screen_cls();

   while (1)
   {
      // reload menu every time to update menu entry descriptions
      // This is necessary to enable toggled options
      menu = read_config_file (menufile);
      if (menu == 0)
      {
         grub_env_set_menu(mainmenu);
         return grub_error (GRUB_ERR_FILE_NOT_FOUND, "Could not open menu file");
      }
      ret = miray_run_menu(menu);

      if (ret < 0)
      {
         grub_normal_free_menu (menu);
         grub_env_set_menu(mainmenu);
         return grub_errno;
      }

      {
         char val[10];
         grub_snprintf(val, 9, "%d", ret);
         grub_env_set("default", val);
      }

      e = grub_menu_get_entry (menu, ret);
      if (e == 0) {
         grub_normal_free_menu (menu);
         continue;
      }

      grub_menu_execute_entry(e, 0);
      miray_screen_reset();

      grub_normal_free_menu(menu);
   }

   grub_env_set_menu(mainmenu);
   miray_screen_redraw();

   return GRUB_ERR_NONE;
}

static int run_screen (grub_menu_t menu, int default_entry)
{
   int timeout;
   grub_uint64_t saved_time;

   timeout = grub_menu_get_timeout();
   saved_time = grub_get_time_ms();

   while (1)
   {
      int c;
      
      if (timeout > 0)
      {
         grub_uint64_t current_time = grub_get_time_ms();
         if (current_time - saved_time >= 1000)
         {
            timeout--;
            saved_time = current_time;
            grub_menu_set_timeout(timeout);
            //miray_screen_redraw_text(0);
            miray_screen_redraw_timeout(0);
         }
      }

      c = grub_getkey_noblock();

      if (c != GRUB_TERM_NO_KEY)
      {
         switch (c)
         {
         case '\e':
            if (timeout < 0) break;
            timeout=-1;
            grub_env_unset("timeout");
            miray_screen_redraw_text(1);
            break;

#if 0
         case 'M':
         case 'm':
            timeout = -1;
            grub_env_unset("timeout");
            miray_bootscreen_run_submenu(0);
            break;
#endif

#if 0
         case 'c':
            timeout = -1;
            grub_env_unset("timeout");
            miray_screen_cls();
            grub_cmdline_run(1);
            miray_screen_redraw();
            break;
#endif

#if 0
         // FIXME: Workaround for some grub menuentry class bug
         case 's':
         case 'S':
            timeout = -1;
            grub_env_unset("timeout");
            {
               grub_menu_entry_t entry;
               int i;
               for (i = 0, entry = menu->entry_list; i < menu->size;
                  i++, entry = entry->next)
               {
                  if (_upcase_ascii(entry->hotkey) == _upcase_ascii(c))
                  {
                     return i;
                  }
               }
            }
#endif

         default:
            {
               grub_menu_entry_t entry;
               int i;
               for (i = 0, entry = menu->entry_list; i < menu->size;
               i++, entry = entry->next)
               {
                  if (_upcase_ascii(entry->hotkey) == _upcase_ascii(c))
                  {
                     timeout = -1;
                     grub_env_unset("timeout");
                     grub_dprintf("miray", "%s, %d", __func__, __LINE__);
                     return i;
                  }
               }
            }
            break;
         }
      }

      if (timeout == 0) {
         grub_env_unset ("timeout");
         // FIXME: At the moment, this assuems that symobi uses the first entry
         return default_entry;
      }
   }
}

grub_err_t miray_bootscreen_execute (grub_menu_t menu, int nested, int auto_boot __attribute__((unused)))
{
   int entry;
   grub_menu_entry_t e;
   //struct grub_term_output *term;
   const char * default_str;

   if (nested)
      return 0; // display normal menu

   //term = grub_term_outputs; // FIXME: enable for all active outputs, handle serial better

   //miray_screen_set_term(term);

   entry = 0;

   if ((default_str = grub_env_get("default")) != 0 )
      entry = grub_strtol(default_str, 0, 10);

   boot_options = grub_env_get(msg_options_env_key);
   if (boot_options == 0)
   {
     if (grub_command_execute("has_native_os", 0, 0) == GRUB_ERR_NONE)
     {
       boot_options = msg_options_native_os;
     }
     else
     {
       while (grub_error_pop()) {}   
       boot_options = msg_options_default;
     }
   } 

   {
      menu = grub_env_get_menu();
      miray_screen_set_splash_menu(menu);
   }

   miray_screen_redraw();

   while (1)
   {
      entry = run_screen (menu, entry);
      if (entry < 0)
         break;

      grub_dprintf("miray", "%s, %d", __func__, __LINE__);

      e = grub_menu_get_entry (menu, entry);
      grub_dprintf("miray", "%s, %d", __func__, __LINE__);

      if (e == 0)
         continue;

      grub_dprintf("miray", "%s, %d", __func__, __LINE__);

      grub_menu_execute_entry(e, 0);
 
      miray_screen_reset();
      miray_screen_redraw();
   }

   grub_dprintf("miray", "%s, %d", __func__, __LINE__);

   return -1;
}


grub_err_t
miray_cmd_cmdline (struct grub_command *cmd __attribute__ ((unused)),
   int argc __attribute__ ((unused)), char *argv[] __attribute__ ((unused)))
{
   miray_screen_cls();
   grub_cmdline_run(1);
   miray_screen_redraw();

   return GRUB_ERR_NONE;
}

grub_err_t
miray_bootscreen_preboot (int noreturn __attribute__ ((unused)))
{
   miray_screen_finish();

   return 0;
}

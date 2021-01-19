/* main.c - the miray bootscreen main routine, parts copied from normal main */
/*
 *  Extention for GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2000,2001,2002,2003,2005,2006,2007,2008,2009  Free Software Foundation, Inc.
 *  Copyright (C) 2010,2011 Miray Software <oss@miray.de>
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
#include <grub/miray_debug.h>

#include "miray_bootscreen.h"
#include "miray_constants.h"
#include "miray_screen.h"

GRUB_MOD_LICENSE ("GPLv3+");

#ifndef GRUB_TERM_DISP_HLINE
#define GRUB_TERM_DISP_HLINE GRUB_UNICODE_HLINE
#endif


static grub_err_t
miray_cmd_fatal (struct grub_command *cmd __attribute__ ((unused)),
		int argc, char *argv[]) __attribute__ ((noreturn));

static grub_err_t
miray_cmd_bootscreen (struct grub_command *cmd __attribute__ ((unused)),
		int argc, char *argv[])
{
  if (argc == 0)
    {
      /* Guess the config filename. It is necessary to make CONFIG static,
         so that it won't get broken by longjmp.  */
      char *config;
      const char *prefix;

      prefix = grub_env_get ("prefix");
      if (prefix)
        {
          config = grub_xasprintf ("%s/grub.cfg", prefix);
          if (! config)
            goto quit;

          grub_enter_normal_mode (config);
          grub_free (config);
        }
      else
        grub_enter_normal_mode (0);
    }
  else
    grub_enter_normal_mode (argv[0]);

quit:
  return GRUB_ERR_NONE;
}

static grub_err_t
miray_cmd_center_env (struct grub_command *cmd __attribute__ ((unused)),
		int argc, char *argv[])
{
   if (argc < 2) return GRUB_ERR_NONE;

   char * string = argv[0];
   char * key = argv[1];

#if 0
   int len = grub_strlen(string);
   int pad = (grub_term_width(grub_term_outputs) - len -5) /2;

   char * new_string = grub_malloc(sizeof(char) * (len + pad + 1));
   grub_memset(new_string, ' ', len + pad);
   new_string[len + pad] = '\0';

   grub_memcpy(&new_string[pad], string, len);
#endif

   char * new_string = miray_screen_submenu_center_entry(string); 

   grub_env_set(key, new_string);
   grub_free(new_string);
 
   return GRUB_ERR_NONE;
}


static grub_err_t
miray_cmd_submenu (struct grub_command *cmd __attribute__ ((unused)),
		int argc, char *argv[])
{
   if (argc < 1) return grub_error(GRUB_ERR_BAD_ARGUMENT, "Argument missing");
   
   return miray_bootscreen_run_submenu(argv[0]);
}

static int getkey_norefresh(void)
{
   while (1)
   {
      int ret = grub_getkey_noblock ();
      if (ret != GRUB_TERM_NO_KEY)
         return ret;
      grub_cpu_idle ();
    }
}

static grub_err_t
miray_cmd_fatal (struct grub_command *cmd __attribute__ ((unused)),
		int argc, char *argv[])
{
   if (!miray_debugmode())
   {
      while(grub_error_pop()) {}
   }
   
   if (argc > 0) 
   {
      //miray_screen_message_box((const char **)argv, 0);
      int i;
      const char ** tmp = grub_zalloc((argc + 1) * sizeof(char *));
      for (i = 0; i < argc; i++)
         tmp[i] = argv[i];
      miray_screen_message_box(tmp, 0);
      grub_free(tmp);
   }  
   else
   {
      const char *msg[] = {"FATAL INTERNAL ERROR", "Press any key to reboot", 0};
      miray_screen_message_box(msg, 0);
   }

   if (grub_term_inputs)
   {
      getkey_norefresh ();
   }
 
   grub_reboot();

}

static grub_err_t
miray_cmd_msg_box(struct grub_command *cmd __attribute__ ((unused)),
		int argc, char *argv[])
{
   if (argc == 0)
   {
      return grub_error(GRUB_ERR_BAD_ARGUMENT, "Argument missing");
   }

   miray_screen_message_box((const char **) argv, 0);

   grub_errno = GRUB_ERR_NONE;
   return GRUB_ERR_NONE;
}

#define INIT_TERMWIDTH 73

static grub_command_t cmd_bootscreen, cmd_center;
static grub_command_t cmd_submenu;
static grub_command_t cmd_fatal;
static grub_command_t cmd_msg_box;
static grub_command_t cmd_cmdline;
// static grub_command_t cmd_booting;
static void *preboot_handle;
GRUB_MOD_INIT(miray_bootscreen)
{
   grub_uint32_t hline[INIT_TERMWIDTH + 1];
   char *c_hline;
   int i;

   for (i = 0; i < INIT_TERMWIDTH; i++) {
	hline[i] = GRUB_TERM_DISP_HLINE;
   }
   hline[INIT_TERMWIDTH] = '\0';

   c_hline = grub_ucs4_to_utf8_alloc(hline, INIT_TERMWIDTH);
   grub_env_set("hline", c_hline);
   grub_free(c_hline);


   if (custom_menu_handler == 0)
      custom_menu_handler = miray_bootscreen_execute;

   miray_screen_init();
   miray_segfile_init();
 
   cmd_bootscreen = grub_register_command("miray_bootscreen",
				      miray_cmd_bootscreen, 
				      0, N_("display the Miray bootscreen"));

   cmd_center = grub_register_command("miray_center_env",
				      miray_cmd_center_env, 
				      0, N_("center string and output to env"));

   cmd_cmdline = grub_register_command ("miray_cmdline",
                                       miray_cmd_cmdline,
                                        0,N_("Open a commandline window"));

   cmd_submenu = grub_register_command("miray_submenu",
				       miray_cmd_submenu,
				       0, N_("run submenu"));

   cmd_fatal = grub_register_command ("miray_fatal",
                                       miray_cmd_fatal,
                                        0,N_("display message and stop"));

   cmd_msg_box = grub_register_command("miray_msg_box",
                                        miray_cmd_msg_box,
                                        0, N_("display message box"));

  preboot_handle = grub_loader_register_preboot_hook (miray_bootscreen_preboot, 0, GRUB_LOADER_PREBOOT_HOOK_PRIO_NORMAL);

}

GRUB_MOD_FINI(miray_bootscreen)
{
   if (custom_menu_handler == miray_bootscreen_execute)
	custom_menu_handler = 0;

   grub_unregister_command (cmd_bootscreen);
   grub_unregister_command (cmd_center);
   grub_unregister_command (cmd_cmdline);
   grub_unregister_command (cmd_submenu);
   grub_unregister_command (cmd_fatal);
   grub_unregister_command (cmd_msg_box);

   if (preboot_handle != 0)
      grub_loader_unregister_preboot_hook(preboot_handle);

   miray_segfile_fini();
   miray_screen_fini();
}

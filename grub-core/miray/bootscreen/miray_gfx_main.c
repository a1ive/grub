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
#include <grub/err.h>
#include <grub/command.h>
#include <grub/normal.h>

#include "miray_screen.h"
#include <grub/miray_debug.h>


GRUB_MOD_LICENSE ("GPLv3+");

extern struct miray_screen * miray_gfx_screen_new(struct grub_term_output *term);
static int active = 0;

static grub_err_t
cmd_test_start (struct grub_command *cmd __attribute__ ((unused)),
                int argc __attribute__ ((unused)), char *argv[] __attribute__ ((unused)))
{
   grub_err_t ret = GRUB_ERR_NONE;
   
   struct miray_screen * scr = miray_gfx_screen_new(grub_term_outputs);
   if (scr == 0)
   {
      return grub_errno;
   }

   if ((ret = miray_screen_set_screen(scr)) != GRUB_ERR_NONE)
   {
      miray_screen_destroy(scr);
      return ret;
   }

   active = 1;
   return GRUB_ERR_NONE;    
}

static grub_err_t
cmd_test_stop (struct grub_command *cmd __attribute__ ((unused)),
               int argc __attribute__ ((unused)), char *argv[] __attribute__ ((unused)))
{
   grub_err_t ret = GRUB_ERR_NONE;

   if (active)
   {
      ret = miray_screen_set_screen(0);
      active = 0;
   }
   
   return ret;
}

static grub_err_t
cmd_gfx_toggle (struct grub_command *cmd __attribute__ ((unused)),
                int argc __attribute__ ((unused)), char *argv[] __attribute__ ((unused)))
{
   grub_err_t ret = GRUB_ERR_NONE;
   
   if (active)
   {
      ret = miray_screen_set_screen(0);
      active = 0;
   }
   else
   {
      struct miray_screen * scr = miray_gfx_screen_new(grub_term_outputs);
      if (scr == 0)
      {
         return grub_errno;
      }
      
      if ((ret = miray_screen_set_screen(scr)) != GRUB_ERR_NONE)
      {
         miray_screen_destroy(scr);
         return ret;
      }

      active = 1;
   }

   return ret;
}

static grub_err_t
cmd_gfx_check(struct grub_command *cmd __attribute__ ((unused)),
              int argc __attribute__ ((unused)), char *argv[] __attribute__ ((unused)))
 {
    if (active)
         return GRUB_ERR_NONE;
    else
         return grub_error(GRUB_ERR_MENU, "GFX menu not active");
 }


static grub_command_t _cmd_gfx_toggle;
static grub_command_t _cmd_test_start, _cmd_test_stop;
static grub_command_t _cmd_gfx_check;

GRUB_MOD_INIT(miray_gfx_bootscreen)
{
   _cmd_test_start = grub_register_command("miray_gfx_start",
                                           cmd_test_start,
                                           0, N_("display the Miray bootscreen"));

   _cmd_test_stop = grub_register_command("miray_gfx_stop",
                                          cmd_test_stop,
                                          0, N_("center string and output to env"));

   _cmd_gfx_toggle = grub_register_command("miray_gfx_toggle",
                                           cmd_gfx_toggle,
                                           0, N_("toggle text and gfx mode"));

   _cmd_gfx_check = grub_register_command("miray_gfx_active",
                                          cmd_gfx_check,
                                          0, N_("Check if gfx menu is active"));

   struct miray_screen * scr = miray_gfx_screen_new(grub_term_outputs);
   if (scr != 0)
   {
      if (miray_screen_set_screen(scr) == GRUB_ERR_NONE)
      {
         active = 1;
      }
      else
      {
         if (miray_debugmode())
         {
            grub_error(GRUB_ERR_MENU, "Could not set gfx menu");
            grub_wait_after_message();
         }
         miray_screen_destroy(scr);
         scr = NULL;
      }
   }
   else
   {
      if (miray_debugmode())
      {
         grub_error(GRUB_ERR_MENU, "Could not create gfx screen");
         grub_wait_after_message();
      }
   }

   while (grub_error_pop()) {};
}

GRUB_MOD_FINI(miray_gfX_bootscreen)
{
   if (active)
      miray_screen_set_screen(0);

   grub_unregister_command (_cmd_test_start);
   grub_unregister_command (_cmd_test_stop);
   grub_unregister_command (_cmd_gfx_toggle);
   grub_unregister_command (_cmd_gfx_check);
}

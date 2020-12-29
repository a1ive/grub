/* main.c - the normal mode main routine */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2000,2001,2002,2003,2005,2006,2007,2008,2009  Free Software Foundation, Inc.
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

#include <grub/kernel.h>
#include <grub/normal.h>
#include <grub/dl.h>
#include <grub/misc.h>
#include <grub/file.h>
#include <grub/mm.h>
#include <grub/lib.h>
#include <grub/env.h>
#include <grub/menu.h>
#include <grub/command.h>
#include <grub/i18n.h>

GRUB_EXPORT(grub_normal_exit_level);

static int nested_level = 0;
int grub_normal_exit_level = 0;

static void
read_lists (const char *val)
{
#ifdef GRUB_MACHINE_EMU
  (void) val;
#else
  read_command_list (val);
  read_fs_list (val);
  read_crypto_list (val);
  read_terminal_list (val);
#endif
}

static char *
read_lists_hook (struct grub_env_var *var __attribute__ ((unused)),
		 const char *val)
{
  read_lists (val);
  return val ? grub_strdup (val) : NULL;
}

/* This starts the normal mode.  */
static void
grub_enter_normal_mode (const char *config)
{
  const char *prefix = grub_env_get ("prefix");

  nested_level++;
  read_lists (prefix);
  read_handler_list ();
  grub_autolist_font = grub_autolist_load ("fonts/font.lst");
  grub_errno = 0;
  grub_register_variable_hook ("prefix", NULL, read_lists_hook);
  grub_command_execute ("parser.grub", 0, 0);
  grub_command_execute ("controller.normal", 0, 0);
  grub_menu_execute (config, 0, 0);
  nested_level--;
  if (grub_normal_exit_level)
    grub_normal_exit_level--;
}

/* Enter normal mode from rescue mode.  */
static grub_err_t
grub_cmd_normal (struct grub_command *cmd __attribute__ ((unused)),
		 int argc, char *argv[])
{
  if (argc == 0)
    {
      /* Guess the config filename. It is necessary to make CONFIG static,
	 so that it won't get broken by longjmp.  */
      static char *config;
      const char *prefix;

      prefix = grub_env_get ("prefix");
      if (prefix)
	{
	  config = grub_xasprintf ("%s/burg.cfg", prefix);
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
  return 0;
}

/* Exit from normal mode to rescue mode.  */
static grub_err_t
grub_cmd_normal_exit (struct grub_command *cmd __attribute__ ((unused)),
		      int argc __attribute__ ((unused)),
		      char *argv[] __attribute__ ((unused)))
{
  if (nested_level <= grub_normal_exit_level)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "not in normal environment");
  grub_normal_exit_level++;
  return GRUB_ERR_NONE;
}

static grub_command_t export_cmd;

static grub_err_t
grub_cmd_export (struct grub_command *cmd __attribute__ ((unused)),
		 int argc, char **args)
{
  if (argc < 1)
    return grub_error (GRUB_ERR_BAD_ARGUMENT,
		       "no environment variable specified");

  grub_env_export (args[0]);
  return 0;
}

GRUB_MOD_INIT(normal)
{
  grub_env_export ("root");
  grub_env_export ("prefix");

  export_cmd = grub_register_command ("export", grub_cmd_export,
				      N_("ENVVAR"), N_("Export a variable."));

  /* Normal mode shouldn't be unloaded.  */
  if (mod)
    grub_dl_ref (mod);

  grub_history_init (GRUB_DEFAULT_HISTORY_SIZE);

  grub_install_newline_hook ();

  /* Register a command "normal" for the rescue mode.  */
  grub_register_command ("normal", grub_cmd_normal,
			 0, N_("Enter normal mode."));
  grub_register_command ("normal_exit", grub_cmd_normal_exit,
			 0, N_("Exit from normal mode."));
}

GRUB_MOD_FINI(normal)
{
  grub_unregister_command (export_cmd);

  grub_history_init (0);
  grub_register_variable_hook ("pager", 0, 0);
  grub_fs_autoload_hook = 0;
  free_handler_list ();
}

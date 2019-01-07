/* read.c - Command to read variables from user.  */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2006,2007,2008  Free Software Foundation, Inc.
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
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/normal.h>
#include <grub/env.h>
#include <grub/term.h>
#include <grub/types.h>
#include <grub/command.h>
#include <grub/i18n.h>

GRUB_MOD_LICENSE ("GPLv3+");

static grub_err_t
grub_cmd_read (grub_command_t cmd __attribute__ ((unused)), int argc, char **args)
{
  char *line = grub_getline ();
  if (! line)
    return grub_errno;
  if (argc > 0)
    grub_env_set (args[0], line);

  grub_free (line);
  return 0;
}

static grub_err_t
grub_cmd_read_from_file (grub_command_t cmd __attribute__ ((unused)), int argc, char **args)
{
  char *line;
  int i = 0;
  grub_file_t file;

  if (argc < 1)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("file name expected"));
  if (argc < 2)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("variable name expected"));
  file = grub_file_open (args[i++], GRUB_FILE_TYPE_CAT);
  if (! file)
    return grub_errno;
  while ( i < argc )
    {
      line = grub_file_getline (file);
      if ( !line )
	break;
      grub_env_set (args[i++], line);
      grub_free (line);
    }
  grub_file_close (file);
  if (i != argc)
    return GRUB_ERR_OUT_OF_RANGE;
  return 0;
}

static grub_command_t cmd;
static grub_command_t cme;

GRUB_MOD_INIT(read)
{
  cmd = grub_register_command ("read", grub_cmd_read,
			       N_("[ENVVAR]"),
			       N_("Set variable with user input."));
  cme = grub_register_command ("read_file", grub_cmd_read_from_file,
			       N_("FILE ENVVAR [...]"),
			       N_("Set variable(s) with line(s) from FILE."));
}

GRUB_MOD_FINI(read)
{
  grub_unregister_command (cmd);
  grub_unregister_command (cme);
}

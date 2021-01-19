/* isprefix.c -- Tests if param1 is a prefix of param2  */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2010
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
#include <grub/command.h>
#include <grub/i18n.h>

GRUB_MOD_LICENSE ("GPLv3+");

static grub_err_t
grub_cmd_isprefix (grub_command_t cmd __attribute__ ((unused)),
	       int argc, char **args)
{
  grub_size_t len = 0;

  if (argc < 2)
    return grub_error (GRUB_ERR_TEST_FAILURE, "false");

  len = grub_strlen(args[0]);

  if (grub_strlen(args[1]) < len)
    return grub_error (GRUB_ERR_TEST_FAILURE, "false");

  return grub_strncmp (args[0], args[1], len);
}

static grub_command_t cmd;

GRUB_MOD_INIT(isprefix)
{
  cmd = grub_register_command ("isprefix", grub_cmd_isprefix,
				 0, N_("Test if arg1 is a prefix of arg2"));
}

GRUB_MOD_FINI(test)
{
  grub_unregister_command (cmd);
}

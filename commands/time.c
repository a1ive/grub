/*
 *  BURG - Brand-new Universal loadeR from GRUB
 *  Copyright 2010 Bean Lee - All Rights Reserved
 *
 *  BURG is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  BURG is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with BURG.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <grub/dl.h>
#include <grub/err.h>
#include <grub/misc.h>
#include <grub/datetime.h>
#include <grub/command.h>
#include <grub/i18n.h>

static grub_err_t
grub_cmd_time (grub_command_t cmd __attribute__ ((unused)),
	       int argc, char **args)
{
  struct grub_datetime t1, t2;
  int delta;
  grub_err_t err;

  if (argc < 1)
    grub_error (GRUB_ERR_BAD_ARGUMENT, "no command");

  if (grub_get_datetime (&t1))
    return grub_errno;

  err = grub_command_execute (args[0], argc - 1, args + 1);

  if (grub_get_datetime (&t2))
    return grub_errno;

  delta = (int) t2.year - (int) t1.year;
  delta = delta * 365 + (int) t2.day - (int) t1.day;
  delta = delta * 24 + (int) t2.hour - (int) t1.hour;
  delta = delta * 60 + (int) t2.minute - (int) t1.minute;
  delta = delta * 60 + (int) t2.second - (int) t1.second;

  grub_printf ("%d seconds\n", delta);

  return err;
}

static grub_command_t cmd;

GRUB_MOD_INIT(time)
{
  cmd =
    grub_register_command ("time", grub_cmd_time,
			   N_("COMMAND"),
			   N_("Execute COMMAND and print run time."));
}

GRUB_MOD_FINI(time)
{
  grub_unregister_command (cmd);
}

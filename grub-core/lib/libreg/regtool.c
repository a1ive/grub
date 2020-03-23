/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2020  Free Software Foundation, Inc.
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
#include <grub/mm.h>
#include <grub/misc.h>
#include <grub/types.h>
#include <grub/file.h>
#include <grub/command.h>
#include <grub/term.h>
#include <grub/i18n.h>
#include <grub/charset.h>

#include "reg.h"

GRUB_MOD_LICENSE ("GPLv3+");

#define MAX_NAME 255

static grub_err_t
grub_cmd_reg (grub_command_t cmd __attribute__ ((unused)),
              int argc, char **args)

{
  HKEY root = 0;
  grub_err_t errno;
  grub_file_t file = 0;
  grub_reg_hive_t *hive = NULL;
  if (argc < 1)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "bad argument.");
  file = grub_file_open (args[0], GRUB_FILE_TYPE_CAT);
  if (!file)
    return grub_error (GRUB_ERR_FILE_NOT_FOUND, "file not found.");
  errno = grub_open_hive (file, &hive);
  if (errno || !hive)
    return grub_error (GRUB_ERR_BAD_OS, "load error.");
  hive->find_root (hive, &root);
  grub_printf ("root: %d\n", root);
  grub_refresh ();
  uint32_t i = 0;
  uint16_t wname[MAX_NAME];
  unsigned char name[MAX_NAME];
  while (1)
  {
    errno = hive->enum_keys (hive, root, i, wname, MAX_NAME);
    if (errno == GRUB_ERR_FILE_NOT_FOUND)
      break;
    if (errno)
    {
      grub_printf ("ERROR: %d %d", i, errno);
      break;
    }
    grub_utf16_to_utf8 (name, wname, MAX_NAME);
    grub_printf ("%d %s\n", i, name);
    grub_refresh ();
    i++;
  }
  hive->close (hive);
  grub_file_close (file);
  return GRUB_ERR_NONE;
}

static grub_command_t cmd;

GRUB_MOD_INIT(libreg)
{
  cmd = grub_register_command ("regtool", grub_cmd_reg, 0,
                               N_("Load Windows Registry."));
}

GRUB_MOD_FINI(libreg)
{
  grub_unregister_command (cmd);
}

/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2019  Free Software Foundation, Inc.
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

#include <grub/types.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/err.h>
#include <grub/dl.h>
#include <grub/extcmd.h>
#include <grub/i18n.h>

#include "ini.h"

GRUB_MOD_LICENSE ("GPLv3+");
GRUB_MOD_DUAL_LICENSE ("MIT");

static grub_err_t
grub_cmd_ini (grub_extcmd_context_t ctxt __attribute__ ((unused)),
              int argc, char **args)
{
  if (argc != 3)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "string required");
  
  ini_t *config = ini_load (args[0]);
 
  if (!config)
  {
    grub_printf ("cannot parse file: %s\n", args[0]);
    return 0;
  }

  const char *name = ini_get(config, args[1], args[2]);
  if (name)
  {
    grub_printf("[%s] %s: %s\n", args[1], args[2], name);
  }

  ini_free(config);
  return 0;
}

static grub_extcmd_t cmd;

GRUB_MOD_INIT(ini)
{
  cmd = grub_register_extcmd ("ini", grub_cmd_ini, 0, N_("FILE SECTION KEY"),
			      N_("INI parser"), 0);
}

GRUB_MOD_FINI(ini)
{
}

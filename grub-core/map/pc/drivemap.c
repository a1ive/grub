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

#include <grub/extcmd.h>
#include <grub/dl.h>
#include <grub/mm.h>
#include <grub/misc.h>
#include <grub/disk.h>
#include <grub/i18n.h>

#include "drivemap.h"

GRUB_MOD_LICENSE ("GPLv3+");

static const struct grub_arg_option options[] = {
  /* TRANSLATORS: In this file "mapping" refers to a change GRUB makes so if
     your language doesn't have an equivalent of "mapping" you can
     use the word like "rerouting".
   */
  {"list", 'l', 0, N_("Show the current mappings."), 0, 0},
  {"reset", 'r', 0, N_("Reset all mappings to the default values."), 0, 0},
  {"swap", 's', 0, N_("Perform both direct and reverse mappings."), 0, 0},
  {0, 0, 0, 0, 0, 0}
};

static grub_extcmd_t cmd;

GRUB_MOD_INIT (drivemap)
{
  cmd = grub_register_extcmd ("drivemap", grub_pcbios_drivemap_cmd, 0,
                              N_("-l | -r | [-s] grubdev osdisk."),
                              N_("Manage the BIOS drive mappings."),
                              options);
}

GRUB_MOD_FINI (drivemap)
{
  grub_unregister_extcmd (cmd);
}

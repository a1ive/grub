/* fwsetup.c - Reboot into firmware setup menu. */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2012  Free Software Foundation, Inc.
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
#include <grub/mm.h>
#include <grub/misc.h>
#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#include <grub/command.h>
#include <grub/i18n.h>
#include <grub/dl.h>

GRUB_MOD_LICENSE ("GPLv3+");

static grub_err_t
grub_cmd_fwsetup (grub_command_t cmd __attribute__ ((unused)),
                  int argc __attribute__ ((unused)),
                  char **args __attribute__ ((unused)))
{
  grub_err_t status;
  status = grub_efi_fwsetup_setvar ();
  if (status != GRUB_ERR_NONE)
    return status;
  grub_reboot ();
  return GRUB_ERR_BUG;
}

static grub_command_t cmd = NULL;

GRUB_MOD_INIT (efifwsetup)
{
  if (grub_efi_fwsetup_is_supported ())
    cmd = grub_register_command ("fwsetup", grub_cmd_fwsetup, NULL,
                    N_("Reboot into firmware setup menu."));

}

GRUB_MOD_FINI (efifwsetup)
{
  if (cmd)
    grub_unregister_command (cmd);
}

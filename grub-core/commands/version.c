/* version.c - Command to print the grub version and build info. */
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
#include <grub/term.h>
#include <grub/time.h>
#include <grub/types.h>
#include <grub/misc.h>
#include <grub/extcmd.h>
#include <grub/i18n.h>
#include <grub/charset.h>
#ifdef GRUB_MACHINE_EFI
#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#endif

GRUB_MOD_LICENSE ("GPLv3+");

static grub_err_t
grub_cmd_version (grub_command_t cmd __attribute__ ((unused)),
                  int argc __attribute__ ((unused)),
                  char **args __attribute__ ((unused)))
{
  grub_printf (_("GNU GRUB version: %s\n"), PACKAGE_VERSION);
  grub_printf (_("Platform: %s-%s\n"), GRUB_TARGET_CPU, GRUB_PLATFORM);
  if (grub_strlen(GRUB_RPM_VERSION) != 0)
    grub_printf (_("RPM package version: %s\n"), GRUB_RPM_VERSION);
  grub_printf (_("Compiler version: %s\n"), __VERSION__);
  grub_printf (_("Build date: %s\n"), GRUB_BUILD_DATE);

#ifdef GRUB_MACHINE_EFI
  const grub_efi_system_table_t *st = grub_efi_system_table;
  grub_efi_uint16_t uefi_major_rev = st->hdr.revision >> 16;
  grub_efi_uint16_t uefi_minor_rev = st->hdr.revision & 0xffff;
  grub_printf ("UEFI revision: v%d.%d (", uefi_major_rev, uefi_minor_rev);
  {
    char *vendor;
    grub_uint16_t *vendor_utf16;
    
    for (vendor_utf16 = st->firmware_vendor; *vendor_utf16; vendor_utf16++);
    vendor = grub_malloc (4 * (vendor_utf16 - st->firmware_vendor) + 1);
    if (!vendor)
      return grub_errno;
    *grub_utf16_to_utf8 ((grub_uint8_t *) vendor, st->firmware_vendor,
			 vendor_utf16 - st->firmware_vendor) = 0;
    grub_printf ("%s, ", vendor);
    grub_free (vendor);
  }
  grub_printf ("0x%08x)\n", st->firmware_revision);
#endif

  return 0;
}

static grub_command_t cmd;

GRUB_MOD_INIT(version)
{
  cmd = grub_register_command ("version", grub_cmd_version, NULL,
			       N_("Print version and build information."));
}

GRUB_MOD_FINI(version)
{
  grub_unregister_command (cmd);
}

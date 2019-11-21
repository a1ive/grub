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
 *
 */

#include <grub/dl.h>
#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#include <grub/device.h>
#include <grub/err.h>
#include <grub/env.h>
#include <grub/extcmd.h>
#include <grub/file.h>
#include <grub/i18n.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/types.h>
#include <grub/term.h>

#include <maplib.h>
#include <private.h>
#include <efiapi.h>
#include <wimboot.h>
#include <vfat.h>

#include "ntboot.h"

GRUB_MOD_LICENSE ("GPLv3+");

static const struct grub_arg_option options_ntboot[] = {
  {"gui", 'g', 0, N_("Display graphical boot messages."), 0, 0},
  {"pause", 'p', 0, N_("Show info and wait for keypress."), 0, 0},
  {"vhd", 'v', 0, N_("Boot NT6+ VHD/VHDX."), 0, 0},
  {"wim", 'w', 0, N_("Boot NT6+ WIM."), 0, 0},
  {"efi", 'e', 0, N_("Specify the bootmgfw.efi file."), N_("FILE"), ARG_TYPE_FILE},
  {"sdi", 's', 0, N_("Specify the boot.sdi file."), N_("FILE"), ARG_TYPE_FILE},
  {0, 0, 0, 0, 0, 0}
};

enum options_ntboot
{
  NTBOOT_GUI,
  NTBOOT_PAUSE,
  NTBOOT_VHD,
  NTBOOT_WIM,
  NTBOOT_EFI,
  NTBOOT_SDI,
};

static grub_err_t
grub_cmd_ntboot (grub_extcmd_context_t ctxt,
                  int argc, char *argv[])
{
  struct grub_arg_list *state = ctxt->state;
  grub_file_t bootmgr = 0;
  grub_file_t bootsdi = 0;
  grub_file_t file = 0;
  char *filename = NULL;
  enum boot_type type;

  if (argc != 1)
  {
    grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));
    goto fail;
  }
  file = grub_file_open (argv[0], GRUB_FILE_TYPE_GET_SIZE);
  if (!file)
  {
    grub_error (GRUB_ERR_FILE_READ_ERROR, N_("failed to open file"));
    goto fail;
  }
  if (!file->device || !file->device->disk ||
      file->device->disk->name[0] != 'h' || !file->device->disk->partition)
  {
    grub_error (GRUB_ERR_BAD_DEVICE,
                "this command is available only for disk devices");
    goto fail;
  }
  if (argv[0][0] == '(')
  {
    filename = grub_strchr (argv[0], '/');
    if (!filename)
      goto fail;
  }
  else if (argv[0][0] == '/')
  {
    filename = &argv[0][0];
  }
  else
    goto fail;

  if (filename[grub_strlen (filename) - 1] == 'm' ||
      filename[grub_strlen (filename) - 1] == 'M') /* wim */
    type = BOOT_WIM;
  else
    type = BOOT_VHD;
  if (state[NTBOOT_WIM].set)
    type = BOOT_WIM;
  if (state[NTBOOT_VHD].set)
    type = BOOT_VHD;

  wimboot_cmd.rawbcd = TRUE;
  if (state[NTBOOT_GUI].set)
    wimboot_cmd.gui = TRUE;
  if (state[NTBOOT_PAUSE].set)
    wimboot_cmd.pause = TRUE;
  if (state[NTBOOT_EFI].set)
    bootmgr = grub_file_open (state[NTBOOT_EFI].arg,
                              GRUB_FILE_TYPE_EFI_CHAINLOADED_IMAGE);
  else
    bootmgr = grub_file_open ("/efi/microsoft/boot/bootmgfw.efi",
                              GRUB_FILE_TYPE_EFI_CHAINLOADED_IMAGE);
  if (!bootmgr)
  {
    grub_error (GRUB_ERR_FILE_READ_ERROR, N_("failed to open bootmgfw.efi"));
    goto fail;
  }
  add_file ("bootmgfw.efi", bootmgr, bootmgr->size, efi_read_file);

  if (type == BOOT_WIM)
  {
    if (state[NTBOOT_SDI].set)
      bootsdi = grub_file_open (state[NTBOOT_SDI].arg,
                                GRUB_FILE_TYPE_LOOPBACK);
    else
      bootsdi = grub_file_open ("/boot/boot.sdi", GRUB_FILE_TYPE_LOOPBACK);
    if (!bootsdi)
    {
      grub_error (GRUB_ERR_FILE_READ_ERROR, N_("failed to open boot.sdi"));
      goto fail;
    }
    add_file ("boot.sdi", bootsdi, bootsdi->size, efi_read_file);
  }

  bcd_patch (type, filename, file->device->disk->name,
             file->device->disk->partition->start,
             file->device->disk->partition->number,
             file->device->disk->partition->partmap->name);
  add_file ("bcd", bcd, bcd_len, mem_read_file);
  if (wimboot_cmd.pause)
    grub_getkey ();
  wimboot_install ();
  wimboot_boot (bootmgfw);
fail:
  if (file)
    grub_file_close (file);
  if (bootmgr)
    grub_file_close (bootmgr);
  if (bootsdi)
    grub_file_close (bootsdi);
  return grub_errno;
}

static grub_extcmd_t cmd_ntboot;

GRUB_MOD_INIT(ntboot)
{
  cmd_ntboot = grub_register_extcmd ("ntboot", grub_cmd_ntboot, 0,
                    N_("[-v|-w] [--efi=FILE] [--sdi=FILE] FILE"),
                    N_("Boot NT6+ VHD/VHDX/WIM"), options_ntboot);
}

GRUB_MOD_FINI(ntboot)
{
  grub_unregister_extcmd (cmd_ntboot);
}

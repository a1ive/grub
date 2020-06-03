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
#include <grub/partition.h>

#include <misc.h>
#include <wimboot.h>
#include <ntboot.h>
#include <vfat.h>

#include "ntboot.h"

GRUB_MOD_LICENSE ("GPLv3+");

static const struct grub_arg_option options_ntboot[] = {
  {"gui", 'g', 0, N_("Display graphical boot messages."), 0, 0},
  {"pause", 'p', 0, N_("Show info and wait for keypress."), 0, 0},
  {"vhd", 'v', 0, N_("Boot NT6+ VHD/VHDX."), 0, 0},
  {"wim", 'w', 0, N_("Boot NT6+ WIM."), 0, 0},
  {"win", 'n', 0, N_("Boot NT6+ Windows."), 0, 0},
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
  NTBOOT_WIN,
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
  grub_disk_t disk = 0;
  grub_file_t bcd_file = 0;
  char bcd_name[64];

  if (argc != 1)
  {
    grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));
    goto fail;
  }

  wimboot_cmd.rawbcd = 1;
  wimboot_cmd.rawwim = 1;
  if (state[NTBOOT_GUI].set)
    wimboot_cmd.gui = 1;
  if (state[NTBOOT_PAUSE].set)
    wimboot_cmd.pause = 1;
  if (state[NTBOOT_EFI].set)
    bootmgr = grub_file_open (state[NTBOOT_EFI].arg, GRUB_FILE_TYPE_GET_SIZE);
  else
    bootmgr = grub_file_open ("/efi/microsoft/boot/bootmgfw.efi",
                              GRUB_FILE_TYPE_GET_SIZE);
  if (!bootmgr)
  {
    grub_error (GRUB_ERR_FILE_READ_ERROR, N_("failed to open bootmgfw.efi"));
    goto fail;
  }
  file_add ("bootmgfw.efi", bootmgr, &wimboot_cmd);

  grub_snprintf (bcd_name, 64, "mem:%p:size:%u", &bcd, bcd_len);
  bcd_file = grub_file_open (bcd_name, GRUB_FILE_TYPE_GET_SIZE);
  file_add ("bcd", bcd_file, &wimboot_cmd);

  if (state[NTBOOT_WIN].set)
  {
    int namelen = grub_strlen (argv[0]);
    if (argv[0][0] == '(' && argv[0][namelen - 1] == ')')
    {
      argv[0][namelen - 1] = 0;
      disk = grub_disk_open (&argv[0][1]);
    }
    else
      disk = grub_disk_open (argv[0]);
    if (!disk || disk->name[0] != 'h' || !disk->partition)
    {
      grub_error (GRUB_ERR_BAD_DEVICE,
                "this command is available only for disk devices");
      goto fail;
    }
    type = BOOT_WIN;
    ntboot_patch_bcd (type, NULL, disk->name,
                      grub_partition_get_start (disk->partition),
                      disk->partition->number,
                      disk->partition->partmap->name);
    grub_wimboot_install ();
    grub_wimboot_boot (bootmgfw, &wimboot_cmd);
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
    file_add ("boot.sdi", bootsdi, &wimboot_cmd);
  }

  ntboot_patch_bcd (type, filename, file->device->disk->name,
             grub_partition_get_start (file->device->disk->partition),
             file->device->disk->partition->number,
             file->device->disk->partition->partmap->name);

  grub_wimboot_install ();
  grub_wimboot_boot (bootmgfw, &wimboot_cmd);
fail:
  if (disk)
    grub_disk_close (disk);
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

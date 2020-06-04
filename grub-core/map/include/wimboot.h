 /*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2019,2020  Free Software Foundation, Inc.
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

#ifndef WIMBOOT_PRIVATE_DATA_H
#define WIMBOOT_PRIVATE_DATA_H

#include <grub/misc.h>
#include <grub/disk.h>
#include <grub/file.h>

#include <misc.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <vfat.h>

struct wimboot_cmdline
{
  grub_uint8_t gui;
  grub_uint8_t rawbcd;
  grub_uint8_t rawwim;
  unsigned int index;
  grub_uint8_t pause;
  wchar_t inject[256];
};

extern struct wimboot_cmdline wimboot_cmd;
extern struct vfat_file *bootmgfw;

#ifdef GRUB_MACHINE_EFI
extern grub_efivdisk_t wimboot_disk, wimboot_part;
#endif

int file_add (const char *name, grub_file_t data, struct wimboot_cmdline *cmd);
void grub_wimboot_extract (struct wimboot_cmdline *cmd);
void grub_wimboot_init (int argc, char *argv[]);

#ifdef GRUB_MACHINE_EFI
grub_err_t grub_wimboot_install (void);
void grub_wimboot_boot (struct vfat_file *file, struct wimboot_cmdline *cmd);
#endif

void vfat_patch_bcd (struct vfat_file *file, void *data, size_t offset, size_t len);

#endif

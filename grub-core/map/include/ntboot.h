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

#ifndef NTBOOT_PRIVATE_DATA_H
#define NTBOOT_PRIVATE_DATA_H

#include <grub/types.h>

extern char bcd[];
extern unsigned int bcd_len;

enum boot_type
{
  BOOT_WIN,
  BOOT_WIM,
  BOOT_VHD,
};

void
ntboot_patch_bcd (enum boot_type type, const char *path, const char *diskname,
           grub_disk_addr_t lba, int partnum, const char *partmap);

#endif

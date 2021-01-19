/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2006,2007  Free Software Foundation, Inc.
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

#ifndef GRUB_EFI_DISK_HEADER
#define GRUB_EFI_DISK_HEADER	1

#include <grub/efi/api.h>
#include <grub/symbol.h>
#include <grub/disk.h>

grub_efi_handle_t
EXPORT_FUNC(grub_efidisk_get_device_handle) (grub_disk_t disk);
char *EXPORT_FUNC(grub_efidisk_get_device_name) (grub_efi_handle_t *handle);

void grub_efidisk_init (void);
void grub_efidisk_fini (void);

// Miray exported functions
grub_efi_device_path_t *
EXPORT_FUNC(miray_efi_duplicate_device_path) (const grub_efi_device_path_t *dp);

char *
EXPORT_FUNC(miray_efi_get_diskname_from_path) (const grub_efi_device_path_t *dp);

#endif /* ! GRUB_EFI_DISK_HEADER */

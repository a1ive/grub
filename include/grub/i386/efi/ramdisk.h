/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2019  Free Software Foundation, Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GRUB_EFI_RAMDISK_HEADER
#define GRUB_EFI_RAMDISK_HEADER

#include <grub/types.h>
#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#include <grub/misc.h>

#define GRUB_EFI_RAMDISK_PROTOCOL_GUID \
  { 0xab38a0df, 0x6873, 0x44a9, \
    { 0x87, 0xe6, 0xd4, 0xeb, 0x56, 0x14, 0x84, 0x49 } \
  }

#define GRUB_EFI_VIRTUAL_CD_GUID \
  { 0x3d5abd30, 0x4175, 0x87ce, \
    { 0x6d, 0x64, 0xd2, 0xad, 0xe5, 0x23, 0xc4, 0xbb } \
  }

#define GRUB_EFI_VIRTUAL_HD_GUID \
  { 0x77ab535a, 0x45fc, 0x624b, \
    { 0x55, 0x60, 0xf7, 0xb2, 0x81, 0xd1, 0xf9, 0x6e } \
  }

struct grub_efi_ramdisk_protocol
{
  grub_efi_status_t (*register_ramdisk) (grub_efi_uint64_t ramdisk_base,
			         grub_efi_uint64_t ramdisk_size,
                     grub_efi_guid_t *ramdisk_type,
                     grub_efi_device_path_t *parent,  /* optional */
                     grub_efi_device_path_protocol_t **device_path);
  grub_efi_status_t (*unregister_ramdisk) (
                     grub_efi_device_path_protocol_t *device_path);
};
typedef struct grub_efi_ramdisk_protocol grub_efi_ramdisk_protocol_t;

#endif
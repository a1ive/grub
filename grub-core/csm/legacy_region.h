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

#ifndef GRUB_EFI_LEGACY_REGION_HEADER
#define GRUB_EFI_LEGACY_REGION_HEADER 1

#include <grub/types.h>
#include <grub/symbol.h>
#include <grub/efi/api.h>

#define EFI_LEGACY_REGION_PROTOCOL_GUID \
  { 0xfc9013a, 0x568, 0x4ba9, \
    { 0x9b, 0x7e, 0xc9, 0xc3, 0x90, 0xa6, 0x60, 0x9b } \
  }

#define EFI_LEGACY_REGION2_PROTOCOL_GUID \
{ 0x70101eaf, 0x85, 0x440c, \
  { 0xb3, 0x56, 0x8e, 0xe3, 0x6f, 0xef, 0x24, 0xf0 } \
}

struct legacy_region_protocol
{
  grub_efi_status_t (*decode) (struct legacy_region_protocol *this,
                               grub_efi_uint32_t start,
                               grub_efi_uint32_t len,
                               grub_efi_boolean_t *on);
  grub_efi_status_t (*lock) (struct legacy_region_protocol *this,
                             grub_efi_uint32_t start,
                             grub_efi_uint32_t len,
                             grub_efi_uint32_t *granularity);
  grub_efi_status_t (*boot_lock) (struct legacy_region_protocol *this,
                                  grub_efi_uint32_t start,
                                  grub_efi_uint32_t len,
                                  grub_efi_uint32_t *granularity);
  grub_efi_status_t (*unlock) (struct legacy_region_protocol *this,
                               grub_efi_uint32_t start,
                               grub_efi_uint32_t len,
                               grub_efi_uint32_t *granularity);
};
typedef struct legacy_region_protocol legacy_region_protocol_t;

typedef enum
{
  LEGACY_REGION_DECODED,
  LEGACY_REGION_NOT_DECODED,
  LEGACY_REGION_WRITE_ENABLED,
  LEGACY_REGION_WRITE_DISABLED,
  LEGACY_REGION_BOOT_LOCKED,
  LEGACY_REGION_NOT_LOCKED,
} legacy_region_attribute;

typedef struct
{
  grub_efi_uint32_t start;
  grub_efi_uint32_t length;
  legacy_region_attribute attr;
  grub_efi_uint32_t granularity;
} legacy_region_descriptor;

struct legacy_region2_protocol
{
  grub_efi_status_t (*decode) (struct legacy_region2_protocol *this,
                               grub_efi_uint32_t start,
                               grub_efi_uint32_t len,
                               grub_efi_uint32_t *granularity,
                               grub_efi_boolean_t *on);
  grub_efi_status_t (*lock) (struct legacy_region2_protocol *this,
                             grub_efi_uint32_t start,
                             grub_efi_uint32_t len,
                             grub_efi_uint32_t *granularity);
  grub_efi_status_t (*boot_lock) (struct legacy_region2_protocol *this,
                                  grub_efi_uint32_t start,
                                  grub_efi_uint32_t len,
                                  grub_efi_uint32_t *granularity);
  grub_efi_status_t (*unlock) (struct legacy_region2_protocol *this,
                               grub_efi_uint32_t start,
                               grub_efi_uint32_t len,
                               grub_efi_uint32_t *granularity);
  grub_efi_status_t (*get_info) (struct legacy_region2_protocol *this,
                                 grub_efi_uint32_t *descriptor_count,
                                 legacy_region_descriptor **descriptor);
};
typedef struct legacy_region2_protocol legacy_region2_protocol_t;

#endif

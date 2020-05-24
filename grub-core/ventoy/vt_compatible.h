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

/******************************************************************************
 *
 * Copyright (c) 2020, longpanda <admin@ventoy.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __VENTOY_COMPATIBLE_H__
#define __VENTOY_COMPATIBLE_H__

#include <grub/types.h>
#include <grub/err.h>
#include <grub/extcmd.h>

#define VENTOY_COMPATIBLE_STR      "VENTOY COMPATIBLE"
#define VENTOY_COMPATIBLE_STR_LEN  17

#define VENTOY_GUID \
  { 0x77772020, 0x2e77, 0x6576, \
    { 0x6e, 0x74, 0x6f, 0x79, 0x2e, 0x6e, 0x65, 0x74 } \
  }

#define VENTOY_BIOS_ADDR_L 0x80000
#define VENTOY_BIOS_ADDR_U 0xA0000

typedef enum ventoy_fs_type
{
    ventoy_fs_exfat = 0, /* 0: exfat */
    ventoy_fs_ntfs,      /* 1: NTFS */
    ventoy_fs_ext,       /* 2: ext2/ext3/ext4 */
    ventoy_fs_xfs,       /* 3: XFS */
    ventoy_fs_udf,       /* 4: UDF */
    ventoy_fs_fat,       /* 5: FAT */

    ventoy_fs_max
}ventoy_fs_type;

#pragma pack(1)

typedef struct
{
  /* Signature for the information
   * the hex value is 20207777772e76656e746f792e6e6574
   */
  grub_packed_guid_t guid;
  /* This value, when added to all other 511 bytes,
   * results in the value 00h (using 8-bit addition calculations).
   */
  grub_uint8_t chksum;                // checksum
  /* GUID to uniquely identify the USB drive */
  grub_uint8_t vtoy_disk_guid[16];
  /* The USB drive size in bytes */
  grub_uint64_t vtoy_disk_size;
  /* The partition ID (begin with 1) which hold the iso file */
  grub_uint16_t vtoy_disk_part_id;
  /* The partition filesystem 0:exfat 1:ntfs other:reserved */
  grub_uint16_t vtoy_disk_part_type;
  /* The full iso file path under the partition, ( begin with '/' ) */
  char vtoy_img_path[384];
  /* The iso file size in bytes */
  grub_uint64_t vtoy_img_size;
  /*
   * Ventoy will write a copy of ventoy_image_location data into runtime memory
   * this is the physically address and length of that memory.
   * Address 0 means no such data exist.
   * Address will be aligned by 4KB.
   */
  grub_uint64_t vtoy_img_location_addr;
  grub_uint32_t vtoy_img_location_len;
  /*
   * These 32 bytes are reserved by ventoy.
   *
   * vtoy_reserved[0]: vtoy_break_level
   * vtoy_reserved[1]: vtoy_debug_level
   *
   */
  grub_uint8_t vtoy_reserved[32];    // Internal use by ventoy
  grub_uint8_t reserved[31];
} ventoy_os_param;
typedef ventoy_os_param ventoy_os_param_t;

#pragma pack()

void vt_compatible_init (void);
void vt_compatible_fini (void);

#endif


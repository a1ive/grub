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

#ifndef MAP_PRIVATE_DATA_H
#define MAP_PRIVATE_DATA_H

#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#include <grub/disk.h>
#include <grub/file.h>

#include <maplib.h>
#include <efiapi.h>
#include <stdint.h>
#include <stddef.h>

#define CD_BOOT_SECTOR 17
#define CD_BLOCK_SIZE 2048
#define CD_SHIFT 11

#define FD_BLOCK_SIZE 512 /* 0x200 */
#define FD_SHIFT 9
#define BLOCK_OF_1_44MB 0xB40

#define VDISK_BLOCKIO_TO_PARENT(a) CR(a, vdisk_t, block_io)

#define MAX_FILE_NAME_STRING_SIZE 255
#define MBR_START_LBA 0
#define PRIMARY_PART_HEADER_LBA 1
#define VDISK_MEDIA_ID 0x1

extern grub_packed_guid_t VDISK_GUID;

enum disk_type
{
  HD,
  CD,
  FD,
  MBR,
  GPT,
  VFAT, /* wimboot */
};

struct map_private_data
{
  grub_efi_boolean_t mem;
  grub_efi_boolean_t pause;
  grub_efi_boolean_t rt;
  enum disk_type type;
  grub_efi_boolean_t disk;
  void *file;
};

typedef struct
{
  grub_efi_boolean_t present;
  grub_efi_handle_t handle;
  grub_efi_device_path_t *dp;
  grub_efi_boolean_t disk;
  void *file;
  grub_efi_boolean_t mem;
  grub_efi_lba_t lba; /* wimboot */
  grub_efi_uintn_t addr;
  grub_efi_uint64_t size;
  grub_efi_uint32_t bs;
  enum disk_type type;
  block_io_protocol_t block_io;
  grub_efi_block_io_media_t media;
} vdisk_t;

/* main */
extern struct map_private_data *cmd;
extern vdisk_t vdisk;
extern vdisk_t vpart;
void file_read (grub_efi_boolean_t disk, void *file,
                void *buf, grub_efi_uintn_t len, grub_efi_uint64_t offset);
grub_efi_uint64_t get_size (grub_efi_boolean_t disk, void *file);
/* vblock */
extern block_io_protocol_t blockio_template;
/* vboot */
grub_efi_handle_t vpart_boot (grub_efi_handle_t *part_handle);
grub_efi_handle_t vdisk_boot (void);
/* vdisk */
grub_efi_status_t vdisk_install (grub_file_t file, grub_efi_boolean_t ro);
/* vpart */
grub_efi_status_t vpart_install (grub_efi_boolean_t ro);

#endif

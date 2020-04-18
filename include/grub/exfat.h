/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2013  Free Software Foundation, Inc.
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

#ifndef GRUB_EXFAT_H
#define GRUB_EXFAT_H	1

#include <grub/types.h>

struct grub_exfat_bpb
{
  grub_uint8_t jmp_boot[3];
  grub_uint8_t oem_name[8];
  grub_uint8_t mbz[53];
  grub_uint64_t num_hidden_sectors;
  grub_uint64_t num_total_sectors;
  grub_uint32_t num_reserved_sectors;
  grub_uint32_t sectors_per_fat;
  grub_uint32_t cluster_offset;
  grub_uint32_t cluster_count;
  grub_uint32_t root_cluster;
  grub_uint32_t num_serial;
  grub_uint16_t fs_revision;
  grub_uint16_t volume_flags;
  grub_uint8_t bytes_per_sector_shift;
  grub_uint8_t sectors_per_cluster_shift;
  grub_uint8_t num_fats;
  grub_uint8_t num_ph_drive;
  grub_uint8_t reserved[8];
} GRUB_PACKED;

typedef struct fat_img_chunk
{
  grub_uint32_t img_start_sector; // sector size: 2KB
  grub_uint32_t img_end_sector;   // included
  grub_uint64_t disk_start_sector; // in disk_sector_size
  grub_uint64_t disk_end_sector;   // included
} fat_img_chunk;

#define DEFAULT_CHUNK_NUM   1024

typedef struct fat_img_chunk_list
{
  grub_uint32_t max_chunk;
  grub_uint32_t cur_chunk;
  fat_img_chunk *chunk;
} fat_img_chunk_list;

int
grub_fat_get_file_chunk (grub_uint64_t part_start, grub_file_t file,
                         fat_img_chunk_list *chunk_list);

#ifdef GRUB_UTIL
#include <grub/disk.h>

grub_disk_addr_t
grub_exfat_get_cluster_sector (grub_disk_t disk, grub_uint64_t *sec_per_lcn);
#endif

#endif

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

#ifndef __VENTOY_WRAP_H__
#define __VENTOY_WRAP_H__

#include <grub/types.h>
#include <grub/iso9660.h>
#include <grub/udf.h>

extern grub_uint8_t g_ventoy_break_level;
extern grub_uint8_t g_ventoy_debug_level;

#pragma pack(4)

typedef struct ventoy_img_chunk
{
  grub_uint32_t img_start_sector; // sector size: 2KB
  grub_uint32_t img_end_sector;   // included

  grub_uint64_t disk_start_sector; // in disk_sector_size
  grub_uint64_t disk_end_sector;   // included
} ventoy_img_chunk;

#define DEFAULT_CHUNK_NUM   1024
typedef struct ventoy_img_chunk_list
{
  grub_uint32_t max_chunk;
  grub_uint32_t cur_chunk;
  ventoy_img_chunk *chunk;
} ventoy_img_chunk_list;

#pragma pack()

int vt_get_file_chunk (grub_uint64_t part_start, grub_file_t file,
                       ventoy_img_chunk_list *chunk_list);

void ventoy_fill_os_param (grub_file_t file, ventoy_os_param *param);

#endif


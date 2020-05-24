/******************************************************************************
 * ventoy.c 
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

#include <grub/types.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/err.h>
#include <grub/dl.h>
#include <grub/disk.h>
#include <grub/device.h>
#include <grub/term.h>
#include <grub/partition.h>
#include <grub/file.h>

#ifdef GRUB_MACHINE_EFI
#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#include <grub/cpu/efi/memory.h>
#endif

#include "ventoy.h"
#include "ventoy_def.h"
#include "ventoy_wrap.h"

grub_uint8_t g_ventoy_break_level = 0;
grub_uint8_t g_ventoy_debug_level = 0;

int
vt_get_file_chunk (grub_uint64_t part_start, grub_file_t file,
                   ventoy_img_chunk_list *chunk_list)
{
  grub_uint32_t max_chunk, i, m = DEFAULT_CHUNK_NUM;
  struct grub_fs_block *p;
  if (!file)
    return -1;
  if (!file->device->disk)
    return -1;
  max_chunk = grub_blocklist_convert (file);
  debug ("file: %s max_chunk=%d\n", file->name, max_chunk);
  if (!max_chunk)
    return -1;
  if (max_chunk > m)
  {
    while (max_chunk > m)
      m = m << 1;
    chunk_list->max_chunk = m;
    chunk_list->chunk = grub_realloc (chunk_list->chunk,
                                      m * sizeof (ventoy_img_chunk));
    debug ("realloc %d\n", m);
  }
  p = file->data;
  for (i = 0; i < max_chunk; i++)
  {
    chunk_list->chunk[i].disk_start_sector
            = (p->offset >> GRUB_DISK_SECTOR_BITS) + part_start;
    chunk_list->chunk[i].disk_end_sector
            = chunk_list->chunk[i].disk_start_sector
              + (p->length >> GRUB_DISK_SECTOR_BITS) - 1;
    chunk_list->chunk[i].img_start_sector =
            i ? (chunk_list->chunk[i-1].img_end_sector + 1) : 0;
    chunk_list->chunk[i].img_end_sector =
            chunk_list->chunk[i].img_start_sector + (p->length >> 11) - 1;
  }
  chunk_list->cur_chunk = max_chunk;
  return 0;
}

static int
ventoy_get_fs_type (const char *fs)
{
  if (!fs)
    return ventoy_fs_max;
  if (grub_strncmp(fs, "exfat", 5) == 0)
    return ventoy_fs_exfat;
  if (grub_strncmp(fs, "ntfs", 4) == 0)
    return ventoy_fs_ntfs;
  if (grub_strncmp(fs, "ext", 3) == 0)
    return ventoy_fs_ext;
  if (grub_strncmp(fs, "xfs", 3) == 0)
    return ventoy_fs_xfs;
  if (grub_strncmp(fs, "udf", 3) == 0)
    return ventoy_fs_udf;
  if (grub_strncmp(fs, "fat", 3) == 0)
    return ventoy_fs_fat;

  return ventoy_fs_max;
}

static int
ventoy_get_disk_guid (const char *filename, grub_uint8_t *guid)
{
  grub_disk_t disk;
  char *device_name;
  char *pos;
  char *pos2;

  device_name = grub_file_get_device_name(filename);
  if (!device_name)
    return 1;

  pos = device_name;
  if (pos[0] == '(')
    pos++;

  pos2 = grub_strstr(pos, ",");
  if (!pos2)
    pos2 = grub_strstr(pos, ")");

  if (pos2)
    *pos2 = 0;

  disk = grub_disk_open(pos);
  if (disk)
  {
    grub_disk_read(disk, 0, 0x180, 16, guid);
    grub_disk_close(disk);
  }
  else
  {
    return 1;
  }
  grub_free(device_name);
  return 0;
}

void
ventoy_fill_os_param (grub_file_t file, ventoy_os_param *param)
{
  char *pos;
  grub_uint32_t i;
  grub_uint8_t chksum = 0;
  grub_disk_t disk;
  const grub_packed_guid_t vtguid = VENTOY_GUID;

  disk = file->device->disk;
  grub_memcpy(&param->guid, &vtguid, sizeof(grub_packed_guid_t));

  param->vtoy_disk_size = disk->total_sectors * (1 << disk->log_sector_size);
  param->vtoy_disk_part_id = disk->partition->number + 1;
  param->vtoy_disk_part_type = ventoy_get_fs_type(file->fs->name);

  pos = grub_strstr (file->name, "/");
  if (!pos)
    pos = file->name;

  grub_snprintf (param->vtoy_img_path, sizeof(param->vtoy_img_path), "%s", pos);

  ventoy_get_disk_guid(file->name, param->vtoy_disk_guid);

  param->vtoy_img_size = file->size;

  param->vtoy_reserved[0] = g_ventoy_break_level;
  param->vtoy_reserved[1] = g_ventoy_debug_level;

  /* calculate checksum */
  for (i = 0; i < sizeof(ventoy_os_param); i++)
  {
    chksum += *((grub_uint8_t *)param + i);
  }
  param->chksum = (grub_uint8_t)(0x100 - chksum);

  return;
}

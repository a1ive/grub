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

#ifdef GRUB_MACHINE_EFI
void *
grub_efi_allocate_iso_buf (grub_size_t size)
{
  return grub_efi_allocate_pages_real (GRUB_EFI_MAX_USABLE_ADDRESS,
                                       GRUB_EFI_BYTES_TO_PAGES (size),
                                       GRUB_EFI_ALLOCATE_ANY_PAGES,
                                       GRUB_EFI_RUNTIME_SERVICES_DATA);
}
#endif

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
  if (!max_chunk)
    return -1;
  while (max_chunk > m)
    m = m << 1;
  chunk_list->max_chunk = m;
  chunk_list->chunk = grub_zalloc (m * sizeof (ventoy_img_chunk));
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

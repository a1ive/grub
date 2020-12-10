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
#include <grub/memory.h>

#ifdef GRUB_MACHINE_EFI
#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#endif

#include <grub4dos.h>

static grub_uint64_t min_con_mem;
static struct g4d_drive_map_slot *g4d_slot = 0;

static int
minmem_hook (grub_uint64_t addr, grub_uint64_t size,
             grub_memory_type_t type,
             void *data __attribute__ ((unused)))
{
  if (type != GRUB_MEMORY_AVAILABLE || addr > min_con_mem || size < 0x1000)
    return 0;
  if (addr + size < 0x9F000)
    min_con_mem = addr + size - 0x1000;
  return 0;
}

static grub_uint64_t
get_min_con_mem (void)
{
  min_con_mem = 0x9F000;
  grub_machine_mmap_iterate (minmem_hook, NULL);
  return min_con_mem;
}

static void
g4d_alloc_data (void)
{
  grub_addr_t address;
  grub_uint8_t *g4d_data;

  if (g4d_slot)
    return;

  address = get_min_con_mem ();
  g4d_data = (void *) address;
  grub_printf ("write grub4dos drive map slot info to %p\n", g4d_data);
#ifdef GRUB_MACHINE_EFI
  grub_efi_status_t status;
  grub_efi_boot_services_t *b = grub_efi_system_table->boot_services;
  status = efi_call_4 (b->allocate_pages, GRUB_EFI_ALLOCATE_ADDRESS,
                       GRUB_EFI_RESERVED_MEMORY_TYPE, 1, (void *) &address);
  if (status != GRUB_EFI_SUCCESS)
    grub_printf ("failed to allocate memory.\n");
#endif
  grub_memcpy (g4d_data + 0xE0, "   $INT13SFGRUB4DOS", 19);
  g4d_slot = (void *)g4d_data;
  grub_memset (g4d_slot, 0, DRIVE_MAP_SLOT_SIZE * DRIVE_MAP_SIZE);
}

static void
read_block_start (grub_disk_addr_t sector,
                  unsigned offset __attribute ((unused)),
                  unsigned length, void *data)
{
  grub_disk_addr_t *start = data;
  *start = sector + 1 - (length >> GRUB_DISK_SECTOR_BITS);
}

void
g4d_add_drive (grub_file_t file, int is_cdrom)
{
  unsigned i;
  grub_disk_addr_t start = 0;
  char tmp[GRUB_DISK_SECTOR_SIZE];
  grub_file_t test = 0;
  g4d_alloc_data ();
  for (i = 0; i < DRIVE_MAP_SIZE; i++)
  {
    if (g4d_slot[i].from_drive != 0)
    {
      if (i == DRIVE_MAP_SIZE)
        grub_printf ("grub4dos drive map slot full.\n");
      continue;
    }

    g4d_slot[i].from_drive = 0x80;
    if (grub_ismemfile (file->name))
      g4d_slot[i].to_drive = 0xff;
    else
      g4d_slot[i].to_drive = 0x80;
    g4d_slot[i].max_head = 0xfe;
    g4d_slot[i].from_cdrom = is_cdrom ? 1 : 0;
    g4d_slot[i].to_sector = 0x02;
    if (grub_ismemfile (file->name))
      g4d_slot[i].start_sector = ((grub_addr_t) file->data) >> GRUB_DISK_SECTOR_BITS;
    else if (file->device->disk)
    {
      test = grub_file_open (file->name, GRUB_FILE_TYPE_CAT);
      if (!test)
        break;
      test->read_hook = read_block_start;
      test->read_hook_data = &start;
      grub_file_read (test, tmp, GRUB_DISK_SECTOR_SIZE);
      grub_file_close (test);
      g4d_slot[i].start_sector = start;
    }
    g4d_slot[i].sector_count = (file->size + 511) >> GRUB_DISK_SECTOR_BITS;
    break;
  }
}

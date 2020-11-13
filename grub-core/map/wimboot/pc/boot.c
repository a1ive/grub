 /*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2019,2020  Free Software Foundation, Inc.
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

#include <grub/types.h>
#include <grub/misc.h>
#include <grub/file.h>
#include <grub/err.h>
#include <grub/script_sh.h>
#include <grub/i386/linux.h>
#include <grub/i386/relocator.h>

#include <misc.h>
#include <vfat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <xz.h>
#include <wimboot.h>
#include <bootapp.h>
#include <int13.h>
#include <peloader.h>

#pragma GCC diagnostic ignored "-Wcast-align"

#define BOOTAPP_MAX_MEM_REGION 32

/** Boot application descriptor set */
static struct win32_bootapps
{
  /** Boot application descriptor */
  struct bootapp_descriptor bootapp;
  /** Boot application memory descriptor */
  struct bootapp_memory_descriptor memory;
  /** Boot application memory descriptor regions */
  struct bootapp_memory_region regions[BOOTAPP_MAX_MEM_REGION];
  /** Boot application entry descriptor */
  struct bootapp_entry_descriptor entry;
  struct bootapp_entry_wtf1_descriptor wtf1;
  struct bootapp_entry_wtf2_descriptor wtf2;
  struct bootapp_entry_wtf3_descriptor wtf3;
  struct bootapp_entry_wtf3_descriptor wtf3_copy;
  /** Boot application callback descriptor */
  struct bootapp_callback_descriptor callback;
  /** Boot application pointless descriptor */
  struct bootapp_pointless_descriptor pointless;
} __attribute__ ((packed)) bootapps =
{
  .bootapp =
  {
    .signature = BOOTAPP_SIGNATURE,
    .version = BOOTAPP_VERSION,
    .len = sizeof (bootapps),
    .arch = BOOTAPP_ARCH_I386,
    .memory = offsetof (struct win32_bootapps, memory),
    .entry = offsetof (struct win32_bootapps, entry),
    .xxx = offsetof (struct win32_bootapps, wtf3_copy),
    .callback = offsetof (struct win32_bootapps, callback),
    .pointless = offsetof (struct win32_bootapps, pointless),
  },
  .memory =
  {
    .version = BOOTAPP_MEMORY_VERSION,
    .len = sizeof (bootapps.memory),
    .num_regions = 0,
    .region_len = sizeof (bootapps.regions[0]),
    .reserved_len = sizeof (bootapps.regions[0].reserved),
  },
  .entry =
  {
    .signature = BOOTAPP_ENTRY_SIGNATURE,
    .flags = BOOTAPP_ENTRY_FLAGS,
  },
  .wtf1 =
  {
    .flags = 0x11000001,
    .len = sizeof (bootapps.wtf1),
    .extra_len = (sizeof (bootapps.wtf2) + sizeof (bootapps.wtf3)),
  },
  .wtf3 =
  {
    .flags = 0x00000006,
    .len = sizeof (bootapps.wtf3),
    .boot_partition_offset = (VDISK_VBR_LBA * VDISK_SECTOR_SIZE),
    .xxx = 0x01,
    .mbr_signature = VDISK_MBR_SIGNATURE,
  },
  .wtf3_copy =
  {
    .flags = 0x00000006,
    .len = sizeof (bootapps.wtf3),
    .boot_partition_offset = (VDISK_VBR_LBA * VDISK_SECTOR_SIZE),
    .xxx = 0x01,
    .mbr_signature = VDISK_MBR_SIGNATURE,
  },
  .callback =
  {
    .callback = &callback,
  },
  .pointless =
  {
    .version = BOOTAPP_POINTLESS_VERSION,
  },
};

static void
add_bootapp_mem_region (void *start, void *end)
{
  uint32_t i = bootapps.memory.num_regions;
  if (i >= BOOTAPP_MAX_MEM_REGION)
  {
    grub_printf ("Boot application memory regions full.\n");
    return;
  }
  bootapps.regions[i].start_page = page_start (start);
  bootapps.regions[i].num_pages = page_len (start, end);
  bootapps.memory.num_regions++;
  grub_printf ("Add memory region %u %p - %p.\n", i, start, end);
}

void grub_wimboot_boot (struct wimboot_cmdline *cmd)
{
  void *raw_pe = NULL;
  grub_relocator_chunk_t ch;
  struct grub_relocator *rel = NULL;
  struct loaded_pe pe;
  /* Read bootmgr.exe into memory */
  rel = grub_relocator_new ();
  if (!rel)
    grub_pause_fatal ("FATAL: grub_relocator_new failed\n");
  if (grub_relocator_alloc_chunk_align (rel, &ch, 0x1000000,
                                      GRUB_LINUX_INITRD_MAX_ADDRESS,
                                      ALIGN_UP (cmd->bootmgfw->len, 4096),
                                      4096, GRUB_RELOCATOR_PREFERENCE_HIGH, 0))
  {
    grub_relocator_unload (rel);
    grub_pause_fatal ("FATAL: grub_relocator_alloc_chunk_align failed\n");
  }
  raw_pe = get_virtual_current_address (ch);
  if (!raw_pe)
    grub_pause_fatal ("FATAL: out of memory\n");
  cmd->bootmgfw->read (cmd->bootmgfw, raw_pe, 0, cmd->bootmgfw->len);
  /* Load bootmgr.exe into memory */
  if (load_pe (raw_pe, cmd->bootmgfw->len, &pe) != 0)
    grub_pause_fatal ("FATAL: Could not load bootmgr.exe\n");
  /* Complete boot application descriptor set */
  bootapps.bootapp.pe_base = pe.base;
  bootapps.bootapp.pe_len = pe.len;
  /* Memory regions */
  add_bootapp_mem_region (pe.base, (uint8_t *) pe.base + pe.len);
  add_bootapp_mem_region ((void *)0x8000, (void *)0x80000);
  add_bootapp_mem_region ((void *)0x100000, (void *)0x800000);
  /* Jump to PE image */
  printf ("Entering bootmgr.exe with parameters at %p\n", &bootapps);
  if (cmd->pause)
    grub_pause_boot ();
  pe.entry (&bootapps.bootapp);
  grub_pause_fatal ("FATAL: bootmgr.exe returned\n");
}

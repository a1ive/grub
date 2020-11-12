/* loadbios.c - command to load a bios dump  */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2009,2020  Free Software Foundation, Inc.
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

#include <grub/dl.h>
#include <grub/misc.h>
#include <grub/file.h>
#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#include <grub/device.h>
#include <grub/disk.h>
#include <grub/efi/disk.h>
#include <grub/mm.h>
#include <grub/extcmd.h>
#include <grub/i18n.h>
#include <grub/script_sh.h>

#include "bios.h"
#include "int10h.c"

GRUB_MOD_LICENSE ("GPLv3+");

static grub_err_t
grub_cmd_vgashim (grub_extcmd_context_t ctxt __attribute__ ((unused)),
                  int argc, char **args)
{
  grub_file_t file = 0;
  grub_efi_physical_address_t ivt_addr, int10h_addr;
  grub_efi_boot_services_t *b = grub_efi_system_table->boot_services;
  bios_ivt_entry_t *int10h, new_int10h;
  char *src = NULL;

  if (argc != 1)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "bad argument");
  file = grub_file_open (args[0], GRUB_FILE_TYPE_EFI_CHAINLOADED_IMAGE);
  if (!file)
    return grub_error (GRUB_ERR_BAD_OS, "file not found");
  grub_file_close (file);
  src = grub_xasprintf ("chainloader -b %s", args[0]);

  efi_call_2 (b->free_pages, BIOS_IVT_ADDR, 1);
  efi_call_4 (b->allocate_pages, GRUB_EFI_ALLOCATE_ANY_PAGES,
              GRUB_EFI_BOOT_SERVICES_CODE, 1, &ivt_addr);

  bios_switch_video_mode (1024, 768);
  if (!bios_match_cur_resolution (1024, 768))
  {
    grub_printf ("Force screen resolution to 1024x768.\n");
    bios_video_mode_hack (1024, 768);
  }

  if (bios_check_int10h ())
  {
    grub_printf ("Skip int10h handler installation.\n");
    goto exit;
  }
  if (bios_mem_lock (BIOS_VGA_ROM_ADDR, BIOS_VGA_ROM_SIZE, FALSE))
  {
    grub_printf ("Unable to unlock VGA ROM memory.\n");
    goto exit;
  }
  grub_memset ((void *)(grub_addr_t) BIOS_VGA_ROM_ADDR, 0, BIOS_VGA_ROM_SIZE);
  grub_memcpy ((void *)(grub_addr_t) BIOS_VGA_ROM_ADDR,
               INT10H_HANDLER, INT10H_HANDLER_LEN);
  if (bios_fill_vesa_info (BIOS_VGA_ROM_ADDR, &int10h_addr))
  {
    grub_printf ("Unable to fill VESA info.\n");
    goto exit;
  }
  new_int10h.segment = (grub_uint16_t) ((grub_uint32_t) BIOS_VGA_ROM_ADDR >> 4);
  new_int10h.offset = (grub_uint16_t) (int10h_addr - BIOS_VGA_ROM_ADDR);
  grub_printf ("Int10h handler address 0x%x (%04x:%04x)\n",
               (unsigned) int10h_addr, new_int10h.segment, new_int10h.offset);

  if (bios_mem_lock (BIOS_VGA_ROM_ADDR, BIOS_VGA_ROM_SIZE, TRUE))
    grub_printf ("Unable to lock VGA ROM memory.\n");

  int10h = (bios_ivt_entry_t *) BIOS_IVT_ADDR + 0x10;
  int10h->segment = new_int10h.segment;
  int10h->offset = new_int10h.offset;
  if (int10h->segment == new_int10h.segment && int10h->offset == new_int10h.offset)
    grub_printf ("Int10h IVT entry modified to point at %04x:%04x\n",
                 int10h->segment, int10h->offset);
  else
    grub_printf ("Int10h IVT entry could not be modified.\n");

  if (bios_check_int10h ())
    grub_printf ("Pre-boot Int10h sanity check success\n");
  else
    grub_printf ("Pre-boot Int10h sanity check failed\n");
exit:
  grub_script_execute_sourcecode (src);
  grub_free (src);

  grub_fatal ("ERROR");
  return GRUB_ERR_NONE;
}


static grub_err_t
grub_cmd_fakebios (grub_extcmd_context_t ctxt __attribute__ ((unused)),
                   int argc __attribute__ ((unused)),
                   char *argv[] __attribute__ ((unused)))
{
  bios_enable_rom_area ();
  bios_fake_data (1);
  bios_lock_rom_area ();

  return 0;
}

static grub_err_t
grub_cmd_loadbios (grub_extcmd_context_t ctxt __attribute__ ((unused)),
                   int argc, char *argv[])
{
  grub_file_t file;
  int size;

  if (argc == 0)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));

  if (argc > 1)
  {
    file = grub_file_open (argv[1], GRUB_FILE_TYPE_VBE_DUMP);
    if (! file)
      return grub_errno;

    if (file->size != 4)
      grub_error (GRUB_ERR_BAD_ARGUMENT, "invalid int10 dump size");
    else
      grub_file_read (file, (void *) 0x40, 4);

    grub_file_close (file);
    if (grub_errno)
      return grub_errno;
  }

  file = grub_file_open (argv[0], GRUB_FILE_TYPE_VBE_DUMP);
  if (! file)
    return grub_errno;

  size = file->size;
  if ((size < 0x10000) || (size > 0x40000))
    grub_error (GRUB_ERR_BAD_ARGUMENT, "invalid bios dump size");
  else
  {
    bios_enable_rom_area ();
    grub_file_read (file, (void *) VBIOS_ADDR, size);
    bios_fake_data (size <= 0x40000);
    bios_lock_rom_area ();
  }

  grub_file_close (file);
  return grub_errno;
}

static grub_extcmd_t cmd_fakebios, cmd_loadbios, cmd_vgashim;

GRUB_MOD_INIT(loadbios)
{
  cmd_fakebios = grub_register_extcmd ("fakebios", grub_cmd_fakebios, 0, 0,
                        N_("Create BIOS-like structures for "
                           "backward compatibility with existing OS."), 0);

  cmd_loadbios = grub_register_extcmd ("loadbios", grub_cmd_loadbios, 0,
                        N_("BIOS_DUMP [INT10_DUMP]"),
                        N_("Load BIOS dump."), 0);
  cmd_vgashim = grub_register_extcmd ("vgashim", grub_cmd_vgashim, 0,
                        N_("FILE"),
                        N_("Create fake int10h handler and load bootmgfw."), 0);
}

GRUB_MOD_FINI(loadbios)
{
  grub_unregister_extcmd (cmd_fakebios);
  grub_unregister_extcmd (cmd_loadbios);
  grub_unregister_extcmd (cmd_vgashim);
}

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

#include <grub/dl.h>
#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#include <grub/efi/disk.h>
#include <grub/err.h>
#include <grub/extcmd.h>
#include <grub/file.h>
#include <grub/i18n.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/types.h>
#include <grub/efi/efiload.h>
#include <grub/i386/efi/ramdisk.h>
#include <grub/i386/efi/ramdiskdxe.h>

GRUB_MOD_LICENSE ("GPLv3+");

static const struct grub_arg_option options_ramdisk[] = {
  {"hd", 'd', 0, N_("RAW disk format"), 0, 0},
  {"update", 'u', 0, N_("Update mappings"), 0, 0},
  {0, 0, 0, 0, 0, 0}
};

static grub_efi_guid_t ramdisk_guid = GRUB_EFI_RAMDISK_PROTOCOL_GUID;

static grub_err_t
grub_cmd_ramdisk (grub_extcmd_context_t ctxt,
        int argc,
        char **args)
{
  struct grub_arg_list *state = ctxt->state;
  grub_file_t file = 0;
  grub_efi_boot_services_t *b;
  grub_efi_ramdisk_protocol_t *ramdisk_p;
  grub_efi_status_t status;
  grub_ssize_t size;
  void *buffer = 0;
  grub_efi_device_path_protocol_t *dp;
  grub_efi_guid_t vdisk_cd_guid = GRUB_EFI_VIRTUAL_CD_GUID;
  grub_efi_guid_t vdisk_hd_guid = GRUB_EFI_VIRTUAL_HD_GUID;
  
  b = grub_efi_system_table->boot_services;
  if (argc != 1)
    goto fail;
  
  grub_efi_driver_load (RamDiskDxe_efi_len, RamDiskDxe_efi, 1);
  
  status = efi_call_3 (b->locate_protocol, &ramdisk_guid, NULL, (void **)&ramdisk_p);
  if (status == GRUB_EFI_SUCCESS)
    grub_printf ("RAMDISK_PROTOCOL installed.\n");
  else
  {
    grub_error (GRUB_ERR_BAD_OS, N_("RAMDISK_PROTOCOL not found."));
    goto fail;
  }

  file = grub_file_open (args[0], GRUB_FILE_TYPE_LOOPBACK);
  if (! file)
    goto fail;
  size = grub_file_size (file);
  if (!size)
    {
      grub_error (GRUB_ERR_BAD_OS, N_("premature end of file %s"),
		  args[0]);
      goto fail;
    }
  grub_printf ("allocate memory\n");
  status = efi_call_3 (b->allocate_pool, GRUB_EFI_RESERVED_MEMORY_TYPE,
			      size, &buffer);
  if (status != GRUB_EFI_SUCCESS)
    {
      grub_error (GRUB_ERR_OUT_OF_MEMORY, N_("out of memory"));
      goto fail;
    }

  if (grub_file_read (file, buffer, size) != size)
    {
      if (grub_errno == GRUB_ERR_NONE)
	grub_error (GRUB_ERR_BAD_OS, N_("premature end of file %s"),
		    args[0]);
      goto fail;
    }
  grub_printf ("register ramdisk\n");
  if (state[0].set)
    status = efi_call_5 (ramdisk_p->register_ramdisk,
                       ((grub_efi_uint64_t)(grub_efi_uintn_t) buffer), size, &vdisk_hd_guid,
                       NULL, &dp);
  else
    status = efi_call_5 (ramdisk_p->register_ramdisk,
                       ((grub_efi_uint64_t)(grub_efi_uintn_t) buffer), size, &vdisk_cd_guid,
                       NULL, &dp);
  
  if (status != GRUB_EFI_SUCCESS)
    {
      grub_error (GRUB_ERR_BAD_OS, "cannot create ramdisk");
      goto fail;
    }
  else
    grub_printf ("ramdisk created\n");
  grub_printf ("Device path: \n");
  grub_efi_print_device_path (dp);
  if (state[1].set)
  {
    grub_printf ("Initialize disks\n");
    grub_efidisk_fini ();
    grub_efidisk_init ();
  }
  return 0;
fail:
  grub_printf ("ERROR\n");
  if (file)
    grub_file_close (file);
  if (buffer)
    efi_call_1 (b->free_pool, buffer);
  return 1;
}

static grub_extcmd_t cmd_ramdisk;

GRUB_MOD_INIT(ramdisk)
{
  cmd_ramdisk = grub_register_extcmd ("ramdisk", grub_cmd_ramdisk, 0, 
                  N_("FILE"),
                  N_("Load RamDisk."), options_ramdisk);
}

GRUB_MOD_FINI(ramdisk)
{
  grub_unregister_extcmd (cmd_ramdisk);
}
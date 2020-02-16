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
#include <grub/efi/efi.h>
#include <grub/efi/api.h>
#include <grub/msdos_partition.h>

#include <private.h>
#include <maplib.h>

static enum disk_type
check_image (void)
{
  struct grub_msdos_partition_mbr *mbr = NULL;
  mbr = grub_zalloc (FD_BLOCK_SIZE);
  if (!mbr)
    return FD;
  file_read (cmd->disk, vdisk.file, mbr, FD_BLOCK_SIZE, 0);
  if (mbr->signature != GRUB_PC_PARTITION_SIGNATURE)
  {
    grub_free (mbr);
    return CD;
  }
  if (mbr->entries[0].type != GRUB_PC_PARTITION_TYPE_GPT_DISK)
  {
    grub_free (mbr);
    return MBR;
  }
  else
  {
    grub_free (mbr);
    return GPT;
  }
}

grub_efi_status_t
vdisk_install (grub_file_t file, grub_efi_boolean_t ro)
{
  grub_efi_status_t status;
  grub_efi_device_path_t *tmp_dp;
  enum disk_type type;
  char *text_dp = NULL;
  grub_efi_boot_services_t *b;
  b = grub_efi_system_table->boot_services;
  /* guid */
  grub_efi_guid_t dp_guid = GRUB_EFI_DEVICE_PATH_GUID;
  grub_efi_guid_t blk_io_guid = GRUB_EFI_BLOCK_IO_GUID;

  vdisk.file = file;
  vdisk.disk = cmd->disk;
  type = check_image ();
  /* block size */
  if (cmd->type == CD || type == CD)
  {
    type = CD;
    vdisk.bs = CD_BLOCK_SIZE;
  }
  else
    vdisk.bs = FD_BLOCK_SIZE;

  vdisk.size = get_size (cmd->disk, vdisk.file);
  if (cmd->mem)
  {
    status = grub_efi_allocate_pool (GRUB_EFI_BOOT_SERVICES_DATA,
                                     (grub_efi_uintn_t)(vdisk.size + 8),
                                     (void **)&vdisk.addr);
    if (status != GRUB_EFI_SUCCESS)
    {
      grub_printf ("out of memory\n");
      return GRUB_EFI_OUT_OF_RESOURCES;
    }
    file_read (cmd->disk, vdisk.file,
          (void *)vdisk.addr, (grub_efi_uintn_t)vdisk.size, 0);
  }
  else
    vdisk.addr = 0;

  tmp_dp = grub_efi_create_device_node (HARDWARE_DEVICE_PATH, HW_VENDOR_DP,
                                        sizeof(grub_efi_vendor_device_path_t));
  guidcpy (&((grub_efi_vendor_device_path_t *)tmp_dp)->vendor_guid, &VDISK_GUID);
  vdisk.dp = grub_efi_append_device_node (NULL, tmp_dp);
  if (tmp_dp)
    grub_free (tmp_dp);
  /* vdisk */
  vdisk.present = TRUE;
  vdisk.handle = NULL;
  vdisk.mem = cmd->mem;
  vdisk.type = type;
  /* block_io */
  grub_memcpy (&vdisk.block_io, &blockio_template, sizeof (block_io_protocol_t));
  /* media */
  vdisk.block_io.media = &vdisk.media;
  vdisk.media.media_id = VDISK_MEDIA_ID;
  vdisk.media.removable_media = FALSE;
  vdisk.media.media_present = TRUE;
  vdisk.media.logical_partition = FALSE;
  vdisk.media.read_only = ro;
  vdisk.media.write_caching = FALSE;
  vdisk.media.io_align = 16;
  vdisk.media.block_size = vdisk.bs;
  vdisk.media.last_block =
              grub_divmod64 (vdisk.size + vdisk.bs - 1, vdisk.bs, 0) - 1;
  /* info */
  grub_printf ("VDISK file=%s type=%d\n",
          vdisk.disk ? ((grub_disk_t)(vdisk.file))->name :
          ((grub_file_t)(vdisk.file))->name, vdisk.type);
  grub_printf ("VDISK addr=%ld size=%lld\n", (unsigned long)vdisk.addr,
          (unsigned long long)vdisk.size);
  grub_printf ("VDISK blksize=%d lastblk=%lld\n", vdisk.media.block_size,
          (unsigned long long)vdisk.media.last_block);
  text_dp = grub_efi_device_path_to_str (vdisk.dp);
  grub_printf ("VDISK DevicePath: %s\n",text_dp);
  if (text_dp)
    grub_free (text_dp);
  if (cmd->pause)
    pause ();
  /* install vpart */
  if (vdisk.type != FD)
  {
    status = vpart_install (ro);
  }
  grub_printf ("Installing block_io protocol for virtual disk ...\n");
  status = efi_call_6 (b->install_multiple_protocol_interfaces,
                          &vdisk.handle,
                          &dp_guid, vdisk.dp,
                          &blk_io_guid, &vdisk.block_io, NULL);
  if (status != GRUB_EFI_SUCCESS)
  {
    grub_printf ("failed to install virtual disk\n");
    return status;
  }
  efi_call_4 (b->connect_controller, vdisk.handle, NULL, NULL, TRUE);
  return GRUB_EFI_SUCCESS;
}

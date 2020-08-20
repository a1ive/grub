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

#include <grub/misc.h>
#include <grub/file.h>
#include <grub/efi/efi.h>
#include <grub/efi/api.h>
#include <grub/msdos_partition.h>

#include <guid.h>
#include <misc.h>

grub_err_t
grub_efivdisk_install (struct grub_efivdisk_data *disk,
                       struct grub_arg_list *state)
{
  grub_efi_status_t status;
  grub_efi_device_path_t *tmp_dp;
  grub_efi_uint32_t bs;
  grub_efi_boot_services_t *b = grub_efi_system_table->boot_services;
  /* guid */
  grub_efi_guid_t dp_guid = GRUB_EFI_DEVICE_PATH_GUID;
  grub_efi_guid_t blk_io_guid = GRUB_EFI_BLOCK_IO_GUID;

  /* block size */
  if (disk->type == CD)
    bs = CD_BLOCK_SIZE;
  else
    bs = FD_BLOCK_SIZE;

  disk->vdisk.addr = 0;
  disk->vdisk.handle = NULL;
  /* device path */
  tmp_dp = grub_efi_create_device_node (HARDWARE_DEVICE_PATH, HW_VENDOR_DP,
                                        sizeof(grub_efi_vendor_device_path_t));
  grub_guidcpy (&((grub_efi_vendor_device_path_t *)tmp_dp)->vendor_guid,
                &disk->guid);
  disk->vdisk.dp = grub_efi_append_device_node (NULL, tmp_dp);
  grub_free (tmp_dp);
  /* block_io */
  grub_memcpy (&disk->vdisk.block_io, &blockio_template,
               sizeof (block_io_protocol_t));
  /* media */
  disk->vdisk.block_io.media = &disk->vdisk.media;
  disk->vdisk.media.media_id = VDISK_MEDIA_ID;
  disk->vdisk.media.removable_media = FALSE;
  disk->vdisk.media.media_present = TRUE;
  disk->vdisk.media.logical_partition = FALSE;
  disk->vdisk.media.read_only = state[MAP_RO].set;
  disk->vdisk.media.write_caching = FALSE;
  disk->vdisk.media.io_align = 16;
  disk->vdisk.media.block_size = bs;
  disk->vdisk.media.last_block =
              grub_divmod64 (disk->vdisk.size + bs - 1, bs, 0) - 1;
  /* info */
  grub_dprintf ("map", "VDISK file=%s type=%d\n",
                disk->vdisk.file->name, disk->type);
  grub_dprintf ("map", "VDISK size=%lld\n",
                (unsigned long long)disk->vdisk.size);
  grub_dprintf ("map", "VDISK blksize=%d lastblk=%lld\n",
                disk->vdisk.media.block_size,
                (unsigned long long)disk->vdisk.media.last_block);
  grub_efi_dprintf_dp (disk->vdisk.dp);
  /* install vpart */
  if (disk->type != FD)
    grub_efivpart_install (disk, state);
  status = efi_call_6 (b->install_multiple_protocol_interfaces,
                       &disk->vdisk.handle, &dp_guid, disk->vdisk.dp,
                       &blk_io_guid, &disk->vdisk.block_io, NULL);

  if (status != GRUB_EFI_SUCCESS)
    return grub_error (GRUB_ERR_BAD_OS, "Failed to install virtual disk.");

  efi_call_4 (b->connect_controller, disk->vdisk.handle, NULL, NULL, TRUE);
  return GRUB_ERR_NONE;
}

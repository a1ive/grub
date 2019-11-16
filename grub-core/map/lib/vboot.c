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
#include <private.h>
#include <maplib.h>

grub_efi_handle_t
vpart_boot (grub_efi_handle_t *part_handle)
{
  grub_efi_status_t status;
  grub_efi_handle_t boot_image_handle;
  char *text_dp = NULL;
  grub_efi_device_path_t *boot_file = NULL;
  grub_efi_boot_services_t *b;
  b = grub_efi_system_table->boot_services;

  if (!part_handle)
  return NULL;

  boot_file = grub_efi_file_device_path (grub_efi_get_device_path (part_handle),
                                         EFI_REMOVABLE_MEDIA_FILE_NAME);
  text_dp = grub_efi_device_path_to_str (boot_file);
  grub_printf ("LoadImage: %s\n", text_dp);
  if (text_dp)
    grub_free (text_dp);

  status = efi_call_6 (b->load_image, TRUE, grub_efi_image_handle,
                       boot_file, NULL, 0, (void **)&boot_image_handle);
  if (boot_file)
    grub_free (boot_file);
  if (status != GRUB_EFI_SUCCESS)
  {
    grub_printf ("Failed to load image\n");
    return NULL;
  }
  return boot_image_handle;
}

grub_efi_handle_t
vdisk_boot (void)
{
  grub_efi_status_t status;
  grub_efi_uintn_t count = 0;
  grub_efi_uintn_t i = 0;
  grub_efi_handle_t *buf = NULL;
  grub_efi_device_path_t *tmp_dp;
  grub_efi_device_path_t *boot_file = NULL;
  grub_efi_handle_t boot_image_handle = NULL;
  grub_efi_guid_t sfs_protocol_guid = GRUB_EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
  char *text_dp = NULL;
  grub_efi_boot_services_t *b;
  b = grub_efi_system_table->boot_services;

  status = efi_call_5 (b->locate_handle_buffer, GRUB_EFI_BY_PROTOCOL,
                              &sfs_protocol_guid, NULL, &count, &buf);
  if(status != GRUB_EFI_SUCCESS)
  {
    grub_printf ("SimpleFileSystemProtocol not found.\n");
    return NULL;
  }
  for (i = 0; i < count; i++)
  {
    tmp_dp = grub_efi_get_device_path (buf[i]);
    text_dp = grub_efi_device_path_to_str (tmp_dp);
    grub_printf ("Checking for %s\n",text_dp);
    if (text_dp)
      grub_free (text_dp);
    if (((grub_efi_vendor_device_path_t *)tmp_dp)->header.type
            != HARDWARE_DEVICE_PATH ||
        ((grub_efi_vendor_device_path_t *)tmp_dp)->header.subtype
            != HW_VENDOR_DP ||
        !guidcmp (&((grub_efi_vendor_device_path_t*)tmp_dp)->vendor_guid,
                  &VDISK_GUID))
      continue;

    boot_file = grub_efi_file_device_path (grub_efi_get_device_path (buf[i]),
                                           EFI_REMOVABLE_MEDIA_FILE_NAME);
    text_dp = grub_efi_device_path_to_str (boot_file);
    grub_printf ("LoadImage: %s\n", text_dp);
    if (text_dp)
      grub_free (text_dp);
    status = efi_call_6 (b->load_image, TRUE, grub_efi_image_handle,
                         boot_file, NULL, 0, (void **)&boot_image_handle);
    if (status != GRUB_EFI_SUCCESS)
    {
      if (boot_file)
        grub_free (boot_file);
      continue;
    }
    break;
  }
  if (!boot_image_handle)
  {
    grub_printf ("boot_image_handle not found\n");
    return NULL;
  }

  return boot_image_handle;
}

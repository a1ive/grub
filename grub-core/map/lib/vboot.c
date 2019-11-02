/*
 *  UEFI example
 *  Copyright (C) 2019  a1ive
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <efi.h>
#include <efilib.h>
#include <grub.h>
#include <sstdlib.h>
#include <private.h>

EFI_HANDLE
vpart_boot (EFI_HANDLE *part_handle)
{
  EFI_STATUS status;
  EFI_HANDLE boot_image_handle;
  CHAR16 *text_dp = NULL;
  EFI_DEVICE_PATH *boot_file;

  if (!part_handle)
  return NULL;

  boot_file = FileDevicePath (part_handle, EFI_REMOVABLE_MEDIA_FILE_NAME);

  status = uefi_call_wrapper (gBS->LoadImage, 6, TRUE, efi_image_handle,
                boot_file, NULL, 0, (VOID **)&boot_image_handle);
  if (status != EFI_SUCCESS)
  {
  printf ("failed to load image\n");
  return NULL;
  }
  text_dp = DevicePathToStr (boot_file);
  printf ("DevicePath: %ls\n", text_dp);
  if (text_dp)
  FreePool (text_dp);
  return boot_image_handle;
}

EFI_HANDLE
vdisk_boot (void)
{
  EFI_STATUS status;
  UINTN count = 0;
  UINTN i = 0;
  EFI_HANDLE *buf = NULL;
  EFI_DEVICE_PATH *tmp_dp;
  EFI_DEVICE_PATH *boot_file = NULL;
  EFI_HANDLE boot_image_handle = NULL;
  CHAR16 *text_dp = NULL;
  EFI_GUID vdisk_guid = VDISK_GUID;

  status = uefi_call_wrapper (gBS->LocateHandleBuffer, 5, ByProtocol,
                              &gEfiSimpleFileSystemProtocolGuid, NULL, &count, &buf);
  if(status != EFI_SUCCESS)
  {
    printf ("SimpleFileSystemProtocol not found.\n");
    return NULL;
  }
  printf ("Handles found %ld\n",count);
  for (i = 0; i < count; i++)
  {
    tmp_dp = DevicePathFromHandle (buf[i]);

    if (((VENDOR_DEVICE_PATH*)tmp_dp)->Header.Type != HARDWARE_DEVICE_PATH ||
        ((VENDOR_DEVICE_PATH*)tmp_dp)->Header.SubType != HW_VENDOR_DP ||
        !CompareGuid (&((VENDOR_DEVICE_PATH*)tmp_dp)->Guid, &vdisk_guid))
      continue;
    boot_file = FileDevicePath (buf[i], EFI_REMOVABLE_MEDIA_FILE_NAME);

    status = uefi_call_wrapper (gBS->LoadImage, 6, TRUE, efi_image_handle,
                                boot_file, NULL, 0, (VOID**)&boot_image_handle);
    if (status != EFI_SUCCESS)
      continue;
    break;
  }
  if (!boot_image_handle)
  {
    printf ("handle not found\n");
    return NULL;
  }
  printf ("handle: %ld\n",i);
  text_dp = DevicePathToStr (boot_file);
  printf ("DevicePath: %ls\n",text_dp);
  if (text_dp)
    FreePool (text_dp);
  return boot_image_handle;
}

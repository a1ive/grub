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

static BOOLEAN
guidcmp (EFI_GUID g1, EFI_GUID g2)
{
  int i;
  if (g1.Data1 != g2.Data1 || g1.Data2 != g2.Data2 || g1.Data3 != g2.Data3)
    return FALSE;
  for (i = 0; i < 8; i++)
  {
    if (g1.Data4[i] != g2.Data4[i])
      return FALSE;
  }
  return TRUE;
}

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
  text_dp = DevicePathToStr (boot_file);
  printf ("DevicePath: %ls\n", text_dp);
  if (text_dp)
    FreePool (text_dp);

  status = uefi_call_wrapper (gBS->LoadImage, 6, TRUE, efi_image_handle,
                boot_file, NULL, 0, (VOID **)&boot_image_handle);
  if (status != EFI_SUCCESS)
  {
    printf ("failed to load image\n");
    return NULL;
  }
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

  status = uefi_call_wrapper (gBS->LocateHandleBuffer, 5, ByProtocol,
                              &gEfiSimpleFileSystemProtocolGuid, NULL, &count, &buf);
  if(status != EFI_SUCCESS)
  {
    printf ("SimpleFileSystemProtocol not found.\n");
    return NULL;
  }
  printf ("handle count: %zd\n", count);
  for (i = 0; i < count; i++)
  {
    tmp_dp = DevicePathFromHandle (buf[i]);
    text_dp = DevicePathToStr (tmp_dp);
    printf ("DevicePath: %ls\n",text_dp);
    if (text_dp)
      FreePool (text_dp);
    if (((VENDOR_DEVICE_PATH*)tmp_dp)->Header.Type != HARDWARE_DEVICE_PATH ||
        ((VENDOR_DEVICE_PATH*)tmp_dp)->Header.SubType != HW_VENDOR_DP ||
        !guidcmp (((VENDOR_DEVICE_PATH*)tmp_dp)->Guid, VDISK_GUID))
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
  printf ("handle: %zd\n", i);
  text_dp = DevicePathToStr (boot_file);
  printf ("DevicePath: %ls\n",text_dp);
  if (text_dp)
    FreePool (text_dp);
  return boot_image_handle;
}

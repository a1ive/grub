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

static enum disk_type
check_image (void)
{
  MASTER_BOOT_RECORD *mbr = NULL;
  mbr = AllocateZeroPool (FD_BLOCK_SIZE);
  if (!mbr)
    return FD;
  uefi_call_wrapper (grub->file_seek, 2, &vdisk.file, 0);
  uefi_call_wrapper (grub->file_read, 2, &vdisk.file, mbr, FD_BLOCK_SIZE);
  if (mbr->Signature != MBR_SIGNATURE)
  {
    FreePool (mbr);
    return CD;
  }
  if (mbr->Partition[0].OSIndicator != PMBR_GPT_PARTITION)
  {
    FreePool (mbr);
    return MBR;
  }
  else
  {
    FreePool (mbr);
    return GPT;
  }
}

EFI_STATUS
vdisk_install (grub_file_t file)
{
  EFI_STATUS status;
  EFI_DEVICE_PATH *tmp_dp;
  UINT64 size;
  enum disk_type type;
  CHAR16 *text_dp = NULL;

  vdisk.file = file;
  type = check_image ();
  /* block size */
  if (cmd->type == CD || type == CD)
  {
    type = CD;
    vdisk.bs = CD_BLOCK_SIZE;
  }
  else
    vdisk.bs = FD_BLOCK_SIZE;

  vdisk.size = uefi_call_wrapper (grub->file_size, 1, vdisk.file);
  uefi_call_wrapper (grub->env_set, 2, "enable_progress_indicator", "1");
  if (cmd->mem)
  {
    vdisk.addr = (UINTN)AllocatePool ((UINTN)(vdisk.size + 8));
    if (!vdisk.addr)
    {
      printf ("out of memory\n");
      return EFI_NOT_FOUND;
    }
    printf ("Loading, please wait ...\n");
    uefi_call_wrapper (grub->file_seek, 2, &vdisk.file, 0);
    size = uefi_call_wrapper (grub->file_read, 3,
                              &vdisk.file, (VOID *)vdisk.addr, (UINTN)vdisk.size);
    if (size != vdisk.size)
    {
      printf ("failed to read\n");
      FreePool (&vdisk.addr);
      return EFI_NOT_FOUND;
    }
  }
  else
    vdisk.addr = 0;

  tmp_dp = CreateDeviceNode (HARDWARE_DEVICE_PATH, HW_VENDOR_DP,
                             sizeof(VENDOR_DEVICE_PATH));
  ((VENDOR_DEVICE_PATH *)tmp_dp)->Guid = VDISK_GUID;
  vdisk.dp = AppendDevicePathNode (NULL, tmp_dp);
  if (tmp_dp)
    FreePool (tmp_dp);
  /* vdisk */
  vdisk.present = TRUE;
  vdisk.handle = NULL;
  vdisk.mem = cmd->mem;
  vdisk.type = type;
  /* block_io */
  CopyMem (&vdisk.block_io, &blockio_template, sizeof (EFI_BLOCK_IO_PROTOCOL));
  /* media */
  vdisk.block_io.Media = &vdisk.media;
  vdisk.media.MediaId = VDISK_MEDIA_ID;
  vdisk.media.RemovableMedia = FALSE;
  vdisk.media.MediaPresent = TRUE;
  vdisk.media.LogicalPartition = FALSE;
  vdisk.media.ReadOnly = TRUE;
  vdisk.media.WriteCaching = FALSE;
  vdisk.media.IoAlign = 16;
  vdisk.media.BlockSize = vdisk.bs;
  vdisk.media.LastBlock = DivU64x32 (vdisk.size + vdisk.bs - 1, vdisk.bs, 0) - 1;
  /* info */
  printf ("VDISK file=%s type=%d\n", vdisk.file->name, vdisk.type);
  printf ("VDISK addr=%ld size=%ld bs=%d\n", vdisk.addr, vdisk.size,
          vdisk.media.BlockSize);
  printf ("VDISK blks=%ld\n", vdisk.media.LastBlock);
  text_dp = DevicePathToStr (vdisk.dp);
  printf ("VDISK DevicePath: %ls\n",text_dp);
  if (text_dp)
    FreePool (text_dp);
  /* install vpart */
  if (vdisk.type != FD)
  {
    status = vpart_install ();
    if (status != EFI_SUCCESS)
      printf ("failed to install virtual partition\n");
  }
  printf ("installing block_io protocol for virtual disk ...\n");
  status = uefi_call_wrapper (gBS->InstallMultipleProtocolInterfaces,
                              6, &vdisk.handle,
                              &gEfiDevicePathProtocolGuid, vdisk.dp,
                              &gEfiBlockIoProtocolGuid, &vdisk.block_io, NULL);
  if (status != EFI_SUCCESS)
  {
    printf ("failed to install virtual disk\n");
    return status;
  }
  uefi_call_wrapper (gBS->ConnectController, 4, vdisk.handle, NULL, NULL, TRUE);
  return EFI_SUCCESS;
}

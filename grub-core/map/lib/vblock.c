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

EFI_BLOCK_IO_PROTOCOL blockio_template =
{
  EFI_BLOCK_IO_PROTOCOL_REVISION,
  (EFI_BLOCK_IO_MEDIA *) 0,
  blockio_reset,
  blockio_read,
  blockio_write,
  blockio_flush
};

EFI_STATUS EFIAPI
blockio_reset (EFI_BLOCK_IO_PROTOCOL *this __unused, BOOLEAN extended __unused)
{
  return EFI_SUCCESS;
}

EFI_STATUS EFIAPI
blockio_read (EFI_BLOCK_IO_PROTOCOL *this, UINT32 media_id,
              EFI_LBA lba, UINTN len, VOID *buf)
{
  vdisk_t *data;
  UINTN block_num;

  if (!buf)
    return EFI_INVALID_PARAMETER;

  if (!len)
    return EFI_SUCCESS;

  data = VDISK_BLOCKIO_TO_PARENT(this);

  if (media_id != data->media.MediaId)
    return EFI_MEDIA_CHANGED;

  if ((len % data->media.BlockSize) != 0)
    return EFI_BAD_BUFFER_SIZE;

  if (lba > data->media.LastBlock)
    return EFI_INVALID_PARAMETER;

  block_num = len / data->media.BlockSize;

  if ((lba + block_num - 1) > data->media.LastBlock)
    return EFI_INVALID_PARAMETER;

  if(data->mem)
  {
    CopyMem (buf, (VOID *)(UINTN)
             (data->addr + MultU64x32 (lba, data->media.BlockSize)), len);
  }
  else
  {
    read (data->disk, &data->file, buf, len,
          (UINT64)data->addr + MultU64x32 (lba, data->media.BlockSize));
  }
  return EFI_SUCCESS;
}

EFI_STATUS EFIAPI
blockio_write (EFI_BLOCK_IO_PROTOCOL *this __unused, UINT32 media_id __unused,
              EFI_LBA lba __unused, UINTN len __unused, VOID *buf __unused)
{
  return EFI_WRITE_PROTECTED;
}

EFI_STATUS EFIAPI
blockio_flush (EFI_BLOCK_IO_PROTOCOL *this __unused)
{
  return EFI_SUCCESS;
}


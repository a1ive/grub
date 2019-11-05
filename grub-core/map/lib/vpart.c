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
#include <efigpt.h>

static BOOLEAN
get_mbr_info (void)
{
  MASTER_BOOT_RECORD *mbr = NULL;
  UINT32 unique_mbr_signature;
  UINTN  part_addr;
  UINTN  part_size;
  EFI_DEVICE_PATH *tmp_dp;
  UINT32 i;
  UINT32 part_num = 0;

  mbr = AllocateZeroPool (FD_BLOCK_SIZE);
  if (!mbr)
    return FALSE;

  read (cmd->disk, &vdisk.file, mbr, FD_BLOCK_SIZE, 0);

  for(i = 0; i < 4; i++)
  {
    if(mbr->Partition[i].BootIndicator == 0x80)
    {
      part_addr = *(UINT32*)(mbr->Partition[i].StartingLBA);
      vpart.addr = part_addr * FD_BLOCK_SIZE;
      part_size = *(UINT32*)(mbr->Partition[i].SizeInLBA);
      vpart.size = part_size * FD_BLOCK_SIZE;
      part_num = i + 1;
      break;
    }
  }
  if(!part_num)
  {
    FreePool (mbr);
    return FALSE;
  }
  unique_mbr_signature = *(UINT32*)(mbr->UniqueMbrSignature);

  vpart.addr = vpart.addr + vdisk.addr;

  tmp_dp = create_device_node (MEDIA_DEVICE_PATH, MEDIA_HARDDRIVE_DP,
                             sizeof (HARDDRIVE_DEVICE_PATH));
  ((HARDDRIVE_DEVICE_PATH*)tmp_dp)->PartitionNumber = part_num;
  ((HARDDRIVE_DEVICE_PATH*)tmp_dp)->PartitionStart  = part_addr;
  ((HARDDRIVE_DEVICE_PATH*)tmp_dp)->PartitionSize   = part_size;
  *(UINT32*)((HARDDRIVE_DEVICE_PATH*)tmp_dp)->Signature = unique_mbr_signature;
  ((HARDDRIVE_DEVICE_PATH*)tmp_dp)->MBRType = 1;
  ((HARDDRIVE_DEVICE_PATH*)tmp_dp)->SignatureType = 1;
  vpart.dp = AppendDevicePathNode (vdisk.dp, tmp_dp);
  FreePool (tmp_dp);
  FreePool (mbr);
  return TRUE;
}

static BOOLEAN
get_gpt_info (void)
{
  EFI_PARTITION_TABLE_HEADER *gpt = NULL;
  EFI_PARTITION_ENTRY *gpt_entry;
  UINTN gpt_entry_size;
  UINT64 gpt_entry_pos;
  UINT64 part_addr;
  UINT64 part_size;
  EFI_GUID gpt_part_signature;
  UINT32 part_num = 0;
  EFI_DEVICE_PATH *tmp_dp;
  UINT32 i;

  gpt = AllocateZeroPool (FD_BLOCK_SIZE);
  if(!gpt)
    return FALSE;

  read (cmd->disk, &vdisk.file, gpt, FD_BLOCK_SIZE,
        PRIMARY_PART_HEADER_LBA * FD_BLOCK_SIZE);

  if (gpt->Header.Signature != _EFI_PTAB_HEADER_ID)
  {
    FreePool (gpt);
    return FALSE; 
  }

  gpt_entry_pos = gpt->PartitionEntryLBA * FD_BLOCK_SIZE;
  gpt_entry_size = gpt->SizeOfPartitionEntry;  
  gpt_entry = AllocateZeroPool (gpt->SizeOfPartitionEntry *
                                gpt->NumberOfPartitionEntries);
  if(!gpt_entry)
    return FALSE;

  for (i=0; i < gpt->NumberOfPartitionEntries; i++)
  {
    read (cmd->disk, &vdisk.file, gpt_entry, gpt_entry_size,
          gpt_entry_pos + i * gpt_entry_size);
    if (CompareGuid (&gpt_entry->PartitionTypeGUID, &EfiPartTypeSystemPartitionGuid))
    {
      part_addr = gpt_entry->StartingLBA * FD_BLOCK_SIZE;
      part_size = (gpt_entry->EndingLBA - gpt_entry->StartingLBA) * FD_BLOCK_SIZE;
      guidcpy (&gpt_part_signature, &gpt_entry->UniquePartitionGUID); 
      part_num = i + 1;
      break;
    }
  }
  if(part_num==0)
  {
    FreePool (gpt_entry);
    FreePool (gpt);
    return FALSE;
  }
  vpart.addr = (UINTN) part_addr + vdisk.addr;
  vpart.size = part_size;

  tmp_dp = create_device_node (MEDIA_DEVICE_PATH, MEDIA_HARDDRIVE_DP,
                             sizeof (HARDDRIVE_DEVICE_PATH));
  ((HARDDRIVE_DEVICE_PATH*)tmp_dp)->PartitionNumber = part_num;
  ((HARDDRIVE_DEVICE_PATH*)tmp_dp)->PartitionStart = part_addr;
  ((HARDDRIVE_DEVICE_PATH*)tmp_dp)->PartitionSize = part_size;
  guidcpy ((EFI_GUID *)&(((HARDDRIVE_DEVICE_PATH*)tmp_dp)->Signature[0]),
            &gpt_part_signature);
  ((HARDDRIVE_DEVICE_PATH*)tmp_dp)->MBRType = 2;
  ((HARDDRIVE_DEVICE_PATH*)tmp_dp)->SignatureType = 2;
  vpart.dp = AppendDevicePathNode (vdisk.dp, tmp_dp);
  FreePool (tmp_dp);
  FreePool (gpt_entry);
  FreePool (gpt);
  return TRUE;
}

static BOOLEAN
get_iso_info (void)
{
  CDROM_VOLUME_DESCRIPTOR *vol = NULL;
  ELTORITO_CATALOG *catalog=NULL;
  UINTN dbr_img_size = sizeof (UINT16);
  UINT16 dbr_img_buf;
  BOOLEAN boot_entry = FALSE;
  UINTN i;
  EFI_DEVICE_PATH_PROTOCOL *tmp_dp;

  vol = AllocatePool (CD_BLOCK_SIZE);
  if (!vol)
    return FALSE;

  read (cmd->disk, &vdisk.file, vol, CD_BLOCK_SIZE, CD_BOOT_SECTOR * CD_BLOCK_SIZE);

  if (vol->Unknown.Type != CDVOL_TYPE_STANDARD ||
      CompareMem (vol->BootRecordVolume.SystemId, CDVOL_ELTORITO_ID,
                  sizeof (CDVOL_ELTORITO_ID) - 1) != 0)
  {
    FreePool (vol);
    return FALSE;
  }

  catalog = (ELTORITO_CATALOG*) vol;
  read (cmd->disk, &vdisk.file, catalog, CD_BLOCK_SIZE,
        *((UINT32*) vol->BootRecordVolume.EltCatalog) * CD_BLOCK_SIZE);
  if (catalog[0].Catalog.Indicator != ELTORITO_ID_CATALOG)
  {
    FreePool (vol);
    return FALSE;
  }

  for (i = 0; i < 64; i++)
  {
    if (catalog[i].Section.Indicator == ELTORITO_ID_SECTION_HEADER_FINAL &&
        catalog[i].Section.PlatformId == EFI_PARTITION &&
        catalog[i+1].Boot.Indicator == ELTORITO_ID_SECTION_BOOTABLE)
    {
      boot_entry = TRUE;
      vpart.addr = catalog[i+1].Boot.Lba * CD_BLOCK_SIZE;
      vpart.size = catalog[i+1].Boot.SectorCount * FD_BLOCK_SIZE;

      read (cmd->disk, &vdisk.file, &dbr_img_buf, dbr_img_size, vpart.addr + 0x13);
      dbr_img_size = dbr_img_buf * FD_BLOCK_SIZE;
      vpart.size = vpart.size > dbr_img_size ? vpart.size : dbr_img_size;

      if (vpart.size < BLOCK_OF_1_44MB * FD_BLOCK_SIZE)
      {
        vpart.size = BLOCK_OF_1_44MB * FD_BLOCK_SIZE;
      }
      break;
    }
  }
  if (!boot_entry)
  {
    FreePool (vol);
    return FALSE;
  }
  vpart.addr = vpart.addr + vdisk.addr;

  tmp_dp = create_device_node (MEDIA_DEVICE_PATH, MEDIA_CDROM_DP,
                             sizeof (CDROM_DEVICE_PATH));
  ((CDROM_DEVICE_PATH*)tmp_dp)->BootEntry = 1;
  ((CDROM_DEVICE_PATH*)tmp_dp)->PartitionStart = (vpart.addr - vdisk.addr) /
                                                 CD_BLOCK_SIZE;
  ((CDROM_DEVICE_PATH*)tmp_dp)->PartitionSize = vpart.size / CD_BLOCK_SIZE;
  vpart.dp = AppendDevicePathNode (vdisk.dp, tmp_dp);
  FreePool (tmp_dp);
  FreePool (vol);
  return TRUE;
}

EFI_STATUS
vpart_install (void)
{
  EFI_STATUS status;
  CHAR16 * text_dp = NULL;

  switch (vdisk.type)
  {
    case CD:
      vpart.present = get_iso_info ();
      printf ("Found ISO Partition\n");
      break;
    case MBR:
      vpart.present = get_mbr_info ();
      printf ("Found MBR Partition\n");
      break;
    case GPT:
      vpart.present = get_gpt_info ();
      printf ("Found GPT Partition\n");
      break;
    default:
      break;
  }

  if (!vpart.present)
    return EFI_NOT_FOUND;

  vpart.handle = NULL;
  vpart.file = vdisk.file;
  vpart.disk = vdisk.disk;
  vpart.mem = vdisk.mem;
  vpart.type = vdisk.type;

  CopyMem (&vpart.block_io, &blockio_template, sizeof (EFI_BLOCK_IO_PROTOCOL));

  vpart.block_io.Media = &vpart.media;
  vpart.media.MediaId = VDISK_MEDIA_ID;
  vpart.media.RemovableMedia = FALSE;
  vpart.media.MediaPresent = TRUE;
  vpart.media.LogicalPartition = TRUE;
  vpart.media.ReadOnly = TRUE;
  vpart.media.WriteCaching = FALSE;
  vpart.media.IoAlign = 16;
  vpart.media.BlockSize = vdisk.bs;
  vpart.media.LastBlock = DivU64x32 (vpart.size + vdisk.bs - 1, vdisk.bs, 0) - 1;
  /* info */
  text_dp = DevicePathToStr (vpart.dp);
  printf ("VPART DevicePath: %ls\n",text_dp);
  if (text_dp)
    FreePool (text_dp);
  printf ("installing block_io protocol for virtual partition ...\n");
  status = uefi_call_wrapper (gBS->InstallMultipleProtocolInterfaces,
                              6, &vpart.handle,
                              &gEfiDevicePathProtocolGuid, vpart.dp,
                              &gEfiBlockIoProtocolGuid, &vpart.block_io, NULL);
  if(status != EFI_SUCCESS)
  {
    printf ("failed to install virtual partition\n");
    return EFI_NOT_FOUND;
  }
  uefi_call_wrapper (gBS->ConnectController, 4, vpart.handle, NULL, NULL, TRUE);

  if(vdisk.type != CD)
    return EFI_SUCCESS;

  /* for DUET UEFI firmware */
  {
    EFI_HANDLE fat_handle = NULL;
    EFI_HANDLE *buf;
    UINTN count;
    UINTN i;
    EFI_COMPONENT_NAME2_PROTOCOL *cn2_protocol;
    CHAR16 *driver_name;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *sfs_protocol = NULL;

    status = uefi_call_wrapper (gBS->HandleProtocol, 3, vpart.handle,
                                &gEfiSimpleFileSystemProtocolGuid,
                                (VOID**)&sfs_protocol);
    if(status == EFI_SUCCESS)
      return EFI_SUCCESS;

    status = uefi_call_wrapper (gBS->LocateHandleBuffer, 5, ByProtocol,
                                &gEfiComponentName2ProtocolGuid, NULL,
                                &count, &buf);
    if(status != EFI_SUCCESS)
    {
      printf ("ComponentNameProtocol not found.\n");
    }
    for (i = 0; i < count; i++)
    {
      uefi_call_wrapper (gBS->HandleProtocol, 3, buf[i],
                         &gEfiComponentName2ProtocolGuid, (VOID**)&cn2_protocol);
      uefi_call_wrapper (cn2_protocol->GetDriverName, 3,
                         cn2_protocol, (CHAR8 *)"en-us", &driver_name);
      if(driver_name && wstrstr (driver_name, L"FAT File System Driver"))
      {
        fat_handle = buf[i];
        break;
      }
    }
    if (fat_handle)
    {
      status = uefi_call_wrapper (gBS->ConnectController, 4,
                                  vpart.handle, &fat_handle, NULL, TRUE);
      return EFI_SUCCESS;
    }
    else
    {
      printf ("FAT Driver not found.\n");
      return EFI_NOT_FOUND;
    }
  }
}

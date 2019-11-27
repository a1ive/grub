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
#include <grub/efi/sfs.h>
#include <grub/eltorito.h>
#include <grub/msdos_partition.h>
#include <grub/gpt_partition.h>

#include <private.h>
#include <maplib.h>

#define EFI_PARTITION   0xef

static grub_efi_boolean_t
get_mbr_info (void)
{
  struct grub_msdos_partition_mbr *mbr = NULL;
  grub_efi_uint32_t unique_mbr_signature;
  grub_efi_uintn_t  part_addr;
  grub_efi_uintn_t  part_size;
  grub_efi_device_path_t *tmp_dp;
  grub_efi_uint32_t i;
  grub_efi_uint32_t part_num = 0;

  mbr = grub_zalloc (FD_BLOCK_SIZE);
  if (!mbr)
    return FALSE;

  file_read (cmd->disk, vdisk.file, mbr, FD_BLOCK_SIZE, 0);

  for(i = 0; i < 4; i++)
  {
    if(mbr->entries[i].flag == 0x80)
    {
      part_addr = mbr->entries[i].start;
      vpart.addr = part_addr << FD_SHIFT;
      part_size = mbr->entries[i].length;
      vpart.size = part_size << FD_SHIFT;
      part_num = i + 1;
      break;
    }
  }
  if(!part_num)
  {
    grub_free (mbr);
    return FALSE;
  }
  unique_mbr_signature = *(grub_efi_uint32_t*)(mbr->unique_signature);

  vpart.addr = vpart.addr + vdisk.addr;

  tmp_dp = grub_efi_create_device_node (MEDIA_DEVICE_PATH, MEDIA_HARDDRIVE_DP,
                                sizeof (grub_efi_hard_drive_device_path_t));
  ((grub_efi_hard_drive_device_path_t*)tmp_dp)->partition_number = part_num;
  ((grub_efi_hard_drive_device_path_t*)tmp_dp)->partition_start  = part_addr;
  ((grub_efi_hard_drive_device_path_t*)tmp_dp)->partition_size   = part_size;
  *(grub_efi_uint32_t*)((grub_efi_hard_drive_device_path_t*)tmp_dp)
        ->partition_signature = unique_mbr_signature;
  ((grub_efi_hard_drive_device_path_t*)tmp_dp)->partmap_type = 1;
  ((grub_efi_hard_drive_device_path_t*)tmp_dp)->signature_type = 1;
  vpart.dp = grub_efi_append_device_node (vdisk.dp, tmp_dp);
  grub_free (tmp_dp);
  grub_free (mbr);
  return TRUE;
}

static grub_efi_boolean_t
get_gpt_info (void)
{
  struct grub_gpt_header *gpt = NULL;
  struct grub_gpt_partentry *gpt_entry;
  grub_efi_uintn_t gpt_entry_size;
  grub_efi_uint64_t gpt_entry_pos;
  grub_efi_uint64_t part_addr;
  grub_efi_uint64_t part_size;
  grub_gpt_part_guid_t gpt_part_signature;
  grub_efi_uint32_t part_num = 0;
  grub_efi_device_path_t *tmp_dp;
  grub_efi_uint32_t i;

  /* "EFI PART" */
  grub_uint8_t GPT_HDR_MAGIC[8] = GRUB_GPT_HEADER_MAGIC;
  grub_packed_guid_t GPT_EFI_SYSTEM_PART_GUID = GRUB_GPT_PARTITION_TYPE_EFI_SYSTEM;
  gpt = grub_zalloc (FD_BLOCK_SIZE);
  if(!gpt)
    return FALSE;

  file_read (cmd->disk, vdisk.file, gpt, FD_BLOCK_SIZE,
             PRIMARY_PART_HEADER_LBA * FD_BLOCK_SIZE);

  for (i = 0; i < 8; i++)
  {
    if (gpt->magic[i] != GPT_HDR_MAGIC[i])
    {
      grub_free (gpt);
      return FALSE;
    }
  }

  gpt_entry_pos = gpt->partitions << FD_SHIFT;
  gpt_entry_size = gpt->partentry_size;
  gpt_entry = grub_zalloc (gpt->partentry_size * gpt->maxpart);
  if(!gpt_entry)
    return FALSE;

  for (i = 0; i < gpt->maxpart; i++)
  {
    file_read (cmd->disk, vdisk.file, gpt_entry, gpt_entry_size,
               gpt_entry_pos + i * gpt_entry_size);
    if (guidcmp (&gpt_entry->type,
                 &GPT_EFI_SYSTEM_PART_GUID))
    {
      part_addr = gpt_entry->start << FD_SHIFT;
      part_size = (gpt_entry->end - gpt_entry->start) << FD_SHIFT;
      guidcpy (&gpt_part_signature, &gpt_entry->guid); 
      part_num = i + 1;
      break;
    }
  }
  if(!part_num)
  {
    grub_free (gpt_entry);
    grub_free (gpt);
    return FALSE;
  }
  vpart.addr = (grub_efi_uintn_t) part_addr + vdisk.addr;
  vpart.size = part_size;

  tmp_dp = grub_efi_create_device_node (MEDIA_DEVICE_PATH, MEDIA_HARDDRIVE_DP,
                                sizeof (grub_efi_hard_drive_device_path_t));
  ((grub_efi_hard_drive_device_path_t*)tmp_dp)->partition_number = part_num;
  ((grub_efi_hard_drive_device_path_t*)tmp_dp)->partition_start = part_addr;
  ((grub_efi_hard_drive_device_path_t*)tmp_dp)->partition_size = part_size;
  guidcpy ((grub_packed_guid_t *)
           &(((grub_efi_hard_drive_device_path_t*)tmp_dp)->partition_signature[0]),
           &gpt_part_signature);
  ((grub_efi_hard_drive_device_path_t*)tmp_dp)->partmap_type = 2;
  ((grub_efi_hard_drive_device_path_t*)tmp_dp)->signature_type = 2;
  vpart.dp = grub_efi_append_device_node (vdisk.dp, tmp_dp);
  grub_free (tmp_dp);
  grub_free (gpt_entry);
  grub_free (gpt);
  return TRUE;
}

static grub_efi_boolean_t
get_iso_info (void)
{
  cdrom_volume_descriptor_t *vol = NULL;
  eltorito_catalog_t *catalog = NULL;
  grub_efi_uintn_t dbr_img_size = sizeof (grub_efi_uint16_t);
  grub_efi_uint16_t dbr_img_buf;
  grub_efi_boolean_t boot_entry = FALSE;
  grub_efi_uintn_t i;
  grub_efi_device_path_t *tmp_dp;

  vol = grub_zalloc (CD_BLOCK_SIZE);
  if (!vol)
    return FALSE;

  file_read (cmd->disk, vdisk.file,
             vol, CD_BLOCK_SIZE, CD_BOOT_SECTOR * CD_BLOCK_SIZE);

  if (vol->unknown.type != CDVOL_TYPE_STANDARD ||
      grub_memcmp (vol->boot_record_volume.system_id, CDVOL_ELTORITO_ID,
                   sizeof (CDVOL_ELTORITO_ID) - 1) != 0)
  {
    grub_free (vol);
    return FALSE;
  }

  catalog = (eltorito_catalog_t *) vol;
  file_read (cmd->disk, vdisk.file, catalog, CD_BLOCK_SIZE,
    *((grub_efi_uint32_t*) vol->boot_record_volume.elt_catalog) * CD_BLOCK_SIZE);
  if (catalog[0].catalog.indicator != ELTORITO_ID_CATALOG)
  {
    grub_free (vol);
    return FALSE;
  }

  for (i = 0; i < 64; i++)
  {
    if (catalog[i].section.indicator == ELTORITO_ID_SECTION_HEADER_FINAL &&
        catalog[i].section.platform_id == EFI_PARTITION &&
        catalog[i+1].boot.indicator == ELTORITO_ID_SECTION_BOOTABLE)
    {
      boot_entry = TRUE;
      vpart.addr = catalog[i+1].boot.lba << CD_SHIFT;
      vpart.size = catalog[i+1].boot.sector_count << FD_SHIFT;

      file_read (cmd->disk, vdisk.file,
                 &dbr_img_buf, dbr_img_size, vpart.addr + 0x13);
      dbr_img_size = dbr_img_buf << FD_SHIFT;
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
    grub_free (vol);
    return FALSE;
  }
  vpart.addr = vpart.addr + vdisk.addr;

  tmp_dp = grub_efi_create_device_node (MEDIA_DEVICE_PATH, MEDIA_CDROM_DP,
                                        sizeof (grub_efi_cdrom_device_path_t));
  ((grub_efi_cdrom_device_path_t *)tmp_dp)->boot_entry = 1;
  ((grub_efi_cdrom_device_path_t *)tmp_dp)->partition_start =
            (vpart.addr - vdisk.addr) >> CD_SHIFT;
  ((grub_efi_cdrom_device_path_t *)tmp_dp)->partition_size =
            vpart.size >> CD_SHIFT;
  vpart.dp = grub_efi_append_device_node (vdisk.dp, tmp_dp);
  grub_free (tmp_dp);
  grub_free (vol);
  return TRUE;
}

grub_efi_status_t
vpart_install (grub_efi_boolean_t ro)
{
  grub_efi_status_t status;
  char *text_dp = NULL;
  grub_efi_boot_services_t *b;
  b = grub_efi_system_table->boot_services;
  /* guid */
  grub_efi_guid_t dp_guid = GRUB_EFI_DEVICE_PATH_GUID;
  grub_efi_guid_t blk_io_guid = GRUB_EFI_BLOCK_IO_GUID;
  grub_efi_guid_t sfs_guid = GRUB_EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
  grub_efi_guid_t cn2_guid = GRUB_EFI_COMPONENT_NAME2_PROTOCOL_GUID;

  switch (vdisk.type)
  {
    case CD:
      grub_printf ("Detecting ElTorito partition ... ");
      vpart.present = get_iso_info ();
      break;
    case MBR:
      grub_printf ("Detecting MBR active partition ... ");
      vpart.present = get_mbr_info ();
      break;
    case GPT:
      grub_printf ("Detecting GPT ESP partition ... ");
      vpart.present = get_gpt_info ();
      break;
    default:
      break;
  }

  if (!vpart.present)
  {
    grub_printf ("NOT FOUND\n");
    return GRUB_EFI_NOT_FOUND;
  }
  grub_printf ("OK\n");

  vpart.handle = NULL;
  vpart.file = vdisk.file;
  vpart.disk = vdisk.disk;
  vpart.mem = vdisk.mem;
  vpart.type = vdisk.type;

  grub_memcpy (&vpart.block_io, &blockio_template, sizeof (block_io_protocol_t));

  vpart.block_io.media = &vpart.media;
  vpart.media.media_id = VDISK_MEDIA_ID;
  vpart.media.removable_media = FALSE;
  vpart.media.media_present = TRUE;
  vpart.media.logical_partition = TRUE;
  vpart.media.read_only = ro;
  vpart.media.write_caching = FALSE;
  vpart.media.io_align = 16;
  vpart.media.block_size = vdisk.bs;
  vpart.media.last_block =
              grub_divmod64 (vpart.size + vdisk.bs - 1, vdisk.bs, 0) - 1;
  /* info */
  grub_printf ("VPART addr=%ld size=%lld\n", (unsigned long)vpart.addr,
          (unsigned long long)vpart.size);
  grub_printf ("VPART blksize=%d lastblk=%lld\n", vpart.media.block_size,
          (unsigned long long)vpart.media.last_block);
  text_dp = grub_efi_device_path_to_str (vpart.dp);
  grub_printf ("VPART DevicePath: %s\n",text_dp);
  if (text_dp)
    grub_free (text_dp);
  grub_printf ("Installing block_io protocol for virtual partition ...\n");
  status = efi_call_6 (b->install_multiple_protocol_interfaces,
                       &vpart.handle,
                       &dp_guid, vpart.dp,
                       &blk_io_guid, &vpart.block_io, NULL);
  if(status != GRUB_EFI_SUCCESS)
  {
    grub_printf ("failed to install virtual partition\n");
    return GRUB_EFI_NOT_FOUND;
  }
  efi_call_4 (b->connect_controller, vpart.handle, NULL, NULL, TRUE);

  if(vdisk.type != CD)
    return GRUB_EFI_SUCCESS;
  /* for DUET UEFI firmware */
  {
    grub_efi_handle_t fat_handle = NULL;
    grub_efi_handle_t *buf;
    grub_efi_uintn_t count;
    grub_efi_uintn_t i;
    grub_efi_component_name2_protocol_t *cn2_protocol;
    grub_efi_char16_t *driver_name;
    grub_efi_simple_fs_protocol_t *sfs_protocol = NULL;

    status = efi_call_3 (b->handle_protocol, vpart.handle,
                         &sfs_guid, (void **)&sfs_protocol);
    if(status == GRUB_EFI_SUCCESS)
      return GRUB_EFI_SUCCESS;

    status = efi_call_5 (b->locate_handle_buffer, GRUB_EFI_BY_PROTOCOL,
                         &cn2_guid, NULL, &count, &buf);
    if(status != GRUB_EFI_SUCCESS)
    {
      grub_printf ("ComponentNameProtocol not found.\n");
    }
    for (i = 0; i < count; i++)
    {
      efi_call_3 (b->handle_protocol, buf[i], &cn2_guid, (void **)&cn2_protocol);
      efi_call_3 (cn2_protocol->get_driver_name,
                  cn2_protocol, (grub_efi_char8_t *)"en-us", &driver_name);
      if(driver_name && wstrstr (driver_name, L"FAT File System Driver"))
      {
        fat_handle = buf[i];
        break;
      }
    }
    if (fat_handle)
    {
      status = efi_call_4 (b->connect_controller,
                           vpart.handle, &fat_handle, NULL, TRUE);
      return GRUB_EFI_SUCCESS;
    }
    else
    {
      grub_printf ("FAT Driver not found.\n");
      return GRUB_EFI_NOT_FOUND;
    }
  }
}

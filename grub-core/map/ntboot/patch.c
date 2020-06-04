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

#include <grub/disk.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/types.h>
#include <grub/term.h>
#include <grub/msdos_partition.h>
#include <grub/gpt_partition.h>
#include <grub/charset.h>

#include <ntboot.h>
#include <stdint.h>
#include <misc.h>
#include <wimboot.h>
#include <guid.h>

static void
bcd_patch_guid_offset (enum boot_type type, grub_size_t offset)
{
  char *p;
  if (bcd_len < offset)
  {
    grub_printf ("bad BCD file.");
    return ;
  }
  p = bcd + offset;
  if (*p != 'd')
  {
    grub_printf ("bad BCD file.");
    return ;
  }
  if (type == BOOT_WIM)
    *p = 'a';
  if (type == BOOT_VHD)
    *p = 'b';
}

static void
bcd_patch_guid (enum boot_type type)
{
  bcd_patch_guid_offset (type, 0x343c);
  bcd_patch_guid_offset (type, 0x35dc);
}

static void
bcd_patch_path_offset (const char *search, const char *path)
{
  char *utf8_path = NULL, *p;
  wchar_t *utf16_path = NULL;
  grub_size_t len = 2 * (grub_strlen (path) + 1);
  utf8_path = grub_strdup (path);
  if (!utf8_path)
  {
    grub_printf ("out of memory.");
    return ;
  }
  utf16_path = grub_zalloc (len);
  if (!utf16_path)
  {
    grub_printf ("out of memory.");
    grub_free (utf8_path);
    return ;
  }
  /* replace '/' to '\\' */
  p = utf8_path;
  while (*p)
  {
    if (*p == '/')
      *p = '\\';
    p++;
  }
  /* UTF-8 to UTF-16le */
  grub_utf8_to_utf16 (utf16_path, len, (grub_uint8_t *)utf8_path, -1, NULL);

  vfat_replace_hex (bcd, bcd_len, search, grub_strlen (search),
                    (char *)utf16_path, len, 2);
  grub_free (utf16_path);
  grub_free (utf8_path);
}

static void
bcd_patch_path (enum boot_type type, const char *path)
{
  const char *wim_path = "\\WIM_FILE_PATH";
  const char *vhd_path = "\\VHD_FILE_PATH";
  if (type == BOOT_WIM)
    bcd_patch_path_offset (wim_path, path);
  if (type == BOOT_VHD)
    bcd_patch_path_offset (vhd_path, path);
}

static void
bcd_patch_mbr_offset (grub_uint8_t *start, grub_uint8_t *sgn)
{
  const char default_sgn[] = { 0x53, 0xb7, 0x53, 0xb7 };
  const char default_start[] = { 0x00, 0x7e, 0x00, 0x00 };
  /* starting lba */
  vfat_replace_hex (bcd, bcd_len, default_start, 4, (char *)start, 8, 0);
  /* unique signature */
  vfat_replace_hex (bcd, bcd_len, default_sgn, 4, (char *)sgn, 4, 0);
}

static void
bcd_patch_mbr (const char *diskname, grub_disk_addr_t lba)
{
  grub_uint8_t start[8];
  grub_disk_t disk = 0;
  struct grub_msdos_partition_mbr *mbr = NULL;
  disk = grub_disk_open (diskname);
  if (!disk)
  {
    grub_printf ("failed to open %s\n", diskname);
    goto mbr_fail;
  }
  mbr = grub_zalloc (GRUB_DISK_SECTOR_SIZE);
  if (!mbr)
  {
    grub_printf ("out of memory");
    goto mbr_fail;
  }
  if (grub_disk_read (disk, 0, 0, GRUB_DISK_SECTOR_SIZE, mbr))
  {
    if (!grub_errno)
      grub_printf ("premature end of disk");
    goto mbr_fail;
  }
  *(grub_uint64_t *)start = lba << GRUB_DISK_SECTOR_BITS;
  /* write */
  bcd_patch_mbr_offset (start, mbr->unique_signature);
mbr_fail:
  if (mbr)
    grub_free (mbr);
  if (disk)
    grub_disk_close (disk);
}

static void
bcd_patch_gpt_offset (grub_uint8_t *diskguid, grub_uint8_t *partguid)
{
  const char default_partmap[] = { 0x01, 0x00, 0x00, 0x00,
                                   0x53, 0xb7, 0x53, 0xb7 };
  const char default_disk[] = { 0x53, 0xb7, 0x53, 0xb7 };
  const char default_part[] = { 0x00, 0x7e, 0x00, 0x00 };
  const char gpt_partmap[] = { 0x00 };
  /* partmap */
  vfat_replace_hex (bcd, bcd_len, default_partmap, 8, gpt_partmap, 1, 0);
  /* disk guid */
  vfat_replace_hex (bcd, bcd_len, default_disk, 4, (char *)diskguid, 16, 0);
  /* part guid */
  vfat_replace_hex (bcd, bcd_len, default_part, 4, (char *)partguid, 16, 0);
}

static void
bcd_patch_gpt (const char *diskname, int partnum)
{
  struct grub_gpt_header *gpt = NULL;
  struct grub_gpt_partentry *gpt_entry = NULL;
  grub_disk_t disk = 0;
  grub_uint32_t gpt_entry_size;
  grub_uint64_t gpt_entry_pos;
  grub_uint8_t diskguid[16];
  grub_uint8_t partguid[16];

  disk = grub_disk_open (diskname);
  if (!disk)
  {
    grub_printf ("failed to open %s\n", diskname);
    goto gpt_fail;
  }
  gpt = grub_zalloc (GRUB_DISK_SECTOR_SIZE);
  if (!gpt)
  {
    grub_printf ("out of memory");
    goto gpt_fail;
  }
  if (grub_disk_read (disk, 1, 0, GRUB_DISK_SECTOR_SIZE, gpt))
  {
    if (!grub_errno)
      grub_printf ("premature end of disk");
    goto gpt_fail;
  }
  grub_guidcpy ((grub_packed_guid_t *)&diskguid, &gpt->guid);
  gpt_entry_pos = gpt->partitions << GRUB_DISK_SECTOR_BITS;
  gpt_entry_size = gpt->partentry_size;
  gpt_entry = grub_zalloc (gpt_entry_size);
  if (!gpt_entry)
  {
    grub_printf ("out of memory");
    goto gpt_fail;
  }
  grub_disk_read (disk, 0, gpt_entry_pos + partnum * gpt_entry_size,
                  gpt_entry_size, gpt_entry);
  grub_guidcpy ((grub_packed_guid_t *)&partguid, &gpt_entry->guid);
  /* write */
  bcd_patch_gpt_offset (diskguid, partguid);
gpt_fail:
  if (gpt)
    grub_free (gpt);
  if (gpt_entry)
    grub_free (gpt_entry);
  if (disk)
    grub_disk_close (disk);
}

void
ntboot_patch_bcd (enum boot_type type, /* vhd or wim */
           const char *path,    /* file path '/boot/boot.wim' */
           const char *diskname,/* disk name 'hd0' */
           grub_disk_addr_t lba,/* starting_lba */
           int partnum,         /* partition number */
           const char *partmap) /* partition table 'msdos' */
{
  vfat_patch_bcd (NULL, bcd, 0, bcd_len);
  if (type != BOOT_WIN)
  {
    bcd_patch_guid (type);
    bcd_patch_path (type, path);
  }
  if (wimboot_cmd.pause)
    grub_getkey ();
  if (partmap[0] == 'g')
    bcd_patch_gpt (diskname, partnum);
  else
    bcd_patch_mbr (diskname, lba);
  if (wimboot_cmd.pause)
    grub_getkey ();
}

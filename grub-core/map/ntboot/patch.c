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

#include <grub/disk.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/types.h>
#include <grub/term.h>
#include <grub/msdos_partition.h>

#include "ntboot.h"
#include <stdint.h>
#include <maplib.h>
#include <wimboot.h>

static void
print_hex (grub_size_t offset,
           const char *prefix, grub_size_t len, int hex)
{
  grub_size_t i, j;
  unsigned char *p;
  p = bcd + offset;
  grub_printf ("0x%04lx %s:\n", (unsigned long)offset, prefix);
  for (i = 0, j = 0; i < len; i++, j++, p++)
  {
    if (j == 64)
    {
      grub_printf ("\n");
      j = 0;
    }
    if (hex)
      grub_printf (" %02x", *p);
    else
    {
      if (*p > 0x19 && *p < 0x7f)
        grub_printf ("%c", *p);
      else
        grub_printf (".");
    }
  }
  grub_printf ("\n");
}

static grub_size_t
replace_hex (const unsigned char *search, grub_size_t search_len,
             const unsigned char *replace, grub_size_t replace_len, int count)
{
  grub_size_t offset, last = 0;
  int cnt = 0;
  for (offset = 0; offset + search_len < bcd_len; offset++)
  {
    if (grub_memcmp (bcd + offset, search, search_len) == 0)
    {
      last = offset;
      grub_printf (" 0x%04x", (unsigned) offset);
      cnt++;
      grub_memcpy (bcd + offset, replace, replace_len);
      if (count && cnt == count)
        break;
    }
  }
  if (cnt)
    grub_printf ("\n");
  return last;
}

static void
bcd_patch_guid_offset (enum boot_type type, grub_size_t offset)
{
  unsigned char *p;
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
  print_hex (offset - 0x10, "guid", 76, 0);
  if (type == BOOT_WIM)
    *p = 'a';
  if (type == BOOT_VHD)
    *p = 'b';
  print_hex (offset - 0x10, "replace", 76, 0);
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
  grub_size_t len = 2 * (grub_strlen (path) + 1);
  wchar_t *upath = NULL;
  grub_size_t offset;
  grub_size_t i;
  char c;
  upath = grub_zalloc (len);
  if (!upath)
  {
    grub_printf ("out of memory.");
    return ;
  }
  for (i = 0; i < grub_strlen (path); i++)
  {
    c = path[i];
    if (c == '/')
      c = '\\';
    upath[i] = c;
  }
  offset = replace_hex ((const unsigned char *)search, grub_strlen (search),
                        (unsigned char *)upath, len, 2);
  print_hex (offset, "path", len, 0);
  grub_free (upath);
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
  const unsigned char default_sgn[] = { 0x53, 0xb7, 0x53, 0xb7 };
  const unsigned char default_start[] = { 0x00, 0x7e, 0x00, 0x00 };
  grub_size_t offset;
  /* starting lba */
  offset = replace_hex (default_start, 4, start, 8, 0);
  print_hex (offset, "replace", 8, 1);
  /* unique signature */
  offset = replace_hex (default_sgn, 4, sgn, 4, 0);
  print_hex (offset, "replace", 4, 1);
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
    goto fail;
  }
  mbr = grub_zalloc (GRUB_DISK_SECTOR_SIZE);
  if (!mbr)
  {
    grub_printf ("out of memory");
    goto fail;
  }
  if (grub_disk_read (disk, 0, 0, GRUB_DISK_SECTOR_SIZE, mbr))
  {
    if (!grub_errno)
      grub_printf ("premature end of disk");
    goto fail;
  }
  *(grub_uint64_t *)start = lba << GRUB_DISK_SECTOR_BITS;
  /* write */
  bcd_patch_mbr_offset (start, mbr->unique_signature);
fail:
  if (mbr)
    grub_free (mbr);
  if (disk)
    grub_disk_close (disk);
}

static void
bcd_patch_gpt_offset (grub_uint8_t *diskguid, grub_uint8_t *partguid)
{
  const unsigned char default_partmap[] = { 0x01, 0x00, 0x00, 0x00,
                                            0x53, 0xb7, 0x53, 0xb7 };
  const unsigned char default_disk[] = { 0x53, 0xb7, 0x53, 0xb7 };
  const unsigned char default_part[] = { 0x00, 0x7e, 0x00, 0x00 };
  grub_size_t offset;
  const unsigned char gpt_partmap[] = { 0x00 };
  /* partmap */
  offset = replace_hex (default_partmap, 8, gpt_partmap, 1, 0);
  print_hex (offset, "replace", 8, 1);
  /* disk guid */
  offset = replace_hex (default_disk, 4, diskguid, 16, 0);
  print_hex (offset, "replace", 16, 1);
  /* part guid */
  offset = replace_hex (default_part, 4, partguid, 16, 0);
  print_hex (offset, "replace", 16, 1);
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
    goto fail;
  }
  gpt = grub_zalloc (GRUB_DISK_SECTOR_SIZE);
  if (!gpt)
  {
    grub_printf ("out of memory");
    goto fail;
  }
  if (grub_disk_read (disk, 1, 0, GRUB_DISK_SECTOR_SIZE, gpt))
  {
    if (!grub_errno)
      grub_printf ("premature end of disk");
    goto fail;
  }
  guidcpy ((grub_packed_guid_t *)&diskguid, &gpt->guid);
  gpt_entry_pos = gpt->partitions << GRUB_DISK_SECTOR_BITS;
  gpt_entry_size = gpt->partentry_size;
  gpt_entry = grub_zalloc (gpt_entry_size);
  if (!gpt_entry)
  {
    grub_printf ("out of memory");
    goto fail;
  }
  grub_disk_read (disk, 0, gpt_entry_pos + partnum * gpt_entry_size,
                  gpt_entry_size, gpt_entry);
  guidcpy ((grub_packed_guid_t *)&partguid, &gpt_entry->guid);
  /* write */
  bcd_patch_gpt_offset (diskguid, partguid);
fail:
  if (gpt)
    grub_free (gpt);
  if (gpt_entry)
    grub_free (gpt_entry);
  if (disk)
    grub_disk_close (disk);
}

void
bcd_patch (enum boot_type type, /* vhd or wim */
           const char *path,    /* file path '/boot/boot.wim' */
           const char *diskname,/* disk name 'hd0' */
           grub_disk_addr_t lba,/* starting_lba */
           int partnum,         /* partition number */
           const char *partmap) /* partition table 'msdos' */
{
  bcd_patch_guid (type);
  bcd_patch_path (type, path);
  if (wimboot_cmd.pause)
    grub_getkey ();
  if (partmap[0] == 'g')
    bcd_patch_gpt (diskname, partnum);
  else
    bcd_patch_mbr (diskname, lba);
  if (wimboot_cmd.pause)
    grub_getkey ();
}

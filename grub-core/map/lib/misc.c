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

#include <grub/types.h>
#include <grub/term.h>
#include <grub/misc.h>
#include <grub/file.h>
#include <grub/msdos_partition.h>

#include <misc.h>

#ifdef GRUB_MACHINE_EFI
#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#include <grub/efi/disk.h>

enum grub_efivdisk_type
grub_vdisk_check_type (grub_file_t file)
{
  struct grub_msdos_partition_mbr mbr;
  file_read (file, &mbr, sizeof (mbr), 0);
  if (mbr.signature != GRUB_PC_PARTITION_SIGNATURE)
    return CD;
  if (mbr.entries[0].type != GRUB_PC_PARTITION_TYPE_GPT_DISK)
    return MBR;
  else
    return GPT;
}

#endif

wchar_t *grub_wstrstr
(const wchar_t *str, const wchar_t *search_str)
{
  const wchar_t *first_match;
  const wchar_t *search_str_tmp;
  if (*search_str == L'\0')
    return (wchar_t *) str;
  while (*str != L'\0')
  {
    search_str_tmp = search_str;
    first_match = str;
    while ((*str == *search_str_tmp) && (*str != L'\0'))
    {
      str++;
      search_str_tmp++;
    }
    if (*search_str_tmp == L'\0')
      return (wchar_t *) first_match;
    if (*str == L'\0')
      return NULL;
    str = first_match + 1;
  }
  return NULL;
}

void
grub_pause_boot (void)
{
  grub_printf ("Press any key to continue booting...");
  grub_getkey ();
  grub_printf ("\n");
}

void
grub_pause_fatal (const char *fmt, ...)
{
  va_list args;
  /* Print message */
  va_start (args, fmt);
  grub_vprintf (fmt, args);
  va_end (args);
  grub_getkey ();
  grub_fatal ("Exit.\n");
}

grub_file_t
file_open (const char *name, int mem, int bl, int rt)
{
  grub_file_t file = 0;
  grub_size_t size = 0;
  enum grub_file_type type = GRUB_FILE_TYPE_LOOPBACK;

  file = grub_file_open (name, type);
  if (!file)
    return NULL;
  size = grub_file_size (file);
  if (bl || (file->fs && file->fs->fast_blocklist))
    grub_blocklist_convert (file);
  if (mem)
  {
    void *addr = NULL;
    char newname[100];
#ifdef GRUB_MACHINE_EFI
    grub_efi_allocate_pool (rt ? GRUB_EFI_RUNTIME_SERVICES_DATA :
                            GRUB_EFI_BOOT_SERVICES_DATA, size, &addr);
#else
    (void)rt;
    addr = grub_malloc (size);
#endif
    if (!addr)
    {
      grub_printf ("out of memory\n");
      grub_file_close (file);
      return NULL;
    }
    grub_printf ("Loading %s ...\n", name);
    grub_file_read (file, addr, size);
    grub_file_close (file);
    grub_snprintf (newname, 100, "mem:%p:size:%lld", addr, (unsigned long long)size);
    file = grub_file_open (newname, type);
  }
  return file;
}

void
file_read (grub_file_t file, void *buf, grub_size_t len, grub_off_t offset)
{
  grub_file_seek (file, offset);
  grub_file_read (file, buf, len);
}

void
file_write (grub_file_t file, const void *buf, grub_size_t len, grub_off_t offset)
{
  if (grub_ismemfile (file->name))
  {
    grub_memcpy((grub_uint8_t *)(file->data) + offset, buf, len);
  }
  else if (file->fs && grub_strcmp (file->fs->name, "blocklist") == 0)
  {
    grub_file_seek (file, offset);
    grub_blocklist_write (file, buf, len);
  }
}

void
file_close (grub_file_t file)
{
  if (!file)
    return;
  if (grub_ismemfile (file->name))
  {
#ifdef GRUB_MACHINE_EFI
    grub_efi_free_pool (file->data);
#else
    grub_free (file->data);
#endif
  }
  grub_file_close (file);
}

#ifdef GRUB_MACHINE_EFI
int grub_isefi = 1;
#else
int grub_isefi = 0;
#endif

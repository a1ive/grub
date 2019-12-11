/*
 * Copyright (C) 2014 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

/**
 * @file
 *
 * EFI file system access
 *
 */

#include <stdio.h>
#include <string.h>
#include <efiapi.h>
#include <wimboot.h>
#include <maplib.h>
#include <vfat.h>
#include <wimpatch.h>
#include <wimfile.h>

/** bootmgfw.efi path within WIM */
static const wchar_t bootmgfw_path[] = L"\\Windows\\Boot\\EFI\\bootmgfw.efi";

/** bootmgfw.efi file */
struct vfat_file *bootmgfw;

/**
 * Read from file
 *
 * @v file              Virtual file
 * @v data              Data buffer
 * @v offset            Offset
 * @v len               Length
 */
void
efi_read_file (struct vfat_file *vfile, void *data, size_t offset, size_t len)
{
  grub_file_t file = vfile->opaque;
  /* Set file position */
  grub_file_seek (file, offset);
  /* Read from file */
  grub_file_read (file, data, len);
}

void
mem_read_file (struct vfat_file *file, void *data, size_t offset, size_t len)
{
  memcpy (data, ((char *)file->opaque + offset), len);
}

#if defined (__i386__)
  #define BOOT_FILE_NAME   L"BOOTIA32.EFI"
#elif defined (__x86_64__)
  #define BOOT_FILE_NAME   L"BOOTX64.EFI"
#elif defined (__arm__)
  #define BOOT_FILE_NAME   L"BOOTARM.EFI"
#elif defined (__aarch64__)
  #define BOOT_FILE_NAME   L"BOOTAA64.EFI"
#else
  #error Unknown Processor Type
#endif

static const wchar_t efi_bootarch[] = BOOT_FILE_NAME;

/**
 * Patch BCD file
 *
 * @v vfile    Virtual file
 * @v data    Data buffer
 * @v offset    Offset
 * @v len    Length
 */
static void
efi_patch_bcd (struct vfat_file *vfile __unused,
               void *data, size_t offset, size_t len)
{
  static const wchar_t search[] = L".exe";
  static const wchar_t replace[] = L".efi";
  size_t i;

  /* Do nothing if BCD patching is disabled */
  if (wimboot_cmd.rawbcd)
    return;
  /* Patch any occurrences of ".exe" to ".efi".  In the common
   * simple cases, this allows the same BCD file to be used for
   * both BIOS and UEFI systems.
   */
  for (i = 0 ;(i + sizeof (search)) < len; i++)
  {
    if (wcscasecmp ((wchar_t *)((char *)data + i), search) == 0)
    {
      memcpy (((char *)data + i), replace, sizeof (replace));
      printf ("...patched BCD at 0x%lx: \".exe\" to \".efi\"\n",
              (unsigned long)(offset + i));
    }
  }
}

static int
isbootmgfw (const char *name)
{
  char bootarch[32];
  if (strcasecmp(name, "bootmgfw.efi") == 0)
    return 1;
  wcstombs (bootarch, efi_bootarch, sizeof (bootarch));
  return strcasecmp (name, bootarch) == 0;
}

int add_file (const char *name, void *data, size_t len,
              void (* read) (struct vfat_file *file,
                             void *data, size_t offset, size_t len))
{
  struct vfat_file *vfile;
  vfile = vfat_add_file (name, data, len, read);

    /* Check for special-case files */
  if (isbootmgfw (name))
  {
    printf ("...found bootmgfw.efi file %s\n", name);
    bootmgfw = vfile;
  }
  else if (strcasecmp (name, "BCD") == 0)
  {
    printf ("...found BCD\n");
    vfat_patch_file (vfile, efi_patch_bcd);
  }
  else if (strlen(name) > 4 &&
           strcasecmp ((name + (strlen (name) - 4)), ".wim") == 0)
  {
    printf ("...found WIM file %s\n", name);
    vfat_patch_file (vfile, patch_wim);
    if ((! bootmgfw) &&
        (bootmgfw = wim_add_file (vfile, wimboot_cmd.index,
                                  bootmgfw_path, efi_bootarch)))
    {
      printf ("...extracted bootmgfw.efi from WIM\n");
    }
  }
  return 0;
}

void
grub_extract (void)
{
  struct grub_vfatdisk_file *f = NULL;
  for (f = vfat_file_list; f; f = f->next)
  {
    if (f->addr)
      add_file (f->name, f->addr, f->file->size, mem_read_file);
    else
      add_file (f->name, f->file, f->file->size, efi_read_file);
  }
  /* Check that we have a boot file */
  if (! bootmgfw)
    die ("FATAL: bootmgfw.efi not found\n");
}

void
grub_wimboot_init (int argc, char *argv[])
{
  int i;
  struct grub_vfatdisk_file *wim = NULL;

  for (i = 0; i < argc; i++)
  {
    const char *fname = argv[i];
    char *file_name = NULL;
    grub_file_t file = 0;
    if (grub_memcmp (argv[i], "@:", 2) == 0)
    {
      const char *ptr, *eptr;
      ptr = argv[i] + 2;
      eptr = grub_strchr (ptr, ':');
      if (eptr)
      {
        file_name = grub_strndup (ptr, eptr - ptr);
        if (!file_name)
          die ("file name error.\n");
        fname = eptr + 1;
      }
    }
    file = grub_file_open (fname,
                GRUB_FILE_TYPE_LINUX_INITRD | GRUB_FILE_TYPE_NO_DECOMPRESS);
    if (!file)
      die ("bad file.\n");
    if (!file_name)
      file_name = grub_strdup (file->name);
    /* Skip wim file */
    if (!wim && strlen(file_name) > 4 &&
        strcasecmp ((file_name + (strlen (file_name) - 4)), ".wim") == 0)
    {
      wim = malloc (sizeof (struct grub_vfatdisk_file));
      wim->name = grub_strdup (file_name);
      wim->file = file;
      wim->addr = NULL;
      wim->next = NULL;
      grub_free (file_name);
      continue;
    }
    append_vfat_list (file, file_name, NULL, 0);
    grub_free (file_name);
  }
  if (wim)
  {
    struct grub_vfatdisk_file *f = vfat_file_list;
    while (f && f->next)
    {
      f = f->next;
    }
    f->next = wim;
  }
}

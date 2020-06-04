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
#include <grub/misc.h>
#include <grub/file.h>

#include <misc.h>
#include <vfat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <wimboot.h>
#include <wimpatch.h>
#include <wimfile.h>

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

/** bootmgfw.efi path within WIM */
static const wchar_t bootmgfw_path[] = L"\\Windows\\Boot\\EFI\\bootmgfw.efi";

/** bootmgfw.efi file */
struct vfat_file *bootmgfw;

static const wchar_t efi_bootarch[] = BOOT_FILE_NAME;

static void
read_wrapper (struct vfat_file *vfile, void *data, size_t offset, size_t len)
{
  file_read (vfile->opaque, data, len, offset);
}

#ifdef GRUB_MACHINE_EFI
#define SEARCH_EXT  L".exe"
#define REPLACE_EXT L".efi"
#else
#define SEARCH_EXT  L".efi"
#define REPLACE_EXT L".exe"
#endif

void
vfat_patch_bcd (struct vfat_file *file __unused,
           void *data, size_t offset __unused, size_t len)
{
  static const wchar_t search[] = SEARCH_EXT;
  static const wchar_t replace[] = REPLACE_EXT;
  size_t i;

  /* Patch any occurrences of ".exe" to ".efi".  In the common
   * simple cases, this allows the same BCD file to be used for
   * both BIOS and UEFI systems.
   */
  for (i = 0 ;(i + sizeof (search)) < len; i++)
  {
    if (wcscasecmp ((wchar_t *)((char *)data + i), search) == 0)
    {
      memcpy (((char *)data + i), replace, sizeof (replace));
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

int file_add (const char *name, grub_file_t data, struct wimboot_cmdline *cmd)
{
  struct vfat_file *vfile;
  vfile = vfat_add_file (name, data, data->size, read_wrapper);

    /* Check for special-case files */
  if (isbootmgfw (name) && grub_isefi)
  {
    printf ("...found bootmgfw.efi file %s\n", name);
    bootmgfw = vfile;
  }
  else if (strcasecmp (name, "BCD") == 0 && !cmd->rawbcd)
  {
    printf ("...found BCD\n");
    vfat_patch_file (vfile, vfat_patch_bcd);
  }
  else if (strlen(name) > 4 &&
           strcasecmp ((name + (strlen (name) - 4)), ".wim") == 0)
  {
    printf ("...found WIM file %s\n", name);
    if (!cmd->rawwim)
      vfat_patch_file (vfile, patch_wim);
    if ((! bootmgfw) && grub_isefi &&
        (bootmgfw = wim_add_file (vfile, cmd->index,
                                  bootmgfw_path, efi_bootarch)))
    {
      printf ("...extracted bootmgfw.efi from WIM\n");
    }
  }
  return 0;
}

void
grub_wimboot_extract (struct wimboot_cmdline *cmd)
{
  struct grub_vfatdisk_file *f = NULL;
  for (f = vfat_file_list; f; f = f->next)
  {
    file_add (f->name, f->file, cmd);
  }
  /* Check that we have a boot file */
  if (! bootmgfw)
    grub_pause_fatal ("FATAL: bootmgfw.efi not found\n");
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
    if (grub_memcmp (argv[i], "@:", 2) == 0 || 
        grub_memcmp (argv[i], "m:", 2) == 0)
    {
      const char *ptr, *eptr;
      ptr = argv[i] + 2;
      eptr = grub_strchr (ptr, ':');
      if (eptr)
      {
        file_name = grub_strndup (ptr, eptr - ptr);
        if (!file_name)
          grub_pause_fatal ("file name error.\n");
        fname = eptr + 1;
      }
    }
    int mem = 0;
    if (argv[i][0] == 'm')
      mem = 1;
    file = file_open (fname, mem, 0, 0);
    if (!file)
      grub_pause_fatal ("bad file.\n");
    if (!file_name)
      file_name = grub_strdup (file->name);
    /* Skip wim file */
    if (!wim && strlen(file_name) > 4 &&
        strcasecmp ((file_name + (strlen (file_name) - 4)), ".wim") == 0)
    {
      wim = malloc (sizeof (struct grub_vfatdisk_file));
      wim->name = grub_strdup (file_name);
      wim->file = file;
      wim->next = NULL;
      grub_free (file_name);
      continue;
    }
    vfat_append_list (file, file_name);
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

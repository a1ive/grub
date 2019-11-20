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

#include <grub/dl.h>
#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#include <grub/device.h>
#include <grub/err.h>
#include <grub/env.h>
#include <grub/file.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/types.h>
#include <grub/term.h>

#include <maplib.h>
#include <private.h>
#include <efiapi.h>
#include <wimboot.h>
#include <vfat.h>

void
print_vfat_help (void)
{
  grub_printf ("\nvfat -- Virtual FAT Disk\n");
  grub_printf ("vfat --create\n");
  grub_printf ("    mount virtual disk to (vfat)\n");
  grub_printf ("vfat [--mem] --add=XXX YYY\n");
  grub_printf ("    Add file \"YYY\" to disk, file name is \"XXX\"\n");
  grub_printf ("vfat --install\n");
  grub_printf ("    Install block_io protocol for virtual disk\n");
  grub_printf ("vfat --boot\n");
  grub_printf ("    Boot bootmgfw.efi from virtual disk\n");
}

static int
grub_vfatdisk_iterate (grub_disk_dev_iterate_hook_t hook, void *hook_data,
                       grub_disk_pull_t pull)
{
  if (pull != GRUB_DISK_PULL_NONE)
    return 0;
  return hook ("vfat", hook_data);
}

static grub_err_t
grub_vfatdisk_open (const char *name, grub_disk_t disk)
{
  if (grub_strcmp (name, "vfat"))
      return grub_error (GRUB_ERR_UNKNOWN_DEVICE, "not a vfat disk");

  disk->total_sectors = VDISK_COUNT;
  disk->max_agglomerate = GRUB_DISK_MAX_MAX_AGGLOMERATE;
  disk->id = 0;

  return GRUB_ERR_NONE;
}

static void
grub_vfatdisk_close (grub_disk_t disk __attribute((unused)))
{
}

static grub_err_t
grub_vfatdisk_read (grub_disk_t disk __attribute((unused)), grub_disk_addr_t sector,
                    grub_size_t size, char *buf)
{
  vfat_read (sector, size, buf);
  return 0;
}

static grub_err_t
grub_vfatdisk_write (grub_disk_t disk __attribute ((unused)),
                     grub_disk_addr_t sector __attribute ((unused)),
                     grub_size_t size __attribute ((unused)),
                     const char *buf __attribute ((unused)))
{
  return grub_error (GRUB_ERR_NOT_IMPLEMENTED_YET, "vfat write is not supported");
}

static struct grub_disk_dev grub_vfatdisk_dev =
{
  .name = "vfat",
  .id = GRUB_DISK_DEVICE_VFAT_ID,
  .disk_iterate = grub_vfatdisk_iterate,
  .disk_open = grub_vfatdisk_open,
  .disk_close = grub_vfatdisk_close,
  .disk_read = grub_vfatdisk_read,
  .disk_write = grub_vfatdisk_write,
  .next = 0
};

void
create_vfat (void)
{
  grub_disk_dev_t dev;
  for (dev = grub_disk_dev_list; dev; dev = dev->next)
  {
    if (grub_strcmp (dev->name, "vfat") == 0)
    {
      grub_printf ("vfat: already exist\n");
      return;
    }
  }
  grub_disk_dev_register (&grub_vfatdisk_dev);
}

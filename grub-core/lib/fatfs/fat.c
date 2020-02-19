/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2009  Free Software Foundation, Inc.
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
 */

#include <grub/dl.h>
#include <grub/err.h>
#include <grub/misc.h>
#include <grub/normal.h>
#include <grub/command.h>
#include <grub/i18n.h>
#include <grub/disk.h>
#include <grub/partition.h>

#include "ff.h"
#include "diskio.h"

GRUB_MOD_LICENSE ("GPLv3+");

static grub_err_t
grub_cmd_mount (grub_command_t cmd __attribute__ ((unused)),
                int argc, char **args)

{
  unsigned int num = 0;
  int namelen;
  grub_disk_t disk = 0;
  if (argc == 1 && grub_strcmp (args[0], "status") == 0)
  {
    int i;
    for (i = 0; i < 10; i++)
    {
      if (!fat_stat[i].disk)
        continue;
      if (!fat_stat[i].disk->partition)
        grub_printf ("%s -> %s:\n", fat_stat[i].disk->name, fat_stat[i].name);
      else
        grub_printf ("%s,%d -> %s:\n", fat_stat[i].disk->name,
                     fat_stat[i].disk->partition->number + 1,
                     fat_stat[i].name);
    }
    return GRUB_ERR_NONE;
  }
  if (argc != 2)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "bad argument");
  num = grub_strtoul (args[1], NULL, 10);
  if (num > 9)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "invalid number");
  namelen = grub_strlen (args[0]);
  if ((args[0][0] == '(') && (args[0][namelen - 1] == ')'))
  {
    args[0][namelen - 1] = 0;
    disk = grub_disk_open (&args[0][1]);
  }
  else
    disk = grub_disk_open (args[0]);
  if (!disk)
    return grub_errno;

  if (fat_stat[num].disk)
  {
    grub_disk_close (disk);
    return grub_error (GRUB_ERR_BAD_DEVICE, "disk number in use");
  }
  fat_stat[num].present = 1;
  grub_snprintf (fat_stat[num].name, 2, "%u", num);
  fat_stat[num].disk = disk;
  fat_stat[num].total_sectors = disk->total_sectors;

  return GRUB_ERR_NONE;
}

static grub_err_t
grub_cmd_umount (grub_command_t cmd __attribute__ ((unused)),
                int argc, char **args)

{
  unsigned int num = 0;

  if (argc != 1)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "bad argument");
  num = grub_strtoul (args[0], NULL, 10);
  if (num > 9)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "invalid number");

  if (fat_stat[num].disk)
    grub_disk_close (fat_stat[num].disk);
  fat_stat[num].disk = 0;
  fat_stat[num].present = 0;
  fat_stat[num].total_sectors = 0;

  return GRUB_ERR_NONE;
}

static grub_command_t cmd_mount, cmd_umount;

GRUB_MOD_INIT(fatfs)
{
  cmd_mount = grub_register_command ("mount", grub_cmd_mount,
                                      N_("status | DISK NUM"),
                                      N_("Mount FAT partition."));
  cmd_umount = grub_register_command ("umount", grub_cmd_umount,
                                      N_("NUM"),
                                      N_("Unmount FAT partition."));
}

GRUB_MOD_FINI(fatfs)
{
  grub_unregister_command (cmd_mount);
  grub_unregister_command (cmd_umount);
}

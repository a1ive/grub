/* partnew.c - command for creating primary partition */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2019  Free Software Foundation, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <grub/types.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/err.h>
#include <grub/dl.h>
#include <grub/normal.h>
#include <grub/device.h>
#include <grub/disk.h>
#include <grub/partition.h>
#include <grub/file.h>
#include <grub/extcmd.h>
#include <grub/i18n.h>
#include <grub/gpt_partition.h>
#include <grub/msdos_partition.h>

#define MAX_MBR_PARTITIONS  4

GRUB_MOD_LICENSE ("GPLv2+");

static void
msdos_part (grub_disk_t disk)
{
  struct grub_msdos_partition_mbr *mbr = NULL;
  mbr = grub_zalloc (sizeof (struct grub_msdos_partition_mbr));
  if (!mbr)
  {
    grub_error (GRUB_ERR_OUT_OF_MEMORY, N_("out of memory"));
    goto fail;
  }
  if (grub_disk_read (disk, 0, 0, sizeof (struct grub_msdos_partition_mbr), mbr))
  {
    if (!grub_errno)
      grub_error (GRUB_ERR_BAD_OS, N_("premature end of disk"));
    goto fail;
  }
  /* get partition info from msdos partition table */
  int i;
  for (i=0; i < MAX_MBR_PARTITIONS; i++)
  {
    grub_printf ("PART %d TYPE=%02X START=%10d LENGTH=%10d ", i,
                 mbr->entries[i].type, mbr->entries[i].start,
                 mbr->entries[i].length);
    if (mbr->entries[i].flag == 0x80)
    {
      grub_printf ("ACTIVE");
    }
    grub_printf ("\n");
  }

fail:
  if (mbr)
    grub_free (mbr);
}

static grub_err_t
grub_cmd_partnew (grub_extcmd_context_t ctxt __attribute__ ((unused)),
                  int argc, char *argv[])
{
  grub_disk_t disk = 0;
  if (argc != 1)
  {
    grub_error (GRUB_ERR_BAD_ARGUMENT, N_("device name expected"));
    goto fail;
  }
  int namelen = grub_strlen (argv[0]);
  if ((argv[0][0] == '(') && (argv[0][namelen - 1] == ')'))
  {
    argv[0][namelen - 1] = 0;
    disk = grub_disk_open (&argv[0][1]);
  }
  else
    disk = grub_disk_open (argv[0]);
  if (!disk)
  {
    grub_error (GRUB_ERR_BAD_DEVICE, N_("failed to open %s"), argv[0]);
    goto fail;
  }

  if (disk->partition)
  {
    grub_printf ("%s is a partition.\n", argv[0]);
    goto fail;
  }
  /* check partmap */
  struct grub_msdos_partition_mbr *mbr = NULL;
  mbr = grub_zalloc (sizeof (struct grub_msdos_partition_mbr));
  if (!mbr)
  {
    grub_error (GRUB_ERR_OUT_OF_MEMORY, N_("out of memory"));
    goto fail;
  }
  if (grub_disk_read (disk, 0, 0, sizeof (struct grub_msdos_partition_mbr), mbr))
  {
    if (!grub_errno)
      grub_error (GRUB_ERR_BAD_OS, N_("premature end of disk"));
    grub_free (mbr);
    goto fail;
  }
  if (mbr->signature != GRUB_PC_PARTITION_SIGNATURE)
  {
    grub_printf ("Unsupported partition table.\n");
    grub_free (mbr);
    goto fail;
  }
  if (mbr->entries[0].type != GRUB_PC_PARTITION_TYPE_GPT_DISK)
  {
    grub_printf ("Partition table: msdos\n");
    grub_free (mbr);
    msdos_part (disk);
  }
  else
  {
    grub_printf ("Partition table: gpt\n");
    grub_free (mbr);
    //gpt_part (disk);
  }
  
fail:
  if (disk)
    grub_disk_close (disk);
  return grub_errno;
}

static grub_extcmd_t cmd;

GRUB_MOD_INIT(partnew)
{
  cmd = grub_register_extcmd ("partnew", grub_cmd_partnew, 0, N_("DISK"),
			      N_("Say `Hello World'."), 0);
}

GRUB_MOD_FINI(partnew)
{
  grub_unregister_extcmd (cmd);
}

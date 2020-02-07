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

GRUB_MOD_LICENSE ("GPLv3+");

static const struct grub_arg_option options_partnew[] = {
  {"active", 'a', 0, N_("Make the partition active."), 0, 0},
  {"file", 'f', 0,
      N_("File that will be used as the content of the new partition"),
      N_("PATH"), ARG_TYPE_STRING},
  {"type", 't', 0,
      N_("Partition type"),
      N_("HEX"), ARG_TYPE_INT},
  {"start", 's', 0, N_("Starting address (in sector units)."),
      N_("n"), ARG_TYPE_INT},
  {"length", 'l', 0, N_("Length (in sector units)."), N_("n"), ARG_TYPE_INT},
  {0, 0, 0, 0, 0, 0}
};

enum options_partnew
{
  PARTNEW_ACTIVE,
  PARTNEW_FILE,
  PARTNEW_TYPE,
  PARTNEW_START,
  PARTNEW_LENGTH,
};

struct block_ctx
{
  grub_disk_addr_t start;
  unsigned long length;
};
static struct block_ctx file_block;

static void
read_block (grub_disk_addr_t sector, unsigned offset __attribute ((unused)),
                unsigned length, void *data)
{
  struct block_ctx *ctx = data;

  ctx->start = sector + 1 - (length >> GRUB_DISK_SECTOR_BITS);
}

static grub_err_t
file_to_block (const char *name)
{
  grub_file_t file = 0;
  char buf[GRUB_DISK_SECTOR_SIZE];

  file = grub_file_open (name,
                GRUB_FILE_TYPE_PRINT_BLOCKLIST | GRUB_FILE_TYPE_NO_DECOMPRESS);
  if (!file)
    return grub_errno;

  if (!file->device->disk)
    return grub_error (GRUB_ERR_BAD_DEVICE,
                       "this command is available only for disk devices");

  // grub_partition_get_start (file->device->disk->partition);

  file->read_hook = read_block;
  file->read_hook_data = &file_block;

  grub_file_read (file, buf, sizeof (buf));

  file_block.length = (grub_file_size (file) + GRUB_DISK_SECTOR_SIZE - 1)
                      >> GRUB_DISK_SECTOR_BITS;

  grub_file_close (file);

  return grub_errno;
}

static void
msdos_part (grub_disk_t disk, unsigned long num, grub_uint8_t type, int active)
{
  struct grub_msdos_partition_mbr *mbr = NULL;
  mbr = grub_zalloc (GRUB_DISK_SECTOR_SIZE);
  if (!mbr)
  {
    grub_error (GRUB_ERR_OUT_OF_MEMORY, N_("out of memory"));
    goto fail;
  }
  if (grub_disk_read (disk, 0, 0, GRUB_DISK_SECTOR_SIZE, mbr))
  {
    if (!grub_errno)
      grub_error (GRUB_ERR_BAD_OS, N_("premature end of disk"));
    goto fail;
  }
  /* get partition info from msdos partition table */
  int i;
  for (i=0; i < MAX_MBR_PARTITIONS; i++)
  {
    grub_printf ("PART %d TYPE=0x%02X START=%10d LENGTH=%10d ", i + 1,
                 mbr->entries[i].type, mbr->entries[i].start,
                 mbr->entries[i].length);
    if (mbr->entries[i].flag == 0x80)
    {
      grub_printf ("ACTIVE");
      if (active)
        mbr->entries[i].flag = 0;
    }
    grub_printf ("\n");
  }

  if (num > 4 || num == 0)
  {
    grub_printf ("Unsupported partition number: %ld\n", num);
    goto fail;
  }
  grub_printf ("Partition %ld:\n", num);
  num--;
  mbr->entries[num].type = type;
  if (active)
    mbr->entries[num].flag = 0x80;
  mbr->entries[num].start = file_block.start;
  mbr->entries[num].length = file_block.length;
  grub_printf ("TYPE=0x%02X START=%10d LENGTH=%10d\n", mbr->entries[num].type,
               mbr->entries[num].start, mbr->entries[num].length);
  if (mbr->hidden_sectors != mbr->entries[num].start)
  {
    grub_printf ("Changing hidden sectors %10d to %10d...",
                 mbr->hidden_sectors, mbr->entries[num].start);
    mbr->hidden_sectors = mbr->entries[num].start;
  }
  /* lba to chs */
  grub_uint8_t start_cl, start_ch, start_dh;
  grub_uint8_t end_cl, end_ch, end_dh;
  lba_to_chs (mbr->entries[num].start, &start_cl, &start_ch, &start_dh);
  lba_to_chs (mbr->entries[num].start + mbr->entries[num].length - 1,
              &end_cl, &end_ch, &end_dh);
  mbr->entries[num].start_head = start_dh;
  mbr->entries[num].start_sector = start_cl;
  mbr->entries[num].start_cylinder = start_ch;
  mbr->entries[num].end_head = end_dh;
  mbr->entries[num].end_sector = end_cl;
  mbr->entries[num].end_cylinder = end_ch;
  /* Write the MBR. */
  grub_disk_write (disk, 0, 0, GRUB_DISK_SECTOR_SIZE, mbr);
fail:
  if (mbr)
    grub_free (mbr);
}

static grub_err_t
grub_cmd_partnew (grub_extcmd_context_t ctxt, int argc, char *argv[])
{
  struct grub_arg_list *state = ctxt->state;
  grub_disk_t disk = 0;
  unsigned long num = 1;
  grub_uint8_t type = 0x00;
  int active = 0;
  if (argc != 2)
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
  mbr = grub_zalloc (GRUB_DISK_SECTOR_SIZE);
  if (!mbr)
  {
    grub_error (GRUB_ERR_OUT_OF_MEMORY, N_("out of memory"));
    goto fail;
  }
  if (grub_disk_read (disk, 0, 0, GRUB_DISK_SECTOR_SIZE, mbr))
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

  if (state[PARTNEW_FILE].set)
  {
    file_to_block (state[PARTNEW_FILE].arg);
    if (grub_errno)
      goto fail;
    grub_printf ("FILE START %10ld LENGTH %10ld\n",
                 (unsigned long) file_block.start, file_block.length);
  }
  else if (state[PARTNEW_START].set && state[PARTNEW_LENGTH].set)
  {
    file_block.start = grub_strtoul (state[PARTNEW_START].arg, NULL, 10);
    file_block.length = grub_strtoul (state[PARTNEW_LENGTH].arg, NULL, 10);
  }
  else
    goto fail;
  num = grub_strtoul (argv[1], NULL, 10);
  if (state[PARTNEW_TYPE].set)
    type = (grub_uint8_t) grub_strtoul (state[PARTNEW_TYPE].arg, NULL, 16);
  if (state[PARTNEW_ACTIVE].set)
    active = 1;
  if (mbr->entries[0].type != GRUB_PC_PARTITION_TYPE_GPT_DISK)
  {
    grub_printf ("Partition table: msdos\n");
    grub_free (mbr);
    msdos_part (disk, num, type, active);
  }
  else
  {
    grub_printf ("Partition table: gpt\n");
    grub_free (mbr);
    //gpt_part (disk, num, type);
  }

fail:
  if (disk)
    grub_disk_close (disk);
  return grub_errno;
}

static grub_extcmd_t cmd;

GRUB_MOD_INIT(partnew)
{
  cmd = grub_register_extcmd ("partnew", grub_cmd_partnew, 0,
                N_("[--active] [--type] [--start --length | --file] DISK PARTNUM"),
                N_("Create a primary partition."), options_partnew);
}

GRUB_MOD_FINI(partnew)
{
  grub_unregister_extcmd (cmd);
}

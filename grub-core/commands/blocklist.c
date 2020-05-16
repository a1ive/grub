/* blocklist.c - print the block list of a file */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2006,2007,2020  Free Software Foundation, Inc.
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
#include <grub/env.h>
#include <grub/misc.h>
#include <grub/file.h>
#include <grub/mm.h>
#include <grub/disk.h>
#include <grub/partition.h>
#include <grub/extcmd.h>
#include <grub/i18n.h>

GRUB_MOD_LICENSE ("GPLv3+");

static grub_size_t
blocklist_to_str (grub_file_t file, int num, char *text, grub_disk_addr_t start)
{
  char str[255];
  int i;
  struct grub_fs_block *p;
  grub_size_t len = 0;
  p = file->data;
  for (i = 0; i < num; i++, p++)
  {
    grub_snprintf (str, 255, "%llu+%lu", (unsigned long long)
                   ((p->offset >> GRUB_DISK_SECTOR_BITS) + start),
                   p->length >> GRUB_DISK_SECTOR_BITS);
    if (text)
      grub_sprintf (text + len, "%s,", str);
    len += grub_strlen (str) + 1;
  }
  if (text && len)
    text[len - 1] = '\0';
  return len;
}

static const struct grub_arg_option options[] =
{
  {"set", 's', 0, N_("Set a variable to return value."), N_("VAR"), ARG_TYPE_STRING},
  {"disk", 'd', 0, N_("Use disk start_sector."), 0, 0},
  {0, 0, 0, 0, 0, 0}
};

enum options
{
  BLOCKLIST_SET,
  BLOCKLIST_DISK,
};

static grub_err_t
grub_cmd_blocklist (grub_extcmd_context_t ctxt, int argc, char **args)
{
  struct grub_arg_list *state = ctxt->state;
  grub_file_t file;
  int num;
  char *text = NULL;
  grub_size_t len;
  grub_disk_addr_t start = 0;

  if (argc < 1)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));

  file = grub_file_open (args[0], GRUB_FILE_TYPE_PRINT_BLOCKLIST
                                  | GRUB_FILE_TYPE_NO_DECOMPRESS);
  if (! file)
    return grub_errno;

  if (! file->device->disk)
  {
    grub_error (GRUB_ERR_BAD_DEVICE,
                "this command is available only for disk devices");
    goto fail;
  }

  num = grub_blocklist_convert (file);
  if (!num)
    goto fail;
  if (state[BLOCKLIST_DISK].set)
    start = grub_partition_get_start (file->device->disk->partition);
  len = blocklist_to_str (file, num, NULL, start);
  text = grub_malloc (len + 1);
  if (!text)
    goto fail;
  blocklist_to_str (file, num, text, start);
  if (state[BLOCKLIST_SET].set)
    grub_env_set (state[BLOCKLIST_SET].arg, text);
  else
    grub_printf ("%s\n", text);
  grub_free (text);

fail:
  if (file)
    grub_file_close (file);
  return grub_errno;
}

static grub_extcmd_t cmd;

GRUB_MOD_INIT(blocklist)
{
  cmd = grub_register_extcmd ("blocklist", grub_cmd_blocklist, 0,
                              N_("[OPTIONS] FILE"), N_("Print a block list."),
                              options);
}

GRUB_MOD_FINI(blocklist)
{
  grub_unregister_extcmd (cmd);
}

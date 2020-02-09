/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2003,2007  Free Software Foundation, Inc.
 *  Copyright (C) 2003  NIIBE Yutaka <gniibe@m17n.org>
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

#include <grub/types.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/err.h>
#include <grub/dl.h>
#include <grub/extcmd.h>
#include <grub/env.h>
#include <grub/file.h>
#include <grub/normal.h>
#include <grub/partition.h>
#include <grub/i18n.h>

GRUB_MOD_LICENSE ("GPLv3+");

static const struct grub_arg_option options[] =
{
  {"set", 's', 0, N_("Set a variable to return value."), N_("VAR"), ARG_TYPE_STRING},
  {"size", 'z', 0, N_("Display file size."), 0, 0},
  {"human", 'm', 0, N_("Display file size in a human readable format."), 0, 0},
  {"fs", 'f', 0, N_("Display filesystem information."), 0, 0},
  {0, 0, 0, 0, 0, 0}
};

enum options
{
  STAT_SET,
  STAT_SIZE,
  STAT_HUMAN,
  STAT_FS,
};

static grub_err_t
grub_cmd_stat (grub_extcmd_context_t ctxt, int argc, char **args)
{
  struct grub_arg_list *state = ctxt->state;
  grub_file_t file = 0;
  grub_off_t size = 0;
  const char *human_size = NULL;
  char str[256];

  if (argc != 1)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "bad argument");
  file = grub_file_open (args[0], GRUB_FILE_TYPE_CAT);
  if (!file)
    return grub_error (GRUB_ERR_BAD_FILENAME, N_("failed to open %s"), args[0]);

  size = grub_file_size (file);
  human_size = grub_get_human_size (size, GRUB_HUMAN_SIZE_SHORT);

  if (state[STAT_SIZE].set)
  {
    grub_snprintf (str, 256, "%llu", (unsigned long long) size);
    grub_printf ("%s\n", str);
  }
  else if (state[STAT_HUMAN].set)
  {
    grub_strncpy (str, human_size, 256);
    grub_printf ("%s\n", str);
  }
  else if (state[STAT_FS].set)
  {
    if (!file->fs || !file->device || !file->device->disk)
      goto fail;
    char *label = NULL;
    char partinfo[64];

    grub_printf ("Filesystem: %s\n", file->fs->name);
    if (file->fs->fs_label)
      file->fs->fs_label (file->device, &label);
    if (label)
      grub_printf ("Label: [%s]\n", label);
    grub_printf ("Disk: %s\n", file->device->disk->name);
    grub_printf ("Total sectors: %llu\n",
                 (unsigned long long)file->device->disk->total_sectors);
    if (file->device->disk->partition)
    {
      grub_snprintf (partinfo, 64, "%s %d %llu %llu %d %u",
                     file->device->disk->partition->partmap->name,
                     file->device->disk->partition->number,
                     (unsigned long long)file->device->disk->partition->start,
                     (unsigned long long)file->device->disk->partition->len,
                     file->device->disk->partition->index,
                     file->device->disk->partition->flag);
      grub_printf ("Partition information: \n%s\n", partinfo);
    }
    else
      grub_strncpy (partinfo, "no_part", 64);
    grub_snprintf (str, 256, "%s [%s] %s %llu %s",
                   file->fs->name, label? label : "",
                   file->device->disk->name,
                   (unsigned long long)file->device->disk->total_sectors,
                   partinfo);
    if (label)
      grub_free (label);
  }
  else
  {
    grub_printf ("File: %s\nSize: %s\nCurrent Offset: %llu\nSeekable: %d\n",
                 file->name, human_size, (unsigned long long) file->offset,
                 !file->not_easily_seekable);
    grub_snprintf (str, 256, "%s %lld %d",
                   human_size, (unsigned long long) file->offset,
                   !file->not_easily_seekable);
  }

  if (state[STAT_SET].set)
    grub_env_set (state[STAT_SET].arg, str);

fail:
  grub_file_close (file);
  return grub_errno;
}

static grub_extcmd_t cmd;

GRUB_MOD_INIT(stat)
{
  cmd = grub_register_extcmd ("stat", grub_cmd_stat, 0, N_("[OPTIONS] FILE"),
                              N_("Display file and filesystem information."),
                              options);
}

GRUB_MOD_FINI(stat)
{
  grub_unregister_extcmd (cmd);
}

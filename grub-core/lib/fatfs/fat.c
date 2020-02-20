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
#include <grub/datetime.h>

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

static grub_err_t
grub_cmd_mkdir (grub_command_t cmd __attribute__ ((unused)),
                int argc, char **args)

{
  char dev[3] = "0:";
  FATFS fs;
  FRESULT res;
  if (argc != 1)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "bad argument");
  if (grub_isdigit (args[0][0]))
    dev[0] = args[0][0];

  f_mount (&fs, dev, 0);
  res = f_mkdir (args[0]);
  if (res)
    return grub_error (GRUB_ERR_WRITE_ERROR, "mkdir failed %d", res);
  f_mount(0, dev, 0);
  return GRUB_ERR_NONE;
}

static FRESULT
copy_file (char *in_name, char *out_name)
{
  FRESULT res;
  BYTE buffer[4096];
  UINT br, bw;
  FIL in, out;
  res = f_open (&in, in_name, FA_READ);
  if (res)
  {
    grub_error (GRUB_ERR_BAD_FILENAME, "src open failed %d", res);
    goto fail;
  }
  res = f_open (&out, out_name, FA_WRITE | FA_CREATE_ALWAYS);
  if (res)
  {
    grub_error (GRUB_ERR_BAD_FILENAME, "dst open failed %d", res);
    goto fail;
  }

  for (;;)
  {
    res = f_read (&in, buffer, sizeof (buffer), &br);
    if (res || br == 0)
      break; /* error or eof */
    res = f_write (&out, buffer, br, &bw);
    if (res || bw < br)
      break; /* error or disk full */
  }
  f_close(&in);
  f_close(&out);
fail:
  return res;
}

static grub_err_t
grub_cmd_cp (grub_command_t cmd __attribute__ ((unused)),
             int argc, char **args)

{
  char in_dev[3] = "0:";
  char out_dev[3] = "0:";
  FATFS in_fs, out_fs;
  FRESULT res;

  if (argc != 2)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "bad argument");

  if (grub_isdigit (args[0][0]))
    in_dev[0] = args[0][0];
  if (grub_isdigit (args[1][0]))
    out_dev[0] = args[1][0];

  f_mount (&in_fs, in_dev, 0);
  f_mount (&out_fs, out_dev, 0);
  res = copy_file (args[0], args[1]);

  f_mount(0, in_dev, 0);
  f_mount(0, out_dev, 0);
  if (res)
    return grub_error (GRUB_ERR_WRITE_ERROR, "copy failed %d", res);
  return GRUB_ERR_NONE;
}

static grub_err_t
grub_cmd_rename (grub_command_t cmd __attribute__ ((unused)),
                 int argc, char **args)

{
  char dev[3] = "0:";
  FATFS fs;
  FRESULT res;
  if (argc != 2)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "bad argument");
  if (grub_isdigit (args[0][0]))
    dev[0] = args[0][0];
  if (grub_isdigit (args[1][0]) && args[1][0] != dev[0])
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "dst drive error");

  f_mount (&fs, dev, 0);
  res = f_rename (args[0], args[1]);
  if (res)
    return grub_error (GRUB_ERR_WRITE_ERROR, "rename failed %d", res);
  f_mount(0, dev, 0);
  return GRUB_ERR_NONE;
}

static grub_err_t
grub_cmd_rm (grub_command_t cmd __attribute__ ((unused)),
             int argc, char **args)

{
  char dev[3] = "0:";
  FATFS fs;
  FRESULT res;
  if (argc != 1)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "bad argument");
  if (grub_isdigit (args[0][0]))
    dev[0] = args[0][0];

  f_mount (&fs, dev, 0);
  res = f_unlink (args[0]);
  if (res)
    return grub_error (GRUB_ERR_WRITE_ERROR, "unlink failed %d", res);
  f_mount(0, dev, 0);
  return GRUB_ERR_NONE;
}

static grub_err_t
grub_cmd_mv (grub_command_t cmd __attribute__ ((unused)),
             int argc, char **args)

{
  char in_dev[3] = "0:";
  char out_dev[3] = "0:";
  FATFS in_fs, out_fs;
  FRESULT res;

  if (argc != 2)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "bad argument");

  if (grub_isdigit (args[0][0]))
    in_dev[0] = args[0][0];
  if (grub_isdigit (args[1][0]))
    out_dev[0] = args[1][0];

  f_mount (&in_fs, in_dev, 0);
  if (in_dev[0] == out_dev[0])
  {
    /* rename */
    res = f_rename (args[0], args[1]);
    if (res)
      return grub_error (GRUB_ERR_WRITE_ERROR, "mv failed %d", res);
    f_mount(0, in_dev, 0);
    return GRUB_ERR_NONE;
  }

  f_mount (&out_fs, out_dev, 0);
  res = copy_file (args[0], args[1]);
  if (res)
    grub_error (GRUB_ERR_WRITE_ERROR, "copy failed %d", res);
  else
    res = f_unlink (args[0]);

  f_mount(0, in_dev, 0);
  f_mount(0, out_dev, 0);
  if (res)
    return grub_error (GRUB_ERR_WRITE_ERROR, "rm failed %d", res);
  return GRUB_ERR_NONE;
}

static FRESULT
set_timestamp (char *name, struct grub_datetime *tm)
{
    FILINFO info;
    info.fdate = (WORD)(((tm->year - 1980) * 512U) | tm->month * 32U | tm->day);
    info.ftime = (WORD)(tm->hour * 2048U | tm->minute * 32U | tm->second / 2U);
    return f_utime(name, &info);
}

static grub_err_t
grub_cmd_touch (grub_command_t cmd __attribute__ ((unused)),
                int argc, char **args)

{
  char dev[3] = "0:";
  struct grub_datetime tm = { 2020, 1, 1, 0, 0, 0};
  FATFS fs;
  FRESULT res;
  FILINFO info;
  FIL file;
  if (argc < 1)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "bad argument");

  grub_get_datetime (&tm);
  if (argc > 1)
    tm.year = grub_strtol (args[1], NULL, 10);
  if (argc > 2)
    tm.month = grub_strtol (args[2], NULL, 10);
  if (argc > 3)
    tm.day = grub_strtol (args[3], NULL, 10);
  if (argc > 4)
    tm.hour = grub_strtol (args[4], NULL, 10);
  if (argc > 5)
    tm.minute = grub_strtol (args[5], NULL, 10);
  if (argc > 6)
    tm.second = grub_strtol (args[6], NULL, 10);

  if (grub_isdigit (args[0][0]))
    dev[0] = args[0][0];

  f_mount (&fs, dev, 0);
  res = f_stat (args[0], &info);
  switch (res)
  {
    case FR_OK:
      res = set_timestamp (args[0], &tm);
      break;
    case FR_NO_FILE:
      res = f_open (&file, args[0], FA_WRITE | FA_CREATE_ALWAYS);
      if (res)
        grub_error (GRUB_ERR_WRITE_ERROR, "file create failed %d", res);
      else
      {
        f_close(&file);
        res = set_timestamp (args[0], &tm);
      }
      break;
    default:
      grub_error (GRUB_ERR_BAD_FILENAME, "stat failed %d", res);
  }
  f_mount(0, dev, 0);
  if (res)
    return grub_error (GRUB_ERR_WRITE_ERROR, "utime failed %d", res);
  return GRUB_ERR_NONE;
}

static grub_command_t cmd_mount, cmd_umount, cmd_mkdir;
static grub_command_t cmd_cp, cmd_rename, cmd_rm;
static grub_command_t cmd_mv, cmd_touch;

GRUB_MOD_INIT(fatfs)
{
  cmd_mount = grub_register_command ("mount", grub_cmd_mount,
                                      N_("status | DISK NUM"),
                                      N_("Mount FAT partition."));
  cmd_umount = grub_register_command ("umount", grub_cmd_umount,
                                      N_("NUM"),
                                      N_("Unmount FAT partition."));
  cmd_mkdir = grub_register_command ("mkdir", grub_cmd_mkdir,
                                      N_("PATH"),
                                      N_("Create new directory."));
  cmd_cp = grub_register_command ("cp", grub_cmd_cp,
                                      N_("FILE1 FILE2"),
                                      N_("Copy file."));
  cmd_rename = grub_register_command ("rename", grub_cmd_rename,
                        N_("FILE FILE_NAME"),
                        N_("Rename file/directory or move to other directory"));
  cmd_rm = grub_register_command ("rm", grub_cmd_rm,
                        N_("FILE | DIR"),
                        N_("Remove a file or empty directory."));
  cmd_mv = grub_register_command ("mv", grub_cmd_mv,
                        N_("FILE1 FILE2"),
                        N_("Move or rename file."));
  cmd_touch = grub_register_command ("touch", grub_cmd_touch,
                        N_("FILE [YEAR MONTH DAY HOUR MINUTE SECOND]"),
                        N_("Change the timestamp of a file or directory."));
}

GRUB_MOD_FINI(fatfs)
{
  grub_unregister_command (cmd_mount);
  grub_unregister_command (cmd_umount);
  grub_unregister_command (cmd_mkdir);
  grub_unregister_command (cmd_cp);
  grub_unregister_command (cmd_rename);
  grub_unregister_command (cmd_rm);
  grub_unregister_command (cmd_mv);
  grub_unregister_command (cmd_touch);
}

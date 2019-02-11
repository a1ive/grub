/* imgwrite.c - copy file to disk  */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2003,2005,2007,2008,2016,2017  Free Software Foundation, Inc.
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
#include <grub/file.h>
#include <grub/disk.h>
#include <grub/term.h>
#include <grub/misc.h>
#include <grub/extcmd.h>
#include <grub/i18n.h>

GRUB_MOD_LICENSE ("GPLv3+");

static void print_progress(grub_uint64_t cur, grub_uint64_t total)
{
   static int lastProgress = -1;
   if (!total)
      return;

   /* Division of uint64 is not available... */
   int curProgress = (grub_uint32_t)(cur * 100) / (grub_uint32_t)total;
   if (curProgress != lastProgress)
   {
      lastProgress = curProgress;
      grub_printf("\r%d%%", curProgress);
      grub_refresh();
   }
}

static grub_err_t
grub_cmd_imgwrite (grub_command_t cmd __attribute__ ((unused)), int argc, char **args)
{
  grub_disk_t disk;
  grub_file_t file;
  grub_uint8_t * buf = 0;
  grub_off_t srcSize;
  grub_off_t dstSize;
  grub_err_t err = 0;

  if (argc < 1)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "filename required");
  if (argc < 2)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "device name required");

  file = grub_file_open (args[0], GRUB_FILE_TYPE_SKIP_SIGNATURE);
  if (! file)
    return grub_errno;

  if (args[1][0] == '(' && args[1][grub_strlen (args[1]) - 1] == ')')
    {
      args[1][grub_strlen (args[1]) - 1] = 0;
      disk = grub_disk_open (args[1] + 1);
      args[1][grub_strlen (args[1])] = ')';
    }
  else
    disk = grub_disk_open (args[1]);

  if (! disk)
    {
      grub_file_close(file);
      return grub_errno;
    }

  dstSize = grub_disk_get_size (disk) << GRUB_DISK_SECTOR_BITS;

  grub_uint32_t bufSectors = 128;
  grub_ssize_t bufSize = GRUB_DISK_SECTOR_SIZE * bufSectors;
  grub_disk_addr_t sector = 0;
  srcSize = grub_file_size (file);

  if (srcSize != GRUB_FILE_SIZE_UNKNOWN && dstSize != GRUB_DISK_SIZE_UNKNOWN && srcSize > dstSize)
    {
      err = grub_error (GRUB_ERR_BAD_ARGUMENT, "source file bigger than capacity of destination");
      goto fail;
    }

  buf = grub_malloc(bufSize);
  if (!buf)
    {
      err = grub_error(GRUB_ERR_OUT_OF_MEMORY, "failed to allocate buffer");
      goto fail;
    }

  for (;;)
    {
      grub_ssize_t size = grub_file_read (file, buf, bufSize);
      if (size < 0)
        {
          err = grub_error(GRUB_ERR_FILE_READ_ERROR, "failed to read from source");
          break;
        }

      if (!size)
          break;

      if (dstSize != GRUB_DISK_SIZE_UNKNOWN && (sector << GRUB_DISK_SECTOR_BITS) + size > dstSize)
        {
          err = grub_error(GRUB_ERR_OUT_OF_RANGE, "input exceeds capacity of destination");
          break;
        }

      if (grub_disk_write (disk, sector, 0, size, buf))
        {
          err = grub_errno;
          break;
        }

      if (size < bufSize)
          break;

      sector += bufSectors;

      if (srcSize != GRUB_FILE_SIZE_UNKNOWN)
          print_progress(sector, srcSize >> GRUB_DISK_SECTOR_BITS);
      else
        {
          static grub_disk_addr_t lastMiB = -1;
          grub_disk_addr_t mib = (sector << GRUB_DISK_SECTOR_BITS) >> 20;
          if (mib != lastMiB)
            {
              lastMiB = mib;
              grub_printf("\r%lluMiB written", (unsigned long long) mib);
            }
        }
    }

 fail:
  grub_disk_close (disk);
  grub_file_close (file);
  grub_free(buf);

  grub_printf(err ?
                 "\rFAILED!                                \n":
                 "\rdone.                                  \n");

  return err;
}

static grub_command_t cmd;

GRUB_MOD_INIT(imgwrite)
{
  cmd = grub_register_command ("imgwrite", grub_cmd_imgwrite,
      N_("FILE DEVICE"),
      N_("Copies data from the source file to the device. "));
}

GRUB_MOD_FINI(imgwrite)
{
  grub_unregister_command (cmd);
}

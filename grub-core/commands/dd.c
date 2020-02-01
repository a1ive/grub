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
 */
/*
 *  BURG - Brand-new Universal loadeR from GRUB
 *  Copyright 2009 Bean Lee - All Rights Reserved
 *
 *  BURG is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 */

#include <grub/dl.h>
#include <grub/mm.h>
#include <grub/file.h>
#include <grub/misc.h>
#include <grub/partition.h>
#include <grub/fs.h>
#include <grub/extcmd.h>
#include <grub/i18n.h>

GRUB_MOD_LICENSE ("GPLv3+");

static const struct grub_arg_option options[] =
{
  {"if", 'i', 0, N_("Specify input file."), "FILE", ARG_TYPE_STRING},
  {"str", 's', 0, N_("Specify input string."), "STRING", ARG_TYPE_STRING},
  {"hex", 'x', 0, N_("Specify input hex string."), "HEX", ARG_TYPE_STRING},
  {"of", 'o', 0, N_("Specify output file."), "FILE", ARG_TYPE_STRING},
  {"bs", 'b', 0, N_("Specify block size."), "BYTES", ARG_TYPE_INT},
  {"count", 'c', 0, N_("Number of blocks to copy."), "BLOCKS", ARG_TYPE_INT},
  {"skip", 'k', 0, N_("Skip N blocks at input."), "BLOCKS", ARG_TYPE_INT},
  {"seek", 'e', 0, N_("Skip N blocks at output."), "BLOCKS", ARG_TYPE_INT},
  {0, 0, 0, 0, 0, 0}
};

enum options
{
  DD_IF,
  DD_STR,
  DD_HEX,
  DD_OF,
  DD_BS,
  DD_COUNT,
  DD_SKIP,
  DD_SEEK,
};

static grub_err_t
grub_cmd_dd (grub_extcmd_context_t ctxt, int argc __attribute__ ((unused)),
             char **args __attribute__ ((unused)))
{
  struct grub_arg_list *state = ctxt->state;
  grub_uint8_t data[GRUB_DISK_SECTOR_BITS];
  /* input */
  char *str = NULL;
  char *hexstr = NULL;
  void *in_file = NULL;
  int in_type = 0;
  grub_off_t in_size = 0;
  /* output */
  void *out_file = NULL;
  int out_type = 0;
  grub_off_t out_size = 0;
  /* block size */
  int bs = 1;
  /* skip & seek */
  grub_off_t skip = 0;
  grub_off_t seek = 0;
  /* count */
  int count = -1;

  if (state[DD_IF].set)
  {
    int namelen = grub_strlen (state[DD_IF].arg);
    /* disk */
    if ((state[DD_IF].arg[0] == '(') && (state[DD_IF].arg[namelen - 1] == ')'))
    {
      in_type = 0;
      state[DD_IF].arg[namelen - 1] = 0;
      in_file = grub_disk_open (&state[DD_IF].arg[1]);
      if (! in_file)
        return grub_error (GRUB_ERR_BAD_DEVICE, N_("failed to open %s"),
                           &state[DD_IF].arg[1]);
      in_size = grub_disk_get_size (in_file) << GRUB_DISK_SECTOR_BITS;
    }
    /* file */
    else
    {
      in_type = 1;
      in_file = grub_file_open (state[DD_IF].arg, GRUB_FILE_TYPE_HEXCAT);
      if (! in_file)
        return grub_error (GRUB_ERR_BAD_FILENAME, N_("failed to open %s"),
                           state[DD_IF].arg);
      in_size = grub_file_size (in_file);
    }
  }

  if (state[DD_STR].set)
  {
    str = state[DD_STR].arg;
    in_size = grub_strlen (str);
  }

  if (state[DD_HEX].set)
  {
    int i, size;
    str = state[DD_HEX].arg;
    size = grub_strlen (str) / 2;
    if (size == 0)
      return grub_error (GRUB_ERR_BAD_ARGUMENT, "invalid hex string");

    hexstr = grub_zalloc (size);
    if (! hexstr)
      return grub_errno;

    for (i = 0; i < size * 2; i++)
    {
      int c;
      if ((str[i] >= '0') && (str[i] <= '9'))
        c = str[i] - '0';
      else if ((str[i] >= 'A') && (str[i] <= 'F'))
        c = str[i] - 'A' + 10;
      else if ((str[i] >= 'a') && (str[i] <= 'f'))
        c = str[i] - 'a' + 10;
      else
      {
        grub_free (hexstr);
        return grub_error (GRUB_ERR_BAD_ARGUMENT, "invalid hex string");
      }
      if ((i & 1) == 0)
        c <<= 4;
      hexstr[i >> 1] |= c;
    }
    str = hexstr;
    in_size = size;
  }

  if (state[DD_OF].set)
  {
    int namelen = grub_strlen (state[DD_OF].arg);
    /* disk */
    if ((state[DD_OF].arg[0] == '(') && (state[DD_OF].arg[namelen - 1] == ')'))
    {
      out_type = 0;
      state[DD_OF].arg[namelen - 1] = 0;
      out_file = grub_disk_open (&state[DD_OF].arg[1]);
      if (! out_file)
        return grub_error (GRUB_ERR_BAD_DEVICE, N_("failed to open %s"),
                           &state[DD_OF].arg[1]);
      out_size = grub_disk_get_size (out_file) << GRUB_DISK_SECTOR_BITS;
    }
    /* file */
    else
    {
      return grub_error (GRUB_ERR_NOT_IMPLEMENTED_YET, N_("not implemented yet."));
    }
  }

  if (((! in_file) && (! str)) || (! out_file))
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "no input or output file");

  if (state[DD_BS].set)
  {
    bs = grub_strtoul (state[DD_BS].arg, 0, 0);
    if (! bs || bs > GRUB_DISK_SECTOR_BITS)
      return grub_error (GRUB_ERR_BAD_ARGUMENT, "invalid block size");
  }

  if (state[DD_COUNT].set)
  {
    count = grub_strtoul (state[DD_COUNT].arg, 0, 0);
    if (! count)
      return grub_error (GRUB_ERR_BAD_ARGUMENT, "invalid count");
  }

  if (state[DD_SKIP].set)
    skip = grub_strtoull (state[DD_SKIP].arg, 0, 0);

  if (state[DD_SEEK].set)
    seek = grub_strtoull (state[DD_SEEK].arg, 0, 0);

  count *= bs;
  skip *= bs;
  seek *= bs;

  if ((skip >= in_size) || (seek >= out_size))
  {
    grub_error (GRUB_ERR_BAD_ARGUMENT, "invalid skip/seek");
    goto fail;
  }

  if (count < 0)
    count = in_size - skip;

  if (skip + count > in_size)
  {
    grub_printf ("WARNING: skip + count > input_size");
    count = in_size - skip;
  }

  if (seek + count > out_size)
  {
    grub_printf ("WARNING: seek + count > output_size\n");
    count = out_size - seek;
  }

  while (count > 0)
  {
    grub_off_t copy_bs;
    copy_bs = (bs > count) ? count : bs;
    /* read */
    if (in_file)
    {
      if (in_type)
      {
        grub_file_seek (in_file, skip);
        grub_file_read (in_file, data, copy_bs);
      }
      else
      {
        grub_disk_read (in_file, 0, skip, copy_bs, data);
      }
      if (grub_errno)
        break;
    }
    else
    {
      grub_memcpy (data, str + skip, copy_bs);
    }
    /* write */
    if (out_type)
    {
      grub_error (GRUB_ERR_NOT_IMPLEMENTED_YET, N_("not implemented yet."));
    }
    else
    {
      grub_disk_write (out_file, 0, seek, copy_bs, data);
    }
    if (grub_errno)
      break;

    skip += copy_bs;
    seek += copy_bs;
    count -= copy_bs;
  }

fail:
  if (hexstr)
    grub_free (hexstr);
  if (in_file)
  {
    if (in_type)
      grub_file_close (in_file);
    else
      grub_disk_close (in_file);
  }
  if (out_file)
  {
    if (out_type)
      grub_file_close (out_file);
    else
      grub_disk_close (out_file);
  }
  return grub_errno;
}

static grub_extcmd_t cmd;

GRUB_MOD_INIT(dd)
{
  cmd = grub_register_extcmd ("dd", grub_cmd_dd, 0,
                              N_("[OPTIONS]"), N_("Copy data."), options);
}

GRUB_MOD_FINI(dd)
{
  grub_unregister_extcmd (cmd);
}

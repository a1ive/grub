/*
 *  BURG - Brand-new Universal loadeR from GRUB
 *  Copyright 2009 Bean Lee - All Rights Reserved
 *
 *  BURG is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  BURG is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with BURG.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <grub/dl.h>
#include <grub/mm.h>
#include <grub/file.h>
#include <grub/extcmd.h>
#include <grub/i18n.h>

static const struct grub_arg_option options[] =
  {
    {"if", 'i', 0, N_("Specify input file."), "FILE", ARG_TYPE_STRING},
    {"str", 's', 0, N_("Specify input string."), "STRING", ARG_TYPE_STRING},
    {"hex", 'x', 0, N_("Specify input hex string."), "HEX", ARG_TYPE_STRING},
    {"of", 'o', 0, N_("Specify output file."), "FILE", ARG_TYPE_STRING},
    {"bs", 'b', 0, N_("Specify block size."), "BYTES", ARG_TYPE_INT},
    {"count", 'c', 0, N_("Number of blocks to copy."), "BLOCKS", ARG_TYPE_INT},
    {"skip", 0x100, 0, N_("Skip N blocks at input."), "BLOCKS", ARG_TYPE_INT},
    {"seek", 0x101, 0, N_("Skip N blocks at output."), "BLOCKS", ARG_TYPE_INT},
    {"verify", 'v', 0, N_("Skip N blocks at output."), 0, 0},
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
    DD_VERIFY,
 };

static grub_err_t
grub_cmd_dd (grub_extcmd_t cmd, int argc __attribute__ ((unused)),
	     char **args __attribute__ ((unused)))
{
  struct grub_arg_list *state = cmd->state;
  char *input = 0;
  char *output = 0;
  char *str = 0;
  char *hexstr = 0;
  grub_size_t bs = 1;
  int copy_bs = 512;
  grub_off_t skip = 0;
  grub_off_t seek = 0;
  grub_off_t in_size = 0;
  int count = -1;
  grub_file_t in;
  grub_file_t out;
  char *buf = 0;
  char *buf2 = 0;
  int verify = (state[DD_VERIFY].set);

  if (state[DD_IF].set)
    input = state[DD_IF].arg;

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
    output = state[DD_OF].arg;

  if (((! input) && (! str)) || (! output))
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "no input or output file");

  if (state[DD_BS].set)
    {
      bs = grub_strtoul (state[DD_BS].arg, 0, 0);
      if (bs == 0)
	return grub_error (GRUB_ERR_BAD_ARGUMENT, "invalid block size");
    }

  if (state[DD_COUNT].set)
    {
      count = grub_strtoul (state[DD_COUNT].arg, 0, 0);
      if (count == 0)
	return grub_error (GRUB_ERR_BAD_ARGUMENT, "invalid count");
    }

  if (state[DD_SKIP].set)
    skip = grub_strtoull (state[DD_SKIP].arg, 0, 0);

  if (state[DD_SEEK].set)
    seek = grub_strtoull (state[DD_SEEK].arg, 0, 0);

  count *= bs;
  skip *= bs;
  seek *= bs;

  if (input)
    {
      in = grub_file_open (input);
      if (! in)
	return grub_errno;
      in_size = in->size;
      in->flags = 1;
    }
  else
    {
      in = 0;
    }

  out = grub_file_open (output);
  if (! out)
    goto quit;
  if (! verify)
    grub_blocklist_convert (out);

  if ((skip >= in->size) || (seek >= out->size))
    {
      if (verify)
	goto verify_fail;
    }

  if (in)
    in->offset = skip;
  else
    str += skip;
  out->offset = seek;

  if (count < 0)
    count = in_size - skip;

  if (skip + count > in_size)
    {
      if (verify)
	goto verify_fail;
      count = in_size - skip;
    }

  if (copy_bs < (int) bs)
    copy_bs = bs;

  buf = grub_malloc (copy_bs);
  if (! buf)
    goto quit;

  if (verify)
    {
      buf2 = grub_malloc (copy_bs);
      if (! buf2)
	goto quit;
    }

  while (count > 0)
    {
      int s1, s2;

      s1 = (copy_bs > count) ? count : copy_bs;

      if (in)
	{
	  s1 = grub_file_read (in, buf, s1);
	  if (grub_errno)
	    break;
	  if (s1 == -1)
	    {
	      grub_error (GRUB_ERR_BAD_ARGUMENT, "read fails");
	      break;
	    }
	}
      else
	{
	  grub_memcpy (buf, str, s1);
	  str += s1;
	}

      if (verify)
	{
	  s2 = grub_file_read (out, buf2, s1);
	  if ((s2 != s1) || (grub_memcmp (buf, buf2, s1)))
	    goto verify_fail;
	}
      else
	{
	  s2 = grub_blocklist_write (out, buf, s1);
	  out->offset += s2;
	}
      if (grub_errno)
	break;
      if (s2 == -1)
	{
	  grub_error (GRUB_ERR_BAD_ARGUMENT, "write fails");
	  break;
	}
      if (s1 != s2)
	break;
      count -= s1;
    }

 quit:
  grub_free (buf);
  grub_free (buf2);
  grub_free (hexstr);
  if (out)
    grub_file_close (out);
  if (in)
    grub_file_close (in);
  return grub_errno;

 verify_fail:
  grub_error (GRUB_ERR_BAD_ARGUMENT, "verify fails");
  goto quit;
}

static grub_extcmd_t cmd;

GRUB_MOD_INIT(dd)
{
  cmd =
    grub_register_extcmd ("dd", grub_cmd_dd,
			  GRUB_COMMAND_FLAG_BOTH,
			  N_("[OPTIONS]"),
			  N_("Copy data."),
			  options);
}

GRUB_MOD_FINI(dd)
{
  grub_unregister_extcmd (cmd);
}

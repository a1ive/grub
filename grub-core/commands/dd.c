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

#define BLOCKLIST_INC_STEP	8

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

struct grub_fs_block
{
  grub_disk_addr_t offset;
  unsigned long length;
};
 
static grub_err_t
grub_fs_blocklist_dd_open (grub_file_t file, const char *name)
{
  char *p = (char *) name;
  unsigned num = 0;
  unsigned i;
  grub_disk_t disk = file->device->disk;
  struct grub_fs_block *blocks;

  /* First, count the number of blocks.  */
  do
    {
      num++;
      p = grub_strchr (p, ',');
      if (p)
	p++;
    }
  while (p);

  /* Allocate a block list.  */
  blocks = grub_zalloc (sizeof (struct grub_fs_block) * (num + 1));
  if (! blocks)
    return 0;

  file->size = 0;
  p = (char *) name;
  if (! *p)
    {
      blocks[0].offset = 0;
      blocks[0].length = (disk->total_sectors == GRUB_ULONG_MAX) ?
	GRUB_ULONG_MAX : (disk->total_sectors << 9);
      file->size = blocks[0].length;
    }
  else for (i = 0; i < num; i++)
    {
      if (*p != '+')
	{
	  blocks[i].offset = grub_strtoull (p, &p, 0);
	  if (grub_errno != GRUB_ERR_NONE || *p != '+')
	    {
	      grub_error (GRUB_ERR_BAD_FILENAME,
			  "invalid file name `%s'", name);
	      goto fail;
	    }
	}

      p++;
      blocks[i].length = grub_strtoul (p, &p, 0);
      if (grub_errno != GRUB_ERR_NONE
	  || blocks[i].length == 0
	  || (*p && *p != ',' && ! grub_isspace (*p)))
	{
	  grub_error (GRUB_ERR_BAD_FILENAME,
		      "invalid file name `%s'", name);
	  goto fail;
	}

      if (disk->total_sectors < blocks[i].offset + blocks[i].length)
	{
	  grub_error (GRUB_ERR_BAD_FILENAME, "beyond the total sectors");
	  goto fail;
	}

      blocks[i].offset <<= GRUB_DISK_SECTOR_BITS;
      blocks[i].length <<= GRUB_DISK_SECTOR_BITS;
      file->size += blocks[i].length;
      p++;
    }

  file->data = blocks;

  return GRUB_ERR_NONE;

 fail:
  grub_free (blocks);
  return grub_errno;
}

static grub_err_t
grub_fs_blocklist_dd_close (grub_file_t file)
{
  grub_free (file->data);
  return grub_errno;
}

struct grub_fs grub_fs_blocklist_dd =
  {
    .name = "blocklist",
    .dir = 0,
    .open = grub_fs_blocklist_dd_open,
    //.read = grub_fs_blocklist_read,
    .close = grub_fs_blocklist_dd_close,
    .next = 0
  };

static grub_ssize_t
grub_fs_blocklist_w (grub_file_t file, char *buf, grub_size_t len)
{
  struct grub_fs_block *p;
  grub_off_t offset;
  grub_ssize_t ret = 0;

  if (len > file->size - file->offset)
    len = file->size - file->offset;

  offset = file->offset;
  for (p = file->data; p->length && len > 0; p++)
    {
      if (offset < p->length)
	{
	  grub_size_t size;

	  size = len;
	  if (offset + size > p->length)
	    size = p->length - offset;

	  if (grub_disk_write (file->device->disk, 0, p->offset + offset,
				size, buf) != GRUB_ERR_NONE)
	    return -1;

	  ret += size;
	  len -= size;
	  if (buf)
	    buf += size;
	  offset += size;
	}
      offset -= p->length;
    }

  return ret;
}

static grub_ssize_t
grub_blocklist_write (grub_file_t file, const char *buf, grub_size_t len)
{
  return (file->fs != &grub_fs_blocklist_dd) ? -1 :
    grub_fs_blocklist_w (file, (char *) buf, len);
}
 
struct read_blocklist_closure
{
  int num;
  struct grub_fs_block *blocks;
  grub_off_t total_size;
  grub_disk_addr_t part_start;
};

static void
read_blocklist (grub_disk_addr_t sector, unsigned offset,
		unsigned length, void *closure)
{
  struct read_blocklist_closure *c = closure;

  sector = ((sector - c->part_start) << GRUB_DISK_SECTOR_BITS) + offset;

  if ((c->num) &&
      (c->blocks[c->num - 1].offset + c->blocks[c->num - 1].length == sector))
    {
      c->blocks[c->num - 1].length += length;
      goto quit;
    }

  if ((c->num & (BLOCKLIST_INC_STEP - 1)) == 0)
    {
      c->blocks = grub_realloc (c->blocks, (c->num + BLOCKLIST_INC_STEP) *
				sizeof (struct grub_fs_block));
      if (! c->blocks)
	return;
    }

  c->blocks[c->num].offset = sector;
  c->blocks[c->num].length = length;
  c->num++;

 quit:
  c->total_size += length;
}

static void
grub_blocklist_convert (grub_file_t file)
{
  struct read_blocklist_closure c;

  if ((file->fs == &grub_fs_blocklist_dd) || (! file->device->disk) ||
      (! file->size))
    return;

  file->offset = 0;

  c.num = 0;
  c.blocks = 0;
  c.total_size = 0;
  c.part_start = grub_partition_get_start (file->device->disk->partition);
  file->read_hook = read_blocklist;
  file->read_hook_data = &c;
  grub_file_read (file, 0, file->size);
  file->read_hook = 0;
  if ((grub_errno) || (c.total_size != file->size))
    {
      grub_errno = 0;
      grub_free (c.blocks);
    }
  else
    {
      if (file->fs->close)
	(file->fs->close) (file);
      file->fs = &grub_fs_blocklist_dd;
      file->data = c.blocks;
    }

  file->offset = 0;
}

static grub_err_t
grub_cmd_dd (grub_extcmd_context_t ctxt, int argc __attribute__ ((unused)),
	     char **args __attribute__ ((unused)))
{
  struct grub_arg_list *state = ctxt->state;
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
      in = grub_file_open (input, GRUB_FILE_TYPE_SKIP_SIGNATURE);
      if (! in)
	return grub_errno;
      in_size = in->size;
    }
  else
    {
      in = 0;
    }

  out = grub_file_open (output, GRUB_FILE_TYPE_SKIP_SIGNATURE);
  if (! out)
    goto quit;
  if (! verify)
    grub_blocklist_convert (out);

  if ((skip >= in->size) || (seek >= out->size))
    {
      if (verify)
	goto verify_fail;
    }

  if (!in)
    str += skip;    
  else
     in->offset = skip;
    
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
      if (input)
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
  if (input)
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
			  0,
			  N_("[OPTIONS]"),
			  N_("Copy data."),
			  options);
}

GRUB_MOD_FINI(dd)
{
  grub_unregister_extcmd (cmd);
}

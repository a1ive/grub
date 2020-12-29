/*
 *  BURG - Brand-new Universal loadeR from GRUB
 *  Copyright 2010 Bean Lee - All Rights Reserved
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
#include <grub/env.h>
#include <grub/file.h>
#include <grub/loader.h>
#include <grub/command.h>
#include <grub/i18n.h>
#include <grub/video.h>
#include <grub/i386/relocator16.h>
#include <grub/machine/biosnum.h>
#include <grub/disk.h>
#include <grub/partition.h>

static grub_dl_t my_mod;

static char *relocator;
static grub_uint32_t dest;
static struct grub_relocator32_state rstate;

static int ibuf_pos, obuf_max;
static char *ibuf_ptr, *obuf_ptr, *obuf;
static grub_uint16_t ibuf_tab[16];

static void
ibuf_tab_init (void)
{
  int i;

  /* ibuf_tab[i] = (1 << (i + 1)) - 1  */
  ibuf_tab[0] = 1;
  for (i = 1; i < 16; i++)
    ibuf_tab[i] = ibuf_tab[i-1] * 2 + 1;
}

static grub_uint16_t
ibuf_read (int nbits)
{
  grub_uint16_t res;

  res = (*((grub_uint32_t *) ibuf_ptr) >> ibuf_pos) & ibuf_tab[nbits - 1];
  ibuf_pos += nbits;
  if (ibuf_pos >= 8)
    {
      ibuf_ptr += (ibuf_pos >> 3);
      ibuf_pos &= 7;
    }
  return res;
}

#define BUF_INC		8192

static grub_err_t
obuf_grow (int size)
{
  int pos;

  pos = obuf_ptr - obuf;
  if (pos + size >= obuf_max)
    {
      obuf = grub_realloc (obuf, obuf_max + BUF_INC);
      if (! obuf)
	return grub_errno;
      obuf_ptr = obuf + pos;
      obuf_max += BUF_INC;
    }
  return 0;
}

static inline void
obuf_putc (char ch)
{
  (*obuf_ptr++) = ch;
}

static inline void
obuf_copy (int ofs, int len)
{
  /* Don't use memcpy here.  */
  for (; len > 0; obuf_ptr++, len--)
    *(obuf_ptr) = *(obuf_ptr - ofs);
}

static grub_err_t
expand_block (int nsec)
{
  while (nsec > 0)
    {
      int cnt;

      cnt = 0x200;
      while (1)
	{
	  int flg, ofs, bts, bse, del, len;

	  flg = ibuf_read (2);

	  if (flg == 0)
	    ofs = ibuf_read (6);
	  else if (flg == 3)
	    {
	      if (ibuf_read (1))
		{
		  ofs = ibuf_read (12) + 0x140;
		  if (ofs == 0x113F)
		    break;
		}
	      else
		ofs = ibuf_read (8) + 0x40;
	    }
	  else
	    {
	      char ch;

	      cnt--;
	      if (cnt < 0)
		goto fail;

	      ch = ibuf_read (7);
	      if (flg & 1)
		ch |= 0x80;

	      if (obuf_grow (0))
		return grub_errno;
	      obuf_putc (ch);
	      continue;
	    }

	  if (ofs == 0)
	    goto fail;

	  bse = 2;
	  del = 0;
	  for (bts = 0; bts < 9; bts++)
	    {
	      if (ibuf_read (1))
		break;
	      bse += del + 1;
	      del = del * 2 + 1;
	    }
	  if (bts == 9)
	    goto fail;

	  len = (bts) ? bse + ibuf_read (bts) : bse;
	  cnt -= len;
	  if (cnt < 0)
	    goto fail;

	  if (obuf_grow (len - 1))
	    return grub_errno;
	  obuf_copy (ofs, len);
	}

      nsec--;
      if ((cnt) && (nsec))
	goto fail;
    }

  return 0;

 fail:
  return grub_error (GRUB_ERR_BAD_ARGUMENT, "Data corrupted");
}

static grub_err_t
expand_file (char *src)
{
  ibuf_tab_init ();

  ibuf_ptr = src;
  ibuf_pos = 0;
  obuf_ptr = obuf = 0;
  obuf_max = 0;

  if (ibuf_read (16) != 0x4d43)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "first CM signature not found");

  while (1)
    {
      int flg, len;

      flg = ibuf_read (8);
      len = ibuf_read(16);
      if (len == 0)
	{
	  int n;

	  n = (ibuf_ptr - src) & 0xF;
	  if ((n) || (ibuf_pos))
	    {
	      ibuf_ptr += 16 - n;
	      ibuf_pos = 0;
	    }

	  return (ibuf_read(16) == 0x4d43) ? grub_errno :
	    grub_error (GRUB_ERR_BAD_ARGUMENT, "second CM signature not found");
	}

      if (flg == 0)
	{
	  grub_memcpy (obuf_ptr, ibuf_ptr, len);
	  ibuf_ptr += len;
	  obuf_ptr += len;
	}
      else
	{
	  char* save_ptr;
	  int sec;

	  sec = (ibuf_read (16) + 511) >> 9;
	  save_ptr = ibuf_ptr;
	  if (ibuf_read (16) != 0x5344)
	    grub_error (GRUB_ERR_BAD_ARGUMENT, "0x5344 signature not found");
	  ibuf_read (16);
	  if (expand_block (sec))
	    return grub_errno;
	  ibuf_ptr = save_ptr + len;
	  ibuf_pos = 0;
	}
    }
}

static grub_err_t
grub_loadbin_unload (void)
{
  grub_dl_unref (my_mod);
  grub_relocator16_free (relocator);
  relocator = 0;
  return GRUB_ERR_NONE;
}

static grub_err_t
grub_loadbin_boot (void)
{
  grub_video_set_mode ("text", 0, 0);
  return grub_relocator16_boot (relocator, dest, rstate);
}

static int
get_drive_number (void)
{
  int drive, part;
  char *p;

  drive = grub_get_root_biosnumber ();
  part = -1;
  p = grub_env_get ("root");
  if (p)
    {
      p = grub_strchr (p, ',');
      if (p)
	{
	  while ((*p) && ((*p < '0') || (*p > '9')))
	    p++;
	  part = grub_strtoul (p, 0, 0) - 1;
	}
    }

  return drive | ((part & 0xff) << 8);
}

static void
read_bpb (void)
{
  grub_device_t dev;

  dev = grub_device_open (0);
  if (dev && dev->disk)
    {
      char *p = (char *) 0x7c00;

      grub_disk_read (dev->disk, 0, 0, 512, p);
      *((grub_uint32_t *) (p + 0x1c)) = (dev->disk->partition) ?
	grub_partition_get_start (dev->disk->partition) : 0;
    }

  if (dev)
    grub_device_close (dev);
}

static grub_err_t
grub_cmd_loadbin (grub_command_t cmd, int argc, char *argv[])
{
  grub_file_t file = 0;
  int size;

  grub_dl_ref (my_mod);
  if (argc == 0)
    {
      grub_error (GRUB_ERR_BAD_ARGUMENT, "no kernel specified");
      goto fail;
    }

  file = grub_file_open (argv[0]);
  if (! file)
    goto fail;

  size = grub_file_size (file);
  if (cmd->name[0] == 'm')
    {
      file->offset = 0x800;
      size -= 0x800;
    }

  if (size <= 0)
    {
      grub_error (GRUB_ERR_BAD_ARGUMENT, "invalid file");
      goto fail;
    }

  grub_relocator16_free (relocator);
  relocator = grub_relocator16_alloc (size);
  if (grub_errno)
    goto fail;

  if (grub_file_read (file, relocator, size) != size)
    {
      grub_error (GRUB_ERR_READ_ERROR, "file read fails");
      goto fail;
    }

  if (cmd->name[0] == 'n')
    {
      read_bpb ();
      dest = 0x20000;
      rstate.esp = 0x7000;
      rstate.eip = 0x20000000;
      rstate.edx = get_drive_number ();
    }
  else if (cmd->name[0] == 'f')
    {
      dest = 0x600;
      rstate.esp = 0x400;
      rstate.eip = 0x600000;
      rstate.ebx = get_drive_number ();
    }
  else if (cmd->name[0] == 'm')
    {
      if (*((grub_uint16_t *) relocator) == 0x4d43)
	{
	  int ns;

	  if (expand_file (relocator))
	    goto fail;

	  grub_relocator16_free (relocator);
	  ns = obuf_ptr - obuf;
	  relocator = grub_relocator16_alloc (ns);
	  if (! relocator)
	    goto fail;

	  grub_memcpy (relocator, obuf, ns);
	}
      dest = 0x700;
      rstate.esp = 0x400;
      rstate.eip = 0x700000;
      rstate.edx = get_drive_number ();
    }

  if (grub_errno == GRUB_ERR_NONE)
    grub_loader_set (grub_loadbin_boot, grub_loadbin_unload, 1);

 fail:
  grub_free (obuf);
  obuf = 0;

  if (file)
    grub_file_close (file);

  if (grub_errno != GRUB_ERR_NONE)
    grub_loadbin_unload ();

  return grub_errno;
}

static grub_command_t cmd_ntldr, cmd_freedos, cmd_msdos;

GRUB_MOD_INIT(loadbin)
{
  cmd_ntldr =
    grub_register_command ("ntldr", grub_cmd_loadbin,
			   0, N_("Load ntldr/grldr."));
  cmd_freedos =
    grub_register_command ("freedos", grub_cmd_loadbin,
			   0, N_("Load kernel.sys."));
  cmd_msdos =
    grub_register_command ("msdos", grub_cmd_loadbin,
			   0, N_("Load io.sys."));
  my_mod = mod;
}

GRUB_MOD_FINI(loadbin)
{
  grub_unregister_command (cmd_ntldr);
  grub_unregister_command (cmd_freedos);
  grub_unregister_command (cmd_msdos);
}

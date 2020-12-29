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
#include <grub/env.h>
#include <grub/misc.h>
#include <grub/command.h>
#include <grub/machine/biosdisk.h>
#include <grub/i18n.h>

static grub_err_t
grub_cmd_geometry (grub_command_t cmd __attribute__ ((unused)),
		   int argc __attribute__ ((unused)),
		   char **args __attribute__ ((unused)))
{
  int n;
  struct grub_biosdisk_data *p;

  grub_printf ("drive flags  spt   heads  max    sectors\n");
  n = grub_biosdisk_num;
  p = grub_biosdisk_geom;
  for (; n > 0; n--, p++)
    {
      char fg[4];
      long long total_sectors;

      fg[0] = (p->flags & GRUB_BIOSDISK_FLAG_LBA) ? 'L' : '-';
      fg[1] = (p->flags & GRUB_BIOSDISK_FLAG_CDROM) ? 'C' : '-';
      fg[2] = (p->flags & GRUB_BIOSDISK_FLAG_FB) ? 'F' : '-';
      fg[3] = 0;

      total_sectors = (p->flags & GRUB_BIOSDISK_FLAG_CDROM) ?
	0 : p->total_sectors;

      grub_printf ("%02x    %s    %-2ld    %-3ld    %-3d    %lld\n",
		   p->drive, fg, p->sectors, p->heads, p->max_sectors,
		   total_sectors);
    }

  return 0;
}

static grub_command_t cmd;

GRUB_MOD_INIT(geom)
{
  cmd = grub_register_command ("geometry", grub_cmd_geometry,
			       0, N_("Display drive geometry."));
}

GRUB_MOD_FINI(geom)
{
  grub_unregister_command (cmd);
}

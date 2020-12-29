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

#include <grub/misc.h>
#include <grub/types.h>
#include <grub/i386/relocator16.h>

GRUB_EXPORT(grub_relocator16_alloc);
GRUB_EXPORT(grub_relocator16_boot);

extern char grub_relocator16_trampoline_start[];
extern char grub_relocator16_trampoline_end[];

void *
grub_relocator16_alloc (grub_size_t size)
{
  char *p;
  int tsize;

  size = ALIGN_UP (size, 4);
  tsize = grub_relocator16_trampoline_end - grub_relocator16_trampoline_start;
  p = grub_relocator32_alloc (size + 4 + tsize);
  if (p)
    {
      *((grub_uint32_t *) p) = size;
      p += 4;
      grub_memcpy (p + size, grub_relocator16_trampoline_start, tsize);
    }

  return p;
}

grub_err_t
grub_relocator16_boot (void *relocator, grub_uint32_t dest,
		       struct grub_relocator32_state state)
{
  char *p = (char *) relocator - 4;
  grub_uint32_t size = *((grub_uint32_t *) p);
  char *trampoline = (char *) relocator + size;

  *((grub_uint32_t *) trampoline) = state.eip;
  state.eip = dest + size + 8;
  grub_memcpy (p, relocator, size);
  return grub_relocator32_boot (p, dest, state);
}

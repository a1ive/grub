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

#ifndef GRUB_RELOCATOR16_CPU_HEADER
#define GRUB_RELOCATOR16_CPU_HEADER	1

#include <grub/i386/relocator.h>

void *grub_relocator16_alloc (grub_size_t size);
grub_err_t grub_relocator16_boot (void *relocator, grub_uint32_t dest,
				  struct grub_relocator32_state state);

static inline void
grub_relocator16_free (void *relocator)
{
  if (relocator)
    grub_relocator32_free ((char *) relocator - 4);
}

#endif

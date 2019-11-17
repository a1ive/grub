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
 *
 */

#ifndef _MAPLIB_H
#define _MAPLIB_H

#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/term.h>
#include <grub/types.h>
#include <grub/gpt_partition.h>
#include <grub/efi/api.h>

#include <stdint.h>

wchar_t *wstrstr
(const wchar_t *str, const wchar_t *search_str);

grub_efi_boolean_t
guidcmp (const grub_packed_guid_t *g1, const grub_packed_guid_t *g2);
grub_packed_guid_t *
guidcpy (grub_packed_guid_t *dst, const grub_packed_guid_t *src);

void pause (void);

static inline void die (const char *fmt, ...)
{
  va_list args;
  /* Print message */
  va_start (args, fmt);
  grub_vprintf (fmt, args);
  va_end (args);
  grub_getkey ();
  grub_fatal ("Exit.\n");
}

#endif

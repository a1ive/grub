/*
 *  SSTD  -- Simple StdLib for UEFI
 *  Copyright (C) 2019  a1ive
 *
 *  SSTD is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  SSTD is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with SSTD.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _SSTDLIB_H
#define _SSTDLIB_H

#include <grub/mm.h>
#include <grub/types.h>
#include <grub/gpt_partition.h>
#include <grub/efi/api.h>

#define __unused __attribute__ (( unused ))

typedef __WCHAR_TYPE__ wchar_t;
typedef __WINT_TYPE__ wint_t;

#define L( x ) _L ( x )
#define _L( x ) L ## x

wchar_t *wstrstr
(const wchar_t *str, const wchar_t *search_str);

grub_efi_boolean_t
guidcmp (const grub_packed_guid_t *g1, const grub_packed_guid_t *g2);
grub_packed_guid_t *
guidcpy (grub_packed_guid_t *dst, const grub_packed_guid_t *src);

void pause (void);

#endif

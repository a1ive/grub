/*
 *  UEFI example
 *  Copyright (C) 2019  a1ive
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <efi.h>
#include <efilib.h>
#include <grub.h>
#include <sstdlib.h>
#include <private.h>

static UINT64
write_unaligned64 (UINT64 *buf, UINT64 val)
{
  return *buf = val;
}

static UINT64
read_unaligned64 (CONST UINT64 *buf)
{
  return *buf;
}

CHAR16 *wstrstr
(CONST CHAR16 *str, CONST CHAR16 *search_str)
{
  CONST CHAR16 *first_match;
  CONST CHAR16 *search_str_tmp;
  if (*search_str == L'\0')
    return (CHAR16 *) str;
  while (*str != L'\0')
  {
    search_str_tmp = search_str;
    first_match = str;
    while ((*str == *search_str_tmp) && (*str != L'\0'))
    {
      str++;
      search_str_tmp++;
    }
    if (*search_str_tmp == L'\0')
      return (CHAR16 *) first_match;
    if (*str == L'\0')
      return NULL;
    str = first_match + 1;
  }
  return NULL;
}

EFI_GUID *
guidcpy (EFI_GUID *dest, CONST EFI_GUID *src)
{
  write_unaligned64 ((UINT64*)dest, read_unaligned64 ((CONST UINT64*)src));
  write_unaligned64 ((UINT64*)dest + 1, read_unaligned64 ((CONST UINT64*)src + 1));
  return dest;
}

EFI_DEVICE_PATH_PROTOCOL *
create_device_node (UINT8 node_type, UINT8 node_subtype, UINT16 node_len)
{
  EFI_DEVICE_PATH_PROTOCOL *dp;
  if (node_len < sizeof (EFI_DEVICE_PATH_PROTOCOL))
    return NULL;
  dp = AllocateZeroPool (node_len);
  if (dp != NULL)
  {
     dp->Type    = node_type;
     dp->SubType = node_subtype;
     SetDevicePathNodeLength (dp, node_len);
  }
  return dp;
}

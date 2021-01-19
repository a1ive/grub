/*
 *  Extention for GRUB  --  GRand Unified Bootloader
 *  Copyright (C) Miray Software 2010-2014
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

#include <grub/miray_lib.h>

// Remove the last section of a device path
// We use when booting from cd to get the cd device from the boot entry
// WARNING: modifies supplied path
grub_efi_device_path_t * miray_efi_chomp_device_path(grub_efi_device_path_t *dp)
{
   grub_efi_device_path_t * cur = dp;

   if (GRUB_EFI_END_ENTIRE_DEVICE_PATH (cur))
      return 0;

   while (1)
   {
      grub_efi_device_path_t * tmp = GRUB_EFI_NEXT_DEVICE_PATH(cur);

      if (GRUB_EFI_END_ENTIRE_DEVICE_PATH(tmp))
      {
         cur->type      = (cur->type & 0x80) | 0x7f;
         cur->subtype   = GRUB_EFI_END_ENTIRE_DEVICE_PATH_SUBTYPE;
         cur->length    = 4;

         return dp;
      }
      cur = tmp;
   }
}

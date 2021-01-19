/* miray_lib.h -- general miray helper functions */
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
 *
 */

#ifndef miray_lib_h
#define miray_lib_h

#if defined(GRUB_MACHINE_EFI)
#include <grub/efi/api.h>

grub_efi_device_path_t * miray_efi_chomp_device_path(grub_efi_device_path_t *dp);
#endif

#endif

/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2020  Free Software Foundation, Inc.
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

#ifndef GRUB_MULTIBOOT_KERNEL_HEADER
#define GRUB_MULTIBOOT_KERNEL_HEADER 1

#include <multiboot.h>
#include <grub/types.h>
#include <grub/dl.h>
#include <grub/i386/coreboot/kernel.h>

#define CHECK_FLAG(flags, bit) ((flags) & (1 << (bit)))

extern struct multiboot_info *EXPORT_VAR(grub_multiboot_info);

static inline grub_uint32_t grub_mb_check_bios_int (grub_uint8_t intno)
{
  grub_uint32_t *ivt = 0x00;
  return ivt[intno];
}

#endif

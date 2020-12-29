/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2002,2005,2006,2007  Free Software Foundation, Inc.
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

#ifndef GRUB_EFI_CONSOLE_HEADER
#define GRUB_EFI_CONSOLE_HEADER	1

#include <grub/types.h>
#include <grub/symbol.h>

#define GRUB_EFI_SCAN_NULL	0
#define GRUB_EFI_SCAN_UP	1
#define GRUB_EFI_SCAN_DOWN	2
#define GRUB_EFI_SCAN_RIGHT	3
#define GRUB_EFI_SCAN_LEFT	4
#define GRUB_EFI_SCAN_HOME	5
#define GRUB_EFI_SCAN_END	6
#define GRUB_EFI_SCAN_INSERT	7
#define GRUB_EFI_SCAN_DELETE	8
#define GRUB_EFI_SCAN_PAGE_UP	9
#define GRUB_EFI_SCAN_PAGE_DOWN	0xa
#define GRUB_EFI_SCAN_F1	0xb
#define GRUB_EFI_SCAN_F2	0xc
#define GRUB_EFI_SCAN_F3	0xd
#define GRUB_EFI_SCAN_F4	0xe
#define GRUB_EFI_SCAN_F5	0xf
#define GRUB_EFI_SCAN_F6	0x10
#define GRUB_EFI_SCAN_F7	0x11
#define GRUB_EFI_SCAN_F8	0x12
#define GRUB_EFI_SCAN_F9	0x13
#define GRUB_EFI_SCAN_F10	0x14
#define GRUB_EFI_SCAN_ESC	0x17

/* Initialize the console system.  */
void grub_console_init (void);

/* Finish the console system.  */
void grub_console_fini (void);

#endif /* ! GRUB_EFI_CONSOLE_HEADER */

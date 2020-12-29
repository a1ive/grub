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

#ifndef GRUB_EFI_LOAD_FILE_HEADER
#define GRUB_EFI_LOAD_FILE_HEADER 1

#define GRUB_EFI_LOAD_FILE_GUID \
  {0x56ec3091, 0x954c, 0x11d2, {0x8e, 0x3f, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}

struct grub_efi_load_file_protocol
{
  grub_efi_status_t
  (*load_file) (struct grub_efi_load_file_protocol *this,
		grub_efi_device_path_t *device_path,
		grub_uint8_t boot_policy,
		grub_efi_uintn_t *buffer_size,
		void *buffer);
};
typedef struct grub_efi_load_file_protocol grub_efi_load_file_protocol_t;

#endif /* GRUB_EFI_LOAD_FILE_HEADER */

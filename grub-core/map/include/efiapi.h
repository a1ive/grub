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

#ifndef _EFIAPI_H
#define _EFIAPI_H

#include <grub/efi/api.h>

#define EFI_REMOVABLE_MEDIA_FILE_NAME_IA32    "/EFI/BOOT/BOOTIA32.EFI"
#define EFI_REMOVABLE_MEDIA_FILE_NAME_X64     "/EFI/BOOT/BOOTX64.EFI"
#define EFI_REMOVABLE_MEDIA_FILE_NAME_ARM     "/EFI/BOOT/BOOTARM.EFI"
#define EFI_REMOVABLE_MEDIA_FILE_NAME_AARCH64 "/EFI/BOOT/BOOTAA64.EFI"

#if defined (__i386__)
  #define EFI_REMOVABLE_MEDIA_FILE_NAME   EFI_REMOVABLE_MEDIA_FILE_NAME_IA32
#elif defined (__x86_64__)
  #define EFI_REMOVABLE_MEDIA_FILE_NAME   EFI_REMOVABLE_MEDIA_FILE_NAME_X64
#elif defined (__arm__)
  #define EFI_REMOVABLE_MEDIA_FILE_NAME   EFI_REMOVABLE_MEDIA_FILE_NAME_ARM
#elif defined (__aarch64__)
  #define EFI_REMOVABLE_MEDIA_FILE_NAME   EFI_REMOVABLE_MEDIA_FILE_NAME_AARCH64
#else
  #error Unknown Processor Type
#endif

#define EFI_REMOVABLE_MEDIA_FILE_NAME_IA32    "/EFI/BOOT/BOOTIA32.EFI"
#define EFI_REMOVABLE_MEDIA_FILE_NAME_X64     "/EFI/BOOT/BOOTX64.EFI"
#define EFI_REMOVABLE_MEDIA_FILE_NAME_ARM     "/EFI/BOOT/BOOTARM.EFI"
#define EFI_REMOVABLE_MEDIA_FILE_NAME_AARCH64 "/EFI/BOOT/BOOTAA64.EFI"

#if defined (__i386__)
  #define EFI_REMOVABLE_MEDIA_FILE_NAME   EFI_REMOVABLE_MEDIA_FILE_NAME_IA32
#elif defined (__x86_64__)
  #define EFI_REMOVABLE_MEDIA_FILE_NAME   EFI_REMOVABLE_MEDIA_FILE_NAME_X64
#elif defined (__arm__)
  #define EFI_REMOVABLE_MEDIA_FILE_NAME   EFI_REMOVABLE_MEDIA_FILE_NAME_ARM
#elif defined (__aarch64__)
  #define EFI_REMOVABLE_MEDIA_FILE_NAME   EFI_REMOVABLE_MEDIA_FILE_NAME_AARCH64
#else
  #error Unknown Processor Type
#endif

#if defined (__x86_64__)
#if (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7)))||(defined(__clang__) && (__clang_major__ > 3 || (__clang_major__ == 3 && __clang_minor__ >= 2)))
  #define EFIAPI __attribute__((ms_abi))
#else
  #error Compiler is too old for GNU_EFI_USE_MS_ABI
#endif
#endif

#ifndef EFIAPI
  #define EFIAPI  // Substitute expresion to force C calling convention 
#endif

struct block_io_protocol
{
  grub_efi_uint64_t revision;
  grub_efi_block_io_media_t *media;
  grub_efi_status_t (EFIAPI *reset) (struct block_io_protocol *this,
			      grub_efi_boolean_t extended_verification);
  grub_efi_status_t (EFIAPI *read_blocks) (struct block_io_protocol *this,
				    grub_efi_uint32_t media_id,
				    grub_efi_lba_t lba,
				    grub_efi_uintn_t buffer_size,
				    void *buffer);
  grub_efi_status_t (EFIAPI *write_blocks) (struct block_io_protocol *this,
				     grub_efi_uint32_t media_id,
				     grub_efi_lba_t lba,
				     grub_efi_uintn_t buffer_size,
				     void *buffer);
  grub_efi_status_t (EFIAPI *flush_blocks) (struct block_io_protocol *this);
};
typedef struct block_io_protocol block_io_protocol_t;

#endif /* _STDINT_H */

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

#ifndef MAP_PRIVATE_DATA_H
#define MAP_PRIVATE_DATA_H

#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#include <grub/disk.h>
#include <grub/file.h>

#include <sstdlib.h>

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

#define CD_BOOT_SECTOR 17
#define CD_BLOCK_SIZE 2048
#define CD_SHIFT 11

#define FD_BLOCK_SIZE 512 /* 0x200 */
#define FD_SHIFT 9
#define BLOCK_OF_1_44MB 0xB40

#define VDISK_BLOCKIO_TO_PARENT(a) CR(a, vdisk_t, block_io)

#define MAX_FILE_NAME_STRING_SIZE 255
#define MBR_START_LBA 0
#define PRIMARY_PART_HEADER_LBA 1
#define VDISK_MEDIA_ID 0x1

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

static const grub_packed_guid_t VDISK_GUID =
{ 0xebe35ad8, 0x6c1e, 0x40f5,
  { 0xaa, 0xed, 0x0b, 0x91, 0x9a, 0x46, 0xbf, 0x4b }
};

enum disk_type
{
  HD,
  CD,
  FD,
  MBR,
  GPT,
};

struct map_private_data
{
  grub_efi_boolean_t mem;
  grub_efi_boolean_t pause;
  enum disk_type type;
  grub_efi_boolean_t disk;
  void *file;
};

typedef struct
{
  grub_efi_boolean_t present;
  grub_efi_handle_t handle;
  grub_efi_device_path_t *dp;
  grub_efi_boolean_t disk;
  void *file;
  grub_efi_boolean_t mem;
  grub_efi_uintn_t addr;
  grub_efi_uint64_t size;
  grub_efi_uint32_t bs;
  enum disk_type type;
  block_io_protocol_t block_io;
  grub_efi_block_io_media_t media;
} vdisk_t;

/* main */
extern struct map_private_data *cmd;
extern vdisk_t vdisk;
extern vdisk_t vpart;
void read (grub_efi_boolean_t disk, void *file,
           void *buf, grub_efi_uintn_t len, grub_efi_uint64_t offset);
grub_efi_uint64_t get_size (grub_efi_boolean_t disk, void *file);
/* vblock */
extern block_io_protocol_t blockio_template;
grub_efi_status_t EFIAPI
blockio_reset (block_io_protocol_t *this __unused,
               grub_efi_boolean_t extended __unused);
grub_efi_status_t EFIAPI
blockio_read (block_io_protocol_t *this, grub_efi_uint32_t media_id,
              grub_efi_lba_t lba, grub_efi_uintn_t len, void *buf);
grub_efi_status_t EFIAPI
blockio_write (block_io_protocol_t *this __unused,
               grub_efi_uint32_t media_id __unused,
               grub_efi_lba_t lba __unused,
               grub_efi_uintn_t len __unused, void *buf __unused);
grub_efi_status_t EFIAPI
blockio_flush (block_io_protocol_t *this __unused);
/* vboot */
grub_efi_handle_t vpart_boot (grub_efi_handle_t *part_handle);
grub_efi_handle_t vdisk_boot (void);
/* vdisk */
grub_efi_status_t vdisk_install (grub_file_t file);
/* vpart */
grub_efi_status_t vpart_install (void);

#endif

/*
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

#ifndef MAP_PRIVATE_DATA_H
#define MAP_PRIVATE_DATA_H

#include <efi.h>
#include <efilib.h>

#include <sstdlib.h>
#include <eltorito.h>
#include <mbr.h>

#define EFI_REMOVABLE_MEDIA_FILE_NAME_IA32    L"\\EFI\\BOOT\\BOOTIA32.EFI"
#define EFI_REMOVABLE_MEDIA_FILE_NAME_X64     L"\\EFI\\BOOT\\BOOTX64.EFI"
#define EFI_REMOVABLE_MEDIA_FILE_NAME_ARM     L"\\EFI\\BOOT\\BOOTARM.EFI"
#define EFI_REMOVABLE_MEDIA_FILE_NAME_AARCH64 L"\\EFI\\BOOT\\BOOTAA64.EFI"

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

#define CD_BOOT_SECTOR 17
#define CD_BLOCK_SIZE 2048

#define FD_BLOCK_SIZE 512
#define BLOCK_OF_1_44MB 0xB40

#define VDISK_BLOCKIO_TO_PARENT(a) STD_CR(a, vdisk_t, block_io)

#define MAX_FILE_NAME_STRING_SIZE 255
#define MBR_START_LBA 0
#define VDISK_MEDIA_ID 0x1

static const EFI_GUID VDISK_GUID =
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

struct grub_private_data
{
  BOOLEAN mem;
  BOOLEAN pause;
  enum disk_type type;
  BOOLEAN disk;
  VOID *file;
};

typedef struct
{
  BOOLEAN present;
  EFI_HANDLE handle;
  EFI_DEVICE_PATH *dp;
  BOOLEAN disk;
  VOID *file;
  BOOLEAN mem;
  UINTN addr;
  UINT64 size;
  UINT32 bs;
  enum disk_type type;
  EFI_BLOCK_IO_PROTOCOL block_io;
  //EFI_BLOCK_IO2_PROTOCOL block_io2;
  EFI_BLOCK_IO_MEDIA media;
} vdisk_t;

/* main */
extern struct grub_private_data *cmd;
extern grub_efi_grub_protocol_t *grub;
extern EFI_HANDLE efi_image_handle;
extern vdisk_t vdisk;
extern vdisk_t vpart;
void pause (void);
VOID read (BOOLEAN disk, VOID **file, VOID *buf, UINTN len, UINT64 offset);
UINT64 get_size (BOOLEAN disk, VOID *file);
/* vblock */
extern EFI_BLOCK_IO_PROTOCOL blockio_template;
EFI_STATUS EFIAPI
blockio_reset (EFI_BLOCK_IO_PROTOCOL *this __unused, BOOLEAN extended __unused);
EFI_STATUS EFIAPI
blockio_read (EFI_BLOCK_IO_PROTOCOL *this, UINT32 media_id,
              EFI_LBA lba, UINTN len, VOID *buf);
EFI_STATUS EFIAPI
blockio_write (EFI_BLOCK_IO_PROTOCOL *this __unused, UINT32 media_id __unused,
              EFI_LBA lba __unused, UINTN len __unused, VOID *buf __unused);
EFI_STATUS EFIAPI
blockio_flush (EFI_BLOCK_IO_PROTOCOL *this __unused);
/* vboot */
EFI_HANDLE
vpart_boot (EFI_HANDLE *part_handle);
EFI_HANDLE
vdisk_boot (void);
/* vdisk */
EFI_STATUS
vdisk_install (grub_file_t file);
/* vpart */
EFI_STATUS
vpart_install (void);
/* dputil */
CHAR16 *wstrstr (CONST CHAR16 *str, CONST CHAR16 *search_str);
EFI_GUID *guidcpy (EFI_GUID *dest, CONST EFI_GUID *src);
EFI_DEVICE_PATH_PROTOCOL *
create_device_node (UINT8 node_type, UINT8 node_subtype, UINT16 node_len);
#endif

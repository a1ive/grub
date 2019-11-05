/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2019  Free Software Foundation, Inc.
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

#ifndef EFI_GRUB_PROTOCOL_HEADER
#define EFI_GRUB_PROTOCOL_HEADER

#include <efi.h>

#define GRUB_EFI_GRUB_PROTOCOL_GUID \
  { 0x437b7116, 0x5053, 0x48ca, \
      { 0xb5, 0x89, 0x63, 0xbe, 0x2f, 0x19, 0x1c, 0x71 } \
  }

enum grub_file_type
{
  GRUB_FILE_TYPE_NONE = 0,
  GRUB_FILE_TYPE_GRUB_MODULE,
  GRUB_FILE_TYPE_LOOPBACK,
  GRUB_FILE_TYPE_LINUX_KERNEL,
  GRUB_FILE_TYPE_LINUX_INITRD,
  GRUB_FILE_TYPE_MULTIBOOT_KERNEL,
  GRUB_FILE_TYPE_MULTIBOOT_MODULE,
  GRUB_FILE_TYPE_XEN_HYPERVISOR,
  GRUB_FILE_TYPE_XEN_MODULE,
  GRUB_FILE_TYPE_BSD_KERNEL,
  GRUB_FILE_TYPE_FREEBSD_ENV,
  GRUB_FILE_TYPE_FREEBSD_MODULE,
  GRUB_FILE_TYPE_FREEBSD_MODULE_ELF,
  GRUB_FILE_TYPE_NETBSD_MODULE,
  GRUB_FILE_TYPE_OPENBSD_RAMDISK,
  GRUB_FILE_TYPE_XNU_INFO_PLIST,
  GRUB_FILE_TYPE_XNU_MKEXT,
  GRUB_FILE_TYPE_XNU_KEXT,
  GRUB_FILE_TYPE_XNU_KERNEL,
  GRUB_FILE_TYPE_XNU_RAMDISK,
  GRUB_FILE_TYPE_XNU_HIBERNATE_IMAGE,
  GRUB_FILE_XNU_DEVPROP,
  GRUB_FILE_TYPE_PLAN9_KERNEL,
  GRUB_FILE_TYPE_NTLDR,
  GRUB_FILE_TYPE_TRUECRYPT,
  GRUB_FILE_TYPE_FREEDOS,
  GRUB_FILE_TYPE_PXECHAINLOADER,
  GRUB_FILE_TYPE_PCCHAINLOADER,
  GRUB_FILE_TYPE_COREBOOT_CHAINLOADER,
  GRUB_FILE_TYPE_EFI_CHAINLOADED_IMAGE,
  GRUB_FILE_TYPE_SIGNATURE,
  GRUB_FILE_TYPE_PUBLIC_KEY,
  GRUB_FILE_TYPE_PUBLIC_KEY_TRUST,
  GRUB_FILE_TYPE_PRINT_BLOCKLIST,
  GRUB_FILE_TYPE_TESTLOAD,
  GRUB_FILE_TYPE_GET_SIZE,
  GRUB_FILE_TYPE_FONT,
  GRUB_FILE_TYPE_ZFS_ENCRYPTION_KEY,
  GRUB_FILE_TYPE_FSTEST,
  GRUB_FILE_TYPE_MOUNT,
  GRUB_FILE_TYPE_FILE_ID,
  GRUB_FILE_TYPE_ACPI_TABLE,
  GRUB_FILE_TYPE_DEVICE_TREE_IMAGE,
  GRUB_FILE_TYPE_CAT,
  GRUB_FILE_TYPE_HEXCAT,
  GRUB_FILE_TYPE_CMP,
  GRUB_FILE_TYPE_HASHLIST,
  GRUB_FILE_TYPE_TO_HASH,
  GRUB_FILE_TYPE_KEYBOARD_LAYOUT,
  GRUB_FILE_TYPE_PIXMAP,
  GRUB_FILE_TYPE_GRUB_MODULE_LIST,
  GRUB_FILE_TYPE_CONFIG,
  GRUB_FILE_TYPE_THEME,
  GRUB_FILE_TYPE_GETTEXT_CATALOG,
  GRUB_FILE_TYPE_FS_SEARCH,
  GRUB_FILE_TYPE_AUDIO,
  GRUB_FILE_TYPE_VBE_DUMP,
  GRUB_FILE_TYPE_LOADENV,
  GRUB_FILE_TYPE_SAVEENV,
  GRUB_FILE_TYPE_VERIFY_SIGNATURE,
  GRUB_FILE_TYPE_MASK = 0xffff,
  GRUB_FILE_TYPE_SKIP_SIGNATURE = 0x10000,
  GRUB_FILE_TYPE_NO_DECOMPRESS = 0x20000
};

typedef VOID (*grub_disk_read_hook_t) (UINT64 sector, UINT32 offset,
                                       UINT32 length, VOID *data);

#define GRUB_DISK_SECTOR_SIZE 0x200
#define GRUB_DISK_SECTOR_BITS 9

struct grub_disk
{
  /* The disk name.  */
  const char *name;
  /* The underlying disk device.  */
  VOID *dev;
  /* The total number of sectors.  */
  UINT64 total_sectors;
  /* Logarithm of sector size.  */
  UINT32 log_sector_size;
  /* Maximum number of sectors read divided by GRUB_DISK_CACHE_SIZE.  */
  UINT32 max_agglomerate;
  /* The id used by the disk cache manager.  */
  UINTN id;
  /* The partition information. This is machine-specific.  */
  VOID *partition;
  /* Called when a sector was read. OFFSET is between 0 and
     the sector size minus 1, and LENGTH is between 0 and the sector size.  */
  grub_disk_read_hook_t read_hook;
  /* Caller-specific data passed to the read hook.  */
  VOID *read_hook_data;
  /* Device-specific data.  */
  VOID *data;
};
typedef struct grub_disk *grub_disk_t;

struct grub_file
{
  /* File name.  */
  char *name;
  /* The underlying device.  */
  VOID *device;
  /* The underlying filesystem.  */
  VOID *fs;
  /* The current offset.  */
  UINT64 offset;
  UINT64 progress_offset;
  /* Progress info. */
  UINT64 last_progress_time;
  UINT64 last_progress_offset;
  UINT64 estimated_speed;
  /* The file size.  */
  UINT64 size;
  /* If file is not easily seekable. Should be set by underlying layer.  */
  INT32 not_easily_seekable;
  /* Filesystem-specific data.  */
  VOID *data;
  /* This is called when a sector is read. Used only for a disk device.  */
  grub_disk_read_hook_t read_hook;
  /* Caller-specific data passed to the read hook.  */
  VOID *read_hook_data;
};
typedef struct grub_file *grub_file_t;

struct grub_efi_grub_protocol
{
  /* file */
  EFI_STATUS (*file_open) (grub_file_t *file,
                                  CONST char *name,
                                  enum grub_file_type type);
  EFI_STATUS (*file_open_w) (grub_file_t *file,
                                    CONST CHAR16 *name,
                                    enum grub_file_type type);
  INTN (*file_read) (grub_file_t *file,
                                VOID *buf,
                                UINTN len);
  UINT64 (*file_seek) (grub_file_t *file,
                                  UINT64 offset);
  EFI_STATUS (*file_close) (grub_file_t *file);
  UINT64 (*file_size) (CONST grub_file_t file);
  UINT64 (*file_tell) (CONST grub_file_t file);
  /* env */
  EFI_STATUS (*env_set) (CONST char *name, CONST char *val);
  CONST char * (*env_get) (CONST char *name);
  VOID (*env_unset) (CONST char *name);
  /* efi */
  BOOLEAN (*secure_boot) (VOID);
  INT32 (*compare_dp) (CONST EFI_DEVICE_PATH *dp1,
                       CONST EFI_DEVICE_PATH *dp2);
  /* command */
  EFI_STATUS (*execute) (CONST char *name, INT32 argc, char **argv);
  /* term */
  UINT32 (*wait_key) (VOID);
  UINT32 (*get_key) (VOID);
  /* test */
  VOID (*test) (VOID);
  /* private data */
  UINTN (*private_data) (VOID **addr);
  /* disk */
  EFI_STATUS (*disk_open) (grub_disk_t *disk, const char *name);
  VOID (*disk_close) (grub_disk_t *disk);
  EFI_STATUS (*disk_read) (grub_disk_t *disk, UINT64 sector,
                           UINT64 offset, UINTN size, VOID *buf);
  UINT64 (*disk_size) (grub_disk_t disk);
};
typedef struct grub_efi_grub_protocol grub_efi_grub_protocol_t;

#endif

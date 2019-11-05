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

#include <grub/types.h>
#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#include <grub/misc.h>
#include <grub/file.h>
#include <grub/disk.h>
#include <grub/env.h>
#include <grub/command.h>

#define GRUB_EFI_GRUB_PROTOCOL_GUID \
  { 0x437b7116, 0x5053, 0x48ca, \
      { 0xb5, 0x89, 0x63, 0xbe, 0x2f, 0x19, 0x1c, 0x71 } \
  }

struct grub_efi_grub_protocol
{
  /* file */
  grub_efi_status_t (*file_open) (grub_file_t *file,
                                  const char *name,
                                  enum grub_file_type type);
  grub_efi_status_t (*file_open_w) (grub_file_t *file,
                                    const grub_efi_char16_t *name,
                                    enum grub_file_type type);
  grub_efi_intn_t (*file_read) (grub_file_t *file,
                                void *buf,
                                grub_efi_uintn_t len);
  grub_efi_uint64_t (*file_seek) (grub_file_t *file,
                                  grub_efi_uint64_t offset);
  grub_efi_status_t (*file_close) (grub_file_t *file);
  grub_efi_uint64_t (*file_size) (const grub_file_t file);
  grub_efi_uint64_t (*file_tell) (const grub_file_t file);
  /* env */
  grub_efi_status_t (*env_set) (const char *name, const char *val);
  const char * (*env_get) (const char *name);
  void (*env_unset) (const char *name);
  /* efi */
  grub_efi_boolean_t (*secure_boot) (void);
  grub_efi_int32_t (*compare_dp) (const grub_efi_device_path_t *dp1,
                                  const grub_efi_device_path_t *dp2);
  /* command */
  grub_efi_status_t (*execute) (const char *name, int argc, char **argv);
  /* term */
  grub_efi_uint32_t (*wait_key) (void);
  grub_efi_uint32_t (*get_key) (void);
  /* test */
  void (*test) (void);
  /* private data */
  grub_efi_uintn_t (*private_data) (void **addr);
  /* disk */
  grub_efi_status_t (*disk_open) (grub_disk_t *disk, const char *name);
  void (*disk_close) (grub_disk_t *disk);
  grub_efi_status_t (*disk_read) (grub_disk_t *disk, grub_efi_uint64_t sector,
                                  grub_efi_uint64_t offset, grub_efi_uintn_t size,
                                  void *buf);
  grub_efi_uint64_t (*disk_size) (grub_disk_t disk);
};
typedef struct grub_efi_grub_protocol grub_efi_grub_protocol_t;

void
grub_prot_init (void);

void
grub_prot_fini (void);

#endif

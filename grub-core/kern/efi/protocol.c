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

#include <grub/dl.h>
#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#include <grub/efi/protocol.h>
#include <grub/charset.h>
#include <grub/command.h>
#include <grub/device.h>
#include <grub/err.h>
#include <grub/file.h>
#include <grub/i18n.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/types.h>
#include <grub/term.h>

GRUB_MOD_LICENSE ("GPLv3+");

static grub_size_t
wcslen (const grub_efi_char16_t *s16)
{
  grub_size_t len = 0;
  while (*(s16++))
    len++;
  return len;
}

static grub_efi_status_t
prot_file_open (grub_file_t *file, const char *name, enum grub_file_type type)
{
  *file = grub_file_open (name, type);
  if (!*file)
    return GRUB_EFI_NOT_FOUND;
  return GRUB_EFI_SUCCESS;
}

static grub_efi_status_t
prot_file_open_w (grub_file_t *file, const grub_efi_char16_t *name,
                  enum grub_file_type type)
{
  grub_size_t s16_len = 0;
  unsigned char *file_name = NULL;

  s16_len = wcslen (name) + 1;
  file_name = grub_malloc (s16_len);
  if (!file_name)
    return GRUB_EFI_OUT_OF_RESOURCES;
  grub_utf16_to_utf8 (file_name, name, s16_len);
  *file = grub_file_open ((char *)file_name, type);
  grub_free (file_name);
  if (!*file)
    return GRUB_EFI_NOT_FOUND;
  return GRUB_EFI_SUCCESS;
}

static grub_efi_intn_t
prot_file_read (grub_file_t *file, void *buf, grub_efi_uintn_t len)
{
  return grub_file_read (*file, buf, len);
}

static grub_efi_uint64_t
prot_file_seek (grub_file_t *file, grub_efi_uint64_t offset)
{
  return grub_file_seek (*file, offset);
}

static grub_efi_status_t
prot_file_close (grub_file_t *file)
{
  grub_err_t err;
  err = grub_file_close (*file);
  if (err)
    return GRUB_EFI_LOAD_ERROR;
  return GRUB_EFI_SUCCESS;
}

static grub_efi_uint64_t 
prot_file_size (const grub_file_t file)
{
  return file->size;
}

static grub_efi_uint64_t 
prot_file_tell (const grub_file_t file)
{
  return file->offset;
}

static grub_efi_status_t
prot_env_set (const char *name, const char *val)
{
  grub_err_t err;
  err = grub_env_set (name, val);
  if (err)
    return GRUB_EFI_LOAD_ERROR;
  return GRUB_EFI_SUCCESS;
}

static const char *
prot_env_get (const char *name)
{
  return grub_env_get (name);
}

static void
prot_env_unset (const char *name)
{
  grub_env_unset (name);
}

static grub_efi_boolean_t
prot_secure_boot (void)
{
  return grub_efi_secure_boot ();
}

static grub_efi_int32_t
prot_compare_dp (const grub_efi_device_path_t *dp1,
                 const grub_efi_device_path_t *dp2)
{
  return grub_efi_compare_device_paths (dp1, dp2);
}

static grub_efi_status_t
prot_execute (const char *name, int argc, char **argv)
{
  grub_err_t err;
  err = grub_command_execute (name, argc, argv);
  if (err)
    return GRUB_EFI_LOAD_ERROR;
  return GRUB_EFI_SUCCESS;
}

static grub_efi_uint32_t
prot_wait_key (void)
{
  return grub_getkey ();
}

static grub_efi_uint32_t
prot_get_key (void)
{
  return grub_getkey_noblock ();
}

static void
prot_test (void)
{
  grub_printf ("UEFI GRUB PROTOCOL\n");
}

static grub_efi_uintn_t
prot_private_data (void **addr)
{
  *addr = grub_efi_protocol_data_addr;
  if (!*addr)
    return 0;
  return grub_efi_protocol_data_len;
}

static grub_efi_grub_protocol_t grub_prot;
static grub_efi_guid_t grub_prot_guid = GRUB_EFI_GRUB_PROTOCOL_GUID;

void
grub_prot_init (void)
{
  grub_efi_protocol_data_addr = NULL;
  grub_efi_protocol_data_len = 0;

  grub_prot.file_open = prot_file_open;
  grub_prot.file_open_w = prot_file_open_w;
  grub_prot.file_read = prot_file_read;
  grub_prot.file_seek = prot_file_seek;
  grub_prot.file_close = prot_file_close;
  grub_prot.file_size = prot_file_size;
  grub_prot.file_tell = prot_file_tell;
  grub_prot.env_set = prot_env_set;
  grub_prot.env_get = prot_env_get;
  grub_prot.env_unset = prot_env_unset;
  grub_prot.secure_boot = prot_secure_boot;
  grub_prot.compare_dp = prot_compare_dp;
  grub_prot.execute = prot_execute;
  grub_prot.wait_key = prot_wait_key;
  grub_prot.get_key = prot_get_key;
  grub_prot.test = prot_test;
  grub_prot.private_data = prot_private_data;
  /* install */
  grub_efi_boot_services_t *b;

  b = grub_efi_system_table->boot_services;
  efi_call_4 (b->install_protocol_interface,
              &grub_efi_image_handle, &grub_prot_guid,
              GRUB_EFI_NATIVE_INTERFACE, &grub_prot);
}

void
grub_prot_fini (void)
{
  grub_efi_boot_services_t *b;

  b = grub_efi_system_table->boot_services;
  efi_call_3 (b->uninstall_protocol_interface,
              &grub_efi_image_handle, &grub_prot_guid, &grub_prot);
}

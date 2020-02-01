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
#include <grub/efi/disk.h>
#include <grub/device.h>
#include <grub/err.h>
#include <grub/env.h>
#include <grub/extcmd.h>
#include <grub/file.h>
#include <grub/i18n.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/types.h>
#include <grub/term.h>
#include <grub/time.h>
#include <grub/uuid.h>

#include <maplib.h>
#include <private.h>
#include <efiapi.h>

GRUB_MOD_LICENSE ("GPLv3+");

static const struct grub_arg_option options_map[] =
{
  {"mem", 'm', 0, N_("Copy to RAM."), 0, 0},
  {"pause", 'p', 0, N_("Show info and wait for keypress."), 0, 0},
  {"type", 't', 0, N_("Specify the disk type."), N_("CD/HD/FD"), ARG_TYPE_STRING},
  {"disk", 'd', 0, N_("Map the entire disk."), 0, 0},
  {"rw", 'w', 0, N_("Add write support for RAM disk."), 0, 0},
  {"nb", 'n', 0, N_("Don't boot virtual disk."), 0, 0},
  {"update", 'u', 0, N_("Update efidisk device mapping."), 0, 0},
  {0, 0, 0, 0, 0, 0}
};

enum options_map
{
  MAP_MEM,
  MAP_PAUSE,
  MAP_TYPE,
  MAP_DISK,
  MAP_RW,
  MAP_NB,
  MAP_UPDATE,
};

vdisk_t vdisk, vpart;

static struct map_private_data map;
struct map_private_data *cmd = &map;

grub_packed_guid_t VDISK_GUID =
{ 0xebe35ad8, 0x6c1e, 0x40f5,
  { 0xaa, 0xed, 0x0b, 0x91, 0x9a, 0x46, 0xbf, 0x4b }
};

void
file_read (grub_efi_boolean_t disk, void *file, void *buf, grub_efi_uintn_t len, grub_efi_uint64_t offset)
{
  if (!disk)
  {
    grub_file_seek (file, offset);
    grub_file_read (file, buf, len);
  }
  else
  {
    grub_disk_read (file, 0, offset, len, buf);
  }
}

grub_efi_uint64_t
get_size (grub_efi_boolean_t disk, void *file)
{
  grub_efi_uint64_t size = 0;
  if (!disk)
    size = grub_file_size (file);
  else
    size = grub_disk_get_size (file) << GRUB_DISK_SECTOR_BITS;
  return size;
}

static void
gen_uuid (void)
{
  grub_srand (grub_get_time_ms());
  int i;
  grub_uint32_t r;
  for (i = 0; i < 4; i++)
  {
    r = grub_rand ();
    grub_memcpy ((grub_uint32_t *)&VDISK_GUID + i, &r, sizeof (grub_uint32_t));
  }
}

static grub_err_t
grub_cmd_map (grub_extcmd_context_t ctxt, int argc, char **args)
{
  struct grub_arg_list *state = ctxt->state;
  grub_efi_boolean_t ro = TRUE;

  if (state[MAP_UPDATE].set)
  {
    grub_printf ("free efidisk devices\n");
    grub_efidisk_fini ();
    grub_printf ("enumerate efidisk devices\n");
    grub_efidisk_init ();
    return GRUB_ERR_NONE;
  }

  gen_uuid ();

  if (state[MAP_MEM].set)
    map.mem = TRUE;
  else
    map.mem = FALSE;

  if (state[MAP_PAUSE].set)
    map.pause = TRUE;
  else
    map.pause = FALSE;

  if (argc != 1)
  {
    grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));
    goto fail;
  }
  if (state[MAP_DISK].set)
  {
    map.disk = TRUE;
    map.file = grub_disk_open (args[0]);
    map.type = HD;
  }
  else
  {
    map.disk = FALSE;
    map.file = grub_file_open (args[0], GRUB_FILE_TYPE_LOOPBACK);
    map.type = CD;
    char c = grub_tolower (args[0][grub_strlen (args[0]) - 1]);
    if (c != 'o') /* iso */
      map.type = HD;
  }

  if (!map.file)
  {
    grub_error (GRUB_ERR_FILE_READ_ERROR, N_("failed to open file/disk"));
    goto fail;
  }

  if (state[MAP_TYPE].set)
  {
    if (state[MAP_TYPE].arg[0] == 'C' || state[MAP_TYPE].arg[0] == 'c')
      map.type = CD;
    if (state[MAP_TYPE].arg[0] == 'H' || state[MAP_TYPE].arg[0] == 'h')
      map.type = HD;
    if (state[MAP_TYPE].arg[0] == 'F' || state[MAP_TYPE].arg[0] == 'f')
      map.type = FD;
  }

  if (state[MAP_RW].set && state[MAP_MEM].set && map.type != CD)
    ro = FALSE;

  grub_efi_status_t status;
  grub_efi_handle_t boot_image_handle = NULL;
  grub_efi_boot_services_t *b;
  b = grub_efi_system_table->boot_services;

  status = vdisk_install (cmd->file, ro);
  if (status != GRUB_EFI_SUCCESS)
  {
    grub_printf ("Failed to install vdisk.\n");
    goto fail;
  }
  if (state[MAP_NB].set)
    return grub_errno;
  if (vpart.handle)
    boot_image_handle = vpart_boot (vpart.handle);
  if (!boot_image_handle)
    boot_image_handle = vdisk_boot ();
  if (!boot_image_handle)
  {
    grub_printf ("Failed to boot vdisk.\n");
    goto fail;
  }
  /* wait */
  if (cmd->pause)
    pause ();
  /* boot */
  grub_script_execute_sourcecode ("terminal_output console");
  grub_printf ("StartImage: %p\n", boot_image_handle);
  status = efi_call_3 (b->start_image, boot_image_handle, 0, NULL);
  grub_printf ("StartImage returned 0x%lx\n", (unsigned long) status);
  status = efi_call_1 (b->unload_image, boot_image_handle);

fail:
  if (map.file)
  {
    if (map.disk)
      grub_disk_close (map.file);
    else
      grub_file_close (map.file);
  }
  if (map.pause)
    pause ();
  return grub_errno;
}

static grub_extcmd_t cmd_map;

GRUB_MOD_INIT(map)
{
  cmd_map = grub_register_extcmd ("map", grub_cmd_map, 0, N_("FILE"),
                                  N_("Create virtual disk."), options_map);
}

GRUB_MOD_FINI(map)
{
  grub_unregister_extcmd (cmd_map);
}

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
#include <grub/device.h>
#include <grub/err.h>
#include <grub/extcmd.h>
#include <grub/file.h>
#include <grub/i18n.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/net.h>
#include <grub/types.h>
#include <grub/term.h>

GRUB_MOD_LICENSE ("GPLv3+");

enum disk_type
{
  HD,
  CD,
  FD,
};

struct grub_private_data
{
  grub_efi_boolean_t mem;
  grub_efi_boolean_t pause;
  enum disk_type type;
  grub_file_t file;
};

static struct grub_private_data map;

static const char map_efi[] = "/map.efi";

static const struct grub_arg_option options_map[] =
{
  {"mem", 'm', 0, N_("Copy to RAM."), 0, 0},
  {"pause", 'p', 0, N_("Show info and wait for keypress."), 0, 0},
  {"type", 't', 0, N_("Specify the disk type."), N_("CD/HD/FD"), ARG_TYPE_STRING},
  {0, 0, 0, 0, 0, 0}
};

enum options_map
{
  MAP_MEM,
  MAP_PAUSE,
  MAP_TYPE,
};

static grub_err_t
grub_efi_map_chain (void)
{
  grub_efi_status_t status;
  grub_efi_boot_services_t *b;
  grub_efi_loaded_image_t *loaded_image;
  grub_efi_handle_t image_handle;
  grub_efi_uintn_t pages = 0;

  b = grub_efi_system_table->boot_services;

  grub_file_t map_bin = 0;
  grub_size_t size;
  grub_efi_physical_address_t map_data = 0;
  map_bin = grub_file_open (map_efi, GRUB_FILE_TYPE_EFI_CHAINLOADED_IMAGE);
  if (!map_bin)
  {
    grub_error (GRUB_ERR_BAD_OS, "failed to open map.efi");
    goto fail;
  }

  size = grub_file_size (map_bin);
  pages = (((grub_efi_uintn_t) size + ((1 << 12) - 1)) >> 12);
  status = efi_call_4 (b->allocate_pages, GRUB_EFI_ALLOCATE_ANY_PAGES,
                       GRUB_EFI_LOADER_CODE,
                       pages, &map_data);
  if (status != GRUB_EFI_SUCCESS)
  {
    grub_error (GRUB_ERR_OUT_OF_MEMORY, N_("out of memory"));
    goto fail;
  }
  grub_script_execute_sourcecode ("terminal_output console");
  grub_file_read (map_bin, (void *) ((grub_addr_t) map_data), size);

  status = efi_call_6 (b->load_image, 0, grub_efi_image_handle, NULL,
                       (void *) ((grub_addr_t) map_data),
                       size, &image_handle);
  if (status != GRUB_EFI_SUCCESS)
  {
    if (status == GRUB_EFI_OUT_OF_RESOURCES)
      grub_error (GRUB_ERR_OUT_OF_MEMORY, "out of resources");
    else
      grub_error (GRUB_ERR_BAD_OS, "cannot load image");
    goto fail;
  }
  loaded_image = grub_efi_get_loaded_image (image_handle);
  if (! loaded_image)
  {
    grub_error (GRUB_ERR_BAD_OS, "no loaded image available");
    goto fail;
  }

  efi_call_3 (b->start_image, image_handle, NULL, NULL);

  status = efi_call_1 (b->unload_image, image_handle);
  if (status != GRUB_EFI_SUCCESS)
    grub_printf ("Exit status code: 0x%08lx\n", (long unsigned int) status);
fail:
  efi_call_2 (b->free_pages, map_data, pages);
  if (map_bin)
    grub_file_close (map_bin);
  return grub_errno;
}

static grub_err_t
grub_cmd_map (grub_extcmd_context_t ctxt, int argc, char **args)
{
  struct grub_arg_list *state = ctxt->state;
  

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
  map.file = grub_file_open (args[0], GRUB_FILE_TYPE_LOOPBACK);
  if (!map.file)
  {
    grub_error (GRUB_ERR_FILE_READ_ERROR, N_("failed to open file"));
    goto fail;
  }

  map.type = CD;
  char c = grub_tolower (args[0][grub_strlen (args[0]) - 1]);
  if (c != 'o') /* iso */
    map.type = HD;

  if (state[MAP_TYPE].set)
  {
    if (state[MAP_TYPE].arg[0] == 'C' || state[MAP_TYPE].arg[0] == 'c')
      map.type = CD;
    if (state[MAP_TYPE].arg[0] == 'H' || state[MAP_TYPE].arg[0] == 'h')
      map.type = HD;
    if (state[MAP_TYPE].arg[0] == 'F' || state[MAP_TYPE].arg[0] == 'f')
      map.type = FD;
  }

  grub_efi_protocol_data_addr = &map;
  grub_efi_map_chain ();

fail:
  if (map.file)
    grub_file_close (map.file);
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

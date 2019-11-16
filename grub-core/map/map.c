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

#include <maplib.h>
#include <private.h>
#include <efiapi.h>
#include <wimboot.h>
#include <vfat.h>

GRUB_MOD_LICENSE ("GPLv3+");

static const struct grub_arg_option options_map[] =
{
  {"mem", 'm', 0, N_("Copy to RAM."), 0, 0},
  {"pause", 'p', 0, N_("Show info and wait for keypress."), 0, 0},
  {"type", 't', 0, N_("Specify the disk type."), N_("CD/HD/FD"), ARG_TYPE_STRING},
  {"disk", 'd', 0, N_("Map the entire disk."), 0, 0},
  {0, 0, 0, 0, 0, 0}
};

enum options_map
{
  MAP_MEM,
  MAP_PAUSE,
  MAP_TYPE,
  MAP_DISK,
};

vdisk_t vdisk, vpart;

static struct map_private_data map;
struct map_private_data *cmd = &map;

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

  grub_efi_status_t status;
  grub_efi_handle_t boot_image_handle = NULL;
  grub_efi_boot_services_t *b;
  b = grub_efi_system_table->boot_services;

  status = vdisk_install (cmd->file);
  if (status != GRUB_EFI_SUCCESS)
  {
    grub_printf ("Failed to install vdisk.\n");
    goto fail;
  }
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

static const struct grub_arg_option options_wimboot[] = {
  //{"gui", 'g', 0, N_("Display graphical boot messages."), 0, 0},
  {"rawbcd", 'b', 0, N_("Disable rewriting .exe to .efi in the BCD file."), 0, 0},
  {"rawwim", 'w', 0, N_("Disable patching the wim file."), 0, 0},
  {"index", 'i', 0, N_("Use WIM image index n."), N_("n"), ARG_TYPE_INT},
  {"pause", 'p', 0, N_("Show info and wait for keypress."), 0, 0},
  {"inject", 'j', 0, N_("Set inject dir."), N_("PATH"), ARG_TYPE_STRING},
  {0, 0, 0, 0, 0, 0}
};

enum options_wimboot
{
  //WIMBOOT_GUI,
  WIMBOOT_RAWBCD,
  WIMBOOT_RAWWIM,
  WIMBOOT_INDEX,
  WIMBOOT_PAUSE,
  WIMBOOT_INJECT
};

struct wimboot_cmdline wimboot_cmd =
{
  FALSE,
  FALSE,
  0,
  FALSE,
  L"\\Windows\\System32",
};

static void
grub_wimboot_close (struct grub_wimboot_context *wimboot_ctx)
{
  int i;
  if (!wimboot_ctx->components)
    return;
  for (i = 0; i < wimboot_ctx->nfiles; i++)
    {
      grub_free (wimboot_ctx->components[i].file_name);
      grub_file_close (wimboot_ctx->components[i].file);
    }
  grub_free (wimboot_ctx->components);
  wimboot_ctx->components = 0;
}

static grub_err_t
grub_wimboot_init (int argc, char *argv[],
                   struct grub_wimboot_context *wimboot_ctx)
{
  int i;

  wimboot_ctx->nfiles = 0;
  wimboot_ctx->components = 0;
  wimboot_ctx->components =
          grub_zalloc (argc * sizeof (wimboot_ctx->components[0]));
  if (!wimboot_ctx->components)
    return grub_errno;

  for (i = 0; i < argc; i++)
  {
    const char *fname = argv[i];
    if (grub_memcmp (argv[i], "@:", 2) == 0)
    {
      const char *ptr, *eptr;
      ptr = argv[i] + 2;
      while (*ptr == '/')
        ptr++;
      eptr = grub_strchr (ptr, ':');
      if (eptr)
      {
        wimboot_ctx->components[i].file_name = grub_strndup (ptr, eptr - ptr);
        if (!wimboot_ctx->components[i].file_name)
        {
          grub_wimboot_close (wimboot_ctx);
          return grub_errno;
        }
        fname = eptr + 1;
      }
    }
    wimboot_ctx->components[i].file = grub_file_open (fname,
                GRUB_FILE_TYPE_LINUX_INITRD | GRUB_FILE_TYPE_NO_DECOMPRESS);
    if (!wimboot_ctx->components[i].file)
    {
      grub_wimboot_close (wimboot_ctx);
      return grub_errno;
    }
    wimboot_ctx->nfiles++;
    grub_printf ("file %d: %s path: %s\n",
                 wimboot_ctx->nfiles, wimboot_ctx->components[i].file_name, fname);
  }

  return GRUB_ERR_NONE;
}

static grub_err_t
grub_cmd_wimboot (grub_extcmd_context_t ctxt,
                  int argc, char *argv[])
{
  struct grub_arg_list *state = ctxt->state;
  struct grub_wimboot_context wimboot_ctx = {0, 0};
  const char *progress = grub_env_get ("enable_progress_indicator");

  if (argc == 0)
  {
    grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));
    goto fail;
  }

  if (grub_wimboot_init (argc, argv, &wimboot_ctx))
    goto fail;

  grub_env_set ("enable_progress_indicator", "1");

  if (state[WIMBOOT_RAWBCD].set)
    wimboot_cmd.rawbcd = TRUE;
  if (state[WIMBOOT_RAWWIM].set)
    wimboot_cmd.rawwim = TRUE;
  if (state[WIMBOOT_PAUSE].set)
    wimboot_cmd.pause = TRUE;
  if (state[WIMBOOT_INDEX].set)
    wimboot_cmd.index = grub_strtoul (state[WIMBOOT_INDEX].arg, NULL, 0);
  if (state[WIMBOOT_INJECT].set)
  {
    int i;
    char *p = state[WIMBOOT_INJECT].arg;
    for (i=0; i < 255; i++)
    {
      if (*p)
      {
        wimboot_cmd.inject[i] = *p;
        p++;
      }
      else
        break;
    }
    wimboot_cmd.inject[i] = 0;
  }

  grub_extract (&wimboot_ctx);
  wimboot_install ();
  wimboot_boot (bootmgfw);
  if (!progress)
    grub_env_unset ("enable_progress_indicator");
  else
    grub_env_set ("enable_progress_indicator", progress);
fail:
  grub_wimboot_close (&wimboot_ctx);
  return grub_errno;
}

static grub_extcmd_t cmd_map, cmd_wimboot;

GRUB_MOD_INIT(map)
{
  cmd_map = grub_register_extcmd ("map", grub_cmd_map, 0, N_("FILE"),
                                  N_("Create virtual disk."), options_map);
  cmd_wimboot = grub_register_extcmd ("wimboot", grub_cmd_wimboot, 0,
                    N_("[--rawbcd] [--index=n] [--pause] @:NAME:PATH"),
                    N_("Windows Imaging Format bootloader"), options_wimboot);
}

GRUB_MOD_FINI(map)
{
  grub_unregister_extcmd (cmd_map);
  grub_unregister_extcmd (cmd_wimboot);
}

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

static const struct grub_arg_option options_wimboot[] = {
  {"gui", 'g', 0, N_("Display graphical boot messages."), 0, 0},
  {"rawbcd", 'b', 0, N_("Disable rewriting .exe to .efi in the BCD file."), 0, 0},
  {"rawwim", 'w', 0, N_("Disable patching the wim file."), 0, 0},
  {"index", 'i', 0, N_("Use WIM image index n."), N_("n"), ARG_TYPE_INT},
  {"pause", 'p', 0, N_("Show info and wait for keypress."), 0, 0},
  {"inject", 'j', 0, N_("Set inject dir."), N_("PATH"), ARG_TYPE_STRING},
  {0, 0, 0, 0, 0, 0}
};

enum options_wimboot
{
  WIMBOOT_GUI,
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
  FALSE,
  0,
  FALSE,
  L"\\Windows\\System32",
};

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

  if (state[WIMBOOT_GUI].set)
    wimboot_cmd.gui = TRUE;
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

static const struct grub_arg_option options_vfat[] = {
  {"create", 'c', 0, N_("Create virtual FAT disk."), 0, 0},
  {"add", 'a', 0, N_("Add files to virtual FAT disk."), N_("FILE"), ARG_TYPE_STRING},
  {"mem", 'm', 0, N_("Copy to memory."), 0, 0},
  {"install", 'i', 0, N_("Install virtual FAT disk to BIOS."), 0, 0},
  {"boot", 'b', 0, N_("Boot virtual FAT disk."), 0, 0},
  {"ls", 'l', 0, N_("List all files in virtual disk."), 0, 0},
  /* patch */
  {"patch", 'p', 0, N_("Patch files in virtual disk."), N_("FILE"), ARG_TYPE_STRING},
  {"offset", 'o', 0, N_("Set the offset."), N_("n"), ARG_TYPE_INT},
  {"search", 's', 0, N_("search"), N_("STRING"), ARG_TYPE_STRING},
  {"count", 'n', 0, N_("count"), N_("FILE"), ARG_TYPE_INT},
  {0, 0, 0, 0, 0, 0}
};

enum options_vfat
{
  OPS_CREATE,
  OPS_ADD,
  OPS_MEM,
  OPS_INSTALL,
  OPS_BOOT,
  OPS_LS,
  OPS_PATCH,
  OPS_OFFSET,
  OPS_SEARCH,
  OPS_COUNT,
};

struct grub_vfatdisk_file *vfat_file_list;

static grub_err_t
grub_cmd_vfat (grub_extcmd_context_t ctxt, int argc, char *argv[])
{
  struct grub_arg_list *state = ctxt->state;
  grub_file_t file = 0;
  void *addr = NULL;
  char *file_name = NULL;
  struct grub_vfatdisk_file *newfile = NULL;
  wimboot_cmd.gui = TRUE;
  wimboot_cmd.rawbcd = TRUE;
  wimboot_cmd.rawwim = TRUE;
  wimboot_cmd.pause = FALSE;
  if (state[OPS_ADD].set && argc == 1)
  {
    file = grub_file_open (argv[0], GRUB_FILE_TYPE_LOOPBACK);
    if (!file)
    {
      grub_file_close (file);
      goto fail;
    }
    file_name = state[OPS_ADD].arg;
    if (!file_name)
      file_name = file->name;
    if (state[OPS_MEM].set)
    {
      addr = grub_malloc (file->size);
      if (!addr)
        goto fail;
      grub_printf ("Loading %s ...\n", file->name);
      grub_file_read (file, addr, file->size);
      add_file (file_name, addr, file->size, mem_read_file);
      grub_printf ("Added: (mem)%p+%ld -> %s\n",
                   addr, (unsigned long) file->size, file_name);
    }
    else
    {
      add_file (file_name, file, file->size, efi_read_file);
      grub_printf ("Added: %s -> %s\n", file->name, file_name);
    }
    newfile = grub_malloc (sizeof (struct grub_vfatdisk_file));
    if (!newfile)
      goto fail;
    newfile->name = grub_strdup (file_name);
    if (!newfile->name)
    {
      grub_free (newfile);
      goto fail;
    }
    newfile->file = file;
    newfile->addr = addr;
    newfile->next = vfat_file_list;
    vfat_file_list = newfile;
  }
  else if (state[OPS_INSTALL].set)
    wimboot_install ();
  else if (state[OPS_BOOT].set)
    wimboot_boot (bootmgfw);
  else if (state[OPS_CREATE].set)
    create_vfat ();
  else if (state[OPS_LS].set)
    ls_vfat ();
  else if (state[OPS_PATCH].set && state[OPS_OFFSET].set && argc == 1)
    patch_vfat_offset (state[OPS_PATCH].arg,
                       grub_strtoul (state[OPS_OFFSET].arg, NULL, 0),
                       argv[0]);
  else if (state[OPS_PATCH].set && state[OPS_SEARCH].set && argc == 1)
  {
    int count = 0;
    if (state[OPS_COUNT].set)
      count = grub_strtoul (state[OPS_COUNT].arg, NULL, 0);
    patch_vfat_search (state[OPS_PATCH].arg, state[OPS_SEARCH].arg, argv[0], count);
  }
  else
    print_vfat_help ();
fail:
  return grub_errno;
}

static grub_extcmd_t cmd_wimboot, cmd_vfat;

GRUB_MOD_INIT(wimboot)
{
  cmd_wimboot = grub_register_extcmd ("wimboot", grub_cmd_wimboot, 0,
                    N_("[--rawbcd] [--index=n] [--pause] @:NAME:PATH"),
                    N_("Windows Imaging Format bootloader"), options_wimboot);
  cmd_vfat = grub_register_extcmd ("vfat", grub_cmd_vfat, 0,
                    N_("[--mem] [--add=FILE PATH]"),
                    N_("Virtual FAT Disk"), options_vfat);
}

GRUB_MOD_FINI(wimboot)
{
  grub_unregister_extcmd (cmd_wimboot);
  grub_unregister_extcmd (cmd_vfat);
}

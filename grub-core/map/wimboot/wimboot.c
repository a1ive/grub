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
#include <grub/device.h>
#include <grub/err.h>
#include <grub/extcmd.h>
#include <grub/file.h>
#include <grub/i18n.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/types.h>
#include <grub/term.h>
#include <grub/wimtools.h>

#include <misc.h>
#include <wimboot.h>
#include <vfat.h>
#include <string.h>

#ifdef GRUB_MACHINE_EFI
#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#include <grub/efi/disk.h>
#endif

GRUB_MOD_LICENSE ("GPLv3+");

struct wimboot_cmdline wimboot_cmd =
{
  0, 0, 0, 0, 0,
  L"\\Windows\\System32",
};

#ifdef GRUB_MACHINE_EFI

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

static grub_err_t
grub_cmd_wimboot (grub_extcmd_context_t ctxt,
                  int argc, char *argv[])
{
  struct grub_arg_list *state = ctxt->state;

  if (argc == 0)
  {
    grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));
    goto fail;
  }

  grub_wimboot_init (argc, argv);

  if (state[WIMBOOT_GUI].set)
    wimboot_cmd.gui = 1;
  if (state[WIMBOOT_RAWBCD].set)
    wimboot_cmd.rawbcd = 1;
  if (state[WIMBOOT_RAWWIM].set)
    wimboot_cmd.rawwim = 1;
  if (state[WIMBOOT_PAUSE].set)
    wimboot_cmd.pause = 1;
  if (state[WIMBOOT_INDEX].set)
    wimboot_cmd.index = grub_strtoul (state[WIMBOOT_INDEX].arg, NULL, 0);
  if (state[WIMBOOT_INJECT].set)
    mbstowcs (wimboot_cmd.inject, state[WIMBOOT_INJECT].arg, 256);

  grub_wimboot_extract (&wimboot_cmd);
  grub_wimboot_install ();
  grub_wimboot_boot (bootmgfw, &wimboot_cmd);
fail:
  grub_pause_fatal ("failed to boot.\n");
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

static grub_err_t
grub_cmd_vfat (grub_extcmd_context_t ctxt, int argc, char *argv[])
{
  struct grub_arg_list *state = ctxt->state;
  grub_file_t file = 0;
  char *file_name = NULL;
  wimboot_cmd.gui = 1;
  wimboot_cmd.rawbcd = 1;
  wimboot_cmd.rawwim = 1;
  wimboot_cmd.pause = 0;
  if (state[OPS_ADD].set && argc == 1)
  {
    file = file_open (argv[0], state[OPS_MEM].set, 0, 0);
    if (!file)
    {
      file_close (file);
      goto fail;
    }
    file_name = state[OPS_ADD].arg;

    vfat_append_list (file, file_name);
    file_add (file_name, file, &wimboot_cmd);
  }
  else if (state[OPS_INSTALL].set)
    grub_wimboot_install ();
  else if (state[OPS_BOOT].set)
    grub_wimboot_boot (bootmgfw, &wimboot_cmd);
  else if (state[OPS_CREATE].set)
    vfat_create ();
  else if (state[OPS_LS].set)
    vfat_ls ();
  else if (state[OPS_PATCH].set && state[OPS_OFFSET].set && argc == 1)
    vfat_patch_offset (state[OPS_PATCH].arg,
                       grub_strtoul (state[OPS_OFFSET].arg, NULL, 0),
                       argv[0]);
  else if (state[OPS_PATCH].set && state[OPS_SEARCH].set && argc == 1)
  {
    int count = 0;
    if (state[OPS_COUNT].set)
      count = grub_strtoul (state[OPS_COUNT].arg, NULL, 0);
    vfat_patch_search (state[OPS_PATCH].arg, state[OPS_SEARCH].arg, argv[0], count);
  }
  else
    vfat_help ();
fail:
  return grub_errno;
}

static grub_extcmd_t cmd_wimboot, cmd_vfat;

#endif

static const struct grub_arg_option options_wimtools[] = {
  {"index", 'i', 0, N_("Use WIM image index n."), N_("n"), ARG_TYPE_INT},
  {"exist", 'e', 0, N_("Check file exists or not."), 0, 0},
  {"is64", 'a', 0, N_("Check winload.exe is 64 bit or not."), 0, 0},
  {0, 0, 0, 0, 0, 0}
};

enum options_wimtools
{
  WIMTOOLS_INDEX,
  WIMTOOLS_EXIST,
  WIMTOOLS_IS64,
};

static grub_err_t
grub_cmd_wimtools (grub_extcmd_context_t ctxt, int argc, char *argv[])
{
  struct grub_arg_list *state = ctxt->state;
  unsigned int index = 0;
  grub_file_t file = 0;
  grub_err_t err = GRUB_ERR_NONE;
  if (argc < 1 || (state[WIMTOOLS_EXIST].set && argc < 2))
    return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));

  if (state[WIMTOOLS_INDEX].set)
    index = grub_strtoul (state[WIMTOOLS_INDEX].arg, NULL, 0);
  file = grub_file_open (argv[0], GRUB_FILE_TYPE_LOOPBACK);
  if (!file)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("failed to open file"));

  if (state[WIMTOOLS_EXIST].set)
  {
    if (grub_wim_file_exist (file, index, argv[1]))
      err = GRUB_ERR_NONE;
    else
      err = GRUB_ERR_TEST_FAILURE;
  }
  else if (state[WIMTOOLS_IS64].set)
  {
    if (grub_wim_is64 (file, index))
      err = GRUB_ERR_NONE;
    else
      err = GRUB_ERR_TEST_FAILURE;
  }
  grub_file_close (file);
  return err;
}

static grub_extcmd_t cmd_wimtools;

GRUB_MOD_INIT(wimboot)
{
#ifdef GRUB_MACHINE_EFI
  cmd_wimboot = grub_register_extcmd ("wimboot", grub_cmd_wimboot, 0,
                    N_("[--rawbcd] [--index=n] [--pause] @:NAME:PATH"),
                    N_("Windows Imaging Format bootloader"), options_wimboot);
  cmd_vfat = grub_register_extcmd ("vfat", grub_cmd_vfat, 0,
                    N_("[--mem] [--add=FILE PATH]"),
                    N_("Virtual FAT Disk"), options_vfat);
#endif
  cmd_wimtools = grub_register_extcmd ("wimtools", grub_cmd_wimtools, 0,
                    N_("[--index=n] [OPTIONS] FILE [PATH]"),
                    N_("WIM Tools"), options_wimtools);
}

GRUB_MOD_FINI(wimboot)
{
#ifdef GRUB_MACHINE_EFI
  grub_unregister_extcmd (cmd_wimboot);
  grub_unregister_extcmd (cmd_vfat);
#endif
  grub_unregister_extcmd (cmd_wimtools);
}

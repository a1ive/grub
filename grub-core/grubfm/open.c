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
 */

#include <grub/types.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/err.h>
#include <grub/dl.h>
#include <grub/file.h>
#include <grub/term.h>
#include <grub/env.h>
#include <grub/normal.h>
#include <grub/command.h>
#include <grub/script_sh.h>

#include "fm.h"

#define REQUIRE(x) do { \
    if (!grubfm_command_exist (x))  \
      return;  \
  } while (0)

static void
grubfm_add_menu_back (const char *filename)
{
  char *dir = NULL;
  char *src = NULL;
  dir = grub_strdup (filename);
  *grub_strrchr (dir, '/') = 0;

  src = grub_xasprintf ("grubfm \"%s/\"", dir);

  grubfm_add_menu (_("Back"), "go-previous", NULL, src, 0);
  grub_free (src);
  if (dir)
    grub_free (dir);
}

static void
grubfm_open_iso_loopback (grub_file_t file, char *path)
{
  REQUIRE ("loopback");
  char *src = NULL;
  src = grub_xasprintf ("loopback iso_loop \"%s\"", path);
  if (!grubfm_run_cmd (src))
    return;
  grub_free (src);
  if (grubfm_file_exist ((char *)"(iso_loop)/boot/grub/loopback.cfg"))
  {
    char *dev_uuid = NULL;
    if (file->fs->fs_uuid)
    {
      int err;
      err = file->fs->fs_uuid (file->device, &dev_uuid);
      if (err)
      {
        grub_errno = 0;
        dev_uuid = NULL;
      }
    }
    grub_env_set ("dev_uuid", dev_uuid);
    grub_env_export ("dev_uuid");
    grub_env_set ("rootuuid", dev_uuid);
    grub_env_export ("rootuuid");
    char *iso_path = NULL;
    iso_path = grub_strchr (path, '/');
    grub_env_set ("iso_path", iso_path);
    grub_env_export ("iso_path");
    grubfm_add_menu (_("Boot ISO (Loopback)"), "gnu-linux", NULL,
        "root=iso_loop\nconfigfile /boot/grub/loopback.cfg", 0);
    grub_free (dev_uuid);
  }
}

static void
grubfm_open_iso_map (grub_file_t file UNUSED, char *path)
{
#ifdef GRUB_MACHINE_EFI
  REQUIRE ("map");
  REQUIRE ("blocklist");
  char *src = NULL;
  src = grub_xasprintf ("map \"%s\"", path);
  if (!src)
    return;
  grubfm_add_menu (_("Boot ISO (map)"), "iso", NULL, src, 0);
  grub_free (src);
#endif
}

static void
grubfm_open_efi (grub_file_t file UNUSED, char *path)
{
#ifdef GRUB_MACHINE_EFI
  REQUIRE ("chainloader");
  char *src = NULL;
  src = grub_xasprintf ("set lang=en_US\nchainloader -b -t \"%s\"", path);
  if (!src)
    return;
  grubfm_add_menu (_("Open As EFI Application"), "uefi", NULL, src, 0);
  grub_free (src);
#endif
}

static void
grubfm_open_lua (grub_file_t file UNUSED, char *path)
{
  REQUIRE ("lua");
  char *src = NULL;
  src = grub_xasprintf ("lua \"%s\"", path);
  if (!src)
    return;
  grubfm_add_menu (_("Open As Lua Script"), "lua", NULL, src, 0);
  grub_free (src);
}

static void
grubfm_open_wim_wimboot (grub_file_t file UNUSED, char *path)
{
  REQUIRE ("loopback");
  char *src = NULL;
#ifdef GRUB_MACHINE_EFI
#if (defined (__i386__) || defined (__x86_64__))
  REQUIRE ("wimboot");
  src = grub_xasprintf ("set lang=en_US\n"
                        "loopback wimboot ${prefix}/wimboot.gz\n"
                        "wimboot @:bootmgfw.efi:(wimboot)/bootmgfw.efi"
                        " @:bcd:(wimboot)/bcd @:boot.sdi:(wimboot)/boot.sdi" 
                        " @:boot.wim:\"%s\"", path);
#endif /* __i386__ || __x86_64__ */
#elif defined (GRUB_MACHINE_PCBIOS)
  REQUIRE ("linux16");
  src = grub_xasprintf ("set lang=en_US\n"
                        "terminal_output console\n"
                        "enable_progress_indicator=1\n"
                        "loopback wimboot /wimboot\n"
                        "linux16 (wimboot)/wimboot\n"
                        "initrd16 newc:bootmgr:(wimboot)/bootmgr"
                        " newc:bootmgr.exe:(wimboot)/bootmgr.exe"
                        " newc:bcd:(wimboot)/bcd newc:boot.sdi:(wimboot)/boot.sdi" 
                        " newc:boot.wim:\"%s\"", path);
#endif
  if (!src)
    return;
  grubfm_add_menu (_("Boot NT6.x WIM (wimboot)"), "wim", NULL, src, 0);
  grub_free (src);
}

static void
grubfm_open_font (grub_file_t file UNUSED, char *path)
{
  char *src = NULL;
  src = grub_xasprintf ("loadfont \"%s\"", path);
  if (!src)
    return;
  grubfm_add_menu (_("Load PF2 Font"), "pf2", NULL, src, 0);
  grub_free (src);
}

static void
grubfm_open_mod (grub_file_t file UNUSED, char *path)
{
  char *src = NULL;
  src = grub_xasprintf ("insmod \"%s\"", path);
  if (!src)
    return;
  grubfm_add_menu (_("Insert Grub2 Module"), "mod", NULL, src, 0);
  grub_free (src);
}

static void
grubfm_open_ipxe (grub_file_t file UNUSED, char *path)
{
#ifdef GRUB_MACHINE_PCBIOS
  REQUIRE ("linux16");
  char *src = NULL;
  src = grub_xasprintf ("linux16 ${prefix}/ipxe.lkrn\ninitrd16 \"%s\"", path);
  if (!src)
    return;
  grubfm_add_menu (_("Open As iPXE Script"), "net", NULL, src, 0);
  grub_free (src);
#endif
}

void
grubfm_open_file (char *path)
{
  grubfm_add_menu_back (path);
  struct grubfm_enum_file_info info;
  grub_file_t file = 0;
  file = grub_file_open (path, GRUB_FILE_TYPE_GET_SIZE |
                           GRUB_FILE_TYPE_NO_DECOMPRESS);
  if (!file)
    return;
  info.name = file->name;
  info.size = (char *) grub_get_human_size (file->size, 
                                            GRUB_HUMAN_SIZE_SHORT);
  grubfm_get_file_type (&info);
  switch (info.type)
  {
    case ISO:
      grubfm_open_iso_loopback (file, path);
      grubfm_open_iso_map (file, path);
      break;
    case DISK:
      break;
    case VHD:
      break;
    case FBA:
      break;
    case IMAGE:
      break;
    case EFI:
      grubfm_open_efi (file, path);
      break;
    case LUA:
      grubfm_open_lua (file, path);
      break;
    case TAR:
      break;
    case WIM:
      grubfm_open_wim_wimboot (file, path);
      break;
    case NT5:
      break;
    case CFG:
      break;
    case FONT:
      grubfm_open_font (file, path);
      break;
    case MOD:
      grubfm_open_mod (file, path);
      break;
    case MBR:
      break;
    case NSH:
      break;
    case LST:
      break;
    case IPXE:
      grubfm_open_ipxe (file, path);
      break;
    case PYTHON:
      break;
    default:
      DBG ("FILE\n");
  }
  grub_file_close (file);
}

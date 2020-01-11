/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2019,2020  Free Software Foundation, Inc.
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
#include <grub/env.h>
#include <grub/dl.h>
#include <grub/extcmd.h>
#include <grub/i18n.h>

#include <grub/term.h>
#include <grub/video.h>
#include <grub/bitmap.h>
#include <grub/gfxmenu_view.h>

#include "fm.h"

GRUB_MOD_LICENSE ("GPLv3+");

static int init = 0;
char grubfm_root[20] = "memdisk";

static void
grubfm_init (void)
{
  if (!init)
  {
    grubfm_ini_enum (grubfm_root);
    init = 1;
  }
}

static grub_err_t
grub_cmd_grubfm (grub_extcmd_context_t ctxt __attribute__ ((unused)),
        int argc, char **args)
{
  grubfm_init ();
  grubfm_clear_menu ();
  if (argc == 0)
    grubfm_enum_device ();
  else
    grubfm_enum_file (args[0]);
  char *src = NULL;
  src = grub_xasprintf ("source (%s)/boot/grub/global.sh\n",
                        grubfm_root);
  if (!src)
    return 0;
  grub_script_execute_sourcecode (src);
  grub_free (src);
  return 0;
}

static grub_err_t
grub_cmd_grubfm_open (grub_extcmd_context_t ctxt __attribute__ ((unused)),
        int argc, char **args)
{
  grubfm_init ();
  grubfm_clear_menu ();
  if (argc != 1)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("bad argument"));
  grubfm_open_file (args[0]);
  char *src = NULL;
  src = grub_xasprintf ("source (%s)/boot/grub/global.sh\n",
                        grubfm_root);
  if (!src)
    return 0;
  grub_script_execute_sourcecode (src);
  grub_free (src);
  return 0;
}

static const struct grub_arg_option options_set[] =
{
  {"root", 'r', 0, N_("root"), 0, 0},
  {0, 0, 0, 0, 0, 0}
};

enum options_set
{
  FM_SET_ROOT,
};

static grub_err_t
grub_cmd_grubfm_set (grub_extcmd_context_t ctxt,
                     int argc, char **args)
{
  struct grub_arg_list *state = ctxt->state;
  if (state[FM_SET_ROOT].set && argc == 1)
  {
    grub_strncpy(grubfm_root, args[0], 19);
  }
  return 0;
}

static grub_err_t
grub_cmd_grubfm_about (grub_extcmd_context_t ctxt __attribute__ ((unused)),
                     int argc __attribute__ ((unused)),
                     char **args __attribute__ ((unused)))
{
  unsigned int w, h;
  grub_video_color_t white = grubfm_get_color (255, 255, 255);
  grubfm_get_screen_info (&w, &h);
  if (w < 640 || h < 480)
    return grub_error (GRUB_ERR_BAD_OS,
                       N_("gfxmode (minimum resolution 640x480) required"));
  grubfm_gfx_clear ();
  /* ascii art */
  grubfm_gfx_printf (white, FONT_SPACE, 2 * FONT_HEIGH, GRUBFM_ASCII_ART1);
  grubfm_gfx_printf (white, FONT_SPACE, 3 * FONT_HEIGH, GRUBFM_ASCII_ART2);
  grubfm_gfx_printf (white, FONT_SPACE, 4 * FONT_HEIGH, GRUBFM_ASCII_ART3);
  grubfm_gfx_printf (white, FONT_SPACE, 5 * FONT_HEIGH, GRUBFM_ASCII_ART4);
  grubfm_gfx_printf (white, FONT_SPACE, 6 * FONT_HEIGH, GRUBFM_ASCII_ART5);
  grubfm_gfx_printf (white, FONT_SPACE, 7 * FONT_HEIGH, GRUBFM_ASCII_ART6);
  grubfm_gfx_printf (white, FONT_SPACE, 9 * FONT_HEIGH, GRUBFM_COPYRIGHT);
  /* info */
  grubfm_gfx_printf (white, 2 * FONT_SPACE, 10 * FONT_SPACE,
                     "Platform: %s-%s", GRUB_TARGET_CPU, GRUB_PLATFORM);
  grubfm_gfx_printf (white, 2 * FONT_SPACE, 11 * FONT_SPACE,
                     "Language: %s", grub_env_get ("lang"));
  grubfm_gfx_printf (white, 2 * FONT_SPACE, 12 * FONT_SPACE,
                     "Resolution: %ux%u", w, h);
  grubfm_gfx_printf (white, 2 * FONT_SPACE, 13 * FONT_SPACE,
                     "GRUB version: %s", GRUB_VERSION);
  grubfm_gfx_printf (white, 2 * FONT_SPACE, 14 * FONT_SPACE,
                     "GRUB build date: %s", GRUB_BUILD_DATE);
  grubfm_gfx_printf (white, 2 * FONT_SPACE, 16 * FONT_SPACE,
                     "License: GNU GPLv3");
  grubfm_gfx_printf (white, 2 * FONT_SPACE, 18 * FONT_SPACE,
                     "Press any key to exit ...");
  grub_getkey ();
  return 0;
}

static grub_err_t
grub_cmd_grubfm_hex (grub_extcmd_context_t ctxt __attribute__ ((unused)),
                     int argc, char **args)
{
  if (argc != 1)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("bad argument"));
  unsigned int w, h;
  grubfm_get_screen_info (&w, &h);
  if (w < 1024 || h < 768)
    return grub_error (GRUB_ERR_BAD_OS,
                       N_("gfxmode (minimum resolution 1024x768) required"));
  grubfm_hexdump (args[0]);
  return 0;
}

static grub_uint8_t NT_VERSION_SRC[] =
{ 0x50, 0x00, 0x72, 0x00, 0x6F, 0x00, 0x64, 0x00,
  0x75, 0x00, 0x63, 0x00, 0x74, 0x00, 0x56, 0x00,
  0x65, 0x00, 0x72, 0x00, 0x73, 0x00, 0x69, 0x00,
  0x6F, 0x00, 0x6E, 0x00 };

static char dll_path[50];

static int
grubfm_ntdir_try (char *name, int (* exist) (const char *path))
{
  int ret = 1;
  grub_size_t i, len = grub_strlen (dll_path);
  grub_strcpy (dll_path + len, name);
  grub_printf ("try %s\n", dll_path);
  if (!exist (dll_path))
  {
    name[0] = grub_tolower (name[0]);
    grub_strcpy (dll_path + len, name);
    grub_printf ("try %s\n", dll_path);
    if (!exist (dll_path))
    {
      for (i = 0; i < grub_strlen (name); i++)
        name[i] = grub_toupper (name[i]);
      grub_strcpy (dll_path + len, name);
      grub_printf ("try %s\n", dll_path);
      if (!exist (dll_path))
        ret = 0;
    }
  }
  return ret;
}

static grub_err_t
grub_cmd_ntversion (grub_extcmd_context_t ctxt __attribute__ ((unused)),
                     int argc, char **args)
{
  if (argc != 2)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("bad argument"));
  char dir_win[] = "Windows/";
  char dir_sys[] = "System32/";
  char dll_ver[] = "Version.dll";
  char ntver[8];
  grub_file_t file = 0;
  grub_uint8_t *data = NULL;
  grub_size_t i;
  grub_size_t len = grub_strlen (args[0]);
  grub_strncpy (dll_path, args[0], len);
  dll_path[len] = '/';
  dll_path[len + 1] = '\0';
  if (!grubfm_ntdir_try (dir_win, grubfm_dir_exist))
    return 1;
  if (!grubfm_ntdir_try (dir_sys, grubfm_dir_exist))
    return 1;
  if (!grubfm_ntdir_try (dll_ver, grubfm_file_exist))
    return 1;
  file = grub_file_open (dll_path, GRUB_FILE_TYPE_HEXCAT);
  if (!file)
    return grub_error (GRUB_ERR_FILE_NOT_FOUND,
                       N_("failed to open %s"), dll_path);
  if (file->size < sizeof(NT_VERSION_SRC) + 12)
  {
    grub_file_close (file);
    return grub_error (GRUB_ERR_FILE_READ_ERROR, N_("bad file size"));
  }
  data = grub_malloc (file->size);
  if (!data)
  {
    grub_file_close (file);
    return grub_error (GRUB_ERR_OUT_OF_MEMORY, N_("out of memory"));
  }
  grub_file_read (file, data, file->size);
  grub_file_close (file);
  for (i = 0; i < file->size - sizeof(NT_VERSION_SRC) - 12; i++)
  {
    if (grub_memcmp (data + i, NT_VERSION_SRC, sizeof(NT_VERSION_SRC)) == 0)
    {
      grub_printf ("found version in %lld: ", (unsigned long long) i);
      ntver[0] = *(data + i + sizeof(NT_VERSION_SRC) + 2);
      ntver[1] = *(data + i + sizeof(NT_VERSION_SRC) + 4);
      ntver[2] = *(data + i + sizeof(NT_VERSION_SRC) + 6);
      ntver[3] = *(data + i + sizeof(NT_VERSION_SRC) + 8);
      ntver[4] = '\0';
      grub_printf ("%s\n", ntver);
      grub_env_set (args[1], ntver);
      grub_free (data);
      return 0;
    }
  }
  grub_free (data);
  return 1;
}

static grub_extcmd_t cmd;
static grub_extcmd_t cmd_open;
static grub_extcmd_t cmd_set;
static grub_extcmd_t cmd_about;
static grub_extcmd_t cmd_hex;
static grub_extcmd_t cmd_nt;

GRUB_MOD_INIT(grubfm)
{
  cmd = grub_register_extcmd ("grubfm", grub_cmd_grubfm, 0, 
                  N_("[PATH]"),
                  N_("GRUB file manager."), 0);
  cmd_open = grub_register_extcmd ("grubfm_open", grub_cmd_grubfm_open, 0,
                  N_("PATH"),
                  N_("GRUB file manager."), 0);
  cmd_set = grub_register_extcmd ("grubfm_set", grub_cmd_grubfm_set, 0,
                                  N_("--root DEVICE"),
                                  N_("GRUB file manager."),
                                  options_set);
  cmd_about = grub_register_extcmd ("grubfm_about", grub_cmd_grubfm_about, 0, 0,
                  N_("GRUB file manager."), 0);
  cmd_hex = grub_register_extcmd ("grubfm_hex", grub_cmd_grubfm_hex, 0,
                  N_("PATH"),
                  N_("GRUB file manager."), 0);
  cmd_nt = grub_register_extcmd ("ntversion", grub_cmd_ntversion, 0,
                  N_("(hdx,y) VARIABLE"),
                  N_("Get NT version."), 0);
}

GRUB_MOD_FINI(grubfm)
{
  grub_unregister_extcmd (cmd);
  grub_unregister_extcmd (cmd_open);
  grub_unregister_extcmd (cmd_set);
  grub_unregister_extcmd (cmd_about);
  grub_unregister_extcmd (cmd_hex);
  grub_unregister_extcmd (cmd_nt);
}

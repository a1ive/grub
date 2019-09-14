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

#ifndef GRUB_FILE_MANAGER_HEADER
#define GRUB_FILE_MANAGER_HEADER

#include <grub/types.h>
#include <grub/misc.h>

/* file type */
enum grubfm_file_type
{
  UNKNOWN,
  DIR,
  ISO,
  DISK,
  VHD,
  FBA,
  IMAGE,
  EFI,
  LUA,
  TAR,
  WIM,
  NT5,
  CFG,
  FONT,
  MOD,
  MBR,
  NSH,
  LST,
  IPXE,
  PYTHON,
};

struct grubfm_file_ext
{
  char ext[8];
  char icon[5];
  enum grubfm_file_type type;
};

struct grubfm_enum_file_info
{
  char *name;
  char *size;
  enum grubfm_file_type type;
};

struct grubfm_enum_file_list
{
  int nfiles;
  struct grubfm_enum_file_info *file_list;
  int ndirs;
  struct grubfm_enum_file_info *dir_list;
  char *dirname;
  int f;
  int d;
};

extern struct grubfm_file_ext grubfm_file_table[];
/* lib.c */
int
grubfm_dir_exist (char *path);
int
grubfm_file_exist (char *path);
int
grubfm_command_exist (const char *str);
grub_err_t
grubfm_run_cmd (const char *cmdline);
void
grubfm_clear_menu (void);
void
grubfm_add_menu (const char *title, const char *icon,
                 const char *hotkey, const char *src, int hidden);

/* list.c */
int
grubfm_enum_device (void);
int
grubfm_enum_file (char *dirname);

/* type.c */
char *
grubfm_get_file_type (struct grubfm_enum_file_info *info);

/* open.c */
void
grubfm_open_file (char *path);

#endif

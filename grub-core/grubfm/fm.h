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

#include <ini.h>

struct grubfm_ini_enum_list
{
  int n;
  int i;
  char **ext;
  char **icon;
  ini_t **config;
};

struct grubfm_enum_file_info
{
  char *name;
  char *size;
  int ext; /* index */
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

extern char grubfm_root[];

extern struct grubfm_ini_enum_list grubfm_ext_table;
extern ini_t *grubfm_ini_config;
/* lib.c */
int
grubfm_dir_exist (const char *path);
int
grubfm_file_exist (const char *path);
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
void
grubfm_ini_enum (const char *devname);
const char *
grubfm_get_file_icon (struct grubfm_enum_file_info *info);

/* open.c */
void
grubfm_open_file (char *path);

#endif

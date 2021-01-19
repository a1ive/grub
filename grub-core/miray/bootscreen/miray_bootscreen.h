/*
 *  Extention for GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2010,2011 Miray Software <oss@miray.de>
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


#ifndef MIRAY_BOOTSCREEN
#define MIRAY_BOOTSCREEN

#include <grub/term.h>
#include <grub/command.h>
#include <grub/menu.h>

/* Function Definitions */

struct miray_text_screen_data;

grub_err_t miray_cmd_cmdline(grub_command_t cmd, int argc, char *args[]);
grub_err_t miray_bootscreen_preboot(int noreturn);
grub_err_t miray_bootscreen_execute (grub_menu_t menu, int nested, int auto_boot);
grub_err_t miray_bootscreen_run_submenu(const char *path);

grub_err_t miray_segfile_init(void);
grub_err_t miray_segfile_fini(void);

#endif

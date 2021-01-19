/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2010-2012 Miray Software <oss@miray.de>
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

#ifndef MIRAY_CMD_DIAG_H
#define MIRAY_CMD_DIAG_H

#include <grub/err.h>
#include <grub/command.h>

// BIOS only methods
grub_err_t miray_cmd_diag_bios_print_hdextinfo (grub_command_t cmd, int argc, char **args );
grub_err_t miray_cmd_diag_bios_print_cdinfo (grub_command_t cmd, int argc, char **args );

// EFI methods
grub_err_t miray_cmd_diag_efi_print_image_path(grub_command_t cmd, int argc, char **args);
grub_err_t miray_cmd_diag_efi_print_device_path(grub_command_t cmd, int argc, char **args);


#endif

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

#include "miray_cmd_diag.h"

#include <grub/dl.h>
#include <grub/misc.h>
#include <grub/i18n.h>

#include <multiboot.h>

#ifndef offsetof
#define offsetof(s, m)  ((grub_size_t)&((s*)0)->m)
#endif

GRUB_MOD_LICENSE ("GPLv3+");

static grub_err_t
miray_cmd_test_bootstruct(grub_command_t cmd __attribute__ ((unused)),
		       int argc __attribute__ ((unused)), char **args __attribute__ ((unused)) )
{
   grub_printf("Size of multiboot struct: %" PRIuGRUB_SIZE "\n", sizeof(struct multiboot_info));
   
   grub_printf("Offset of fb type: %" PRIuGRUB_SIZE "\n", offsetof(struct multiboot_info, framebuffer_type));
   grub_printf("Offset of fb palette address: %" PRIuGRUB_SIZE "\n", offsetof(struct multiboot_info, framebuffer_palette_addr));
   grub_printf("offset of fb red field: %" PRIuGRUB_SIZE "\n", offsetof(struct multiboot_info, framebuffer_red_field_position));

   return 0;
} 

static grub_command_t cmd_test_bootstruct;

#if defined(GRUB_MACHINE_PCBIOS)
static grub_command_t cmdcd_bios;
static grub_command_t cmdhd_bios;
#endif

#if defined(GRUB_MACHINE_EFI)
static grub_command_t cmdimg_efi;
static grub_command_t cmddev_efi;
#endif

GRUB_MOD_INIT(miray_diag)
{
  cmd_test_bootstruct = grub_register_command ("miray_bootstruct_offsets",
                  miray_cmd_test_bootstruct,
                  0, N_("Print int 13 ah=0x412 result"));


#if defined(GRUB_MACHINE_PCBIOS)
  cmdhd_bios = grub_register_command ("miray_hdextinfo_bios",
                  miray_cmd_diag_bios_print_hdextinfo,
                  0, N_("Print int 13 ah=0x412 result"));

  cmdcd_bios = grub_register_command ("miray_cdinfo_bios",
                  miray_cmd_diag_bios_print_cdinfo,
				      0, N_("Print int 13 0x4B01 result"));
#endif  

#if defined(GRUB_MACHINE_EFI)
  cmdimg_efi = grub_register_command ("miray_image_path",
                  miray_cmd_diag_efi_print_image_path,
				      0, N_("Print loaded image path"));
  cmddev_efi = grub_register_command ("miray_device_path",
                  miray_cmd_diag_efi_print_device_path,
				      0, N_("Print device path for device"));
#endif
}

GRUB_MOD_FINI(miray_diag)
{
  grub_unregister_command (cmd_test_bootstruct);
#if defined(GRUB_MACHINE_PCBIOS)
  grub_unregister_command (cmdcd_bios);
  grub_unregister_command (cmdhd_bios);
#elif defined(GRUB_MACHINE_EFI)
  grub_unregister_command (cmdimg_efi);
  grub_unregister_command (cmddev_efi);
#endif
}

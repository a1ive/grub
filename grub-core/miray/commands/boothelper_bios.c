/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) Miray Software 2010-2012
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
 
#include "miray_cmd_boothelper.h"

#include <grub/env.h>
#include <grub/device.h>
#include <grub/i386/pc/kernel.h>

static inline grub_uint8_t grub_boot_drive(void)
{
   return grub_boot_device >> 24;
}

static inline grub_uint8_t grub_install_dos_part(void)
{
   return grub_boot_device >> 16;
}

static inline grub_uint8_t grub_install_bsd_part(void)
{
   return grub_boot_device >> 8;
}
   
grub_err_t
grub_cmd_boot_native (grub_command_t cmd __attribute__ ((unused)),
	       int argc __attribute__ ((unused)),
	       char **args __attribute__ ((unused)) )
{
#if 0 // Does not yet work correctly
  if (grub_boot_drive() == 0x80)
  {
    char *func_args[] = {"-s", "(hd0)", "(hd1)"};
    grub_command_execute("drivemap", 3, func_args);
#if 0
  } else {
    grub_env_set("root", "hd0");
#endif
  }

  {
    grub_env_set("root", "hd0");
    const char *func_args[] = {"+1", 0};
    grub_command_execute("chainloader", 1, func_args);
  }

//  grub_command_execute("drivemap", 0, 0);
  grub_command_execute("boot", 0, 0);

  grub_exit();
#endif

   // TODO: FIXME!!
   return GRUB_ERR_NONE;
}

struct iterate_disks_data
{
   const char * partname;
};

static int iterate_disks(const char *name, void *data) 
{
   struct iterate_disks_data * it_data = (struct iterate_disks_data *) data;

   grub_dprintf("miray", "Iter: %s\n", name);
   if (grub_strcmp(name, it_data->partname) == 0)
      return 1;

   return 0;
}


grub_err_t
miray_machine_bootdev(char * buffer, grub_size_t buffer_size)
{
   grub_dprintf("miray", "Boot Device: %d\n", grub_boot_drive());
   grub_dprintf("miray", "grub_install_dos_part: %d\n", grub_install_dos_part());
   grub_dprintf("miray", "grub_install_bsd_part: %d\n", grub_install_bsd_part());


   if (grub_boot_drive() & 0x80) 
   {
      char partname[40];

      struct iterate_disks_data it_data = {
         .partname = partname
      };

      /* HDClone is always installed to the first partition */
      grub_snprintf(partname, 39, "hd%d,msdos1", grub_boot_drive() & 0x7f);


      if (grub_device_iterate(iterate_disks, &it_data) > 0)
      {
         grub_snprintf(buffer, buffer_size, "(%s)", partname);
      }
      else
      {
         grub_snprintf(buffer, buffer_size, "(hd%d)", grub_boot_drive() & 0x7f);
      }

   }
     else
   {
      /* FDD */
      grub_snprintf(buffer, buffer_size, "(fd%d)", grub_boot_drive() & 0x7f);

   }



  return GRUB_ERR_NONE;
}


void *
miray_machine_smbios_ptr(void)
{
   return 0; /* Currently not supported on BIOS */ 
}

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

#include <grub/misc.h>
#include <grub/mm.h>

#include <grub/efi/efi.h>
#include <grub/efi/disk.h>

#include <grub/miray_lib.h>


grub_err_t
miray_cmd_diag_efi_print_image_path(grub_command_t cmd __attribute__((unused)), int argc __attribute__((unused)), char **args __attribute__((unused)))
{
   grub_efi_loaded_image_t *image = NULL;
   char *device = NULL;

   image = grub_efi_get_loaded_image (grub_efi_image_handle);
   if (image == NULL)
   {
      grub_printf("get_loaded_image returned NULL\n");
      return -1;
   }

   device = grub_efidisk_get_device_name (image->device_handle);
   if (device == NULL)
   {
      grub_efi_device_path_t *dp;
      grub_efi_device_path_t *subpath;
      grub_efi_device_path_t *dp_copy;

      dp = grub_efi_get_device_path (image->device_handle);
      if (dp == 0)
        return 0;
      
      dp_copy = miray_efi_duplicate_device_path(dp);
      subpath = dp_copy;

      while ((subpath = miray_efi_chomp_device_path(subpath)) != 0)
      {
         device = miray_efi_get_diskname_from_path(subpath);
         if (device != 0) break;
      }

      grub_free(dp_copy);
   }

   grub_printf("loaded image path: %s\n", device);

   grub_free(device);

   return 0;
}

grub_err_t
miray_cmd_diag_efi_print_device_path(grub_command_t cmd __attribute__((unused)), int argc, char **argv)
{
   grub_disk_t disk = NULL;
   grub_efi_handle_t handle = NULL;
   grub_efi_device_path_t *dp = NULL;

   if (argc < 1) return -1;


   disk = grub_disk_open(argv[0]);
   if (disk == 0)
   {
      grub_printf("could not open disk %s\n", argv[0]);
      return -1;
   }

   handle = grub_efidisk_get_device_handle(disk);
   if (handle == 0)
   {
      grub_printf("could not get handle for %s\n", argv[0]);
      return -1;
   }

   dp = grub_efi_get_device_path (handle);
   if (dp == 0)
   {
      grub_printf("could not get device path for %s\n", argv[0]);
      return -1;
   }
   
   grub_efi_print_device_path(dp);

   {
      grub_efi_device_path_t *subpath;
      grub_efi_device_path_t *dp_copy = miray_efi_duplicate_device_path(dp);
      subpath = dp_copy;

      while ((subpath = miray_efi_chomp_device_path(subpath)) != 0)
      {
         grub_efi_print_device_path(subpath);
      }

      grub_free(dp_copy);
   }

   return 0;
}

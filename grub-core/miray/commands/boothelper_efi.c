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

#include <grub/misc.h>
#include <grub/mm.h>

#include <grub/efi/efi.h>
#include <grub/efi/disk.h>

#include <grub/miray_lib.h>


grub_err_t
grub_cmd_boot_native (grub_command_t cmd __attribute__ ((unused)),
	       int argc __attribute__ ((unused)),
	       char **args __attribute__ ((unused)) )
{
	// FIXME
	
	return GRUB_ERR_NONE;
			   
}

grub_err_t
miray_machine_bootdev(char * buffer, grub_size_t buffer_size)
{
   grub_efi_loaded_image_t *image = NULL;
   char *device = NULL;

   image = grub_efi_get_loaded_image (grub_efi_image_handle);
   if (image == NULL)
      return -1;

   device = grub_efidisk_get_device_name (image->device_handle);
   if (device == NULL)
   {
      // Try to find a boot device in the path,
      // This works for cd image boot
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

   if (device == NULL)
      return -1;

   grub_snprintf(buffer, buffer_size, "(%s)", device);


   grub_free (device);

   return GRUB_ERR_NONE;
}


static grub_efi_packed_guid_t smbios_guid = GRUB_EFI_SMBIOS_TABLE_GUID;

void *
miray_machine_smbios_ptr(void)
{
   /* this method is based on loadbios.c */ 
   
   void * smbios = 0;
   unsigned i;

   for (i = 0; i < grub_efi_system_table->num_table_entries; i++)
   {
      grub_efi_packed_guid_t *guid = &grub_efi_system_table->configuration_table[i].vendor_guid;

      if (! grub_memcmp (guid, &smbios_guid, sizeof (grub_efi_guid_t)))
      {
         smbios = grub_efi_system_table->configuration_table[i].vendor_table;
         grub_dprintf ("efi", "SMBIOS: %p\n", smbios);
      }
   }

   return smbios;
}

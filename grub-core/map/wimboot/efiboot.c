/*
 * Copyright (C) 2014 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

/**
 * @file
 *
 * EFI boot manager invocation
 *
 */

#include <stdio.h>
#include <string.h>
#include <vfat.h>
#include <efiapi.h>
#include <wimboot.h>
#include <maplib.h>

void
wimboot_boot (struct vfat_file *file)
{
  grub_efi_boot_services_t *b;
  b = grub_efi_system_table->boot_services;
  grub_efi_device_path_t *path = NULL;
  grub_efi_physical_address_t phys;
  void *data;
  unsigned int pages;
  grub_efi_handle_t handle;
  grub_efi_status_t status;

  /* Allocate memory */
  pages = ((file->len + PAGE_SIZE - 1) / PAGE_SIZE);
  status = efi_call_4 (b->allocate_pages, GRUB_EFI_ALLOCATE_ANY_PAGES,
                       GRUB_EFI_BOOT_SERVICES_DATA, pages, &phys);
  if (status != GRUB_EFI_SUCCESS)
  {
    die ("Could not allocate %d pages: 0x%lx\n", pages, ((unsigned long) status));
  }
  data = ((void *)(intptr_t) phys);

  /* Read image */
  file->read (file, data, 0, file->len);
  printf ("Read %s\n", file->name);

  /* DevicePath */
  path = grub_efi_file_device_path (grub_efi_get_device_path (wimboot_part.handle),
                                    EFI_REMOVABLE_MEDIA_FILE_NAME);
  /* Load image */
  status = efi_call_6 (b->load_image, FALSE, grub_efi_image_handle,
                         path, data, file->len, &handle);
  if (path)
    free (path);
  if (status != GRUB_EFI_SUCCESS)
  {
    die ("Could not load %s: 0x%lx\n", file->name, ((unsigned long) status));
  }
  printf ("Loaded %s\n", file->name);

  /* Start image */
  if (wimboot_cmd.pause)
    pause();
  status = efi_call_3 (b->start_image, handle, NULL, NULL);
  if (status != GRUB_EFI_SUCCESS)
  {
    die ("Could not start %s: 0x%lx\n", file->name, ((unsigned long) status));
  }
  die ("%s returned\n", file->name);
}

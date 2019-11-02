/*
 *  UEFI example
 *  Copyright (C) 2019  a1ive
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

#include <efi.h>
#include <efilib.h>
#include <grub.h>
#include <sstdlib.h>
#include <private.h>

/** EFI image handle */
EFI_HANDLE efi_image_handle;

/** GRUB PROTOCOL */
EFI_GUID grub_guid = GRUB_EFI_GRUB_PROTOCOL_GUID;
grub_efi_grub_protocol_t *grub = NULL;

struct grub_private_data *cmd;
vdisk_t vdisk, vpart;

static void
die (const char *fmt, ...)
{
  va_list args;
  /* Print message */
  va_start ( args, fmt );
  vprintf ( fmt, args );
  va_end ( args );
  /* Exit */
  uefi_call_wrapper (gBS->Exit, 4, efi_image_handle, EFI_LOAD_ERROR, 0, NULL);
}

void
pause (void)
{
  printf ( "Press any key to continue booting..." );
  uefi_call_wrapper (grub->wait_key, 0);
  printf ( "\n" );
}

EFI_STATUS
efi_main (EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *systab)
{
  EFI_STATUS status;
  EFI_HANDLE boot_image_handle = NULL;

  InitializeLib(image_handle, systab);
  efi_image_handle = image_handle;

  uefi_call_wrapper (gST->ConOut->ClearScreen, 1, gST->ConOut);
  printf ("UEFI TEST %p %p\n", image_handle, systab);

  status = uefi_call_wrapper (gBS->LocateProtocol, 3,
                              &grub_guid, NULL, (VOID **)&grub);
  if (status != EFI_SUCCESS)
  {
    die ("Could not open grub protocol: %#lx\n", ((unsigned long) status));
  }
  uefi_call_wrapper (grub->test, 0);
  uefi_call_wrapper (grub->private_data, 1, (VOID **)&cmd);

  printf ("mem: %d\n", cmd->mem);
  printf ("type: %d\n", cmd->type);
  printf ("file: %s\n", cmd->file->name);

  printf ("vdisk_install\n");
  status = vdisk_install (cmd->file);
  if (status != EFI_SUCCESS)
  {
    printf ("Failed to install vdisk.\n");
    goto fail;
  }
  if (vpart.handle)
    boot_image_handle = vpart_boot (vpart.handle);
  if (!boot_image_handle)
    boot_image_handle = vdisk_boot ();
  if (!boot_image_handle)
  {
    printf ("Failed to boot vdisk.\n");
    goto fail;
  }
  /* wait */
  if (cmd->pause)
    pause ();
  /* boot */
  status = uefi_call_wrapper (gBS->StartImage, 3, boot_image_handle, 0, NULL);
  printf ("StartImage returned %#lx\n", (unsigned long) status);
  return EFI_SUCCESS;
fail:
  printf ("Exit.\n");
  return status;
}

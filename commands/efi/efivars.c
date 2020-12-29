/*
 *  BURG - Brand-new Universal loadeR from GRUB
 *  Copyright 2010 Bean Lee - All Rights Reserved
 *
 *  BURG is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  BURG is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with BURG.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <grub/dl.h>
#include <grub/mm.h>
#include <grub/err.h>
#include <grub/misc.h>
#include <grub/term.h>
#include <grub/efi/efi.h>
#include <grub/command.h>
#include <grub/i18n.h>
#include <grub/lib.h>

static grub_err_t
grub_cmd_efivars (grub_command_t cmd __attribute__ ((unused)),
		  int argc __attribute__ ((unused)),
		  char *argv[] __attribute__ ((unused)))
{
  grub_efi_runtime_services_t *rt = grub_efi_system_table->runtime_services;
  grub_efi_guid_t vendor;
  grub_efi_char16_t *name = 0;
  char *nstr = 0;
  char *data = 0;
  int data_size, name_size;

  name_size = 64;
  name = grub_malloc (name_size);
  if (! name)
    goto quit;
  nstr = grub_malloc (name_size >> 1);
  if (! nstr)
    goto quit;
  data_size = 1024;
  data = grub_malloc (data_size);
  if (! data)
    goto quit;

  *name = 0;
  while (1)
    {
      grub_efi_status_t status;
      grub_efi_uintn_t size;
      grub_efi_uint32_t attr;
      grub_efi_char16_t *p1;
      char *p2;

      size = name_size;
      status = efi_call_3 (rt->get_next_variable_name, &size, name,
			   &vendor);
      if (status == GRUB_EFI_BUFFER_TOO_SMALL)
	{
	  name_size = size;
	  name = grub_realloc (name, name_size);
	  if (! name)
	    break;
	  nstr = grub_realloc (nstr, name_size >> 1);
	  if (! nstr)
	    break;
	  status = efi_call_3 (rt->get_next_variable_name, &size, name,
			       &vendor);
	}
      if (status != GRUB_EFI_SUCCESS)
	break;

      p1 = name;
      p2 = nstr;
      while (1)
	{
	  *p2 = *p1;
	  if (! *p1)
	    break;
	  p1++;
	  p2++;
	}

      grub_printf ("%s (%svolatile", nstr,
		   (attr & GRUB_EFI_VARIABLE_NON_VOLATILE) ? "non-" : "");
      if (attr & GRUB_EFI_VARIABLE_BOOTSERVICE_ACCESS)
	grub_printf (",boot");
      if (attr & GRUB_EFI_VARIABLE_RUNTIME_ACCESS)
	grub_printf (",runtime");
      grub_printf (")\n");

      size = data_size;
      status = efi_call_5 (rt->get_variable, name, &vendor, &attr, &size,
			   data);
      if (status == GRUB_EFI_BUFFER_TOO_SMALL)
	{
	  data_size = size;
	  data = grub_realloc (data, data_size);
	  if (! data)
	    break;
	  status = efi_call_5 (rt->get_variable, name, &vendor, &attr, &size,
			       data);
	}

      if ((! grub_strcmp (nstr, "ConIn")) ||
	  (! grub_strcmp (nstr, "ConInDev")) ||
	  (! grub_strcmp (nstr, "ConOut")) ||
	  (! grub_strcmp (nstr, "ConOutDev")) ||
	  (! grub_strcmp (nstr, "ErrOut")) ||
	  (! grub_strcmp (nstr, "ErrOutDev")))
	grub_efi_print_device_path ((grub_efi_device_path_t *) data);
      else
	hexdump (0, data, size);
      grub_putchar ('\n');
    }

 quit:
  grub_free (name);
  grub_free (nstr);
  grub_free (data);
  return grub_errno;
}

static grub_command_t cmd;

GRUB_MOD_INIT(efivars)
{
  cmd = grub_register_command ("efivars", grub_cmd_efivars,
			       0, N_("Display EFI variables."));

}

GRUB_MOD_FINI(efivars)
{
  grub_unregister_command (cmd);
}

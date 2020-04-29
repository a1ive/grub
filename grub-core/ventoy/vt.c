/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2020  Free Software Foundation, Inc.
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

#include <grub/types.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/env.h>
#include <grub/err.h>
#include <grub/dl.h>
#include <grub/extcmd.h>
#include <grub/i18n.h>
#include "vt_compatible.h"

#ifdef GRUB_MACHINE_EFI
#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#endif

#ifdef GRUB_MACHINE_PCBIOS
static int
get_vt_data (ventoy_os_param_t *data)
{
  grub_packed_guid_t ventoy_guid = VENTOY_GUID;
  grub_addr_t addr = VENTOY_BIOS_ADDR_L;
  while (addr < VENTOY_BIOS_ADDR_U)
  {
    if (grub_memcmp (&ventoy_guid, (void *)addr, sizeof (grub_packed_guid_t)))
    {
      addr++;
      continue;
    }
    grub_printf ("VentoyOsParam found.\n");
    grub_memcpy (data, (void *)addr, sizeof (ventoy_os_param_t));
    return 1;
  }
  return 0;
}
#elif defined (GRUB_MACHINE_EFI)
static int
get_vt_data (ventoy_os_param_t *data)
{
  grub_efi_guid_t ventoy_guid = VENTOY_GUID;
  void *value = NULL;
  grub_size_t datasize = 0;
  value = grub_efi_get_variable("VentoyOsParam", &ventoy_guid, &datasize);
  if (!value || !datasize || datasize != sizeof (ventoy_os_param_t))
  {
    if (value)
      grub_free (value);
    return 0;
  }
  grub_printf ("VentoyOsParam found.\n");
  grub_memcpy (data, value, sizeof (ventoy_os_param_t));
  grub_free (value);
  return 1;
}
#else
static int
get_vt_data (ventoy_os_param_t *data __attribute__ ((unused)))
{
  return 0;
}
#endif

grub_err_t
grub_cmd_ventoy (grub_extcmd_context_t ctxt __attribute__ ((unused)),
                 int argc, char **args)
{
  ventoy_os_param_t os_param;
  if (!get_vt_data (&os_param))
    return grub_error (GRUB_ERR_FILE_NOT_FOUND, N_("Ventoy data not found"));

  if (argc > 0)
    grub_env_set (args[0], os_param.vtoy_img_path);
  else
    grub_printf ("%s\n", os_param.vtoy_img_path);
  return GRUB_ERR_NONE;
}

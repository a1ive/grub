/* bootmgr.c - change EFI boot order.  */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2019  Free Software Foundation, Inc.
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

#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#include <grub/charset.h>
#include <grub/dl.h>
#include <grub/env.h>
#include <grub/err.h>
#include <grub/extcmd.h>
#include <grub/term.h>
#include <grub/i18n.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/types.h>

GRUB_MOD_LICENSE ("GPLv3+");

#define INSYDE_SETUP_VAR_SIZE		(0x2bc)
#define MAX_VARIABLE_SIZE		(1024)
#define MAX_VAR_DATA_SIZE		(65536)

static const struct grub_arg_option options_bootmgr[] = {
  {"set", 's', GRUB_ARG_OPTION_OPTIONAL,
   N_("Set BootOrder"),
   N_("####"), ARG_TYPE_STRING},
  {0, 0, 0, 0, 0, 0}
};

struct bootopts
{
  char *name; /* Boot####\0 */
  struct bootopts *next;
};

static grub_efi_uint16_t strtohex(char *in) {
  int len = (int)grub_strlen(in);
  if (len != 4)
    return 0;
  grub_efi_uint16_t out = 0;
  for (int i = 0; i < 4; i++)
  {
    if (in[i] >= '0' && in[i] <= '9')
      out = (out << 4) + in[i] - '0';
    if (in[i] >= 'a' && in[i] <= 'f')
      out = (out << 4) + in[i] - 'a' + 10;
    if (in[i] >= 'A' && in[i] <= 'F')
      out = (out << 4) + in[i] - 'A' + 10;
  }
  return out;
}

static void print_bootopts(char *name_str)
{
  grub_efi_uint16_t *bootvar = NULL;
  grub_efi_uint16_t bootvar_attr = 0;
  grub_efi_guid_t global = GRUB_EFI_GLOBAL_VARIABLE_GUID;
  grub_size_t datasize = 0;
  unsigned char bootvar_dsp[MAX_VARIABLE_SIZE/2+1];
  grub_printf ("%s: ", name_str);
  bootvar = grub_efi_get_variable (name_str, &global, &datasize);
  bootvar_attr = bootvar[0];
  int i = 6;
  if (bootvar_attr == 0) {
    grub_printf ("CB*"); i--;
  } /* category boot */
  if (bootvar_attr & 1) {
    grub_printf ("A* "); i--;
  } /* active */
  if (bootvar_attr & 2) {
    grub_printf ("FR*"); i--;
  } /* force reconnect */
  if (bootvar_attr & 8) {
    grub_printf ("H* "); i--;
  } /* hidden */
  if (bootvar_attr & 0x100) {
    grub_printf ("CA*"); i--;
  } /* category app */
  while (i--)
    grub_printf ("   ");
  grub_utf16_to_utf8 (bootvar_dsp, bootvar+3, MAX_VARIABLE_SIZE/2+1);
  grub_printf (" %s\n", bootvar_dsp);
}

static grub_err_t
grub_cmd_bootmgr (grub_extcmd_context_t ctxt, int argc __attribute__ ((unused)), char **args __attribute__ ((unused)))
{
  struct grub_arg_list *state = ctxt->state;
  grub_efi_status_t status;
  grub_efi_char16_t name[MAX_VARIABLE_SIZE/2];
  grub_efi_uintn_t name_size;
  unsigned char name_str[MAX_VARIABLE_SIZE/2+1];
  name[0] = 0x0;
  unsigned int i;
  grub_efi_uintn_t setup_var_size = INSYDE_SETUP_VAR_SIZE;
  grub_efi_uint32_t setup_var_attr = 0x7;
  grub_uint8_t tmp_data[MAX_VAR_DATA_SIZE];
  grub_efi_guid_t guid;
  grub_efi_guid_t global = GRUB_EFI_GLOBAL_VARIABLE_GUID;
  struct bootopts boot_list = {NULL, 0};
  struct bootopts *list = &boot_list;
  grub_efi_uint16_t *bootvar = NULL;
  grub_size_t datasize = 0;
  
  if (state[0].set)
  {
    bootvar = grub_efi_get_variable("BootOrder", &global, &datasize);
    bootvar[0] = strtohex (state[0].arg);
    grub_efi_set_variable ("BootOrder", &global, bootvar, datasize);
    grub_printf ("Set first boot to Boot%04x\n", bootvar[0]);
    goto done;
  }

  do
  {
    name_size = MAX_VARIABLE_SIZE;
    status = efi_call_3(grub_efi_system_table->runtime_services->get_next_variable_name,
      &name_size, name, &guid);

    if(status == GRUB_EFI_NOT_FOUND)
      break;

    if(! status)
    {
      
      setup_var_size = 1;
      status = efi_call_5(grub_efi_system_table->runtime_services->get_variable, 
        name, &guid, &setup_var_attr, &setup_var_size, tmp_data);
      if (status && status != GRUB_EFI_BUFFER_TOO_SMALL)
      {
          grub_printf("error (0x%x) getting var size:\n  ", (grub_uint32_t)status);
          setup_var_size = 0;
      }
      
      status = 0;
      grub_utf16_to_utf8 (name_str, name, MAX_VARIABLE_SIZE/2+1);

      if (!grub_strncmp((char *) name_str, "Boot", 4) &&
           grub_isdigit(name_str[4]) && grub_isdigit(name_str[5]) &&
           grub_isdigit(name_str[6]) && grub_isdigit(name_str[7]) )
      {
        list->next = grub_malloc (sizeof (struct bootopts));
        if (!list->next)
        {
          grub_errno = GRUB_ERR_OUT_OF_MEMORY;
          goto done;
        }
        list->name = grub_strdup ((const char *) name_str);
        list = list->next;
        list->next = NULL;
      }
    }
  } while (1);
  list = &boot_list;
  while (list->next)
  {
    if (!list->name)
      break;
    print_bootopts(list->name);
    list = list->next;
  }
  
  bootvar = grub_efi_get_variable("BootCurrent", &global, &datasize);
  if (bootvar != NULL)
    grub_printf ("BootCurrent: %04x\n", *bootvar);

  bootvar = grub_efi_get_variable("BootOrder", &global, &datasize);
  if (bootvar != NULL)
  {
    grub_printf ("BootOrder:\n");
    for (i=0; i<(datasize/2); i++)
      grub_printf (" %04x", bootvar[i]);
    grub_printf ("\n");
  }
  grub_errno = GRUB_ERR_NONE;
done:
  /* TODO: free */
  return grub_errno;
}

static grub_extcmd_t cmd_bootmgr;

GRUB_MOD_INIT(bootmgr)
{
  cmd_bootmgr = grub_register_extcmd ("bootmgr", grub_cmd_bootmgr, 0,
				   N_("[--set=####]"),
				   N_("Show/set BootOrder, show Boot####, BootCurrent"),
				   options_bootmgr);
}

GRUB_MOD_FINI(bootmgr)
{
  grub_unregister_extcmd (cmd_bootmgr);
}

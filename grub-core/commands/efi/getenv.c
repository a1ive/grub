/* getenv.c - retrieve EFI variables.  */
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
#include <grub/dl.h>
#include <grub/env.h>
#include <grub/err.h>
#include <grub/extcmd.h>
#include <grub/i18n.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/types.h>

GRUB_MOD_LICENSE ("GPLv3+");

static const struct grub_arg_option options_getenv[] = {
  {"var-name", 'e', 0,
   N_("Environment variable to query"),
   N_("VARNAME"), ARG_TYPE_STRING},
  {"var-guid", 'g', 0,
   N_("GUID of environment variable to query"),
   N_("GUID"), ARG_TYPE_STRING},
  {"type", 't', GRUB_ARG_OPTION_OPTIONAL,
   N_("Parse EFI_VAR as specific type (hex, uint8, string). Default: hex."),
   N_("TYPE"), ARG_TYPE_STRING},
  {0, 0, 0, 0, 0, 0}
};

enum options_getenv
{
  GETENV_VAR_NAME,
  GETENV_VAR_GUID,
  GETENV_VAR_TYPE
};

enum efi_var_type
{
  EFI_VAR_STRING = 0,
  EFI_VAR_UINT8,
  EFI_VAR_HEX,
  EFI_VAR_INVALID = -1
};

static enum efi_var_type
parse_efi_var_type (const char *type)
{
  if (!grub_strncmp (type, "string", sizeof("string")))
    return EFI_VAR_STRING;

  if (!grub_strncmp (type, "uint8", sizeof("uint8")))
    return EFI_VAR_UINT8;

  if (!grub_strncmp (type, "hex", sizeof("hex")))
    return EFI_VAR_HEX;

  return EFI_VAR_INVALID;
}

static grub_err_t
grub_cmd_getenv (grub_extcmd_context_t ctxt, int argc, char **args)
{
  struct grub_arg_list *state = ctxt->state;
  char *envvar = NULL, *guid = NULL, *data = NULL, *setvar = NULL;
  grub_size_t datasize = 0;
  grub_efi_guid_t efi_var_guid;
  enum efi_var_type efi_type = EFI_VAR_HEX;
  grub_efi_guid_t global = GRUB_EFI_GLOBAL_VARIABLE_GUID;
  unsigned int i;

  if (state[GETENV_VAR_TYPE].set)
    efi_type = parse_efi_var_type (state[GETENV_VAR_TYPE].arg);

  if (efi_type == EFI_VAR_INVALID)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("invalid EFI variable type"));
    
  if (!state[GETENV_VAR_NAME].set)
    {
      grub_error (GRUB_ERR_INVALID_COMMAND, N_("-e is required"));
      goto done;
    }

  if (argc != 1)
    {
      grub_error (GRUB_ERR_BAD_ARGUMENT, N_("unexpected arguments"));
      goto done;
    }

  envvar = state[GETENV_VAR_NAME].arg;
  
  if (state[GETENV_VAR_GUID].set)
    {
      guid = state[GETENV_VAR_GUID].arg;

      if (grub_strlen(guid) != 36 ||
        guid[8] != '-' ||
        guid[13] != '-' ||
        guid[18] != '-' ||
        guid[23] != '-')
      {
        grub_error (GRUB_ERR_BAD_ARGUMENT, N_("invalid GUID"));
        goto done;
      }

      guid[8] = 0;
      efi_var_guid.data1 = grub_strtoul(guid, NULL, 16);
      guid[13] = 0;
      efi_var_guid.data2 = grub_strtoul(guid + 9, NULL, 16);
      guid[18] = 0;
      efi_var_guid.data3 = grub_strtoul(guid + 14, NULL, 16);
      efi_var_guid.data4[7] = grub_strtoul(guid + 34, NULL, 16);
      guid[34] = 0;
      efi_var_guid.data4[6] = grub_strtoul(guid + 32, NULL, 16);
      guid[32] = 0;
      efi_var_guid.data4[5] = grub_strtoul(guid + 30, NULL, 16);
      guid[30] = 0;
      efi_var_guid.data4[4] = grub_strtoul(guid + 28, NULL, 16);
      guid[28] = 0;
      efi_var_guid.data4[3] = grub_strtoul(guid + 26, NULL, 16);
      guid[26] = 0;
      efi_var_guid.data4[2] = grub_strtoul(guid + 24, NULL, 16);
      guid[23] = 0;
      efi_var_guid.data4[1] = grub_strtoul(guid + 21, NULL, 16);
      guid[21] = 0;
      efi_var_guid.data4[0] = grub_strtoul(guid + 19, NULL, 16);
    }
  else
    efi_var_guid = global;
  
  data = grub_efi_get_variable(envvar, &efi_var_guid, &datasize);
  

  if (!data || !datasize)
    {
      grub_error (GRUB_ERR_FILE_NOT_FOUND, N_("No such variable"));
      goto done;
    }

  switch (efi_type)
  {
    case EFI_VAR_STRING:
      setvar = grub_malloc (datasize + 1);
      if (!setvar)
        {
          grub_error (GRUB_ERR_OUT_OF_MEMORY, N_("out of memory"));
          break;
        }
      grub_memcpy(setvar, data, datasize);
      setvar[datasize] = '\0';
      break;

    case EFI_VAR_UINT8:
      setvar = grub_malloc (4);
      if (!setvar)
        {
          grub_error (GRUB_ERR_OUT_OF_MEMORY, N_("out of memory"));
          break;
        }
      grub_snprintf (setvar, 4, "%u", *((grub_uint8_t *)data));
      break;

    case EFI_VAR_HEX:
      setvar = grub_malloc (datasize * 2 + 1);
      if (!setvar)
        {
          grub_error (GRUB_ERR_OUT_OF_MEMORY, N_("out of memory"));
          break;
        }
      for (i = 0; i < datasize; i++)
        grub_snprintf (setvar + (i * 2), 3, "%02x", ((grub_uint8_t *)data)[i]);
      break;

    default:
      grub_error (GRUB_ERR_BUG, N_("should not happen (bug in module?)"));
  }
  
  grub_env_set (args[0], setvar);
  
  if (setvar)
    grub_free (setvar);

  grub_errno = GRUB_ERR_NONE;

done:
  grub_free(data);
  return grub_errno;
}

static grub_extcmd_t cmd_getenv;

GRUB_MOD_INIT(getenv)
{
  cmd_getenv = grub_register_extcmd ("getenv", grub_cmd_getenv, 0,
				   N_("-e ENVVAR [-g GUID] [-t TYPE] SETVAR"),
				   N_("Read a firmware environment variable"),
				   options_getenv);
}

GRUB_MOD_FINI(getenv)
{
  grub_unregister_extcmd (cmd_getenv);
}

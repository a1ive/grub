/* reboot.c - command to reboot the computer.  */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2005,2007,2008,2020  Free Software Foundation, Inc.
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

#include <grub/dl.h>
#include <grub/command.h>
#include <grub/misc.h>
#include <grub/i18n.h>

#ifdef GRUB_MACHINE_EFI
#include <grub/extcmd.h>
#include <grub/kernel.h>
#include <grub/loader.h>
#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#endif

GRUB_MOD_LICENSE ("GPLv3+");

static grub_err_t __attribute__ ((noreturn))
grub_cmd_reboot (grub_command_t cmd __attribute__ ((unused)),
                 int argc __attribute__ ((unused)),
                 char **args __attribute__ ((unused)))
{
  grub_reboot ();
}

#ifdef GRUB_MACHINE_EFI
static grub_efi_boolean_t
efifwsetup_is_supported (void)
{
  grub_efi_uint64_t *os_indications_supported = NULL;
  grub_size_t oi_size = 0;
  grub_efi_guid_t global = GRUB_EFI_GLOBAL_VARIABLE_GUID;
  os_indications_supported = grub_efi_get_variable ("OsIndicationsSupported",
                              &global, &oi_size);
  if (!os_indications_supported)
    return 0;
  if (*os_indications_supported & GRUB_EFI_OS_INDICATIONS_BOOT_TO_FW_UI)
    return 1;
  return 0;
}

static grub_err_t
efifwsetup_setvar (void)
{
  grub_efi_uint64_t *old_os_indications;
  grub_efi_uint64_t os_indications = GRUB_EFI_OS_INDICATIONS_BOOT_TO_FW_UI;
  grub_err_t status;
  grub_size_t oi_size;
  grub_efi_guid_t global = GRUB_EFI_GLOBAL_VARIABLE_GUID;
  old_os_indications = grub_efi_get_variable ("OsIndications", &global, &oi_size);
  if (old_os_indications != NULL && oi_size == sizeof (os_indications))
    os_indications |= *old_os_indications;
  status = grub_efi_set_variable ("OsIndications", &global, &os_indications,
                                  sizeof (os_indications));
  return status;
}

static grub_err_t
grub_cmd_fwsetup (grub_command_t cmd __attribute__ ((unused)),
                  int argc __attribute__ ((unused)),
                  char **args __attribute__ ((unused)))
{
  grub_err_t status;
  status = efifwsetup_setvar ();
  if (status != GRUB_ERR_NONE)
    return status;
  grub_reboot ();
  return GRUB_ERR_BUG;
}

static const struct grub_arg_option options[] =
{
  {"shutdown", 's', 0, N_("Perform a shutdown."), 0, 0},
  {"warm", 'w', 0, N_("Perform a warm boot."), 0, 0},
  {"cold", 'c', 0, N_("Perform a cold boot. [default]"), 0, 0},
  {"fwui", 'f', 0, N_("Perform a reset back to the firmware user interface."), 0, 0},
  {0, 0, 0, 0, 0, 0}
};

enum options_setenv
{
  RESET_S,
  RESET_W,
  RESET_C,
  RESET_F,
};

static grub_err_t __attribute__ ((noreturn))
grub_cmd_reset (grub_extcmd_context_t ctxt,
                int argc __attribute__ ((unused)),
                char **args __attribute__ ((unused)))
{
  struct grub_arg_list *state = ctxt->state;
  grub_efi_reset_type_t rst = GRUB_EFI_RESET_COLD;

  if (state[RESET_S].set)
    rst = GRUB_EFI_RESET_SHUTDOWN;
  if (state[RESET_W].set)
    rst = GRUB_EFI_RESET_WARM;
  if (state[RESET_C].set)
    rst = GRUB_EFI_RESET_COLD;
  if (state[RESET_F].set && efifwsetup_is_supported ())
    efifwsetup_setvar ();
  grub_machine_fini (GRUB_LOADER_FLAG_NORETURN |
                     GRUB_LOADER_FLAG_EFI_KEEP_ALLOCATED_MEMORY);
  efi_call_4 (grub_efi_system_table->runtime_services->reset_system,
              rst, GRUB_EFI_SUCCESS, 0, NULL);
  for (;;) ;
}

static grub_extcmd_t reset_cmd;
static grub_command_t fwsetup_cmd = NULL;
#else
static grub_command_t reset_cmd;
#endif

static grub_command_t reboot_cmd;

GRUB_MOD_INIT(reboot)
{
  reboot_cmd = grub_register_command ("reboot", grub_cmd_reboot,
                  0, N_("Reboot the computer."));
#ifdef GRUB_MACHINE_EFI
  reset_cmd = grub_register_extcmd ("reset", grub_cmd_reset, 0,
                  N_("[-w|-s|-c] [-f]"),
                  N_("Reset the system."), options);
  if (efifwsetup_is_supported ())
    fwsetup_cmd = grub_register_command ("fwsetup", grub_cmd_fwsetup, NULL,
                  N_("Reboot into firmware setup menu."));
#else
  reset_cmd = grub_register_command ("reset", grub_cmd_reboot,
                  0, N_("Reboot the computer."));
#endif
}

GRUB_MOD_FINI(reboot)
{
  grub_unregister_command (reboot_cmd);
#ifdef GRUB_MACHINE_EFI
  grub_unregister_extcmd (reset_cmd);
  if (fwsetup_cmd)
    grub_unregister_command (fwsetup_cmd);
#else
  grub_unregister_command (reset_cmd);
#endif
}

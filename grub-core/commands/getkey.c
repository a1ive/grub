/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2017  Free Software Foundation, Inc.
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
#include <grub/misc.h>
#include <grub/term.h>
#include <grub/env.h>
#include <grub/extcmd.h>
#include <grub/i18n.h>

GRUB_MOD_LICENSE ("GPLv3+");

static const struct grub_arg_option options[] =
  {
    {0, 'n', 0, N_("grub_getkey_noblock"), 0, 0},
    {0, 0, 0, 0, 0, 0}
  };

static grub_err_t
grub_cmd_getkey (grub_extcmd_context_t ctxt, int argc, char **args)

{
  struct grub_arg_list *state = ctxt->state;
  int key;
  char keyenv[20];
  if (state[0].set)
    key = grub_getkey_noblock ();
  else
    key = grub_getkey ();

  grub_printf ("0x%08x\n", key);
  if (argc == 1)
    {
      grub_snprintf (keyenv, 20, "%d", key);
      grub_env_set (args[0], keyenv);
    }
  return GRUB_ERR_NONE;
}

static grub_extcmd_t cmd;

GRUB_MOD_INIT(getkey)
{
  cmd = grub_register_extcmd ("getkey", grub_cmd_getkey,
			      GRUB_COMMAND_ACCEPT_DASH
			      | GRUB_COMMAND_OPTIONS_AT_START,
			       N_("[-n] [VARNAME]"),
			       N_("Return the value of the pressed key. "),
			       options);
}

GRUB_MOD_FINI(getkey)
{
  grub_unregister_extcmd (cmd);
}

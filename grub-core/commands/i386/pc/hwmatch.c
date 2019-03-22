/* hwmatch.c - Match hardware against a whitelist/blacklist.  */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2011  Free Software Foundation, Inc.
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
#include <grub/command.h>
#include <grub/pci.h>
#include <grub/normal.h>
#include <grub/file.h>
#include <grub/env.h>
#include <grub/i18n.h>
#include <regex.h>

GRUB_MOD_LICENSE ("GPLv3+");

/* Context for grub_cmd_hwmatch.  */
struct hwmatch_ctx
{
  grub_file_t matches_file;
  int class_match;
  int match;
};

/* Helper for grub_cmd_hwmatch.  */
static int
hwmatch_iter (grub_pci_device_t dev, grub_pci_id_t pciid, void *data)
{
  struct hwmatch_ctx *ctx = data;
  grub_pci_address_t addr;
  grub_uint32_t class, baseclass, vendor, device;
  grub_pci_id_t subpciid;
  grub_uint32_t subvendor, subdevice, subclass;
  char *id, *line;

  addr = grub_pci_make_address (dev, GRUB_PCI_REG_CLASS);
  class = grub_pci_read (addr);
  baseclass = class >> 24;

  if (ctx->class_match != baseclass)
    return 0;

  vendor = pciid & 0xffff;
  device = pciid >> 16;

  addr = grub_pci_make_address (dev, GRUB_PCI_REG_SUBVENDOR);
  subpciid = grub_pci_read (addr);

  subclass = (class >> 16) & 0xff;
  subvendor = subpciid & 0xffff;
  subdevice = subpciid >> 16;

  id = grub_xasprintf ("v%04xd%04xsv%04xsd%04xbc%02xsc%02x",
		       vendor, device, subvendor, subdevice,
		       baseclass, subclass);

  grub_file_seek (ctx->matches_file, 0);
  while ((line = grub_file_getline (ctx->matches_file)) != NULL)
    {
      char *anchored_line;
      regex_t regex;
      int ret;

      if (! *line || *line == '#')
	{
	  grub_free (line);
	  continue;
	}

      anchored_line = grub_xasprintf ("^%s$", line);
      ret = regcomp (&regex, anchored_line, REG_EXTENDED | REG_NOSUB);
      grub_free (anchored_line);
      if (ret)
	{
	  grub_free (line);
	  continue;
	}

      ret = regexec (&regex, id, 0, NULL, 0);
      regfree (&regex);
      grub_free (line);
      if (! ret)
	{
	  ctx->match = 1;
	  return 1;
	}
    }

  return 0;
}

static grub_err_t
grub_cmd_hwmatch (grub_command_t cmd __attribute__ ((unused)),
		  int argc, char **args)
{
  struct hwmatch_ctx ctx = { .match = 0 };
  char *match_str;

  if (argc < 2)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "list file and class required");

  ctx.matches_file = grub_file_open (args[0], GRUB_FILE_TYPE_CONFIG);
  if (! ctx.matches_file)
    return grub_errno;

  ctx.class_match = grub_strtol (args[1], 0, 10);

  grub_pci_iterate (hwmatch_iter, &ctx);

  match_str = grub_xasprintf ("%d", ctx.match);
  grub_env_set ("match", match_str);
  grub_free (match_str);

  grub_file_close (ctx.matches_file);

  return GRUB_ERR_NONE;
}

static grub_command_t cmd;

GRUB_MOD_INIT(hwmatch)
{
  cmd = grub_register_command ("hwmatch", grub_cmd_hwmatch,
			       N_("MATCHES-FILE CLASS"),
			       N_("Match PCI devices."));
}

GRUB_MOD_FINI(hwmatch)
{
  grub_unregister_command (cmd);
}

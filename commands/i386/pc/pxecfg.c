/*
 *  BURG - Brand-new Universal loadeR from GRUB
 *  Copyright 2009 Bean Lee - All Rights Reserved
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
#include <grub/env.h>
#include <grub/misc.h>
#include <grub/command.h>
#include <grub/uitree.h>
#include <grub/parser.h>
#include <grub/machine/pxe.h>

static int
match_string (grub_uitree_t node, const char *str)
{
  grub_uitree_t child;
  char *cmd;

  child = node->child;
  while (child)
    {
      int len;

      len = grub_strlen (child->name);
      if (! grub_memcmp (child->name, str, len))
	{
	  const char *p;

	  p = str + len;
	  if ((*p == '.') || (*p == '-'))
	    p++;

	  if (match_string (child, p))
	    return 1;
	}

      child = child->next;
    }

  cmd = grub_uitree_get_prop (node, "command");
  if (cmd)
    {
      grub_parser_execute (cmd);
      return 1;
    }

  return 0;
}

static void
pxe_find (grub_uitree_t node)
{
  const char *mac, *ip;

  mac = grub_env_get ("net_pxe_mac");
  ip = grub_env_get ("net_pxe_ip");

  node = node->child;
  while (node)
    {
      if ((! grub_strcmp (node->name, "mac")) && (mac))
	match_string (node, mac);
      else if ((! grub_strcmp (node->name, "ip")) && (ip))
	match_string (node, ip);

      node = node->next;
    }
}

static grub_err_t
grub_cmd_pxecfg (grub_command_t cmd __attribute__ ((unused)),
		 int argc, char **args)
{
  struct grub_uitree root;

  if (! grub_pxe_pxenv)
    return grub_error (GRUB_ERR_FILE_NOT_FOUND, "no pxe environment");

  grub_memset (&root, 0, sizeof (root));
  while (argc)
    {
      grub_uitree_load_file (&root, *args, GRUB_UITREE_LOAD_METHOD_MERGE);
      argc--;
      args++;
    }

  pxe_find (&root);

  return grub_errno;
}

static grub_command_t cmd;

GRUB_MOD_INIT(pxecfg)
{
  cmd = grub_register_command ("pxecfg", grub_cmd_pxecfg,
			       "pxecfg FILES", "Load PXE config file");
}

GRUB_MOD_FINI(pxecfg)
{
  grub_unregister_command (cmd);
}

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
#include <grub/command.h>
#include <grub/uitree.h>

static grub_err_t
grub_cmd_dump (grub_command_t cmd __attribute__ ((unused)),
	       int argc, char **args)
{
  grub_uitree_t root;

  root = &grub_uitree_root;
  if (argc)
    {
      root = grub_uitree_find (root, args[0]);
      if (! root)
	return grub_error (GRUB_ERR_BAD_ARGUMENT, "section not found");
    }

  grub_uitree_dump (root);

  return grub_errno;
}

static grub_err_t
grub_cmd_free (grub_command_t cmd __attribute__ ((unused)),
	       int argc, char **args)
{
  while (argc)
    {
      grub_uitree_t node;

      node = grub_uitree_find (&grub_uitree_root, args[0]);
      if (node)
	{
	  grub_tree_remove_node (GRUB_AS_TREE (node));
	  grub_uitree_free (node);
	}
      argc--;
      args++;
    }

  return grub_errno;
}

static grub_err_t
grub_cmd_loadcfg (grub_command_t cmd, int argc, char **args)
{
  while (argc)
    {
      if (cmd->name[5] == 's')
	grub_uitree_load_string (&grub_uitree_root, *args,
			       GRUB_UITREE_LOAD_FLAG_ROOT);
      else
	grub_uitree_load_file (&grub_uitree_root, *args,
			       GRUB_UITREE_LOAD_FLAG_ROOT);

      if (grub_errno == GRUB_ERR_FILE_NOT_FOUND)
	grub_errno = 0;
      else if (grub_errno)
	break;

      argc--;
      args++;
    }

  return grub_errno;
}

static grub_command_t cmd_dump, cmd_load, cmd_str, cmd_free;

GRUB_MOD_INIT(loadcfg)
{
  cmd_dump =
    grub_register_command ("dump_config", grub_cmd_dump,
			   "dump_config [NAMES]", "Dump config section.");
  cmd_free =
    grub_register_command ("free_config", grub_cmd_free,
			   "free_config [NAMES]", "Free config section.");
  cmd_load =
    grub_register_command ("load_config", grub_cmd_loadcfg,
			   "load_config [FILES]", "Load config file.");
  cmd_str =
    grub_register_command ("load_string", grub_cmd_loadcfg,
			   "load_string [STRINGS]", "Load config string.");
}

GRUB_MOD_FINI(loadcfg)
{
  grub_unregister_command (cmd_dump);
  grub_unregister_command (cmd_free);
  grub_unregister_command (cmd_load);
  grub_unregister_command (cmd_str);
}

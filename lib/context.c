/* env.c - Environment variables */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2003,2005,2006,2007,2008,2009  Free Software Foundation, Inc.
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

#include <grub/env.h>
#include <grub/env_private.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/command.h>
#include <grub/normal.h>

GRUB_EXPORT(grub_env_context_open);
GRUB_EXPORT(grub_env_context_close);
GRUB_EXPORT(grub_env_export);

grub_err_t
grub_env_context_open (int export)
{
  struct grub_env_context *context;
  int i;
  struct menu_pointer *menu;

  context = grub_zalloc (sizeof (*context));
  if (! context)
    return grub_errno;
  menu = grub_zalloc (sizeof (*menu));
  if (! menu)
    return grub_errno;

  context->prev = grub_current_context;
  grub_current_context = context;

  menu->prev = grub_current_menu;
  grub_current_menu = menu;

  /* Copy exported variables.  */
  for (i = 0; i < HASHSZ; i++)
    {
      struct grub_env_var *var;

      for (var = context->prev->vars[i]; var; var = var->next)
	{
	  if (export && var->global)
	    {
	      if (grub_env_set (var->name, var->value) != GRUB_ERR_NONE)
		{
		  grub_env_context_close ();
		  return grub_errno;
		}
	      grub_env_export (var->name);
	      grub_register_variable_hook (var->name, var->read_hook, var->write_hook);
	    }
	}
    }

  return GRUB_ERR_NONE;
}

grub_err_t
grub_env_context_close (void)
{
  struct grub_env_context *context;
  int i;
  struct menu_pointer *menu;

  if (! grub_current_context->prev)
    return grub_error (GRUB_ERR_BAD_ARGUMENT,
		       "cannot close the initial context");

  /* Free the variables associated with this context.  */
  for (i = 0; i < HASHSZ; i++)
    {
      struct grub_env_var *p, *q;

      for (p = grub_current_context->vars[i]; p; p = q)
	{
	  q = p->next;
          grub_free (p->name);
	  grub_free (p->value);
	  grub_free (p);
	}
    }

  /* Restore the previous context.  */
  context = grub_current_context->prev;
  grub_free (grub_current_context);
  grub_current_context = context;

  menu = grub_current_menu->prev;
  grub_free (grub_current_menu);
  grub_current_menu = menu;

  return GRUB_ERR_NONE;
}

grub_err_t
grub_env_export (const char *name)
{
  struct grub_env_var *var;

  var = grub_env_find (name);
  if (! var)
    {
      grub_err_t err;

      err = grub_env_set (name, "");
      if (err)
	return err;
      var = grub_env_find (name);
    }
  var->global = 1;

  return GRUB_ERR_NONE;
}

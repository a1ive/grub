/* main.c - the kernel main routine */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2002,2003,2005,2006,2008,2009  Free Software Foundation, Inc.
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

#include <grub/kernel.h>
#include <grub/misc.h>
#include <grub/symbol.h>
#include <grub/dl.h>
#include <grub/term.h>
#include <grub/file.h>
#include <grub/device.h>
#include <grub/env.h>
#include <grub/mm.h>
#include <grub/command.h>
#include <grub/reader.h>
#include <grub/parser.h>
#include <grub/controller.h>
#include <grub/menu.h>

GRUB_EXPORT(grub_module_iterate);
GRUB_EXPORT(grub_machine_fini);
GRUB_EXPORT(grub_controller_class);

struct grub_handler_class grub_controller_class =
  {
    .name = "controller"
  };

void
grub_module_iterate (int (*hook) (struct grub_module_header *header))
{
  struct grub_module_info *info;
  struct grub_module_header *header;

  info = &grub_modinfo;

  /* Check if there are any modules.  */
  if (info->magic != GRUB_MODULE_MAGIC)
    return;

  for (header = (struct grub_module_header *) (info + 1);
       header < (struct grub_module_header *) ((char *) info + info->size);
       header = (struct grub_module_header *) ((char *) header + header->size))
    {
      if (hook (header))
	break;
    }
}

static int
grub_load_modules_hook (struct grub_module_header *header)
{
  grub_dl_t mod;
  struct grub_module_object *obj;
  char *code_start = &grub_code_start[0];
  char *name, *sym;
  grub_uint32_t *sym_value;

  /* Not a module, skip.  */
  if (header->type != OBJ_TYPE_OBJECT)
    return 0;

  obj = (struct grub_module_object *) (header + 1);

  mod = (grub_dl_t) grub_zalloc (sizeof (*mod));
  if (! mod)
    grub_fatal ("can't init module");

  name = obj->name;
  mod->name = grub_strdup (name);
  mod->ref_count++;

  grub_dl_resolve_dependencies (mod, name);

  sym = name + obj->symbol_name;
  sym_value = (grub_uint32_t *) (name + obj->symbol_value);
  while (*sym)
    {
      grub_dl_register_symbol (sym, code_start + *(sym_value++), mod);
      sym += grub_strlen (sym) + 1;
    }

  if (obj->init_func)
    {
      mod->init = (void (*) (grub_dl_t)) (code_start + obj->init_func);
      (mod->init) (mod);
    }

  if (obj->fini_func)
    mod->fini = (void (*) (void)) (code_start + obj->fini_func);

  grub_dl_add (mod);

  return 0;
}

/* Load all modules in core.  */
static inline void
grub_load_modules (void)
{
  grub_module_iterate (grub_load_modules_hook);
}

static int
grub_load_config_hook (struct grub_module_header *header)
{
  /* Not a config, skip.  */
  if (header->type != OBJ_TYPE_CONFIG)
    return 0;

  grub_parser_execute ((char *) header +
		       sizeof (struct grub_module_header));
  return 1;
}

static inline void
grub_load_config (void)
{
  char *p = grub_arch_menu_addr ();

  if (p)
    grub_parser_execute (p);
  else
    grub_module_iterate (grub_load_config_hook);
}

/* Write hook for the environment variables of root. Remove surrounding
   parentheses, if any.  */
static char *
grub_env_write_root (struct grub_env_var *var __attribute__ ((unused)),
		     const char *val)
{
  /* XXX Is it better to check the existence of the device?  */
  grub_size_t len = grub_strlen (val);

  if (val[0] == '(' && val[len - 1] == ')')
    return grub_strndup (val + 1, len - 2);

  return grub_strdup (val);
}

/* Set the root device according to the dl prefix.  */
static void
grub_set_root_dev (void)
{
  const char *prefix;

  grub_register_variable_hook ("root", 0, grub_env_write_root);

  prefix = grub_env_get ("prefix");

  if (prefix)
    {
      char *dev;

      dev = grub_file_get_device_name (prefix);
      if (dev)
	{
	  grub_env_set ("root", dev);
	  grub_free (dev);
	}
    }
}

/* Load the normal mode module and execute the normal mode if possible.  */
static void
grub_load_normal_mode (void)
{
  /* Load the module.  */
  grub_dl_load ("normal");

  /* Something went wrong.  Print errors here to let user know why we're entering rescue mode.  */
  grub_print_error ();
  grub_errno = 0;

  grub_command_execute ("normal", 0, 0);
}

/* The main routine.  */
void
grub_main (void)
{
  /* First of all, initialize the machine.  */
  grub_machine_init ();

  grub_env_set_menu (grub_zalloc (sizeof (struct grub_menu)));

#if 0
  /* Hello.  */
  grub_setcolorstate (GRUB_TERM_COLOR_HIGHLIGHT);
  grub_printf ("Welcome to GRUB!\n\n");
  grub_setcolorstate (GRUB_TERM_COLOR_STANDARD);
#endif

  /* Load pre-loaded modules and free the space.  */
#ifdef GRUB_LINKER_HAVE_INIT
  grub_arch_dl_init_linker ();
#endif
  grub_load_modules ();

  /* It is better to set the root device as soon as possible,
     for convenience.  */
  grub_machine_set_prefix ();
  grub_set_root_dev ();

  grub_register_core_commands ();
  grub_register_rescue_parser ();

  grub_load_config ();
  grub_load_normal_mode ();
  grub_rescue_run ();
}

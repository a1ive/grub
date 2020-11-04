/* menuentry.c - menuentry command */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2010  Free Software Foundation, Inc.
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
#include <grub/err.h>
#include <grub/dl.h>
#include <grub/extcmd.h>
#include <grub/i18n.h>
#include <grub/normal.h>
#include <grub/env_private.h>
#include <grub/mm.h>

static const struct grub_arg_option options[] =
  {
    {"class", 1, GRUB_ARG_OPTION_REPEATABLE,
     N_("Menu entry type."), N_("STRING"), ARG_TYPE_STRING},
    {"users", 2, GRUB_ARG_OPTION_OPTIONAL,
     N_("List of users allowed to boot this entry."), N_("USERNAME[,USERNAME]"),
     ARG_TYPE_STRING},
    {"hotkey", 3, 0,
     N_("Keyboard key to quickly boot this entry."), N_("KEYBOARD_KEY"), ARG_TYPE_STRING},
    {"source", 4, 0,
     N_("Use STRING as menu entry body."), N_("STRING"), ARG_TYPE_STRING},
    {"id", 0, 0, N_("Menu entry identifier."), N_("STRING"), ARG_TYPE_STRING},
    /* TRANSLATORS: menu entry can either be bootable by anyone or only by
       handful of users. By default when security is active only superusers can
       boot a given menu entry. With --unrestricted (this option)
       anyone can boot it.  */
    {"unrestricted", 0, 0, N_("This entry can be booted by any user."),
     0, ARG_TYPE_NONE},
    {"help", 0, GRUB_ARG_OPTION_OPTIONAL,  N_("Menu entry help message."), N_("STRING"), ARG_TYPE_STRING},
    {0, 0, 0, 0, 0, 0}
  };

static struct
{
  const char *name;
  int key;
} hotkey_aliases[] =
  {
    {"backspace", GRUB_TERM_BACKSPACE},
    {"tab", GRUB_TERM_TAB},
    {"delete", GRUB_TERM_KEY_DC},
    {"insert", GRUB_TERM_KEY_INSERT},
    {"esc", GRUB_TERM_ESC},
    {"f1", GRUB_TERM_KEY_F1},
    {"f2", GRUB_TERM_KEY_F2},
    {"f3", GRUB_TERM_KEY_F3},
    {"f4", GRUB_TERM_KEY_F4},
    {"f5", GRUB_TERM_KEY_F5},
    {"f6", GRUB_TERM_KEY_F6},
    {"f7", GRUB_TERM_KEY_F7},
    {"f8", GRUB_TERM_KEY_F8},
    {"f9", GRUB_TERM_KEY_F9},
    {"f10", GRUB_TERM_KEY_F10},
    {"f11", GRUB_TERM_KEY_F11},
    {"f12", GRUB_TERM_KEY_F12},
  };

extern int grub_normal_exit_level;

/* Add a menu entry to the current menu context (as given by the environment
   variable data slot `menu').  As the configuration file is read, the script
   parser calls this when a menu entry is to be created.  */
grub_err_t
grub_normal_add_menu_entry (int argc, const char **args,
			    char **classes, const char *id,
			    const char *users, const char *hotkey,
			    const char *prefix, const char *sourcecode, const char * help_message,
			    int submenu, int hidden, int *index, struct bls_entry *bls)
{
  int menu_hotkey = 0;
  char **menu_args = NULL;
  char *menu_users = NULL;
  char *menu_title = NULL;
  char *menu_sourcecode = NULL;
  char *menu_id = NULL;
  char *menu_help_message = NULL;
  struct grub_menu_entry_class *menu_classes = NULL;
  const char *enable_hotkey = NULL;

  grub_menu_t menu;
  grub_menu_entry_t *last;

  menu = grub_env_get_menu ();
  if (! menu)
    return grub_error (GRUB_ERR_MENU, "no menu context");

  last = &menu->entry_list;

  menu_sourcecode = grub_xasprintf ("%s%s", prefix ?: "", sourcecode);
  if (! menu_sourcecode)
    return grub_errno;

  if (classes && classes[0])
    {
      int i;
      for (i = 0; classes[i]; i++); /* count # of menuentry classes */
      menu_classes = grub_zalloc (sizeof (struct grub_menu_entry_class)
				  * (i + 1));
      if (! menu_classes)
	goto fail;

      for (i = 0; classes[i]; i++)
	{
	  menu_classes[i].name = grub_strdup (classes[i]);
	  if (! menu_classes[i].name)
	    goto fail;
	  menu_classes[i].next = classes[i + 1] ? &menu_classes[i + 1] : NULL;
	}
    }

  if (users)
    {
      menu_users = grub_strdup (users);
      if (! menu_users)
	goto fail;
    }

  if (hotkey)
    {
      unsigned i;
      for (i = 0; i < ARRAY_SIZE (hotkey_aliases); i++)
        if (grub_strcmp (hotkey, hotkey_aliases[i].name) == 0)
        {
          menu_hotkey = hotkey_aliases[i].key;
          break;
        }
      if (i == ARRAY_SIZE (hotkey_aliases))
      {
        if (grub_strlen (hotkey) >= 3 && hotkey[0] == '0' && hotkey[1] == 'x')
          menu_hotkey = (int) grub_strtoul (hotkey, NULL, 16);
        else
          menu_hotkey = hotkey[0];
      }
    }

  if(help_message) 
    {
      menu_help_message = grub_strdup(help_message);
      if(!menu_help_message)
         goto fail;
    }

  if (! argc)
    {
      grub_error (GRUB_ERR_MENU, "menuentry is missing title");
      goto fail;
    }

  enable_hotkey = grub_env_get ("grub_enable_menu_hotkey");
  if (enable_hotkey && enable_hotkey[0] == '1' && hotkey)
    menu_title = grub_xasprintf ("[%s] %s", hotkey, args[0]);
  else
    menu_title = grub_strdup (args[0]);
  if (! menu_title)
    goto fail;

  grub_dprintf ("menu", "id:\"%s\"\n", id);
  grub_dprintf ("menu", "title:\"%s\"\n", menu_title);
  menu_id = grub_strdup (id ? : menu_title);
  if (! menu_id)
    goto fail;
  grub_dprintf ("menu", "menu_id:\"%s\"\n", menu_id);

  /* Save argc, args to pass as parameters to block arg later. */
  menu_args = grub_calloc (argc + 1, sizeof (char *));
  if (! menu_args)
    goto fail;

  {
    int i;
    for (i = 0; i < argc; i++)
      {
	menu_args[i] = grub_strdup (args[i]);
	if (! menu_args[i])
	  goto fail;
      }
    menu_args[argc] = NULL;
  }

  /* Add the menu entry at the end of the list.  */
  int ind=0;
  while (*last)
    {
      ind++;
      last = &(*last)->next;
    }

  *last = grub_zalloc (sizeof (**last));
  if (! *last)
    goto fail;

  (*last)->title = menu_title;
  (*last)->id = menu_id;
  (*last)->hotkey = menu_hotkey;
  (*last)->classes = menu_classes;
  if (menu_users)
    (*last)->restricted = 1;
  (*last)->users = menu_users;
  (*last)->argc = argc;
  (*last)->args = menu_args;
  (*last)->sourcecode = menu_sourcecode;
  (*last)->submenu = submenu;
  (*last)->hidden = hidden;
  (*last)->bls = bls;
  (*last)->help_message = menu_help_message;

  if (!hidden)
    menu->size++;
  if (index)
    *index = ind;
  return GRUB_ERR_NONE;

 fail:

  grub_free (menu_sourcecode);
  {
    int i;
    for (i = 0; menu_classes && menu_classes[i].name; i++)
      grub_free (menu_classes[i].name);
    grub_free (menu_classes);
  }

  {
    int i;
    for (i = 0; menu_args && menu_args[i]; i++)
      grub_free (menu_args[i]);
    grub_free (menu_args);
  }
  grub_free (menu_help_message);
  grub_free (menu_users);
  grub_free (menu_title);
  grub_free (menu_id);
  return grub_errno;
}

void
grub_normal_clear_menu (void)
{
  grub_menu_t menu = grub_env_get_menu ();
  if (!menu)
    return;
  menu->entry_list = NULL;
  menu->size=0;
}

static char *
setparams_prefix (int argc, char **args)
{
  int i;
  int j;
  char *p;
  char *result;
  grub_size_t len = 10;

  /* Count resulting string length */
  for (i = 0; i < argc; i++)
    {
      len += 3; /* 3 = 1 space + 2 quotes */
      p = args[i];
      while (*p)
	len += (*p++ == '\'' ? 3 : 1);
    }

  result = grub_malloc (len + 2);
  if (! result)
    return 0;

  grub_strcpy (result, "setparams");
  p = result + 9;

  for (j = 0; j < argc; j++)
    {
      *p++ = ' ';
      *p++ = '\'';
      p = grub_strchrsub (p, args[j], '\'', "'\\''");
      *p++ = '\'';
    }
  *p++ = '\n';
  *p = '\0';
  return result;
}

static grub_err_t
grub_cmd_menuentry (grub_extcmd_context_t ctxt, int argc, char **args)
{
  char ch;
  char *src;
  char *prefix;
  unsigned len;
  grub_err_t r;
  const char *users;

  if (! argc)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "missing arguments");

  if (ctxt->state[3].set && ctxt->script)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "multiple menuentry definitions");

  if (! ctxt->state[3].set && ! ctxt->script)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "no menuentry definition");

  if (ctxt->state[1].set && ctxt->state[1].arg)
    users = ctxt->state[1].arg;
  else if (ctxt->state[5].set)
    users = NULL;
  else
    users = "";

  if (! ctxt->script)
    return grub_normal_add_menu_entry (argc, (const char **) args,
				       (ctxt->state[0].set ? ctxt->state[0].args
					: NULL),
				       ctxt->state[4].arg,
				       users,
				       ctxt->state[2].arg, 0,
				       ctxt->state[3].arg,
                                       (ctxt->state[6].set ? ctxt->state[6].arg : NULL),
				       ctxt->extcmd->cmd->name[0] == 's',
                       ctxt->extcmd->cmd->name[0] == 'h', NULL, NULL);

  src = args[argc - 1];
  args[argc - 1] = NULL;

  len = grub_strlen(src);
  ch = src[len - 1];
  src[len - 1] = '\0';

  prefix = setparams_prefix (argc - 1, args);
  if (! prefix)
    return grub_errno;

  r = grub_normal_add_menu_entry (argc - 1, (const char **) args,
				  ctxt->state[0].args, ctxt->state[4].arg,
				  users,
				  ctxt->state[2].arg, prefix, src + 1,
                                  (ctxt->state[6].set ? ctxt->state[6].arg : NULL),
				  ctxt->extcmd->cmd->name[0] == 's',
                  ctxt->extcmd->cmd->name[0] == 'h', NULL, NULL);

  src[len - 1] = ch;
  args[argc - 1] = src;
  grub_free (prefix);
  return r;
}

static grub_err_t
grub_cmd_pop_env (grub_extcmd_context_t ctxt __attribute__ ((unused)), int argc, char **args)
{

  while (argc)
    {
      struct grub_env_context *cc = grub_current_context;
      const char *value;

      value = grub_env_get (args[0]);
      if (value)
        {
	  grub_current_context = grub_current_context->prev;
	  while(grub_current_context && grub_env_get(args[0]))
	    {
              grub_env_set(args[0], value);
              grub_current_context = grub_current_context->prev;
	    }
          grub_current_context = cc;
        }
      argc--;
      args++;
    }

  return 0;
}

static grub_err_t
grub_cmd_submenu_exit (grub_extcmd_context_t ctxt __attribute__ ((unused)), int argc __attribute__ ((unused)), char **args __attribute__ ((unused)))
{
  grub_normal_exit_level = -1;
  return GRUB_ERR_NONE;
}

static grub_err_t
grub_cmd_clear_menu (grub_extcmd_context_t ctxt __attribute__ ((unused)), int argc __attribute__ ((unused)), char **args __attribute__ ((unused)))
{
  grub_normal_clear_menu ();
  return GRUB_ERR_NONE;
}

static grub_extcmd_t cmd, cmd_sub, cmd_hidden, cmd_pop, cmd_sub_exit, cmd_clear_menu;

void
grub_menu_init (void)
{
  cmd = grub_register_extcmd ("menuentry", grub_cmd_menuentry,
			      GRUB_COMMAND_FLAG_BLOCKS
			      | GRUB_COMMAND_ACCEPT_DASH
			      | GRUB_COMMAND_FLAG_EXTRACTOR,
			      N_("BLOCK"), N_("Define a menu entry."), options);
  cmd_sub = grub_register_extcmd ("submenu", grub_cmd_menuentry,
				  GRUB_COMMAND_FLAG_BLOCKS
				  | GRUB_COMMAND_ACCEPT_DASH
				  | GRUB_COMMAND_FLAG_EXTRACTOR,
				  N_("BLOCK"), N_("Define a submenu."),
				  options);
  cmd_hidden = grub_register_extcmd ("hiddenentry", grub_cmd_menuentry,
                GRUB_COMMAND_FLAG_BLOCKS
                | GRUB_COMMAND_ACCEPT_DASH
                | GRUB_COMMAND_FLAG_EXTRACTOR,
                N_("BLOCK"),
                N_("Define a hidden menu entry."),
                options);
  cmd_pop = grub_register_extcmd ("pop_env", grub_cmd_pop_env, 0,
			     N_("variable_name [...]"),
			     N_("Pass variable value to parent contexts."), 0);
  cmd_sub_exit = grub_register_extcmd ("submenu_exit", grub_cmd_submenu_exit, 0, 0,
			     N_("Exit from current submenu."), 0);
  cmd_clear_menu = grub_register_extcmd ("clear_menu", grub_cmd_clear_menu, 0, 0,
			     N_("Clear the current (sub)menu."), 0);
}

void
grub_menu_fini (void)
{
  grub_unregister_extcmd (cmd);
  grub_unregister_extcmd (cmd_sub);
  grub_unregister_extcmd (cmd_hidden);
  grub_unregister_extcmd (cmd_pop);
  grub_unregister_extcmd (cmd_sub_exit);
  grub_unregister_extcmd (cmd_clear_menu);
}

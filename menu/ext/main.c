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
#include <grub/term.h>
#include <grub/extcmd.h>
#include <grub/widget.h>
#include <grub/dialog.h>
#include <grub/menu_data.h>
#include <grub/menu_region.h>
#include <grub/controller.h>

static const struct grub_arg_option options[] =
  {
    {"string", 's', 0, "load from string", 0, 0},
    {"file", 'f', 0, "load from file.", 0, 0},
    {"copy", 'c', 0, "copy node.", 0, 0},
    {"index", 'i', 0, "set index", 0, ARG_TYPE_INT},
    {"relative", 'r', 0, "relative to current node.", 0, 0},
    {"template", 't', 0, "template to generate menu", 0, ARG_TYPE_STRING},
    {0, 0, 0, 0, 0, 0}
  };

static grub_uitree_t
get_screen (void)
{
  if (! grub_widget_screen)
    grub_error (GRUB_ERR_BAD_ARGUMENT, "menu not initialized");

  return grub_widget_screen;
}

static void
set_position (grub_uitree_t root, grub_uitree_t node)
{
  char buf[12], *p, *pos;
  grub_widget_t screen, widget;
  int v;
  int horizontal;
  int right, bottom;

  pos = grub_uitree_get_prop (grub_widget_current_node->parent, "popup");
  if (! pos)
    pos = "";
  else if (! grub_strcmp (pos, "abs"))
    return;

  horizontal = 0;
  p = grub_uitree_get_prop (grub_widget_current_node->parent, "direction");
  if ((p) &&
      ((! grub_strcmp (p, "left_to_right")) ||
       (! grub_strcmp (p, "right_to_left"))))
    horizontal = 1;

  screen = root->data;
  widget = grub_widget_current_node->data;

  right = (horizontal) ? -1 :
    (widget->org_x * 2 + widget->width <= screen->width);

  if (! grub_strcmp (pos, "right"))
    right = 1;
  else if (! grub_strcmp (pos, "left"))
    right = 0;

  if (right == 0)
    {
      p = "attach_right";
      v = screen->width - widget->org_x;
    }
  else
    {
      p = "attach_left";
      v = widget->org_x;
      if (right > 0)
	v += widget->width;
    }
  grub_snprintf (buf, sizeof (buf), "%d/%d", v, v);
  grub_uitree_set_prop (node, p, buf);

  bottom = (! horizontal) ? -1 :
    (widget->org_y * 2 + widget->height <= screen->height);

  if (! grub_strcmp (pos, "bottom"))
    bottom = 1;
  else if (! grub_strcmp (pos, "top"))
    bottom = 0;

  if (bottom == 0)
    {
      p = "attach_bottom";
      v = screen->height - widget->org_y;
    }
  else
    {
      p = "attach_top";
      v = widget->org_y;
      if (bottom > 0)
	v += widget->height;
    }
  grub_snprintf (buf, sizeof (buf), "%d/%d", v, v);
  grub_uitree_set_prop (node, p, buf);
}

static void add_sys_menu (const char *name, grub_uitree_t node, int index,
			  int default_num, int main_menu);

static grub_err_t
grub_cmd_popup (grub_extcmd_t cmd, int argc, char **args)
{
  struct grub_arg_list *state = cmd->state;
  grub_uitree_t root, node, menu, save;
  int edit, i, r;
  char *parm, *parm1;
  char *name;

  if (! argc)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "not enough parameter");

  if (! grub_widget_current_node)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "no current node");

  root = get_screen ();
  if (! root)
    return grub_errno;

  name = args[0];
  if (name[0] == ':')
    {
      name = grub_uitree_get_prop (grub_widget_current_node, name + 1);
      if (! name)
	return 0;
    }

  menu = save = 0;
  if (state[0].set)
    node = grub_uitree_load_string (root, name,
				    GRUB_UITREE_LOAD_FLAG_SINGLE |
				    GRUB_UITREE_LOAD_FLAG_ROOT);
  else if (state[1].set)
    node = grub_uitree_load_file (root, name,
				  GRUB_UITREE_LOAD_FLAG_SINGLE |
				  GRUB_UITREE_LOAD_FLAG_ROOT);
  else if (state[5].set)
    {
      grub_uitree_t p;

      node = grub_dialog_create (state[5].arg, 1, 0, 0, 0);
      if (! node)
	{
	  node = grub_dialog_create ("template_submenu", 1, 0, 0, 0);
	  if (! node)
	    return 0;
	}

      p = grub_uitree_find_id (node, "__child__");
      if (! p)
	p = node;

      grub_tree_add_child (GRUB_AS_TREE (root), GRUB_AS_TREE (node), -1);
      add_sys_menu (name, node, 0, -1, 0);
    }
  else
    {
      node = grub_dialog_create (name, state[2].set, (state[3].set) ?
				 grub_strtoul (state[3].arg, 0, 0) : 0,
				 &menu, &save);
      if (node)
	grub_tree_add_child (GRUB_AS_TREE (root), GRUB_AS_TREE (node), -1);
    }

  if (! node)
    return grub_errno;

  edit = (cmd->cmd->name[5] == 'e');
  parm = grub_uitree_get_prop (node, "parameters");
  parm1 = grub_uitree_get_prop (grub_widget_current_node, "parameters");

  for (i = 1; i < argc; i++)
    {
      char *p, *s;

      p = grub_menu_next_field (args[i], '=');
      if (! p)
	continue;

      s = (edit) ?
	grub_dialog_get_parm (grub_widget_current_node, parm1, p) : p;
      if (s)
	{
	  if (! grub_dialog_set_parm (node, parm, args[i], s))
	    {
	      grub_menu_restore_field (p, '=');
	      grub_dialog_free (node, menu, save);
	      return grub_error (GRUB_ERR_BAD_ARGUMENT,
				 "can\'t set parameter");
	    }
	}

      grub_menu_restore_field (p, '=');
    }

  if (state[4].set)
    set_position (root, node);

  r = grub_dialog_popup (node);

  if ((r == 0) && (edit))
    for (i = 1; i < argc; i++)
      {
	char *p, *s;

	p = grub_menu_next_field (args[i], '=');
	if (! p)
	  continue;

	s = grub_dialog_get_parm (node, parm, args[i]);
	if (s)
	  grub_dialog_set_parm (grub_widget_current_node, parm1, p, s);
	grub_menu_restore_field (p, '=');
      }

  grub_dialog_free (node, menu, save);
  grub_errno = r;
  return r;
}

static grub_err_t
grub_cmd_read (grub_command_t cmd __attribute__ ((unused)),
	       int argc, char **args)
{
  grub_uitree_t node;

  if (argc < 2)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "not enough parameter");

  if (! grub_widget_current_node)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "no current node");

  node = grub_widget_current_node;
  while (node)
    {
      char *parm;

      parm = grub_uitree_get_prop (node, "parameters");
      if (parm)
	{
	  char *result;

	  result = grub_dialog_get_parm (node, parm, args[0]);
	  if (result)
	    grub_env_set (args[1], result);
	  break;
	}
      node = node->parent;
    }

  return 0;
}

static grub_err_t
grub_cmd_refresh (grub_command_t cmd __attribute__ ((unused)),
		  int argc __attribute__ ((unused)),
		  char **args __attribute__ ((unused)))
{
  grub_widget_refresh = GRUB_WIDGET_REFRESH;
  return 0;
}

static grub_err_t
grub_cmd_toggle_mode (grub_command_t cmd __attribute__ ((unused)),
		      int argc __attribute__ ((unused)),
		      char **args __attribute__ ((unused)))
{
  grub_widget_refresh = GRUB_WIDGET_TOGGLE_MODE;
  return 0;
}

static grub_err_t
grub_cmd_reload_mode (grub_command_t cmd __attribute__ ((unused)),
		      int argc __attribute__ ((unused)),
		      char **args __attribute__ ((unused)))
{
  grub_widget_refresh = GRUB_WIDGET_RELOAD_MODE;
  return 0;
}

#if 0
  grub_uitree_t node;
  grub_menu_region_t region;
  char *p;

  node = get_screen ();
  if (! node)
    return grub_errno;

  grub_widget_free (node);
  region = grub_menu_region_get_current ();
  p = ((! region) || (! grub_strcmp (region->name, "gfx"))) ?
    "menu_region.text" : "menu_region.gfx";
  grub_command_execute (p, 0, 0);
  grub_command_execute ("menu_region.gfx", 0, 0);
  grub_widget_create (node);
  grub_widget_init (node);
  grub_widget_draw (node);

  return grub_errno;
#endif

static grub_uitree_t
create_menuitem (int main_menu)
{
  grub_uitree_t node;

  if (main_menu)
    node = grub_dialog_create ("template_menuitem", 1, 0, 0, 0);
  else
    {
      node = grub_dialog_create ("template_subitem", 1, 0, 0, 0);
      if (! node)
	node = grub_dialog_create ("template_menuitem", 1, 0, 0, 0);
    }

  return node;
}

static void
free_section (const char *name)
{
  grub_uitree_t node;

  node = grub_uitree_find (&grub_uitree_root, name);
  if (node)
    {
      grub_tree_remove_node (GRUB_AS_TREE (node));
      grub_uitree_free (node);
    }
}

static grub_uitree_t
create_node (grub_uitree_t menu, grub_uitree_t tree, int *num,
	     int main_menu, const char *tree_name)
{
  grub_uitree_t node;
  char *parm, *p;

  node = create_menuitem (main_menu);
  if (! node)
    return 0;

  parm = grub_uitree_get_prop (node, "parameters");

  grub_dialog_set_parm (node, parm, "title", menu->name);
  p = grub_uitree_get_prop (menu, "class");
  if (p)
    grub_dialog_set_parm (node, parm, "class", p);

  p = grub_uitree_get_prop (menu, "users");
  if (p)
    grub_uitree_set_prop (node, "users", p);

  p = grub_uitree_get_prop (menu, "submenu");
  if (p)
    grub_uitree_set_prop (node, "submenu", p);

  if (menu->child)
    {
      char* buf;
      grub_uitree_t sub, child;

      buf = grub_xasprintf ("menu_popup -ri %d %s",
			    *num - menu->parent->flags - 1, tree_name);
      if (! buf)
	{
	  grub_uitree_free (node);
	  return 0;
	}
      grub_uitree_set_prop (node, "command", buf);
      grub_free (buf);

      sub = grub_dialog_create ("template_submenu", 1, 0, 0, 0);
      if (! sub)
	{
	  grub_uitree_free (node);
	  return 0;
	}

      grub_tree_add_child (GRUB_AS_TREE (tree), GRUB_AS_TREE (sub), -1);
      child = grub_uitree_find_id (sub, "__child__");
      menu->data = (child) ? child : sub;
      menu->flags = *num;
      (*num)++;
    }
  else
    {
      char *cmd;

      cmd = grub_uitree_get_prop (menu, "command");
      if (cmd)
	grub_uitree_set_prop (node, "command", cmd);
    }

  return node;
}

static void
add_sys_menu (const char *name, grub_uitree_t node, int index,
	      int default_num, int main_menu)
{
  grub_uitree_t menu, tree;
  int num;
  char *tree_name = 0;

  menu = grub_uitree_find (&grub_uitree_root, name);
  if (! menu)
    return;

  tree_name = grub_xasprintf ("%s_tree", name);
  if (! tree_name)
    return;

  free_section (tree_name);
  tree = grub_uitree_create_node (tree_name);
  if (! tree)
    goto quit;
  grub_tree_add_child (GRUB_AS_TREE (&grub_uitree_root), GRUB_AS_TREE (tree),
		       -1);

  num = 1;
  menu = menu->child;
  while (menu)
    {
      grub_uitree_t child;
      grub_uitree_t n;
      char buf[8];

      n = create_node (menu, tree, &num, main_menu, tree_name);
      if (! n)
	goto quit;

      grub_snprintf (buf, sizeof (buf), "%d", index);
      grub_uitree_set_prop (n, "index", buf);

      grub_tree_add_child (GRUB_AS_TREE (node), GRUB_AS_TREE (n), -1);
      if (index == default_num)
	grub_widget_select_node (n, 1);

      child = menu->child;
      while (child)
	{
	  grub_uitree_t parent;

	  n = create_node (child, tree, &num, 0, tree_name);
	  if (! n)
	    goto quit;

	  parent = child->parent->data;
	  if (! parent)
	    {
	      grub_error (GRUB_ERR_BAD_ARGUMENT, "parant shouldn't be null");
	      goto quit;
	    }

	  grub_tree_add_child (GRUB_AS_TREE (parent), GRUB_AS_TREE (n), -1);
	  child = grub_tree_next_node (GRUB_AS_TREE (menu),
				       GRUB_AS_TREE (child));
	}
      menu = menu->next;
      index++;
    }

 quit:
  grub_free (tree_name);
}

static grub_uitree_t
build_menuitem (grub_menu_entry_t entry, int main_menu)
{
  grub_uitree_t item;
  char *parm;

  item = create_menuitem (main_menu);
  if (! item)
    return 0;

  parm = grub_uitree_get_prop (item, "parameters");
  if (entry->title)
    grub_dialog_set_parm (item, parm, "title", entry->title);

  if (entry->classes->next)
    grub_dialog_set_parm (item, parm, "class", entry->classes->next->name);

  if (entry->users)
    grub_uitree_set_prop (item, "users", entry->users);

  if (entry->sourcecode)
    grub_uitree_set_prop (item, "command", entry->sourcecode);

  return item;
}

static int
add_user_menu (grub_uitree_t node, grub_menu_t menu, int default_num,
	       int main_menu)
{
  grub_menu_entry_t entry;
  int index;
  int fold;
  char *pp;

  if (! menu)
    return 0;

  pp = grub_env_get ("theme_fold");
  if (pp)
    fold = (*pp != 0);
  else
    fold = 0;

  for (index = 0, entry = menu->entry_list; entry; entry = entry->next)
    {
      grub_uitree_t item;
      char buf[8];
      grub_uitree_t m;

      if ((fold) && (entry->group))
	m = grub_uitree_find (&grub_uitree_root, entry->group);
      else
	m = NULL;

      item = build_menuitem (entry, main_menu && (m == NULL));
      if (! item)
	return index;

      if ((fold) && (entry->group))
	{
	  grub_uitree_t submenu;
	  grub_uitree_t subroot;
	  grub_uitree_t p;

	  if (! m)
	    {
	      submenu = grub_uitree_create_node (entry->group);
	      if (! submenu)
		return index;

	      grub_tree_add_child (GRUB_AS_TREE (&grub_uitree_root),
				   GRUB_AS_TREE (submenu), -1);
	    }
	  else
	    submenu = m;

	  subroot = submenu->child;
	  if (! subroot)
	    {
	      subroot = grub_dialog_create ("template_submenu", 1, 0, 0, 0);
	      if (! subroot)
		return index;

	      grub_tree_add_child (GRUB_AS_TREE (submenu),
				   GRUB_AS_TREE (subroot), -1);
	    }

	  p = grub_uitree_find_id (subroot, "__child__");
	  if (p)
	    subroot = p;

	  if (! m)
	    {
	      grub_uitree_set_prop (item, "group", entry->group);
	      p = build_menuitem (entry, 0);
	      if (! p)
		return index;
	    }
	  else
	    p = item;

	  grub_tree_add_child (GRUB_AS_TREE (subroot), GRUB_AS_TREE (p), -1);

	  if (m)
	    continue;
	}

      grub_tree_add_child (GRUB_AS_TREE (node), GRUB_AS_TREE (item), -1);
      grub_snprintf (buf, sizeof (buf), "%d", index);
      grub_uitree_set_prop (item, "index", buf);
      if (index == default_num)
	grub_widget_select_node (item, 1);

      index++;
    }

  return index;
}

static grub_uitree_t gfx_pb_node;

static void
gfx_pb_init (void)
{
  grub_uitree_t root, node;

  root = get_screen ();
  if (! root)
    return;

  gfx_pb_node = grub_dialog_create ("file_pb", 1, 0, 0, 0);
  if (! gfx_pb_node)
    return;

  node = gfx_pb_node;
  grub_tree_add_child (GRUB_AS_TREE (root), GRUB_AS_TREE (node), -1);

  grub_widget_create (node);
  grub_widget_init (node);
  node->flags |= GRUB_WIDGET_FLAG_FIXED_XY;
  grub_widget_draw (node);
}

static void
gfx_pb_fini (void)
{
  if (gfx_pb_node)
    {
      grub_dialog_update_screen (gfx_pb_node);
      grub_dialog_free (gfx_pb_node, 0, 0);
      gfx_pb_node = 0;
    }
}

static void
gfx_pb_show (int num, int total)
{
  if (gfx_pb_node)
    {
      grub_uitree_t child;
      grub_menu_region_update_list_t head;

      num = total - num;
      head = 0;
      child = gfx_pb_node;
      while (child)
	{
	  grub_widget_t widget;

	  widget = child->data;
	  if ((widget) && (widget->class->set_timeout))
	    {
	      widget->class->set_timeout (widget, total, num);
	      widget->class->draw (widget, &head, 0, 0,
				   widget->width, widget->height);
	    }

	  child = grub_tree_next_node (GRUB_AS_TREE (gfx_pb_node),
				       GRUB_AS_TREE (child));
	}
      grub_menu_region_apply_update (head);
    }
}

static grub_err_t
show_menu (grub_menu_t menu, int nested)
{
  grub_uitree_t root;
  grub_err_t err;

  if (! nested)
    {
      root = grub_uitree_find (&grub_uitree_root, "screen");
      if (! root)
	goto quit;

      if (grub_widget_screen)
	grub_uitree_free (grub_widget_screen);

      grub_widget_screen = grub_uitree_clone (root);
    }

  root = get_screen ();
  if (! root)
    goto quit;

  if (! nested)
    {
      grub_term_output_t term, gfxmenu_term;
      void (*pb_init) (void) = grub_file_pb_init;
      void (*pb_fini) (void) = grub_file_pb_fini;
      void (*pb_show) (int, int) = grub_file_pb_show;

      grub_file_pb_init = gfx_pb_init;
      grub_file_pb_fini = gfx_pb_fini;
      grub_file_pb_show = gfx_pb_show;

      if (! grub_menu_region_get_current ())
	{
	  grub_error (GRUB_ERR_BAD_ARGUMENT, "no menu region handler");
	  goto quit;
	}

      gfxmenu_term = 0;
      FOR_DISABLED_TERM_OUTPUTS(term)
      {
	if (! grub_strcmp (term->name, "gfxmenu"))
	  {
	    gfxmenu_term = term;
	    break;
	  }
      }

      if (! gfxmenu_term)
	{
	  grub_error (GRUB_ERR_BAD_ARGUMENT, "gfxmenu term not found");
	  goto quit;
	}

      grub_menu_region_text_term = grub_term_outputs;
      grub_list_remove (GRUB_AS_LIST_P (&(grub_term_outputs_disabled)),
			GRUB_AS_LIST (gfxmenu_term));
      grub_term_outputs = gfxmenu_term;
      gfxmenu_term->next = NULL;

      while (1)
	{
//	  grub_err_t r = 0;
	  grub_uitree_t node;

	  node = grub_uitree_find_id (root, "__menu__");
	  if (node)
	    {
	      int index, default_num;
	      char *default_str;

	      default_str = grub_env_get ("default");
	      default_num = (default_str) ?
		grub_strtol (default_str, 0, 0) : -1;
	      if (grub_errno == GRUB_ERR_BAD_NUMBER)
		{
		  default_num = 0;
		  grub_errno = 0;
		}
	      index = add_user_menu (node, menu, default_num, 1);
	      add_sys_menu ("menu", node, index, default_num, 1);
	    }

	  if (grub_widget_create (root) == GRUB_ERR_NONE)
	    {
	      grub_widget_init (root);
//	      r = grub_widget_input (root, 0);
				grub_widget_input (root, 0);

	    }
	  grub_widget_free (root);
	  if (grub_widget_refresh)
	    {
	      grub_menu_entry_t entry;
	      grub_menu_region_t region;

	      grub_errno = 0;
	      if ((node) && (menu))
		for (entry = menu->entry_list; entry; entry = entry->next)
		  if (entry->group)
		    free_section (entry->group);

	      root = grub_uitree_find (&grub_uitree_root, "screen");
	      if (! root)
		goto quit;

	      grub_uitree_free (grub_widget_screen);
	      grub_widget_screen = grub_uitree_clone (root);

	      root = get_screen ();
	      if (! root)
		goto quit;
	      grub_env_unset ("timeout");

	      region = grub_menu_region_get_current ();
	      if (grub_widget_refresh == GRUB_WIDGET_TOGGLE_MODE)
		{
		  char *p;
		  p = ((! region) || (! grub_strcmp (region->name, "gfx"))) ?
		    "menu_region.text" : "menu_region.gfx";
		  grub_command_execute (p, 0, 0);
		}
	      else if (grub_widget_refresh == GRUB_WIDGET_RELOAD_MODE)
		{
		  if (region->fini)
		    region->fini ();
		  if (region->init)
		    region->init ();
		}
	      grub_widget_refresh = 0;
	    }
	  else
	    break;
	}

      grub_list_push (GRUB_AS_LIST_P (&grub_term_outputs_disabled),
		      GRUB_AS_LIST (gfxmenu_term));
      grub_term_outputs = grub_menu_region_text_term;

      grub_file_pb_init = pb_init;
      grub_file_pb_fini = pb_fini;
      grub_file_pb_show = pb_show;
    }
  else
    {
      grub_uitree_t node, child;

      node = grub_dialog_create ("template_submenu", 1, 0, 0, 0);
      if (! node)
	return grub_errno;

      grub_tree_add_child (GRUB_AS_TREE (root), GRUB_AS_TREE (node), -1);
      child = grub_uitree_find_id (node, "__child__");
      if (! child)
	child = node;
      add_user_menu (child, menu, -1, 0);
      set_position (root, node);
      grub_dialog_popup (node);
      grub_dialog_free (node, 0, 0);
    }

 quit:
  err = grub_errno;

  if (! nested)
    grub_command_execute ("menu_region.text", 0, 0);

  return err;
}

struct grub_controller grub_ext_controller =
{
  .name = "ext",
  .show_menu = show_menu
};

static grub_extcmd_t cmd_popup, cmd_edit;
static grub_command_t cmd_read, cmd_refresh, cmd_toggle_mode, cmd_reload_mode;

GRUB_MOD_INIT(emenu)
{
  grub_controller_register ("ext", &grub_ext_controller);

  cmd_popup = grub_register_extcmd ("menu_popup", grub_cmd_popup,
				    GRUB_COMMAND_FLAG_BOTH,
				    "[OPTIONS] NAME [PARM=VALUE]..",
				    "popup window", options);
  cmd_edit = grub_register_extcmd ("menu_edit", grub_cmd_popup,
				   GRUB_COMMAND_FLAG_BOTH,
				   "[OPTIONS] NAME [PARM=PARM1]..",
				   "popup edit window", options);
  cmd_read = grub_register_command ("menu_read", grub_cmd_read,
				    "PARM_NAME VAR_NAME",
				    "read property from dialog and assign its value to variable");
  cmd_refresh = grub_register_command ("menu_refresh", grub_cmd_refresh,
				       0, "refresh menu");
  cmd_toggle_mode =
    grub_register_command ("menu_toggle_mode", grub_cmd_toggle_mode,
			   0, "toggle mode");
  cmd_reload_mode =
    grub_register_command ("menu_reload_mode", grub_cmd_reload_mode,
			   0, "reload mode");
}

GRUB_MOD_FINI(emenu)
{
  grub_controller_unregister (&grub_ext_controller);

  grub_unregister_extcmd (cmd_popup);
  grub_unregister_extcmd (cmd_edit);
  grub_unregister_command (cmd_read);
  grub_unregister_command (cmd_refresh);
  grub_unregister_command (cmd_toggle_mode);
  grub_unregister_command (cmd_reload_mode);
}

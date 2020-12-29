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

#include <grub/mm.h>
#include <grub/misc.h>
#include <grub/widget.h>
#include <grub/dialog.h>
#include <grub/menu_data.h>
#include <grub/auth.h>

GRUB_EXPORT(grub_dialog_create);
GRUB_EXPORT(grub_dialog_set_parm);
GRUB_EXPORT(grub_dialog_get_parm);
GRUB_EXPORT(grub_dialog_popup);
GRUB_EXPORT(grub_dialog_update_screen);
GRUB_EXPORT(grub_dialog_free);
GRUB_EXPORT(grub_dialog_message);
GRUB_EXPORT(grub_dialog_password);

grub_uitree_t
grub_dialog_create (const char *name, int copy, int index,
		    grub_uitree_t *menu, grub_uitree_t *save)
{
  grub_uitree_t node;
  grub_uitree_t parent;

  parent = grub_uitree_find (&grub_uitree_root, name);
  if (! parent)
    return 0;

  node = parent->child;
  while ((index) && (node))
    {
      index--;
      node = node->next;
    }

  if (! node)
    return 0;

  if (copy)
    node = grub_uitree_clone (node);
  else
    {
      *menu = parent;
      *save = parent->child;
      parent->child = node->next;
    }

  return node;
}

static grub_uitree_t
find_prop (grub_uitree_t node, char *path, char **out)
{
  while (1)
    {
      char *n;
      grub_uitree_t child;

      n = grub_menu_next_field (path, '.');
      if (! n)
	{
	  *out = path;
	  return node;
	}

      child = grub_uitree_find (node, path);
      node = (child) ? child : grub_uitree_find_id (node, path);
      grub_menu_restore_field (n, '.');
      if (! node)
	return 0;
      path = n;
    }
}

static grub_uitree_t
find_parm (grub_uitree_t node, char *parm, char *name,
	   char **out, char **next)
{
  *next = 0;
  *out = name;

  if (grub_strchr (name, '.'))
    node = find_prop (node, name, out);
  else if (parm)
    do
      {
	char *n, *v;

	n = grub_menu_next_field (parm, ':');
	v = grub_menu_next_field (parm, '=');
	if ((v) && (! grub_strcmp (parm, name)))
	  {
	    grub_menu_restore_field (v, '=');
	    *next = n;
	    return find_prop (node, v, out);
	  }
	grub_menu_restore_field (v, '=');
	grub_menu_restore_field (n, ':');
	parm = n;
      } while (parm);

  return node;
}

int
grub_dialog_set_parm (grub_uitree_t node, char *parm,
		      char *name, const char *value)
{
  char *out, *next;
  int result;

  node = find_parm (node, parm, name, &out, &next);
  result = (node) ? (grub_uitree_set_prop (node, out, value) == 0) : 0;
  grub_menu_restore_field (next, ':');

  return result;
}

char *
grub_dialog_get_parm (grub_uitree_t node, char *parm, char *name)
{
  char *out, *next, *result;

  node = find_parm (node, parm, name, &out, &next);
  result = (node) ? grub_uitree_get_prop (node, out) : 0;
  grub_menu_restore_field (next, ':');

  return result;
}

void
grub_dialog_update_screen (grub_uitree_t node)
{
  grub_uitree_t root, child;
  grub_widget_t widget;
  grub_menu_region_update_list_t head;

  widget = node->data;
  root = node->parent;
  head = 0;
  ((grub_widget_t) root->data)->class->draw (root->data, &head,
					     widget->org_x, widget->org_y,
					     widget->width, widget->height);
  for (child = root->child; ((child) && (child != node)); child = child->next)
    {
      int x, y, width, height;
      grub_widget_t c;

      c = child->data;
      if (! c)
	continue;

      x = widget->org_x - c->org_x;
      y = widget->org_y - c->org_y;
      width = widget->width;
      height = widget->height;

      if (grub_menu_region_check_rect (&x, &y, &width, &height, 0, 0,
				       c->width, c->height))
	grub_widget_draw_region (&head, child, x, y, width, height);
    }
  grub_menu_region_apply_update (head);
}

grub_err_t
grub_dialog_popup (grub_uitree_t node)
{
  grub_err_t r;
  grub_uitree_t save;

  grub_widget_create (node);
  grub_widget_init (node);
  node->flags |= GRUB_WIDGET_FLAG_FIXED_XY;
  save = grub_widget_current_node;
  r = grub_widget_input (node, 1);
  grub_widget_current_node = save;
  if (! grub_widget_refresh)
    grub_dialog_update_screen (node);
  grub_widget_free (node);

  return r;
}

void
grub_dialog_free (grub_uitree_t node, grub_uitree_t menu, grub_uitree_t save)
{
  grub_tree_remove_node (GRUB_AS_TREE (node));
  if (save)
    {
      node->next = menu->child;
      node->parent = menu;
      menu->child = save;
    }
  else
    grub_uitree_free (node);
}

static grub_uitree_t
create_dialog (const char *name)
{
  grub_uitree_t node;

  if (! grub_widget_screen)
    return 0;

  node = grub_dialog_create (name, 1, 0, 0, 0);
  if (! node)
    return 0;

  grub_tree_add_child (GRUB_AS_TREE (grub_widget_screen),
		       GRUB_AS_TREE (node), -1);
  return node;
}

void
grub_dialog_message (const char *text)
{
  grub_uitree_t node;

  node = create_dialog ("dialog_message");
  if (! node)
    return;

  if (text)
    {
      char *parm;

      parm = grub_uitree_get_prop (node, "parameters");
      grub_dialog_set_parm (node, parm, "text", text);
    }
  grub_dialog_popup (node);
  grub_dialog_free (node, 0, 0);
}

int
grub_dialog_password (const char *userlist)
{
  grub_uitree_t node;
  int result = 0;

  if (grub_auth_check_password (userlist, 0, 0))
    return 1;

  node = create_dialog ("dialog_password");
  if (! node)
    return 0;

  grub_errno = 0;
  if (grub_dialog_popup (node) == 0)
    {
      char *parm, *user;
      char pass[GRUB_AUTH_MAX_PASSLEN];

      parm = grub_uitree_get_prop (node, "parameters");
      user = grub_dialog_get_parm (node, parm, "username");
      if (user)
	{
	  char *p;
	  grub_memset (pass, 0, sizeof (pass));
	  p = grub_dialog_get_parm (node, parm, "password");
	  grub_strcpy (pass, p);
	  grub_memset (p, 0, grub_strlen (p));
	  result = (grub_auth_check_password (userlist, user, pass));
	  grub_memset (pass, 0, sizeof (pass));
	}

      if (! result)
	grub_dialog_message ("Access denied.");
    }

  grub_dialog_free (node, 0, 0);
  return result;
}

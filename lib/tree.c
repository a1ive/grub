/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2009  Free Software Foundation, Inc.
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

#include <grub/tree.h>

GRUB_EXPORT(grub_tree_add_sibling);
GRUB_EXPORT(grub_tree_add_child);
GRUB_EXPORT(grub_tree_remove_node);
GRUB_EXPORT(grub_tree_next_node);

void
grub_tree_add_sibling (grub_tree_t pre, grub_tree_t cur)
{
  cur->next = pre->next;
  pre->next = cur;
  cur->parent = pre->parent;
}

void
grub_tree_add_child (grub_tree_t parent, grub_tree_t cur, int index)
{
  grub_tree_t *p, q;

  p = &parent->child;
  q = *p;
  while ((index) && (q))
    {
      index--;
      p = &q->next;
      q = *p;
    }

  cur->next = *p;
  *p = cur;
  cur->parent = parent;
}

void
grub_tree_remove_node (grub_tree_t cur)
{
  grub_tree_t *p, q;

  if (! cur->parent)
    return;

  p = &cur->parent->child;
  q = *p;
  while (q != cur)
    {
      p = &q->next;
      q = *p;
    }

  *p = cur->next;
}

void *
grub_tree_next_node (grub_tree_t root, grub_tree_t pre)
{
  if (! pre)
    return root;

  if (pre->child)
    return pre->child;

  while (1)
    {
      if (pre == root)
	return 0;

      if (pre->next)
	break;

      pre = pre->parent;
    }

  return pre->next;
}

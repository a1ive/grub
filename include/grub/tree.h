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

#ifndef GRUB_TREE_HEADER
#define GRUB_TREE_HEADER 1

#include <grub/list.h>

struct grub_tree
{
  struct grub_tree *parent;
  struct grub_tree *child;
  struct grub_tree *next;
};
typedef struct grub_tree *grub_tree_t;

void grub_tree_add_sibling (grub_tree_t pre, grub_tree_t cur);
void grub_tree_add_child (grub_tree_t parent, grub_tree_t cur, int index);
void grub_tree_remove_node (grub_tree_t cur);
void * grub_tree_next_node (grub_tree_t root, grub_tree_t pre);

#define GRUB_AS_TREE(ptr) \
  ((GRUB_FIELD_MATCH (ptr, grub_tree_t, parent) && \
    GRUB_FIELD_MATCH (ptr, grub_tree_t, child) && \
    GRUB_FIELD_MATCH (ptr, grub_tree_t, next)) ? \
   (grub_tree_t) ptr : grub_bad_type_cast ())

#endif

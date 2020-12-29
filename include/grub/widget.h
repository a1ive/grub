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

#ifndef GRUB_WIDGET_HEADER
#define GRUB_WIDGET_HEADER 1

#include <grub/err.h>
#include <grub/uitree.h>
#include <grub/menu_region.h>

#define GRUB_WIDGET_FLAG_TRANSPARENT	1
#define GRUB_WIDGET_FLAG_EXTEND		2
#define GRUB_WIDGET_FLAG_FIXED_X	4
#define GRUB_WIDGET_FLAG_FIXED_Y	8
#define GRUB_WIDGET_FLAG_FIXED_WIDTH	16
#define GRUB_WIDGET_FLAG_FIXED_HEIGHT	32
#define GRUB_WIDGET_FLAG_ANCHOR		64
#define GRUB_WIDGET_FLAG_NODE		128
#define GRUB_WIDGET_FLAG_ROOT		256
#define GRUB_WIDGET_FLAG_SELECTED	512
#define GRUB_WIDGET_FLAG_MARKED		1024
#define GRUB_WIDGET_FLAG_HIDDEN		2048
#define GRUB_WIDGET_FLAG_DYNAMIC	4096

#define GRUB_WIDGET_FLAG_FIXED_XY	\
  (GRUB_WIDGET_FLAG_FIXED_X | GRUB_WIDGET_FLAG_FIXED_Y)

#define GRUB_WIDGET_RESULT_DONE		-1
#define GRUB_WIDGET_RESULT_SKIP		-2

struct grub_widget
{
  struct grub_widget_class *class;
  struct grub_uitree *node;
  void *data;
  int org_x;
  int org_y;
  int x;
  int y;
  int width;
  int height;
  int inner_x;
  int inner_y;
  int inner_width;
  int inner_height;
};
typedef struct grub_widget *grub_widget_t;

struct grub_widget_class
{
  struct grub_widget_class *next;
  const char *name;
  int (*get_data_size) (void);
  void (*init_size) (grub_widget_t widget);
  void (*fini_size) (grub_widget_t widget);
  void (*free) (grub_widget_t widget);
  void (*draw) (grub_widget_t widget, grub_menu_region_update_list_t *head,
		int x, int y, int width, int height);
  void (*draw_cursor) (grub_widget_t widget);
  int (*onkey) (grub_widget_t widget, int key);
  void (*set_timeout) (grub_widget_t widget, int total, int left);
};
typedef struct grub_widget_class *grub_widget_class_t;

extern grub_widget_class_t grub_widget_class_list;

static inline void
grub_widget_class_register (grub_widget_class_t class)
{
  grub_list_push (GRUB_AS_LIST_P (&grub_widget_class_list),
		  GRUB_AS_LIST (class));
}

static inline void
grub_widget_class_unregister (grub_widget_class_t class)
{
  grub_list_remove (GRUB_AS_LIST_P (&grub_widget_class_list),
		    GRUB_AS_LIST (class));
}

#define GRUB_WIDGET_REFRESH		1
#define GRUB_WIDGET_TOGGLE_MODE		2
#define GRUB_WIDGET_RELOAD_MODE		3

extern grub_uitree_t grub_widget_current_node;
extern grub_uitree_t grub_widget_screen;
extern int grub_widget_refresh;

grub_err_t grub_widget_create (grub_uitree_t node);
void grub_widget_init (grub_uitree_t node);
void grub_widget_free (grub_uitree_t node);
void grub_widget_draw (grub_uitree_t node);
void grub_widget_draw_region (grub_menu_region_update_list_t *head,
			      grub_uitree_t node, int x, int y,
			      int width, int height);
void grub_widget_select_node (grub_uitree_t node, int selected);
int grub_widget_input (grub_uitree_t root, int nested);
char *grub_widget_get_prop (grub_uitree_t node, const char *name);

#endif

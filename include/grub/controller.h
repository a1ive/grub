/*
 *  BURG - Brand-new Universal loadeR from GRUB
 *  Copyright 2010 Bean Lee - All Rights Reserved
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

#ifndef GRUB_CONTROLLER_HEADER
#define GRUB_CONTROLLER_HEADER 1

#include <grub/err.h>
#include <grub/menu.h>
#include <grub/handler.h>

struct grub_controller
{
  struct grub_controller *next;
  const char *name;
  grub_err_t (*init) (void);
  grub_err_t (*fini) (void);
  grub_err_t (*show_menu) (grub_menu_t menu, int nested);
};
typedef struct grub_controller *grub_controller_t;

extern struct grub_handler_class grub_controller_class;

#define grub_controller_register(name, controller) \
  grub_controller_register_internal (controller); \
  GRUB_MODATTR ("handler", "controller." name);

static inline void
grub_controller_register_internal (grub_controller_t controller)
{
  grub_handler_register (&grub_controller_class, GRUB_AS_HANDLER (controller));
}

static inline void
grub_controller_unregister (grub_controller_t controller)
{
  grub_handler_unregister (&grub_controller_class, GRUB_AS_HANDLER (controller));
}

static inline grub_controller_t
grub_controller_get_current (void)
{
  return (grub_controller_t) grub_controller_class.cur_handler;
}

static inline grub_err_t
grub_controller_show_menu (grub_menu_t menu, int nested)
{
  return grub_controller_get_current () ?
    grub_controller_get_current ()->show_menu (menu, nested) : 0;
}

#endif

/* gui_activity_bar.h - GUI activity bar component, based on GUI progress bar */
/*
 *  Extention for GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2008,2009  Free Software Foundation, Inc.
 *  Copyright (C) 2014 Miray Software <oss@miray.de>
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

#ifndef GUI_ACTIVITY_BAR
#define GUI_ACTIVITY_BAR

#include <grub/gui.h>

static const int gui_activity_bar_advance_val = -GRUB_INT_MAX -1;

struct grub_gui_activity
{
  struct grub_gui_component component;
  struct grub_gui_progress_ops *ops;
};

grub_gui_component_t grub_miray_gui_activity_bar_new (void);
grub_gui_component_t grub_miray_gui_progress_bar_new (void);

#endif

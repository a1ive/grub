/*
 *  Extention for GRUB  --  GRand Unified Bootloader
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

#ifndef MIRAY_GFX_SCREEN
#define MIRAY_GFX_SCREEN

#include "miray_screen.h"
#include <grub/video.h>
#include <grub/gui.h>

void miray_screen_draw_rect(const grub_video_rect_t * bounds, int clear);
void miray_screen_draw_activity(void);


#endif

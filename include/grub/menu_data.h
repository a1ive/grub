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

#ifndef GRUB_MENU_DATA_HEADER
#define GRUB_MENU_DATA_HEADER 1

#include <grub/menu_region.h>

grub_video_color_t grub_menu_parse_color (const char *str, grub_uint32_t *fill,
					  grub_video_color_t *color_selected,
					  grub_uint32_t *fill_selected);
grub_menu_region_common_t
grub_menu_parse_bitmap (const char *str, grub_uint32_t def_fill,
			grub_menu_region_common_t *bitmap_selected);
long grub_menu_parse_size (const char *str, int parent_size, int horizontal);
char *grub_menu_next_field (char *name, char c);
void grub_menu_restore_field (char *name, char c);

#endif

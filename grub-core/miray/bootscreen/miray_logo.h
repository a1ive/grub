/*
 *  Extention for GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2010,2011 Miray Software <oss@miray.de>
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


#ifndef miray_logo_h
#define miray_logo_h


#include <grub/types.h>
#include <grub/term.h>

#define UPPER_HALF_BLOCK  0x2580
#define LOWER_HALF_BLOCK  0x2584
#define FULL_BLOCK        0x2588

#define LOGO_MAX_COLORS	       4
#define LOGO_MAX_HEIGHT	       8
#define LOGO_MAX_WIDTH	      16

struct text_logo_element
{
  const grub_uint32_t ucs4;
  const grub_uint8_t  color;
};

struct text_logo_data
{ 
  const char * name;
  const char * color[LOGO_MAX_COLORS];
  grub_uint32_t height;
  grub_uint32_t width;
  const struct text_logo_element data[LOGO_MAX_HEIGHT][LOGO_MAX_WIDTH];
};

void miray_draw_logo(const struct text_logo_data *logo, int offset_left, int offset_top, struct grub_term_output *term);

#endif

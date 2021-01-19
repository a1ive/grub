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


#include <grub/normal.h>
#include "miray_bootscreen.h"

#define LOGO_SOURCE
#include "miray_logo.h"
#undef LOGO_SOURCE

#if 0
static inline void logo_putline(int line, struct grub_term_output *term)
{
	int x;

	for (x = 0; x < miray_logo_width; x++) {
		grub_putcode (miray_logo_ucs4[line][x], term);
	}

}
#endif

static inline int can_display_logo(void)
{
// EFI cannot display half blocks, so the logo will look garbled
#if defined(GRUB_MACHINE_PCBIOS)
   return !0;
#else
   return 0;
#endif
   
}


void miray_draw_logo(const struct text_logo_data *logo, int offset_left, int offset_top, struct grub_term_output *term)
{

   if (!can_display_logo() || logo == 0)
      return;

   int i;
   unsigned int x, y;   
   struct grub_term_coordinate pos_save;
   grub_uint8_t normal_color_save, highlight_color_save;
   grub_uint8_t color_map[LOGO_MAX_COLORS];
   grub_uint8_t last_color = 0xff; // initialize to invalid value
   
   
   pos_save = grub_term_getxy(term);
 
 	/* see call in grub_term_restore_pos() */
	grub_term_gotoxy (term, pos_save);
   normal_color_save    = grub_term_normal_color;
   highlight_color_save = grub_term_highlight_color;

	/* Make sure that the logo isn't wrapped */
	if (offset_left + (int)logo->width >= (int)grub_term_width (term))
		offset_left = grub_term_width (term) - logo->width;
	if (offset_left < 0) offset_left = 0;
	if (offset_top + (int)logo->height >= (int)grub_term_height (term))
		offset_top = grub_term_height (term) - logo->height;
	if (offset_top < 0) offset_top = 0;

   for (i = 0; i < LOGO_MAX_COLORS; i++)
   {
      if (logo->color[i] != 0 && logo->color[i][0] != '\0')
      {
         grub_parse_color_name_pair(&color_map[i], logo->color[i]); 
      }
   }

   for (y = 0; y < logo->height; y++)
   {
      struct grub_term_coordinate pos = { .x = offset_left, .y = y + offset_top };
      
      grub_term_gotoxy (term, pos);
      for (x = 0; x < logo->width; x++)
      {
         if (logo->data[y][x].color != last_color)
         {
            last_color = logo->data[y][x].color;
            if (last_color > LOGO_MAX_COLORS) last_color = 0;

            grub_term_normal_color    = color_map[last_color];
            grub_term_highlight_color = highlight_color_save;
            grub_term_setcolorstate (term, GRUB_TERM_COLOR_NORMAL);
         }
         grub_putcode (logo->data[y][x].ucs4, term);
      }
   } 

   grub_term_normal_color    = normal_color_save;
   grub_term_highlight_color = highlight_color_save;
   grub_term_setcolorstate (term, GRUB_TERM_COLOR_NORMAL);

}

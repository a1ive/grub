/*
 *  Extention for GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2010-2014 Miray Software <oss@miray.de>
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

#ifndef TEXT_PROGRESS_BAR
#define TEXT_PROGRESS_BAR

#include <grub/types.h>
#include <grub/term.h>

typedef struct text_bar text_bar_t;

void text_bar_destroy(struct text_bar *);
void text_bar_set_term(struct text_bar *, grub_term_output_t);
void text_bar_set_progress(struct text_bar *, grub_uint64_t cur, grub_uint64_t max);
void text_bar_draw(struct text_bar *);
void text_bar_finish(struct text_bar * bar); // run highlighted region to end of bar (in activity_bar)

struct text_bar * text_progress_bar_new (grub_term_output_t term, int offset_left, int offset_top, int len);
struct text_bar * text_progress_bar_new_verbose(grub_term_output_t term, int offset_left, int offset_top, int len, grub_uint32_t char_normal, grub_uint32_t char_highlight, const char * color_normal, const char * color_highlight);


struct text_bar * text_activity_bar_new (grub_term_output_t term, int offset_left, int offset_top, int len);
struct text_bar * text_activity_bar_new_verbose(grub_term_output_t term, int offset_left, int offset_top, int len, grub_uint32_t char_normal, grub_uint32_t char_highlight, const char * color_normal, const char * color_highlight);

static const grub_uint64_t activity_bar_advance_val = 0xffffffffffffffffULL;
static inline void text_activity_bar_advance(struct text_bar * bar)
{
   text_bar_set_progress(bar, activity_bar_advance_val, 0);
}
//void text_activity_bar_destroy (struct text_activity_bar *);
//void text_activity_bar_draw(struct text_activity_bar *, grub_term_output_t);
//void text_activity_bar_advance(struct text_activity_bar *, grub_term_output_t);
//void text_activity_bar_finish(struct text_activity_bar * bar, grub_term_output_t); // run highlighted region to end of bar


#endif

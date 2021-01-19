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
#include <grub/types.h>
#include <grub/mm.h>
#include <grub/time.h>
#include <grub/miray_activity.h>

#include "miray_bootscreen.h"
#include "miray_constants.h"
#include "text_progress_bar.h"

//# define PBAR_CHAR 0x2591
//# define PBAR_CHAR_HIGHLIGHT 0x2588
# define PBAR_CHAR GRUB_UNICODE_LIGHT_HLINE
# define PBAR_CHAR_HIGHLIGHT GRUB_UNICODE_LIGHT_HLINE


struct text_bar {
   void (*set_progress) (struct text_bar * bar, grub_uint64_t cur, grub_uint64_t max);
   void (*draw) (struct text_bar * bar);
   void (*finish) (struct text_bar * bar);
   
	grub_uint8_t	color_normal;
	grub_uint8_t	color_highlight;
	grub_uint32_t 	char_normal;
	grub_uint32_t	char_highlight;
   grub_term_output_t term;
   struct grub_term_coordinate pos;
	int		bar_len;
};

typedef struct text_progress_bar {
	struct text_bar bar;
	int current_pos;
} text_progress_bar_t;


typedef struct text_activity_bar {
	struct text_bar bar;
	int current_pos;
} text_activity_bar_t;

// void draw_textbar(struct text_progress_bar *bar, grub_term_output_t term, int highlight_start, int highlight_end);

/* highlight_start and highlight_end: start and end of highlighted bar, 
 * relative to textbar */
static void draw_textbar(struct text_bar *bar, int highlight_start, int highlight_end)
{
   if (bar->term == 0) return;
 
	int x;
	grub_uint32_t c;
	struct grub_term_coordinate pos_save;
	grub_uint8_t normal_color_save;
   grub_uint8_t highlight_color_save;

	pos_save = grub_term_getxy (bar->term);
   normal_color_save    = grub_term_normal_color;
   highlight_color_save = grub_term_highlight_color;

	grub_term_gotoxy (bar->term, bar->pos);
   grub_term_normal_color    = bar->color_normal;
   grub_term_highlight_color = bar->color_highlight;
   if (highlight_start > 0) {
		grub_term_setcolorstate (bar->term, GRUB_TERM_COLOR_NORMAL);
		c = bar->char_normal;
	} else {
		grub_term_setcolorstate (bar->term, GRUB_TERM_COLOR_HIGHLIGHT);
		c = bar->char_highlight;

	}

	for (x = 0; x < bar->bar_len; x++)
   {
		if (x == highlight_start)
      {
			grub_term_setcolorstate (bar->term, GRUB_TERM_COLOR_HIGHLIGHT);
			c = bar->char_highlight;
		}
		if (x == (highlight_end))
      {
			grub_term_setcolorstate (bar->term, GRUB_TERM_COLOR_NORMAL);
			c = bar->char_normal;
		}
		grub_putcode (c, bar->term);
	}

   /* see call in grub_term_restore_pos() */
   grub_term_gotoxy (bar->term, pos_save);

   grub_term_normal_color    = normal_color_save;
   grub_term_highlight_color = highlight_color_save;
   grub_term_setcolorstate (bar->term, GRUB_TERM_COLOR_NORMAL);
}


void text_bar_destroy(struct text_bar * bar)
{
   grub_free(bar);
}

void text_bar_set_progress(struct text_bar * bar, grub_uint64_t cur, grub_uint64_t max)
{
   if (bar->set_progress != 0) bar->set_progress(bar, cur, max);
}

void text_bar_draw(struct text_bar * bar)
{
   if (bar->draw != 0) bar->draw(bar);
}

void text_bar_set_term(struct text_bar * bar, grub_term_output_t term)
{
   bar->term = term;
   text_bar_draw(bar);
}

void text_bar_finish(struct text_bar * bar)
{
   if (bar->finish != 0) bar->finish(bar);
}


/*
 * Progress bar
 */


static void text_progress_bar_draw(struct text_bar * tbar)
{
   text_progress_bar_t * bar = (text_progress_bar_t *)tbar;

	draw_textbar (&bar->bar, 0, bar->current_pos);
}

static void text_progress_bar_set_progress(struct text_bar * tbar, grub_uint64_t cur, grub_uint64_t max)
{
   if (cur == activity_bar_advance_val) // Sort out activities 
      return;

   if (max == 0 || cur > max)
      return;
   
   text_progress_bar_t * bar = (text_progress_bar_t *)tbar;

   bar->current_pos = grub_divmod64(cur * tbar->bar_len, max, 0);

	//text_progress_bar_draw(tbar);
}


static void text_progress_bar_finish(struct text_bar * tbar)
{
   // This might not be necessary any more...
   
   text_progress_bar_t * bar = (text_progress_bar_t *)tbar;   

	for (; bar->current_pos <= bar->bar.bar_len; bar->current_pos++)
   {
		grub_millisleep (miray_activity_tick_throttle_ms);
		draw_textbar(&bar->bar, 0, bar->current_pos);
	}
}


struct text_bar *
text_progress_bar_new_verbose(grub_term_output_t term, int offset_left, int offset_top, int len, grub_uint32_t char_normal, grub_uint32_t char_highlight, const char * color_normal, const char * color_highlight)
{
	struct text_activity_bar *ret;

	ret = grub_malloc(sizeof(struct text_progress_bar));

   ret->bar.set_progress = text_progress_bar_set_progress;
   ret->bar.draw = text_progress_bar_draw;
   ret->bar.finish = text_progress_bar_finish;

   ret->bar.term = term;
	ret->bar.pos.x = offset_left;
	ret->bar.pos.y = offset_top;
	ret->bar.bar_len = len;
	ret->bar.char_normal = char_normal;
   ret->bar.char_highlight = char_highlight;
	grub_parse_color_name_pair (&ret->bar.color_normal, color_normal);
	grub_parse_color_name_pair (&ret->bar.color_highlight, color_highlight);

	ret->current_pos = 0;

	return &(ret->bar);

}

struct text_bar *
text_progress_bar_new (grub_term_output_t term, int offset_left, int offset_top, int len)
{
   //return text_progress_bar_new_verbose(term, offset_left, offset_top, len, PBAR_CHAR, PBAR_CHAR_HIGHLIGHT, "light-gray/black", "blue/black");
   return text_progress_bar_new_verbose(term, offset_left, offset_top, len, PBAR_CHAR, PBAR_CHAR_HIGHLIGHT, "dark-gray/black", "white/black");
}


/*
 * Activity bar
 */

static const int activity_bar_highlight_width = 3;


static void text_activity_bar_draw(struct text_bar * tbar)
{
   text_activity_bar_t * bar = (text_activity_bar_t *)tbar;

	draw_textbar (&bar->bar, bar->current_pos, bar->current_pos + activity_bar_highlight_width);
}

static void text_activity_bar_set_progress(struct text_bar * tbar, grub_uint64_t cur __attribute__((unused)), grub_uint64_t max __attribute__((unused)))
{
   text_activity_bar_t * bar = (text_activity_bar_t *)tbar;

	bar->current_pos++;
	if (bar->current_pos >= bar->bar.bar_len)
		bar->current_pos = 1 - activity_bar_highlight_width;

	text_activity_bar_draw(tbar);
}


//void text_activity_bar_destroy (struct text_activity_bar * bar)
//{
//	grub_free (bar);
//}

static void text_activity_bar_finish(struct text_bar * tbar)
{
   text_activity_bar_t * bar = (text_activity_bar_t *)tbar;   
   
	// Finish current run
	while (bar->current_pos > 0)
   {
		grub_millisleep (miray_activity_tick_throttle_ms * 10);
		text_activity_bar_set_progress(tbar, 0, 0);
	}

	if (bar->current_pos <= 0)
		bar->current_pos = activity_bar_highlight_width;
		//bar->current_pos = 1;

	for (; bar->current_pos <= bar->bar.bar_len; bar->current_pos++)
   {
		grub_millisleep (miray_activity_tick_throttle_ms);
		draw_textbar(&bar->bar, 0, bar->current_pos);
	}
}


struct text_bar *
text_activity_bar_new_verbose(grub_term_output_t term, int offset_left, int offset_top, int len, grub_uint32_t char_normal, grub_uint32_t char_highlight, const char * color_normal, const char * color_highlight)
{
	struct text_activity_bar *ret;

	ret = grub_malloc(sizeof(struct text_activity_bar));

   ret->bar.set_progress = text_activity_bar_set_progress;
   ret->bar.draw = text_activity_bar_draw;
   ret->bar.finish = text_activity_bar_finish;

   ret->bar.term = term;
	ret->bar.pos.x = offset_left;
	ret->bar.pos.y = offset_top;
	ret->bar.bar_len = len;
	ret->bar.char_normal = char_normal;
   ret->bar.char_highlight = char_highlight;
	grub_parse_color_name_pair (&ret->bar.color_normal, color_normal);
	grub_parse_color_name_pair (&ret->bar.color_highlight, color_highlight);

	ret->current_pos = 1 - activity_bar_highlight_width;

	return &(ret->bar);

}

struct text_bar *
text_activity_bar_new (grub_term_output_t term, int offset_left, int offset_top, int len)
{
   //return text_activity_bar_new_verbose(term, offset_left, offset_top, len, PBAR_CHAR, PBAR_CHAR_HIGHLIGHT, "light-gray/black", "blue/black");
   return text_activity_bar_new_verbose(term, offset_left, offset_top, len, PBAR_CHAR, PBAR_CHAR_HIGHLIGHT, "dark-gray/black", "white/black");
   
#if 0
	struct text_activity_bar *ret;

	ret = grub_malloc(sizeof(struct text_activity_bar));

	ret->bar.pos.x = offset_left;
	ret->bar.pos.y = offset_top;
	ret->bar.bar_len = len;
   ret->bar.char_highlight = PBAR_CHAR_HIGHLIGHT;
	ret->bar.char_normal = PBAR_CHAR;
	//grub_parse_color_name_pair (&ret->bar.color_normal, "dark-gray/black");
	grub_parse_color_name_pair (&ret->bar.color_normal, "light-gray/black");
	grub_parse_color_name_pair (&ret->bar.color_highlight, "blue/black");


	ret->current_pos = 1 - activity_bar_highlight_width;

	return ret;
#endif
}


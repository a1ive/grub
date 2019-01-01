/* engine_sound.c - Motherboard speaker play sound.  */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright 2015,2017 Ruyi Boy - All Rights Reserved
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

#include <grub/types.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/env.h>
#include <grub/speaker.h>
#include <grub/engine_sound.h>

/*
 * Tested in July 2015.
 * In some public places, or a quiet place, whether it will disturb others?
 */

/* Chinese folk songs: Jasmine Flower  */
static grub_uint16_t default_start[] =
  { 659, 0, 659, 784, 880, 1046, 1046, 880, 784, 0, 784, 880, 784, 0, 0, 0 };

/* Change option's sound.  */
static grub_uint16_t default_select[] =
  { 587, 262 };

sound_class_t
engine_sound_new (void)
{
  sound_class_t sound;
  sound = grub_malloc (sizeof(*sound));
  if (!sound)
	{
	  return 0;
	}

  /* Parse sound file, to be continued.  */
  sound->start_buf = 0;
  sound->start_len = sizeof(default_start) / sizeof(grub_uint16_t);

  sound->select_buf = 0;
  sound->select_len = sizeof(default_select) / sizeof(grub_uint16_t);

  sound->cur_index = 0;
  sound->play_mark = ENGINE_SOUND_PLAY;

  return sound;
}

void
engine_player_refresh (int is_selected, int cur_sound, void *data)
{
  sound_class_t sound = data;

  if (sound->selected != is_selected)
	{
	  sound->selected = is_selected;
	  sound->cur_index = 0;
	  sound->play_mark = ENGINE_SOUND_PLAY;
	}

  if (sound->play_mark)
	{
	  int i = sound->cur_index;
	  int len = 0;
	  grub_uint16_t cur_pitch = 0;

	  switch (cur_sound)
		{
		case ENGINE_START_SOUND:
		  len = sound->start_len;
		  cur_pitch = default_start[i];
		  break;

		case ENGINE_SELECT_SOUND:
		  len = sound->select_len;
		  cur_pitch = default_select[i];
		  break;
		}

	  if (!cur_pitch)
		{
		  grub_speaker_beep_off ();
		}
	  else
		{
		  grub_speaker_beep_on (cur_pitch);
		}

	  sound->cur_index++;
	  if (sound->cur_index > len)
		{
		  grub_speaker_beep_off ();
		  sound->cur_index = 0;
		  sound->play_mark = ENGINE_SOUND_STOP;
		}
	}
}

void
engine_sound_destroy (sound_class_t sound)
{
  /* grub_free (sound->start_buf);
   grub_free (sound->select_buf); */
  grub_speaker_beep_off ();
  grub_free (sound);
}

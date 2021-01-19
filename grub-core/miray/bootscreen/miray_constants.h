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


#ifndef MIRAY_CONSTANTS
#define MIRAY_CONSTANTS

/* Constants for bootscreen */
static const unsigned int miray_pos_logo_top = 3;
static const unsigned int miray_pos_msg_starting_top = 13;
static const unsigned int miray_pos_activity_bar_top = 15;
static const unsigned int miray_size_activity_bar = 24;

// static const unsigned int miray_activity_bar_speed = 50;

static char const* const msg_options_native_os = "<S> Start Symobi | <ESC> Boot Native OS | <M> More Boot Options";
static char const* const msg_options_default = "<S> Start Symobi | <M> More Boot Options";
static char const* const msg_starting_default = "Starting Miray Symobi";

// Environment keys to overwrite the default values
static char const* const msg_options_env_key = "miray_options_message";
static char const* const msg_starting_env_key = "miray_starting_message";
 
#endif

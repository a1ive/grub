/* miray_activity.h - enable periodic updates for actions */
/*
 *  Extention for GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2000,2001,2002,2003,2005,2006,2007,2008,2009  Free Software Foundation, Inc.
 *  Copyright (C) 2010,2011,2012,2014 Miray Software <oss@miray.de>
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

#ifndef MIRAY_ACTIVITY_HEADER
#define MIRAY_ACTIVITY_HEADER 1

#include <grub/types.h>
#include <grub/err.h>

//#ifdef DISABLE_ACTIVITY
//# define ACTIVITY_DESCRIPTION(str) do {} while (0)
//# define ACTIVITY_TICK() do {} while (0)
//#else
# define ACTIVITY_DESCRIPTION(str) miray_activity_set_description(str)
# define ACTIVITY_TICK() miray_activity_tick()
//#endif

/* minimum pause between calls to tick hooks */
//static const grub_uint32_t miray_activity_tick_throttle_ms = 50;
static const grub_uint32_t miray_activity_tick_throttle_ms = 5;

/* Call the register activity hooks */
void miray_activity_set_description(const char *);
void EXPORT_FUNC(miray_activity_tick) (void);

/* Register an activity hook */
/* BEWARE: tick() can happen very ofen, so the handler should be fast */
void *miray_activity_register_hook (void (*description)(void *data, const char *str), void (*tick)(void *data), void *data); 

void miray_activity_unregister_hook (void *hnd);

#endif

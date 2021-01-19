/*
 *  GRUB  --  GRand Unified Bootloader
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


#include <grub/miray_activity.h>
#include <grub/time.h>
#include <grub/misc.h>
#include <grub/dl.h>
#include <grub/mm.h>

GRUB_MOD_LICENSE ("GPLv3+");

// FIXME: draft for now.
// Only supports tick, and only supports one target

struct miray_activity_hook
{
   struct miray_activity_hook * next;

   void (*descr)(void*, const char *);
   void (*tick)(void*);
   void *data;
}; 

static struct miray_activity_hook *hooks = 0;

static int last_tick = 0;

void
miray_activity_set_description(const char *str __attribute__((__unused__)))
{	
  /* Do nothing */
  //str = 0;  
}

void
miray_activity_tick(void)
{
   if (hooks == 0)
      return;

   grub_uint32_t curr_time = grub_get_time_ms(); 
   if (last_tick + miray_activity_tick_throttle_ms > curr_time)
      return;

   struct miray_activity_hook * cur;
   for (cur = hooks; cur != 0; cur = cur->next)
   {
      if (cur->tick == 0)
         continue;

      cur->tick(cur->data);
   }

   last_tick = curr_time;
}

void *miray_activity_register_hook (void (*description)(void *data, const char *str), void (*tick)(void *data), void *data)
{
   if (description == 0 && tick == 0)
      return 0;

   {
      struct miray_activity_hook * cur;
      for (cur = hooks; cur != 0; cur = cur->next)
      {
         if (cur->descr == description && cur->tick == tick && cur->data == data)
            return cur; /* Already added (CHECK: return 0 instead?) */
      }
   }
   
   struct miray_activity_hook * new = grub_zalloc(sizeof(struct miray_activity_hook));
   if (new == 0)
   {
      grub_error(GRUB_ERR_OUT_OF_MEMORY, "OUT OF MEMORY");
      return 0;
   }
   
   new->descr = description;
   new->tick = tick;
   new->data = data;

   new->next = hooks;
   hooks = new;

   return new;
}

void miray_activity_unregister_hook (void *hnd)
{
   struct miray_activity_hook * prev = 0;
   struct miray_activity_hook * cur;
   for (cur = hooks; cur != 0; cur = cur->next)
   {
      if (cur == hnd)
      {
         if (prev != 0)
            prev->next = cur->next;
         else
            hooks = cur->next;

         grub_free(cur);
         break;
      }
      prev = cur;
   }
}

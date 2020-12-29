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

#include <grub/types.h>
#include <grub/list.h>
#include <grub/font.h>
#include <grub/lib.h>
#include <grub/mm.h>
#include <grub/env.h>
#include <grub/misc.h>

GRUB_EXPORT (grub_autolist_load);
GRUB_EXPORT (grub_autolist_font);

grub_autolist_t grub_autolist_font;

static void
insert_item (grub_autolist_t *list, char *name, char *file)
{
  grub_autolist_t p;

  p = grub_malloc (sizeof (*p));
  if (! p)
    return;

  p->name = grub_malloc (grub_strlen (name) + grub_strlen (file) + 2);
  if (! p->name)
    {
      grub_free (p);
      return;
    }

  grub_strcpy (p->name, name);
  p->value = p->name + grub_strlen (name) + 1;
  grub_strcpy (p->value, file);
  grub_list_push (GRUB_AS_LIST_P (list), GRUB_AS_LIST (p));
}

grub_autolist_t
grub_autolist_load (const char *name)
{
  grub_autolist_t result = 0;
  const char *prefix;

  prefix = grub_env_get ("prefix");
  if (prefix)
    {
      char *filename;

      filename = grub_xasprintf ("%s/%s", prefix, name);
      if (filename)
	{
	  grub_file_t file;

	  file = grub_file_open (filename);
	  if (file)
	    {
	      char *buf = NULL;
	      for (;; grub_free (buf))
		{
		  char *p;

		  buf = grub_getline (file);

		  if (! buf)
		    break;

		  if (! grub_isgraph (buf[0]))
		    continue;

		  p = grub_strchr (buf, ':');
		  if (! p)
		    continue;

		  *p = '\0';
		  while (*++p == ' ')
		    ;

		  insert_item (&result, buf, p);
		}
	      grub_file_close (file);
	    }
	  grub_free (filename);
	}
    }

  return result;
}

/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2009  Free Software Foundation, Inc.
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

#include <grub/mm.h>
#include <grub/lib.h>
#include <grub/misc.h>

GRUB_EXPORT(grub_getline);

/* Read a line from the file FILE.  */
char *
grub_getline (grub_file_t file)
{
  char c;
  int pos = 0;
  int literal = 0;
  char *cmdline;
  int max_len = 64;

  /* Initially locate some space.  */
  cmdline = grub_malloc (max_len);
  if (! cmdline)
    return 0;

 retry:
  while (1)
    {
      if (grub_file_read (file, &c, 1) != 1)
	break;

      /* Skip all carriage returns.  */
      if (c == '\r')
	continue;

      /* Replace tabs with spaces.  */
      if (c == '\t')
	c = ' ';

      /* The previous is a backslash, then...  */
      if (literal)
	{
	  /* If it is a newline, replace it with a space and continue.  */
	  if (c == '\n')
	    {
	      c = ' ';

	      /* Go back to overwrite the backslash.  */
	      if (pos > 0)
		pos--;
	    }

	  literal = 0;
	}

      if (c == '\\')
	literal = 1;

      if (pos == 0)
	{
	  if (! grub_isspace (c))
	    cmdline[pos++] = c;
	}
      else
	{
	  if (pos >= max_len)
	    {
	      char *old_cmdline = cmdline;
	      max_len = max_len * 2;
	      cmdline = grub_realloc (cmdline, max_len);
	      if (! cmdline)
		{
		  grub_free (old_cmdline);
		  return 0;
		}
	    }

	  if (c == '\n')
	    break;

	  cmdline[pos++] = c;
	}
    }

  cmdline[pos] = '\0';

  if (((grub_uint8_t) cmdline[0] == 0xef) &&
      ((grub_uint8_t) cmdline[1] == 0xbb) &&
      ((grub_uint8_t) cmdline[2] == 0xbf))
    {
      pos -= 3;
      if (pos == 0)
	goto retry;
      grub_strcpy (cmdline, cmdline + 3);
    }

  /* If the buffer is empty, don't return anything at all.  */
  if (pos == 0)
    {
      grub_free (cmdline);
      cmdline = 0;
    }

  return cmdline;
}

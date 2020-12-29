/* menu_text.c - Basic text menu implementation.  */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2003,2004,2005,2006,2007,2008,2009  Free Software Foundation, Inc.
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
#include <grub/term.h>
#include <grub/misc.h>
#include <grub/loader.h>
#include <grub/mm.h>
#include <grub/time.h>
#include <grub/env.h>
#include <grub/menu_viewer.h>

/* Time to delay after displaying an error message about a default/fallback
   entry failing to boot.  */
#define DEFAULT_ENTRY_ERROR_DELAY_MS  2500

/* vmlite */
int g_boot_entry = -1;
int g_restore = 0;
int g_revert = 0;
int g_take_snapshot = 0;
char* g_snapshot_file_name = NULL;
char* g_snapshot_title = NULL;
int g_immutable = 0;

static grub_uint8_t grub_color_menu_normal;
static grub_uint8_t grub_color_menu_highlight;

/* Wait until the user pushes any key so that the user
   can see what happened.  */
void
grub_wait_after_message (void)
{
  grub_printf ("\nPress any key to continue...");
  (void) grub_getkey ();
  grub_putchar ('\n');
}

static void
draw_border (void)
{
  unsigned i;

  grub_setcolorstate (GRUB_TERM_COLOR_NORMAL);

  grub_gotoxy (GRUB_TERM_MARGIN, GRUB_TERM_TOP_BORDER_Y);
  grub_putcode (GRUB_TERM_DISP_UL);
  for (i = 0; i < (unsigned) GRUB_TERM_BORDER_WIDTH - 2; i++)
    grub_putcode (GRUB_TERM_DISP_HLINE);
  grub_putcode (GRUB_TERM_DISP_UR);

  for (i = 0; i < (unsigned) GRUB_TERM_NUM_ENTRIES; i++)
    {
      grub_gotoxy (GRUB_TERM_MARGIN, GRUB_TERM_TOP_BORDER_Y + i + 1);
      grub_putcode (GRUB_TERM_DISP_VLINE);
      grub_gotoxy (GRUB_TERM_MARGIN + GRUB_TERM_BORDER_WIDTH - 1,
		   GRUB_TERM_TOP_BORDER_Y + i + 1);
      grub_putcode (GRUB_TERM_DISP_VLINE);
    }

  grub_gotoxy (GRUB_TERM_MARGIN,
	       GRUB_TERM_TOP_BORDER_Y + GRUB_TERM_NUM_ENTRIES + 1);
  grub_putcode (GRUB_TERM_DISP_LL);
  for (i = 0; i < (unsigned) GRUB_TERM_BORDER_WIDTH - 2; i++)
    grub_putcode (GRUB_TERM_DISP_HLINE);
  grub_putcode (GRUB_TERM_DISP_LR);

  grub_setcolorstate (GRUB_TERM_COLOR_NORMAL);

  grub_gotoxy (GRUB_TERM_MARGIN,
	       (GRUB_TERM_TOP_BORDER_Y + GRUB_TERM_NUM_ENTRIES
		+ GRUB_TERM_MARGIN + 1));
}

static void
print_message (int nested, int edit)
{
  grub_setcolorstate (GRUB_TERM_COLOR_NORMAL);

  if (edit)
    {
      grub_printf ("\n\
      Minimum Emacs-like screen editing is supported. TAB lists\n\
      completions. Press Ctrl-x to boot, Ctrl-c for a command-line\n\
      or ESC to return menu.");
    }
  else
    {
      grub_printf ("\n\
      Use the %C and %C keys to select which entry is highlighted.\n",
		   (grub_uint32_t) GRUB_TERM_DISP_UP, (grub_uint32_t) GRUB_TERM_DISP_DOWN);
      grub_printf ("\
      Press enter to boot the selected OS, \'e\' to edit the\n\
      commands before booting or \'c\' for a command-line.");
	  grub_printf ("\n\
      Press \'i\' to boot the selected OS as immutable session.");
	  grub_printf ("\n\
      Press \'s\' to take a snapshot then boot the selected OS.");
	  grub_printf ("\n\
      Press \'t\' on a snapshot to revert and boot to its fresh state.");
	  grub_printf ("\n\
      Press \'r\' on a snapshot to restore and boot to its parent state.");
      if (nested)
	grub_printf ("\n\
      ESC to return previous menu.");
    }
}

/* return number of entries displayed */
static void
print_entry (int y, int highlight, grub_menu_entry_t entry)
{
  int x;
  const char *title;
  grub_size_t title_len;
  grub_ssize_t len;
  grub_uint32_t *unicode_title;
  grub_ssize_t i;
  grub_uint8_t old_color_normal, old_color_highlight;
  int indent;

  title = entry ? entry->title : "";  
  title_len = grub_strlen (title);
  unicode_title = grub_malloc (title_len * sizeof (*unicode_title));
  if (! unicode_title)
    /* XXX How to show this error?  */
    return;

  len = grub_utf8_to_ucs4 (unicode_title, title_len,
                           (grub_uint8_t *) title, -1, 0);
  if (len < 0)
    {
      /* It is an invalid sequence.  */
      grub_free (unicode_title);
      return;
    }

  indent = 0;
  /* calculate indent for snapshot */
  if (entry && entry->is_snapshot)
  {	  
	  grub_menu_entry_t p = entry->parent;	  
	  while (p)
	  {
		  indent += 4;
		  p = p->parent;
	  }
  }

  grub_getcolor (&old_color_normal, &old_color_highlight);
  grub_setcolor (grub_color_menu_normal, grub_color_menu_highlight);
  grub_setcolorstate (highlight
		      ? GRUB_TERM_COLOR_HIGHLIGHT
		      : GRUB_TERM_COLOR_NORMAL);
  
  if (indent > 0)
  {
     int j;
     grub_gotoxy (GRUB_TERM_LEFT_BORDER_X + GRUB_TERM_MARGIN, y);
	 for (j = 0; j < indent; j ++)
       grub_putchar (' ');
  }
  grub_gotoxy (GRUB_TERM_LEFT_BORDER_X + GRUB_TERM_MARGIN + indent, y);
   
  for (x = GRUB_TERM_LEFT_BORDER_X + GRUB_TERM_MARGIN + 2 + indent, i = 0;
       x < GRUB_TERM_LEFT_BORDER_X + GRUB_TERM_BORDER_WIDTH - GRUB_TERM_MARGIN;
       i++)
    {
      if (i < len
	  && x <= (GRUB_TERM_LEFT_BORDER_X + GRUB_TERM_BORDER_WIDTH
		   - GRUB_TERM_MARGIN - 1))
	{
	  grub_ssize_t width;

	  width = grub_getcharwidth (unicode_title[i]);

	  if (x + width > (GRUB_TERM_LEFT_BORDER_X + GRUB_TERM_BORDER_WIDTH
			   - GRUB_TERM_MARGIN - 1))
	    grub_putcode (GRUB_TERM_DISP_RIGHT);
	  else
	    grub_putcode (unicode_title[i]);

	  x += width;
	}
      else
	{
	  grub_putchar (' ');
	  x++;
	}
  }
    
  grub_setcolorstate (GRUB_TERM_COLOR_NORMAL);
  grub_putchar (' ');

  if (entry && !entry->is_snapshot)
    grub_gotoxy (GRUB_TERM_CURSOR_X, y);

  grub_setcolor (old_color_normal, old_color_highlight);
  grub_setcolorstate (GRUB_TERM_COLOR_NORMAL);
  grub_free (unicode_title);
}

static void
print_entries (grub_menu_t menu, int first, int offset)
{
  grub_menu_entry_t e;
  int i, j;
  int num_snapshots;
  grub_menu_entry_t snapshot;

  grub_gotoxy (GRUB_TERM_LEFT_BORDER_X + GRUB_TERM_BORDER_WIDTH,
	       GRUB_TERM_FIRST_ENTRY_Y);

  if (first)
    grub_putcode (GRUB_TERM_DISP_UP);
  else
    grub_putchar (' ');

  e = grub_menu_get_entry (menu, first);

  for (i = 0; i < GRUB_TERM_NUM_ENTRIES; )
    {      
      print_entry (GRUB_TERM_FIRST_ENTRY_Y + i, offset == i, e);
      i++;

	  /* modified by VMLite */
	  if (e)
	  {
		  num_snapshots = get_number_snapshots(e);
		  for (j = 0; j < num_snapshots; j++, i++)
		  {
			 if (i >= GRUB_TERM_NUM_ENTRIES)
				break;
			 snapshot = get_snapshot(e, j);
			 print_entry (GRUB_TERM_FIRST_ENTRY_Y + i, offset == i, snapshot);
		  }
       
		 if (!e->is_snapshot)
	       e = e->next;
		 else
           e = get_outmost_parent(e);
	  }
    }

  grub_gotoxy (GRUB_TERM_LEFT_BORDER_X + GRUB_TERM_BORDER_WIDTH,
	       GRUB_TERM_TOP_BORDER_Y + GRUB_TERM_NUM_ENTRIES);

  if (e)
    grub_putcode (GRUB_TERM_DISP_DOWN);
  else
    grub_putchar (' ');

  grub_gotoxy (GRUB_TERM_CURSOR_X, GRUB_TERM_FIRST_ENTRY_Y + offset);
}

/* Initialize the screen.  If NESTED is non-zero, assume that this menu
   is run from another menu or a command-line. If EDIT is non-zero, show
   a message for the menu entry editor.  */
void
grub_menu_init_page (int nested, int edit)
{
  grub_uint8_t old_color_normal, old_color_highlight;

  grub_getcolor (&old_color_normal, &old_color_highlight);

  /* By default, use the same colors for the menu.  */
  grub_color_menu_normal = old_color_normal;
  grub_color_menu_highlight = old_color_highlight;

  /* Then give user a chance to replace them.  */
  grub_parse_color_name_pair (&grub_color_menu_normal, grub_env_get ("menu_color_normal"));
  grub_parse_color_name_pair (&grub_color_menu_highlight, grub_env_get ("menu_color_highlight"));

  grub_normal_init_page ();
  grub_setcolor (grub_color_menu_normal, grub_color_menu_highlight);
  draw_border ();
  grub_setcolor (old_color_normal, old_color_highlight);
  print_message (nested, edit);
}

/* Get the entry number from the variable NAME.  */
static int
get_entry_number (const char *name)
{
  char *val;
  int entry;

  val = grub_env_get (name);
  if (! val)
    return -1;

  grub_error_push ();

  entry = (int) grub_strtoul (val, 0, 0);

  if (grub_errno != GRUB_ERR_NONE)
    {
      grub_errno = GRUB_ERR_NONE;
      entry = -1;
    }

  grub_error_pop ();

  return entry;
}

static void
print_timeout (int timeout, int offset, int second_stage)
{
  /* NOTE: Do not remove the trailing space characters.
     They are required to clear the line.  */
  char *msg = "   The highlighted entry will be booted automatically in %ds.    ";
  char *msg_end = grub_strchr (msg, '%');

  grub_gotoxy (second_stage ? (msg_end - msg) : 0, GRUB_TERM_HEIGHT /*- 3*/);
  grub_printf (second_stage ? msg_end : msg, timeout);
  grub_gotoxy (GRUB_TERM_CURSOR_X, GRUB_TERM_FIRST_ENTRY_Y + offset);
  grub_refresh ();
};

/* Show the menu and handle menu entry selection.  Returns the menu entry
   index that should be executed or -1 if no entry should be executed (e.g.,
   Esc pressed to exit a sub-menu or switching menu viewers).
   If the return value is not -1, then *AUTO_BOOT is nonzero iff the menu
   entry to be executed is a result of an automatic default selection because
   of the timeout.  */
static int
run_menu (grub_menu_t menu, int nested, int *auto_boot,  int *take_snapshot, int *restore, int *revert, int *immutable)
{
  int first, offset;
  grub_uint64_t saved_time;
  int default_entry;
  int timeout;

  first = 0;

  default_entry = get_entry_number ("default");

  /* If DEFAULT_ENTRY is not within the menu entries, fall back to
     the first entry.  */
  if (default_entry < 0 || default_entry >= menu->size)
    default_entry = 0;

  /* If timeout is 0, drawing is pointless (and ugly).  */
  if (grub_menu_get_timeout () == 0)
    {
      *auto_boot = 1;
      return default_entry;
    }

  offset = default_entry;
  if (offset > GRUB_TERM_NUM_ENTRIES - 1)
    {
      first = offset - (GRUB_TERM_NUM_ENTRIES - 1);
      offset = GRUB_TERM_NUM_ENTRIES - 1;
    }

  /* Initialize the time.  */
  saved_time = grub_get_time_ms ();

 refresh:
  grub_setcursor (0);
  grub_menu_init_page (nested, 0);
  print_entries (menu, first, offset);
  grub_refresh ();

  timeout = grub_menu_get_timeout ();

  if (timeout > 0)
    print_timeout (timeout, offset, 0);

  while (1)
    {
      int c;
      timeout = grub_menu_get_timeout ();

      if (timeout > 0)
	{
	  grub_uint64_t current_time;

	  current_time = grub_get_time_ms ();
	  if (current_time - saved_time >= 1000)
	    {
	      timeout--;
	      grub_menu_set_timeout (timeout);
	      saved_time = current_time;
	      print_timeout (timeout, offset, 1);
	    }
	}

      if (timeout == 0)
	{
	  grub_env_unset ("timeout");
          *auto_boot = 1;
	  return default_entry;
	}

      if (grub_checkkey () >= 0 || timeout < 0)
	{
	  c = GRUB_TERM_ASCII_CHAR (grub_getkey ());

	  if (timeout >= 0)
	    {
	      grub_gotoxy (0, GRUB_TERM_HEIGHT /*- 3*/);
              grub_printf ("\
                                                                        ");
	      grub_env_unset ("timeout");
	      grub_env_unset ("fallback");
	      grub_gotoxy (GRUB_TERM_CURSOR_X, GRUB_TERM_FIRST_ENTRY_Y + offset);
	    }    

	  *restore = 0;
	  *revert = 0;
	  *immutable = 0;
	  *take_snapshot = 0;

	  switch (c)
	    {
	    case GRUB_TERM_HOME:
	      first = 0;
	      offset = 0;
	      print_entries (menu, first, offset);
	      break;

	    case GRUB_TERM_END:
	      offset = menu->size - 1;
	      if (offset > GRUB_TERM_NUM_ENTRIES - 1)
		{
		  first = offset - (GRUB_TERM_NUM_ENTRIES - 1);
		  offset = GRUB_TERM_NUM_ENTRIES - 1;
		}
		print_entries (menu, first, offset);
	      break;

	    case GRUB_TERM_UP:
	    case '^':
	      if (offset > 0)
		{
		  print_entry (GRUB_TERM_FIRST_ENTRY_Y + offset, 0,
			       grub_menu_get_entry (menu, first + offset));
		  offset--;
		  print_entry (GRUB_TERM_FIRST_ENTRY_Y + offset, 1,
			       grub_menu_get_entry (menu, first + offset));
		}
	      else if (first > 0)
		{
		  first--;
		  print_entries (menu, first, offset);
		}
	      break;

	    case GRUB_TERM_DOWN:
	    case 'v':
	      if (menu->size > first + offset + 1)
		{
		  if (offset < GRUB_TERM_NUM_ENTRIES - 1)
		    {
		      print_entry (GRUB_TERM_FIRST_ENTRY_Y + offset, 0,
				   grub_menu_get_entry (menu, first + offset));
		      offset++;
		      print_entry (GRUB_TERM_FIRST_ENTRY_Y + offset, 1,
				   grub_menu_get_entry (menu, first + offset));
		    }
		  else
		    {
		      first++;
		      print_entries (menu, first, offset);
		    }
		}
	      break;

	    case GRUB_TERM_PPAGE:
	      if (first == 0)
		{
		  offset = 0;
		}
	      else
		{
		  first -= GRUB_TERM_NUM_ENTRIES;

		  if (first < 0)
		    {
		      offset += first;
		      first = 0;
		    }
		}
	      print_entries (menu, first, offset);
	      break;

	    case GRUB_TERM_NPAGE:
	      if (offset == 0)
		{
		  offset += GRUB_TERM_NUM_ENTRIES - 1;
		  if (first + offset >= menu->size)
		    {
		      offset = menu->size - first - 1;
		    }
		}
	      else
		{
		  first += GRUB_TERM_NUM_ENTRIES;

		  if (first + offset >= menu->size)
		    {
		      first -= GRUB_TERM_NUM_ENTRIES;
		      offset += GRUB_TERM_NUM_ENTRIES;

		      if (offset > menu->size - 1 ||
			  offset > GRUB_TERM_NUM_ENTRIES - 1)
			{
			  offset = menu->size - first - 1;
			}
		      if (offset > GRUB_TERM_NUM_ENTRIES)
			{
			  first += offset - GRUB_TERM_NUM_ENTRIES + 1;
			  offset = GRUB_TERM_NUM_ENTRIES - 1;
			}
		    }
		}
	      print_entries (menu, first, offset);
	      break;

		case 'i':
		  grub_setcursor (1);
          *auto_boot = 0;
		  *restore = 0;
		  *revert = 0;
		  *take_snapshot = 0;
		  *immutable = 1;
	      return first + offset;	

        case 's':
		  grub_setcursor (1);
          *auto_boot = 0;
		  *restore = 0;
		  *revert = 0;
		  *immutable = 0;
		  *take_snapshot = 1;
	      return first + offset;	

		case 'r':
		{
		  grub_menu_entry_t snapshot = grub_menu_get_entry (menu, first + offset);
		  if (snapshot && snapshot->is_snapshot)
		  {			  
			  if (get_number_snapshots(snapshot) > 0)
			  {		 
				 grub_normal_init_page ();
				 grub_setcursor (1);

				 grub_printf ("\n\nYou selected a snapshot that contains child snapshots.\n\
Restore operation can be only used to a leaf snapshot without any siblings.\n\
Press eny key to go back to the menu...");
				 grub_getkey ();

				 break;
			  }
			  else if (get_number_snapshots(snapshot->parent) > 1)
			  {
				 grub_normal_init_page ();
				 grub_setcursor (1);

				 grub_printf ("\n\nYou selected a snapshot that has sibling snapshots.\n\
Restore operation can be only used to a leaf snapshot without any siblings.\n\
Press eny key to go back to the menu...");
				 grub_getkey ();

				 break;
			  }
			  else
			  {
				  grub_setcursor (1);
				  *auto_boot = 0;
				  *restore = 1;
				  *revert = 0;
				  *immutable = 0;
				  *take_snapshot = 0;
				  return first + offset;
			  }
		  }
		  else
		  {
			  break;
		  }
	    }

		case 't':
		{
		  grub_menu_entry_t snapshot = grub_menu_get_entry (menu, first + offset);
		  if (snapshot && snapshot->is_snapshot)
		  {
			  grub_setcursor (1);
			  *auto_boot = 0;
			  *restore = 0;
			  *revert = 1;
			  *immutable = 0;
			  *take_snapshot = 0;
			  return first + offset;
		  }
		  else
		  {
			  break;
		  }
	    }

	    case '\n':
	    case '\r':
	    case 6:
	      grub_setcursor (1);
              *auto_boot = 0;
	      return first + offset;	

	    case '\e':
	      if (nested)
		{
		  grub_setcursor (1);
		  return -1;
		}
	      break;

	    case 'c':
	      grub_cmdline_run (1);
	      goto refresh;

	    case 'e':
		{
		  grub_menu_entry_t e = grub_menu_get_entry (menu, first + offset);
		  if (e)
		    grub_menu_entry_run (e);
		}
	      goto refresh;

	    default:
	      break;
	    }

	  grub_refresh ();
	}
    }

  /* Never reach here.  */
  return -1;
}

/* Callback invoked immediately before a menu entry is executed.  */
static void
notify_booting (grub_menu_entry_t entry,
		void *userdata __attribute__((unused)))
{
  grub_printf ("  Booting \'%s\'\n\n", entry->title);
}

/* Callback invoked when a default menu entry executed because of a timeout
   has failed and an attempt will be made to execute the next fallback
   entry, ENTRY.  */
static void
notify_fallback (grub_menu_entry_t entry,
		 void *userdata __attribute__((unused)))
{
  grub_printf ("\n  Falling back to \'%s\'\n\n", entry->title);
  grub_millisleep (DEFAULT_ENTRY_ERROR_DELAY_MS);
}

/* Callback invoked when a menu entry has failed and there is no remaining
   fallback entry to attempt.  */
static void
notify_execution_failure (void *userdata __attribute__((unused)))
{
  if (grub_errno != GRUB_ERR_NONE)
    {
      grub_print_error ();
      grub_errno = GRUB_ERR_NONE;
    }
  grub_printf ("\n  Failed to boot default entries.\n");
  grub_wait_after_message ();
}

/* Callbacks used by the text menu to provide user feedback when menu entries
   are executed.  */
static struct grub_menu_execute_callback execution_callback =
{
  .notify_booting = notify_booting,
  .notify_fallback = notify_fallback,
  .notify_failure = notify_execution_failure
};

/* vmlite */
/* find a default path for a differencing disk to the specified menu entry */
static char *
grub_default_differencing_path(grub_menu_entry_t e)
{
	int num_childs = 0;
	grub_menu_entry_t snapshot = e->snapshots;
	char *harddisk_path = NULL;

	/* find number of direct children */
	while (snapshot)
	{		
		snapshot = snapshot->next;
		num_childs ++;
	}

	if (0 == get_harddisk_path(e, &harddisk_path))
	{
		char *simple_path = harddisk_path;
		char *p = grub_strchr(harddisk_path, ';'); /* harddisk= may contains ';' separated paths */
		if (p) *p = 0;

		char *new_path = (char *)grub_malloc(grub_strlen(simple_path) + 16);

		//grub_printf("grub_default_differencing_path(): harddisk_path=%s\n", harddisk_path);
		if (new_path)
		{
			char *ext = grub_strrchr(simple_path, '.');
			if (ext) *ext = 0;
			while (1)
			{
				grub_file_t file;

				grub_sprintf(new_path, "%s-s%d.%s", simple_path, num_childs + 1, ext+1);

				file = grub_file_open (new_path);
				if (file)
				{
					grub_file_close (file);
				}
				else
				{
					break;
				}

				num_childs ++;
			}
		}

		grub_free(harddisk_path);

		//grub_printf("grub_default_differencing_path(): new_path=%s\n", new_path);

		return new_path;
	}	

	return NULL;
}

/* find a default name of the new snapshot to be taken given the specified menu entry
 * caller must free the returned string
 */
static char *
grub_default_snapshot_name(grub_menu_entry_t e)
{
	int num_entries = 0;
	grub_menu_entry_t snapshot = e->snapshots;
	char *title = (char *)grub_malloc(grub_strlen("Snapshot ") + 16);
	if (!title)
		return NULL;

	while (snapshot)
	{
		num_entries ++;
		snapshot = snapshot->next;
	}
	
	grub_sprintf(title, "Snapshot %d", num_entries + 1);

	return title;
}

/* check whether the device (mostly, disk) exists */
static int
check_device_exist (char *device_name)
{
  int found = 0;
  char *dup = grub_strdup(device_name);
  char *device, *p;

  /* doesn't work well for UUID= */
  if (grub_strstr(device_name, "UUID=") || grub_strstr(device_name, "LABEL="))
	  return 1;

  if (dup[0] == '(')
	device = dup + 1;
  else
    device = dup;

   p = grub_strchr(device, ')');
   if (p)
	   *p = 0;
   
  auto int check (const char *name);
  int check (const char *name)
    {
	  //grub_printf("name=%s, device=%s\n", name, device);
      if (!grub_strcmp(name, device))
	  {
		  found = 1;
		  return 1;
	  }
	  else
         return 0;
    }

  grub_device_iterate (check);

  grub_free(dup);

  return found;

/*
  int found = 0;
  grub_file_t file = grub_file_open (device_name);
  if (file)
  {
	  grub_file_close (file);
	  found = 1;
  }

  return found;
*/
}
/* end, vmlite modification */

static grub_err_t
show_text_menu (grub_menu_t menu, int nested)
{
  while (1)
    {
      int boot_entry;
      grub_menu_entry_t e;
      int auto_boot;
	  int take_snapshot = 0;
	  char *snapshot_file_name = NULL;
	  char *snapshot_title = NULL;
	  int restore = 0;
	  int revert = 0;
	  int immutable = 0;

      boot_entry = run_menu (menu, nested, &auto_boot, &take_snapshot, &restore, &revert, &immutable);
      if (boot_entry < 0)
	break;
	
      e = grub_menu_get_entry (menu, boot_entry);
      if (! e)
	continue; /* Menu is empty.  */
	
	  if (restore)
	  {
	     int c;
		 int ok = 0;
		 grub_normal_init_page ();
		 grub_setcursor (1);
		 while (1)
		 {
			 grub_printf (
"\n\nThe restore operation throws away the current snapshot child disk, then restore to its parent or the base image.\n\n\
The virtual disk driver will perform the real work.\n\n\
Are you sure to continue?(y/n)");
			 c = GRUB_TERM_ASCII_CHAR( grub_getkey () );			 
			 grub_putchar ('\n');
			 if (c == 'y')
			 {
				 ok = 1;
				 break;
			 }
			 else if (c == 'n')
			 {
				 ok = 0;
				 break;
			 }
		 }
		 if (!ok)
			continue; 
	  }
	  else if (revert)
	  {
	     int c;
		 int ok = 0;
		 grub_normal_init_page ();
		 grub_setcursor (1);
		 while (1)
		 {
			 grub_printf (
"\n\nThe revert operation discards all changes stored in the current snapshot and resets to its fresh state.\n\
The virtual disk driver will perform the real work.\n\n\
Are you sure to continue?(y/n)");
			 c = GRUB_TERM_ASCII_CHAR( grub_getkey () );			 
			 grub_putchar ('\n');
			 if (c == 'y')
			 {
				 ok = 1;
				 break;
			 }
			 else if (c == 'n')
			 {
				 ok = 0;
				 break;
			 }
		 }
		 if (!ok)
			continue; 
	  }
	  else if (take_snapshot)
	  {		 
		 char tmp_buffer[1024];
		 char *default_path, *default_title;
		 char *p;
		 grub_file_t file;

		 grub_normal_init_page ();
		 grub_setcursor (1);
		 
		 grub_printf ("\n\n\
This operation takes a new snapshot, resulting in a new differencing disk created and a new boot\n\
boot entry inserted to the grub.cfg file. The virtual disk driver will perform the real work.\n\n\
You can enter a path for the differencing disk file, the path must be in one of the following formats:\n\n\
(1) (hdx,y)/path/file.ext, e.g., (hd0,1)/xp-s1.vmdk\n\n\
(2) (UUID=xxx)/path/file.ext, e.g., (UUID=cc24feb624fea31e)/xp-s1.vmdk\n\
    You can run \"probe -u\" under boot loader command window to find the uuid.\n\
    You can run \"vbootctl uuid\" command under Windows to find the uuid for a volume.\n\n\
(3) (LABEL=label)/path/file.ext, e.g. (LABEL=my_disk)/xp-s1.vmdk\n\n\
Please specify a path then press Enter, or simply press Enter to accept the default path:\n"
);

		default_path = grub_default_differencing_path(e);
		if (default_path)
		{
			grub_strcpy(tmp_buffer, default_path);
			grub_free(default_path);
		}
		else
		{
			tmp_buffer[0] = 0;
		}

retry:
		//grub_printf("tmp_buffer=%s\n", tmp_buffer);
		if (!grub_cmdline_get ("", tmp_buffer, sizeof (tmp_buffer) - 1, 0, 0, 0))	    
		   continue;
		
		p = grub_strchr(tmp_buffer, '/');
		if (p) *p = 0; /* (hd0,1)/ */
		if (!p || !check_device_exist(tmp_buffer))
		{
			if (p) *p = '/';
			grub_printf("You have entered a path on a disk that does not exist. Please enter a different path.");
			goto retry;
		}
		if (p) *p = '/';
		
		file = grub_file_open (tmp_buffer);
		if (file)
		{
			grub_file_close (file);
			grub_printf("You have selected an existing file. Please enter a different path.");
			goto retry;
		}

		snapshot_file_name = grub_strdup(tmp_buffer);

		grub_printf ("\n\n\
Please specify the name for the snapshot then press Enter, or simply press Enter to accept the default name:\n"
);
		default_title = grub_default_snapshot_name(e);
		if (default_title)
		{
			grub_strcpy(tmp_buffer, default_title);
			grub_free(default_title);
		}
		else
		{
			tmp_buffer[0] = 0;
		}
	
		if (!grub_cmdline_get ("", tmp_buffer, sizeof (tmp_buffer) - 1, 0, 0, 0))	    
		   continue;
		
		snapshot_title = grub_strdup(tmp_buffer);
	  }
	  else if (get_number_snapshots(e) > 0)
	  {		 
		 grub_normal_init_page ();
		 grub_setcursor (1);

		 grub_printf ("\n\nYou selected a disk image that contains child snapshots.\n\
You need to press 's' or 'r' for such an image.\n\n\
Press eny key to go back to the menu...");
		 grub_getkey ();			

		 continue;
	  }

	  /* we need to use this boot entry, index to menu entries, inside vboot kernel drivers */
		g_boot_entry = boot_entry;
		g_restore = restore;
		g_revert = revert;
		g_immutable = immutable;
		g_take_snapshot = take_snapshot;
		g_snapshot_file_name = snapshot_file_name;
		g_snapshot_title = snapshot_title;

		/* debug 
		grub_normal_init_page ();
		grub_setcursor (1);
		grub_printf("g_boot_entry=%d, g_restore=%d, g_immutable=%d, g_take_snapshot=%d, g_snapshot_file_name=%s\n", 
			g_boot_entry, g_restore, g_immutable, g_take_snapshot, g_snapshot_file_name);
		grub_printf("press any key to continue...");
		grub_getkey();
		*/

      grub_cls ();
      grub_setcursor (1);
	   
      if (auto_boot)
        {
          grub_menu_execute_with_fallback (menu, e, &execution_callback, 0);
        }
      else
        {
          grub_errno = GRUB_ERR_NONE;
          grub_menu_execute_entry (e);
          if (grub_errno != GRUB_ERR_NONE)
            {
              grub_print_error ();
              grub_errno = GRUB_ERR_NONE;
              grub_wait_after_message ();
            }
        }
    }

  return GRUB_ERR_NONE;
}

struct grub_menu_viewer grub_normal_text_menu_viewer =
{
  .name = "text",
  .show_menu = show_text_menu
};

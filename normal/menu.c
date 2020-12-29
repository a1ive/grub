/* menu.c - General supporting functionality for menus.  */
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
#include <grub/misc.h>
#include <grub/loader.h>
#include <grub/mm.h>
#include <grub/time.h>
#include <grub/env.h>
#include <grub/menu_viewer.h>
#include <grub/command.h>
#include <grub/parser.h>
#include <grub/auth.h>

/* vmlite */
extern int g_boot_entry;
extern int g_restore;
extern int g_revert;
extern int g_take_snapshot;
extern char *g_snapshot_file_name;
extern char *g_snapshot_title;
extern int g_immutable;

/* modified by VMLite */
static grub_menu_entry_t find_snapshot(grub_menu_entry_t entry, int index, int *current_index)
{
	grub_menu_entry_t found;
	grub_menu_entry_t snapshot = entry->snapshots;
	while (snapshot)
	{
		if (*current_index == index)
			return snapshot;		
		
		*current_index = *current_index + 1;
		
		found = find_snapshot(snapshot, index, current_index);
		if (found)
			return found;

		snapshot = snapshot->next;
	}

	return NULL;
}

/* get the i-th snapshot of the top level menu_entry, recursively */
grub_menu_entry_t get_snapshot(grub_menu_entry_t menu_entry, int index)
{
	int current_index = 0;

	return find_snapshot(menu_entry, index, &current_index);
}

/* number of snapshots, including nested ones */
int get_number_snapshots(grub_menu_entry_t e)
{
  int num_entries = 0;
  grub_menu_entry_t snapshot = e->snapshots;

  while (snapshot)
  {
    num_entries += get_number_snapshots(snapshot);
	snapshot = snapshot->next;
	num_entries ++;
  }

  return num_entries;
}

/* get outmost menu entry */
grub_menu_entry_t get_outmost_parent(grub_menu_entry_t entry)
{  
  grub_menu_entry_t p = entry->parent;  
  grub_menu_entry_t prev = NULL;

  while (p)
  {		 
	  prev = p;
	  p = p->parent;
  }

  return prev;
}

/* end, vmlite modification */

/* Get a menu entry by its index in the entry list.  */
grub_menu_entry_t
grub_menu_get_entry (grub_menu_t menu, int no)
{
  grub_menu_entry_t e;
  int i, next_i;

  for (e = menu->entry_list, i = 0; e && (i<=no); e = e->next)
  {    
    if (i == no)
		break;	
	next_i = i + 1 + get_number_snapshots(e);
	if (no < next_i)
	{
		e = get_snapshot(e, no - i - 1);
		break;
	}
    i = next_i;
  }	
  return e;
}

/* Return the current timeout. If the variable "timeout" is not set or
   invalid, return -1.  */
int
grub_menu_get_timeout (void)
{
  char *val;
  int timeout;

  val = grub_env_get ("timeout");
  if (! val)
    return -1;

  grub_error_push ();

  timeout = (int) grub_strtoul (val, 0, 0);

  /* If the value is invalid, unset the variable.  */
  if (grub_errno != GRUB_ERR_NONE)
    {
      grub_env_unset ("timeout");
      grub_errno = GRUB_ERR_NONE;
      timeout = -1;
    }

  grub_error_pop ();

  return timeout;
}

/* Set current timeout in the variable "timeout".  */
void
grub_menu_set_timeout (int timeout)
{
  /* Ignore TIMEOUT if it is zero, because it will be unset really soon.  */
  if (timeout > 0)
    {
      char buf[16];

      grub_sprintf (buf, "%d", timeout);
      grub_env_set ("timeout", buf);
    }
}

/* Get the first entry number from the value of the environment variable NAME,
   which is a space-separated list of non-negative integers.  The entry number
   which is returned is stripped from the value of NAME.  If no entry number
   can be found, -1 is returned.  */
static int
get_and_remove_first_entry_number (const char *name)
{
  char *val;
  char *tail;
  int entry;

  val = grub_env_get (name);
  if (! val)
    return -1;

  grub_error_push ();

  entry = (int) grub_strtoul (val, &tail, 0);

  if (grub_errno == GRUB_ERR_NONE)
    {
      /* Skip whitespace to find the next digit.  */
      while (*tail && grub_isspace (*tail))
	tail++;
      grub_env_set (name, tail);
    }
  else
    {
      grub_env_unset (name);
      grub_errno = GRUB_ERR_NONE;
      entry = -1;
    }

  grub_error_pop ();

  return entry;
}

/* vmlite */

#define min(Value1, Value2)                  ( (Value1) <= (Value2) ? (Value1) : (Value2) )

int EXPORT_FUNC(grub_getkey) (void);

/* retrieve harddisk path for the specified menu entry 
   caller must grub_free harddisk_path

   pattern is "hardidsk=" or "vloop="
*/
static int get_harddisk_path_from_pattern(grub_menu_entry_t entry, char *pattern, char **harddisk_path)
{
	char *src;
	char *p;
	char *space = NULL;
	char *quote = NULL;
    grub_size_t length;
	char *path;

	/* first loop, count the length */
	length = 0;
	src = (char *)entry->sourcecode;
	p = grub_strstr(src, pattern);
	if (p) // harddisk, it may contain ';' for differencing disk and base disk files
	{
		p += grub_strlen(pattern);
		/* skip spaces in front */
		while (*p == ' ') p++;
		space = NULL;
		quote = NULL;
		if (p[0] == '"')
		{
			p++;
			quote = grub_strchr(p, '"');
			if (quote) 
			{					
				length += quote - p;
			}
			else
			{
				/* invalid format */
				grub_printf("[VBoot] no closing quote '\"' was found for harddisk path.\n");
				for (;;); /* never returns */										 
				return -2;
			}
		}
		else
		{			
			char *eol1 = grub_strchr(p, '\n');
			char *eol2 = grub_strchr(p, '\r');
			space = grub_strchr(p, ' ');
			if (eol1 || eol2)
			{
				char *end;
				if (!eol2)
					end = eol1;
				else if (!eol1)
					end = eol2;
				else
					end = min(eol1, eol2);

				if (space && space < end)
					length += space - p;
				else
					length += end - p;
			}
			else if (space) 
			{
				length += space - p;
			}
			else
			{
				length += grub_strlen(p);
			}			
		}		
	}

	if (length == 0)
		return -1;

	path = (char *)grub_malloc(length + 1);
	if (!path)
		return -1;
	
	path[0] = 0;

	/* 2nd loop, fill the buufer */
	length = 0;
	src = (char *)entry->sourcecode;
	p = grub_strstr(src, pattern);
	if (p) // harddisk, it may contain ';' for differencing disk and base disk files
	{
		p += grub_strlen(pattern);
		/* skip spaces in front */
		while (*p == ' ') p++;
		space = NULL;
		quote = NULL;
		if (p[0] == '"')
		{
			p++;
			quote = grub_strchr(p, '"');
			if (quote) 
			{					
				grub_strncpy(path, p, quote - p);
				length += quote - p;					
			}
			else
			{
				/* invalid format */
				grub_printf("[VBoot] no closing quote '\"' was found for harddisk path.\n");
				for (;;); /* never returns */										 
				return -2;
			}
		}
		else
		{			
			char *eol1 = grub_strchr(p, '\n');
			char *eol2 = grub_strchr(p, '\r');
			space = grub_strchr(p, ' ');
			if (eol1 || eol2)
			{
				char *end;
				if (!eol2)
					end = eol1;
				else if (!eol1)
					end = eol2;
				else
					end = min(eol1, eol2);

				
				if (space && space < end)
				{
					grub_strncpy(path, p, space - p);
					length += space - p;
				}
				else
				{
					grub_strncpy(path, p, end - p);
					length += end - p;
				}
			}
			else if (space) 
			{
				grub_strncpy(path, p, space - p);
				length += space - p;
			}
			else
			{
				grub_strcat(path + length, p);
				length += grub_strlen(p);
			}
		}
	}

	path[length] = 0;

//grub_printf("get_harddisk_path=%s\n", path);
//grub_printf("press any key to continue...");
//grub_getkey();

	*harddisk_path = path;

	return 0;
}

/* retrieve harddisk path for the specified menu entry 
   caller must grub_free harddisk_path
*/
int get_harddisk_path(grub_menu_entry_t entry, char **harddisk_path)
{
	int rc = get_harddisk_path_from_pattern(entry, "harddisk=", harddisk_path);

	if (rc != 0)
		rc = get_harddisk_path_from_pattern(entry,  "vloop=", harddisk_path);

	return rc;
}

/* return a ';' separated disk names, in the format, "childn;...;child2;child1;base"
  the input entry is the deepest child disk
  caller must free returned pDiskChain

  pattern is "hardidsk=" or "vloop="
*/
static int get_vboot_differencing_disk_full_name(grub_menu_entry_t entry, char *pattern, char **pDiskChain)
{ 
	grub_menu_entry_t e;
	char *src;
	char *p;
	char *space = NULL;
	char *quote = NULL;
	char *new_disk_files;
    grub_size_t length;

	if (!entry->is_snapshot)
	{
		return -1;
	}

	/* first loop, count the length */
	length = 0;
	e = entry; 
	while (e)
	{	  
		src = (char *)e->sourcecode;
		p = grub_strstr(src, pattern);
		if (p) // harddisk, it may contain ';' for differencing disk and base disk files
		{
			p += grub_strlen(pattern);
			/* skip spaces in front */
			while (*p == ' ') p++;
			space = NULL;
			quote = NULL;
			if (p[0] == '"')
			{
				p++;
				quote = grub_strchr(p, '"');
				if (quote) 
				{					
					length += quote - p;
				}
				else
				{
					/* invalid format */
					grub_printf("[VBoot] no closing quote '\"' was found for harddisk path.\n");
					for (;;); /* never returns */										 
					return -2;
				}
			}
			else
			{			
				char *eol1 = grub_strchr(p, '\n');
				char *eol2 = grub_strchr(p, '\r');
				space = grub_strchr(p, ' ');
				if (eol1 || eol2)
				{
					char *end;
					if (!eol2)
						end = eol1;
					else if (!eol1)
						end = eol2;
					else
						end = min(eol1, eol2);

					if (space && space < end)
					{						
						length += space - p;
					}
					else
					{
						length += end - p;
					}
				}
				else if (space) 
				{
					length += space - p;
				}
				else
				{
					length += grub_strlen(p);
				}			
			}

			length ++; /* need a ';' */
		}

		e = e->parent;
	}

	if (length == 0)
		return -1;

	new_disk_files = (char *)grub_malloc(length + 2);
	if (!new_disk_files)
		return -1;
	
	new_disk_files[0] = 0;

	/* 2nd loop, fill the buufer */
	length = 0;
	e = entry; 
	while (e)
	{	  
		src = (char *)e->sourcecode;
		p = grub_strstr(src, pattern);
		if (p) // harddisk, it may contain ';' for differencing disk and base disk files
		{
			p += grub_strlen(pattern);
			/* skip spaces in front */
			while (*p == ' ') p++;
			space = NULL;
			quote = NULL;
			if (p[0] == '"')
			{
				p++;
				quote = grub_strchr(p, '"');
				if (quote) 
				{					
					grub_strncpy(new_disk_files + length, p, quote - p);
					length += quote - p;					
				}
				else
				{
					/* invalid format */
					grub_printf("[VBoot] no closing quote '\"' was found for harddisk path.\n");
					for (;;); /* never returns */										 
					return -2;
				}
			}
			else
			{				
				char *eol1 = grub_strchr(p, '\n');
				char *eol2 = grub_strchr(p, '\r');
				space = grub_strchr(p, ' ');
				if (eol1 || eol2)
				{
					char *end;
					if (!eol2)
						end = eol1;
					else if (!eol1)
						end = eol2;
					else
						end = min(eol1, eol2);

					if (space && space < end)
					{
						grub_strncpy(new_disk_files  + length, p, space - p);
						length += space - p;
					}
					else
					{
						grub_strncpy(new_disk_files  + length, p, end - p);
						length += end - p;
					}
				}
				else if (space) 
				{
					grub_strncpy(new_disk_files  + length, p, space - p);
					length += space - p;
				}
				else
				{
					grub_strcat(new_disk_files + length, p);
					length += grub_strlen(p);
				}
			}

			new_disk_files[length] = 0;
			grub_strcat(new_disk_files, ";");
			length ++;
		}

		e = e->parent;
	}

	new_disk_files[grub_strlen(new_disk_files) - 1] = 0; /* remove last ';' */
	*pDiskChain = new_disk_files;
	
	return 0;
}
/* end, vmlite modification */

/* replace harddisk= or vloop= with the full diskfile path, ';' separated path
   caller needs to free returned string

   pattern is "hardidsk=" or "vloop="
 */
static char* replace_harddisk_path(char *pattern, char *sourcecode, char *new_path, char *extra_args)
{
	char *start, *end, *p;
	char *space = NULL;
	char *quote = NULL;    
	char *new_sourcecode;
	int diff;
	grub_size_t len;

	p = grub_strstr(sourcecode, pattern);
	if (!p)
	{
		return NULL;
	}
	
	p += grub_strlen(pattern);	
	/* skip spaces in front */
	while (*p == ' ') p++;

	start = p;
	
	if (p[0] == '"')
	{
		quote = grub_strchr(p + 1, '"');
		if (quote) 
		{					
			end = quote + 1;
		}
		else
		{
			/* invalid format */
			grub_printf("[VBoot] no closing quote '\"' was found for harddisk path.\n");
			for (;;); /* never returns */									 
			return NULL;
		}
	}
	else
	{
		space = grub_strchr(p, ' ');
		if (space) 
			end = space + 1;
		else
			end = p + grub_strlen(p);
	}

	diff = grub_strlen(new_path) - (end - start);
	if (diff < 0)
		diff = 0;

	len = grub_strlen(sourcecode) + diff;
	if (extra_args)
		len += grub_strlen(extra_args) + 3;
	new_sourcecode = (char *)grub_malloc(len + 3);
	if (new_sourcecode)
	{
		grub_strncpy(new_sourcecode, sourcecode, start - sourcecode);
		new_sourcecode[start - sourcecode] = 0;
		grub_strcat(new_sourcecode, "\"");
		grub_strcat(new_sourcecode, new_path);		
		if (*end == 0)
		{
			grub_strcat(new_sourcecode, "\"");
			if (extra_args)
			{
				grub_strcat(new_sourcecode, " ");
				grub_strcat(new_sourcecode, extra_args);
				grub_strcat(new_sourcecode, " ");
			}
		}
		else
		{
			grub_strcat(new_sourcecode, "\" ");
			if (extra_args)
			{				
				grub_strcat(new_sourcecode, extra_args);
				grub_strcat(new_sourcecode, " ");
			}
			grub_strcat(new_sourcecode, end);
		}		
	}

	return new_sourcecode;
}

/* replace harddisk with the full diskfile path, ';' separated path
   caller needs to free returned string

   vhd vhd0 (hd1,1)/appliances/ubuntu-1010/ubuntu-1010-desktop-i386.vhd --partitions
 */
static char* replace_harddisk_path_for_vhd_cmd(char *sourcecode, char *new_path, char *extra_args)
{
	char *pattern;
	int arg;
	char *start, *end, *p;
	char *space = NULL;
	char *quote = NULL;    
	char *new_sourcecode = NULL;
	int diff;
	grub_size_t len;

	// find vhd command inside source code
	pattern = "vhd ";
	p = grub_strstr(sourcecode, pattern);
	if (!p)
	{		
		pattern = "vhd\t";
		p = grub_strstr(sourcecode, pattern);
		if (!p)
		{			
			return NULL;			
		}		
	}
	
	if (p != sourcecode && *(p-1) != ' ' && *(p-1) != '\t' && *(p-1) != '\n' && *(p-1) != '\r')
		return NULL;

	p += grub_strlen(pattern) - 1;

	// find the disk path, 2nd arg
	arg = 0;
    while (*p && (*p != '\n') && (*p != '\r'))
	{						
		if (*p == ' ' || *p == '\t')
		{			
			while (*p && (*p == ' ' || *p == '\t')) p++;	
		
			if (*p == 0)
				break;

			// skip options
			if (*p == '-')
			{				
				p++;				
			}
			else
			{
				if (arg == 1)
				{
					break;
				}

				arg ++;				
			}			
		}

		if (*p == '"')
		{
			p++;
			while (*p && (*p != '"')) p++;
		}		

		if (*p == 0)
			break;

		p ++;
	}
		
	start = p;

	if (p[0] == '"')
	{
		quote = grub_strchr(p + 1, '"');
		if (quote) 
		{					
			end = quote + 1;
		}
		else
		{
			/* invalid format */
			grub_printf("[VBoot] no closing quote '\"' was found for harddisk path.\n");
			for (;;); /* never returns */									 
			return NULL;
		}
	}
	else
	{
		space = grub_strchr(p, ' ');
		if (space) 
			end = space + 1;
		else
			end = p + grub_strlen(p);
	}

	diff = grub_strlen(new_path) - (end - start);
	if (diff < 0)
		diff = 0;

	len = grub_strlen(sourcecode) + diff;
	if (extra_args)
		len += grub_strlen(extra_args) + 3;
	new_sourcecode = (char *)grub_malloc(len + 3);
	if (new_sourcecode)
	{
		grub_strncpy(new_sourcecode, sourcecode, start - sourcecode);
		new_sourcecode[start - sourcecode] = 0;
		grub_strcat(new_sourcecode, "\"");
		grub_strcat(new_sourcecode, new_path);		
		if (*end == 0)
		{
			grub_strcat(new_sourcecode, "\"");
			if (extra_args)
			{
				grub_strcat(new_sourcecode, " ");
				grub_strcat(new_sourcecode, extra_args);
				grub_strcat(new_sourcecode, " ");
			}
		}
		else
		{
			grub_strcat(new_sourcecode, "\" ");
			if (extra_args)
			{				
				grub_strcat(new_sourcecode, extra_args);
				grub_strcat(new_sourcecode, " ");
			}
			grub_strcat(new_sourcecode, end);
		}		
	}

	return new_sourcecode;
}

/* Run a menu entry.  */
void
grub_menu_execute_entry(grub_menu_entry_t entry)
{
  grub_err_t err = GRUB_ERR_NONE;  
  char *sourcecode = (char *) entry->sourcecode;
  grub_menu_entry_t base_entry;
  char *original_sourcecode;
  char *snapshot_start;
  char *extra_args = NULL;

  /* restore is clicked on a snapshot (as indicated by boot_entry), we need to boot to the parent */
  if (g_restore && entry->parent)
  {
	  entry = entry->parent;
	  sourcecode = (char *) entry->sourcecode;
  }

  if (entry->restricted)
    err = grub_auth_check_authentication (entry->users);

  if (err)
    {
      grub_print_error ();
      grub_errno = GRUB_ERR_NONE;
      return;
    }


  if (entry->is_snapshot)
  {
	  base_entry = get_outmost_parent(entry);
	  sourcecode = (char *) base_entry->sourcecode;	  
  }

  snapshot_start = grub_strstr(sourcecode, "snapshotentry ");
  if (snapshot_start) *snapshot_start = 0; /* base source code ends with first snapshotentry */

  original_sourcecode = sourcecode;
  
  if (g_boot_entry != -1 || g_immutable || g_take_snapshot || g_restore || g_revert)
  {
    grub_size_t len = 0;
	//char *new_sourcecode;
	
	if (g_boot_entry != -1)
	  len += grub_strlen(" boot_entry=") + 8;
  
	if (g_take_snapshot && g_snapshot_file_name)
	{
		len += grub_strlen(" takesnapshot=") + 1 + grub_strlen(g_snapshot_file_name) + 1 + 1;	

		if (g_snapshot_title)
		{			
			len += grub_strlen(" new_menu_entry=") + 1 + grub_strlen(g_snapshot_title) + 1 + 1;	
		}
	}

	if (g_immutable)
      len += grub_strlen(" immutable");

	if (g_restore)
	{ 
		len += grub_strlen(" restore");
	}
	
	if (g_revert)
	{ 
		len += grub_strlen(" revert");
	}

	if (len > 0)
	{
		char *root;
		char *config_file = NULL;
		char *config_file_env;

		root = grub_env_get("root");
		config_file_env = grub_env_get("config_file");
		if (root && config_file_env)
		{
			config_file = (char *)grub_malloc(grub_strlen(" config_file=\"(") + grub_strlen(root) + 2 + grub_strlen(config_file_env) + 10);
			if (config_file)
				grub_sprintf(config_file, " config_file=\"(%s)%s\"", root, config_file_env);

			len += grub_strlen(config_file) + 2;
		}

		extra_args = (char *)grub_malloc(len);
		if (extra_args)
		{
			extra_args[0] = 0;

			if (g_boot_entry != -1)
			  grub_sprintf(extra_args, " boot_entry=%d", g_boot_entry);
			
			if (g_take_snapshot && g_snapshot_file_name)
			{			
				grub_strcat(extra_args, " take_snapshot=\"");
				grub_strcat(extra_args, g_snapshot_file_name);
				grub_strcat(extra_args, "\"");

				if (g_snapshot_title)
				{			
					grub_strcat(extra_args, " new_menu_entry=\"");
					grub_strcat(extra_args, g_snapshot_title);
					grub_strcat(extra_args, "\"");
				}
			}

			if (g_immutable)
			  grub_strcat(extra_args, " immutable");
			
			if (g_restore)
				grub_strcat(extra_args, " restore");

			if (config_file)
				grub_strcat(extra_args, config_file);
		}
	}
  }

  /* we need to find parent disks for snapshot, and append to harddisk= */
  if (entry->is_snapshot)
  {
	char *diskFiles;	

	if (0 == get_vboot_differencing_disk_full_name(entry, "harddisk=", &diskFiles))
	{		
		char *p = replace_harddisk_path("harddisk=", sourcecode, diskFiles, extra_args);		
		if (p)
		   sourcecode = p;
		grub_free(diskFiles);
	}
	else if (0 == get_vboot_differencing_disk_full_name(entry, "vloop=", &diskFiles))
	{		
		char *p = replace_harddisk_path("vloop=", sourcecode, diskFiles, extra_args);		
		if (p)
		   sourcecode = p;

		// also need to replace diskfile for vhd command
		p = replace_harddisk_path_for_vhd_cmd(sourcecode, diskFiles, NULL);
		if (p)
		{		   
		   sourcecode = p;
		}

		grub_free(diskFiles);
	}	
  }
  else if (extra_args)
  {	  
	char *diskFile;	

	if (0 == get_harddisk_path_from_pattern(entry, "harddisk=", &diskFile))
	{		
		char *p = replace_harddisk_path("harddisk=", sourcecode, diskFile, extra_args);		
		if (p)
		   sourcecode = p;
		grub_free(diskFile);
	}
	else if (0 == get_harddisk_path_from_pattern(entry, "vloop=", &diskFile))
	{		
		char *p = replace_harddisk_path("vloop=", sourcecode, diskFile, extra_args);		
		if (p)
		   sourcecode = p;
		grub_free(diskFile);
	}	
  }

  if (snapshot_start)
	*snapshot_start = 's';

  if (extra_args)
	grub_free(extra_args);

  /* debug  
  grub_printf("sourcecode=%s\n", sourcecode);			
  grub_printf("press any key to continue...");
  grub_getkey();
  */

  grub_parser_execute (sourcecode);
 
  if (sourcecode != (char *) entry->sourcecode)
	grub_free(sourcecode);

  if (grub_errno == GRUB_ERR_NONE && grub_loader_is_loaded ())
    /* Implicit execution of boot, only if something is loaded.  */
    grub_command_execute ("boot", 0, 0);
}

/* Execute ENTRY from the menu MENU, falling back to entries specified
   in the environment variable "fallback" if it fails.  CALLBACK is a
   pointer to a struct of function pointers which are used to allow the
   caller provide feedback to the user.  */
void
grub_menu_execute_with_fallback (grub_menu_t menu,
				 grub_menu_entry_t entry,
				 grub_menu_execute_callback_t callback,
				 void *callback_data)
{
  int fallback_entry;

  callback->notify_booting (entry, callback_data);

  grub_menu_execute_entry (entry);

  /* Deal with fallback entries.  */
  while ((fallback_entry = get_and_remove_first_entry_number ("fallback"))
	 >= 0)
    {
      grub_print_error ();
      grub_errno = GRUB_ERR_NONE;

      entry = grub_menu_get_entry (menu, fallback_entry);
      callback->notify_fallback (entry, callback_data);
      grub_menu_execute_entry (entry);
      /* If the function call to execute the entry returns at all, then this is
	 taken to indicate a boot failure.  For menu entries that do something
	 other than actually boot an operating system, this could assume
	 incorrectly that something failed.  */
    }

  callback->notify_failure (callback_data);
}

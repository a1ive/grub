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

#include <config.h>
#include <grub/types.h>
#include <grub/util/obj.h>
#include <grub/util/misc.h>
#include <grub/i18n.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include "progname.h"

int
grub_strcmp (const char *s1, const char *s2)
{
  return strcmp (s1, s2);
}

static struct option options[] = {
  {"help", no_argument, 0, 'h'},
  {"version", no_argument, 0, 'V'},
  {"verbose", no_argument, 0, 'v'},
  {"all-headers", no_argument, 0, 'x'},
  {"segment", no_argument, 0, 's'},
  {"syms", no_argument, 0, 't'},
  {"reloc", no_argument, 0, 'r'},
  {0, 0, 0, 0}
};

static void
usage (int status)
{
  if (status)
    fprintf (stderr, _("Try `%s --help' for more information.\n"), program_name);
  else
    printf (_("\
Usage: %s [OPTIONS] [MODULE_FILES].\n\
\n\
Tool to generate mod file from object files.\n\
\nOptions:\n\
  -h, --help              display this message and exit\n\
  -V, --version           print version information and exit\n\
  -v, --verbose           print verbose messages\n\
  -x, --all               display all information\n\
  -s, --segment           display segment information\n\
  -t, --syms              display symbol table\n\
  -r, --reloc             display reloc table\n\
\n\
Report bugs to <%s>.\n"), program_name, PACKAGE_BUGREPORT);

  exit (status);
}

#define FLAG_DUMP_SEGMENT	1
#define FLAG_DUMP_SYMBOL	2
#define FLAG_DUMP_RELOC		4
#define FLAG_DUMP_ALL		7

static void
dump_strings (char *header, char *image)
{
  printf ("%s:", header);
  while (*image)
    {
      printf (" %s", image);
      image += strlen (image) + 1;
    }
  printf ("\n");
}

static void
objdump (char *objs[], int flag)
{
  while (*objs)
    {
      struct grub_util_obj *obj;
      char *image;
      struct grub_obj_header *e;
      int size;

      image = grub_util_read_image (*objs);
      size = grub_util_get_image_size (*objs);
      obj = grub_obj_load (image, size, 1);

      e = (struct grub_obj_header *) image;
      printf ("filename: %s\n", *objs);
      printf ("mod name: %s\n", image + grub_target_to_host32 (e->mod_deps));
      dump_strings ("mod deps",
		    image + grub_target_to_host32 (e->mod_deps) +
		    strlen (image + grub_target_to_host32 (e->mod_deps)) + 1);
      dump_strings ("mod attr",
		    image + obj->mod_attr);
      printf ("init func: 0x%x\n", grub_target_to_host16 (e->init_func));
      printf ("fini func: 0x%x\n", grub_target_to_host16 (e->fini_func));

      if (flag & FLAG_DUMP_SEGMENT)
	{
	  printf ("\n");
	  grub_obj_dump_segments (obj);
	}

      if (flag & FLAG_DUMP_SYMBOL)
	{
	  printf ("\n");
	  grub_obj_dump_symbols (obj);
	}

      if (flag & FLAG_DUMP_RELOC)
	{
	  printf ("\n");
	  grub_obj_dump_relocs (obj);
	}

      grub_obj_free (obj);
      free (image);
      objs++;
    }
}

int
main (int argc, char *argv[])
{
  int flag = 0;

  set_program_name (argv[0]);

  grub_util_init_nls ();

  /* Check for options.  */
  while (1)
    {
      int c = getopt_long (argc, argv, "hVvxstr", options, 0);

      if (c == -1)
	break;
      else
	switch (c)
	  {
	  case 'h':
	    usage (0);
	    break;

	  case 'V':
	    printf ("%s (%s) %s\n", program_name, PACKAGE_NAME, PACKAGE_VERSION);
	    return 0;

	  case 'v':
	    verbosity++;
	    break;

	  case 'x':
	    flag |= FLAG_DUMP_ALL;
	    break;

	  case 's':
	    flag |= FLAG_DUMP_SEGMENT;
	    break;

	  case 't':
	    flag |= FLAG_DUMP_SYMBOL;
	    break;

	  case 'r':
	    flag |= FLAG_DUMP_RELOC;
	    break;

	  default:
	    usage (1);
	    break;
	  }
    }

  objdump (argv + optind, flag);

  return 0;
}

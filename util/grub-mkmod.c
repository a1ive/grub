/* grub-mkmod.c - tool to generate mod file from object files.  */
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
  {"output", required_argument, 0, 'o'},
  {"name", required_argument, 0, 'n'},
  {0, 0, 0, 0}
};

static void
usage (int status)
{
  if (status)
    fprintf (stderr, _("Try `%s --help' for more information.\n"), program_name);
  else
    printf (_("\
Usage: %s [OPTIONS] [OBJECT_FILES].\n\
\n\
Tool to generate mod file from object files.\n\
\nOptions:\n\
  -h, --help              display this message and exit\n\
  -V, --version           print version information and exit\n\
  -v, --verbose           print verbose messages\n\
  -o, --output=FILE       output a generated image to FILE [default=stdout]\n\
  -n, --name=NAME         set module name\n\
\n\
Report bugs to <%s>.\n"), program_name, PACKAGE_BUGREPORT);

  exit (status);
}

extern int sss;

static void
mkmod (char *objs[], char *name, FILE *fp)
{
  struct grub_util_obj *obj;
  int merge = GRUB_OBJ_MERGE_SAME;

  obj = xmalloc_zero (sizeof (*obj));
  while (*objs)
    {
      char *image;
      int size;

      image = grub_util_read_image (*objs);
      size = grub_util_get_image_size (*objs);
      grub_obj_import (obj, image, size);
      free (image);
      objs++;
    }

  grub_obj_csym_done (obj);
  grub_obj_got_done (obj);

  grub_obj_reverse (obj);
  grub_obj_sort_segments (obj);
  grub_obj_merge_segments (obj, 0, merge);
  grub_obj_reloc_symbols (obj, merge);
  grub_obj_filter_symbols (obj);
  grub_obj_save (obj, name, fp);

  grub_obj_free (obj);
}

int
main (int argc, char *argv[])
{
  FILE *fp = stdout;
  char *output = NULL;
  char *name = NULL;

  set_program_name (argv[0]);

  grub_util_init_nls ();

  /* Check for options.  */
  while (1)
    {
      int c = getopt_long (argc, argv, "hVvo:n:", options, 0);

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

	  case 'o':
	    if (output)
	      free (output);

	    output = xstrdup (optarg);
	    break;

	  case 'n':
	    if (name)
	      free (name);

	    name = xstrdup (optarg);
	    break;

	  default:
	    usage (1);
	    break;
	  }
    }

  if (! name)
    {
      if (! output)
	grub_util_error ("no module name");
      name = grub_util_get_module_name (output);
    }

  if (output)
    {
      fp = fopen (output, "wb");
      if (! fp)
	grub_util_error ("cannot open %s", output);
      free (output);
    }

  mkmod (argv + optind, name, fp);

  fclose (fp);
  free (name);

  return 0;
}

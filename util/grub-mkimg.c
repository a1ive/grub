/* grub-mkrawmod.c - tool to generate raw image from object files.  */
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
  {"base", required_argument, 0, 'b'},
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
Tool to generate img file from object files.\n\
\nOptions:\n\
  -h, --help              display this message and exit\n\
  -V, --version           print version information and exit\n\
  -v, --verbose           print verbose messages\n\
  -o, --output=FILE       output a generated image to FILE [default=stdout]\n\
  -b, --base=ADDR         set base address\n\
\n\
Report bugs to <%s>.\n"), program_name, PACKAGE_BUGREPORT);

  exit (status);
}

static void
save_image (struct grub_util_obj *obj, grub_uint32_t base, FILE *fp)
{
  struct grub_util_obj_segment *seg;

  seg = obj->segments;
  while (seg)
    {
      if (seg->segment.type == GRUB_OBJ_SEG_BSS)
	break;

      grub_util_write_image_at (seg->data, seg->segment.size,
				seg->segment.offset - base, fp);
      seg = seg->next;
    }
}

static void
mkrawimage (char *objs[], grub_uint32_t base, FILE *fp)
{
  struct grub_util_obj *obj;

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

  grub_obj_reverse (obj);
  grub_obj_sort_segments (obj);
  grub_obj_merge_segments (obj, 0, GRUB_OBJ_MERGE_ALL);
  grub_obj_reloc_symbols (obj, GRUB_OBJ_MERGE_ALL);
  grub_obj_link (obj, base);
  save_image (obj, base, fp);

  grub_obj_free (obj);
}

int
main (int argc, char *argv[])
{
  FILE *fp = stdout;
  char *output = NULL;
  grub_uint32_t base = 0;

  set_program_name (argv[0]);

  grub_util_init_nls ();

  /* Check for options.  */
  while (1)
    {
      int c = getopt_long (argc, argv, "hVvo:b:", options, 0);

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

	  case 'b':
	    base = strtoul (optarg, 0, 0);
	    break;

	  default:
	    usage (1);
	    break;
	  }
    }

  if (output)
    {
      fp = fopen (output, "wb");
      if (! fp)
	grub_util_error ("cannot open %s", output);
      free (output);
    }

  mkrawimage (argv + optind, base, fp);

  fclose (fp);

  return 0;
}

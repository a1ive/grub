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
#include <grub/util/misc.h>
#include <grub/lib.h>
#include <grub/handler.h>
#include <grub/i18n.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>

#include "progname.h"

void
grub_putchar (int c)
{
  putchar (c);
}

void
grub_refresh (void)
{
  fflush (stdout);
}

struct grub_handler_class grub_term_input_class;
struct grub_handler_class grub_term_output_class;

int
grub_getkey (void)
{
  return 0;
}

char *
grub_env_get (const char *name __attribute__ ((unused)))
{
  return NULL;
}

static struct option options[] = {
  {"help", no_argument, 0, 'h'},
  {"version", no_argument, 0, 'V'},
  {"verbose", no_argument, 0, 'v'},
  {0, 0, 0, 0}
};

static void
usage (int status)
{
  if (status)
    fprintf (stderr, "Try ``grub-mkpasswd --help'' for more information.\n");
  else
    printf ("\
Usage: grub-mkpasswd [OPTIONS] PASSWORD\n\
\n\
Tool to generate md5 password.\n\
\nOptions:\n\
  -h, --help                display this message and exit\n\
  -V, --version             print version information and exit\n\
  -v, --verbose             print verbose messages\n\
\n\
Report bugs to <%s>.\n", PACKAGE_BUGREPORT);

  exit (status);
}

static void
generate_password (char *password)
{
  char crypted[36];
  char key[32];
  time_t t;
  int i;
  const char *const seedchars =
    "./0123456789ABCDEFGHIJKLMNOPQRST"
    "UVWXYZabcdefghijklmnopqrstuvwxyz";

  /* First create a salt.  */

  /* The magical prefix.  */
  memset (crypted, 0, sizeof (crypted));
  memcpy (crypted, "$1$", 3);

  srand (time (&t));
  /* Generate a salt.  */
  for (i = 0; i < 7; i++)
    {
      /* FIXME: This should be more random.  */
      crypted[3 + i] = seedchars[rand () & 0x3f];
    }

  /* A salt must be terminated with `$', if it is less than 8 chars.  */
  crypted[3 + i] = '$';
  strncpy (key, password, sizeof (key));
  key[sizeof (key) - 1] = 0;

  grub_make_md5_password (key, crypted);

  printf ("%s\n", crypted);
}

int
main (int argc, char *argv[])
{
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  /* Check for options.  */
  while (1)
    {
      int c = getopt_long (argc, argv, "hVv", options, 0);

      if (c == -1)
	break;
      else
	switch (c)
	  {
	  case 'h':
	    usage (0);
	    break;

	  case 'V':
	    printf ("grub-mkpasswd (%s) %s\n", PACKAGE_NAME, PACKAGE_VERSION);
	    return 0;

	  case 'v':
	    verbosity++;
	    break;

	  default:
	    usage (1);
	    break;
	  }
    }

  /* Obtain the filename.  */
  if (optind >= argc)
    {
      fprintf (stderr, "no password specified\n");
      usage (1);
    }

  generate_password (argv[optind]);
  return 0;
}

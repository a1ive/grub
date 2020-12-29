/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 1992-1999,2001,2003,2004,2005,2009 Free Software Foundation, Inc.
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

#include <grub/types.h>
#include <grub/crypto.h>
#include <grub/util/misc.h>
#include <grub/i18n.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <getline.h>
#include <fcntl.h>
#include <errno.h>

#include "progname.h"

#ifndef O_BINARY
#define O_BINARY	0
#endif

#ifndef O_NONBLOCK
#define O_NONBLOCK	0
#endif

/* Few functions to make crypto happy.  */
void *
grub_memmove (void *dest, const void *src, grub_size_t n)
{
  return memmove (dest, src, n);
}

void *
grub_memset (void *s, int c, grub_size_t n)
{
  return memset (s, c, n);
}

int
grub_vprintf (const char *fmt, va_list args)
{
  return vprintf (fmt, args);
}

int
grub_vsnprintf (char *str, grub_size_t n, const char *fmt, va_list args)
{
  return vsnprintf (str, n, fmt, args);
}

void
grub_abort (void)
{
  abort ();
}

static struct option options[] =
  {
    {"iteration_count", required_argument, 0, 'c'},
    {"buflen", required_argument, 0, 'l'},
    {"saltlen", required_argument, 0, 's'},
    {"output", required_argument, 0, 'o'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
  };

static void
usage (int status)
{
  if (status)
    fprintf (stderr, "Try `%s --help' for more information.\n", program_name);
  else
    printf ("\
Usage: %s [OPTIONS]\n\
\nOptions:\n\
     -o FILE, --output=FILE               File to store the password\n\
     -c number, --iteration-count=number  Number of PBKDF2 iterations\n\
     -l number, --buflen=number           Length of generated hash\n\
     -s number, --salt=number             Length of salt\n\
\n\
Report bugs to <%s>.\n", program_name, PACKAGE_BUGREPORT);

  exit (status);
}

static void
hexify (char *hex, grub_uint8_t *bin, grub_size_t n)
{
  while (n--)
    {
      if (((*bin & 0xf0) >> 4) < 10)
	*hex = ((*bin & 0xf0) >> 4) + '0';
      else
	*hex = ((*bin & 0xf0) >> 4) + 'A' - 10;
      hex++;

      if ((*bin & 0xf) < 10)
	*hex = (*bin & 0xf) + '0';
      else
	*hex = (*bin & 0xf) + 'A' - 10;
      hex++;
      bin++;
    }
  *hex = 0;
}

#ifdef __MINGW32__

#include <conio.h>
#include <time.h>

#define SIZE_INC	16

static char *
get_line (void)
{
  char *buf = 0;
  int pos, size;

  pos = size = 0;
  while (1)
    {
      int ch;

      if (pos >= size)
	{
	  size += SIZE_INC;
	  buf = xrealloc (buf, size);
	}
      ch = getch ();
      if (ch == '\r')
	break;
      else if ((ch == '\b') && (pos > 0))
	buf[pos--] = 0;
      else
	buf[pos++] = ch;
    }
  buf[pos] = 0;

  return buf;
}

static char *
get_pass (grub_uint8_t *buf, char *bufhex, grub_uint8_t *salt, char *salthex)
{
  char *pass1, *pass2;
  _cprintf ("Enter password: ");
  pass1 = get_line ();
  _cprintf ("\nReenter password: ");
  pass2 = get_line ();
  _cprintf ("\n");

  if (strcmp (pass1, pass2) != 0)
    {
      memset (pass1, 0, strlen (pass1));
      memset (pass2, 0, strlen (pass2));
      free (pass1);
      free (pass2);
      free (buf);
      free (bufhex);
      free (salthex);
      free (salt);
      grub_util_error ("passwords don't match");
    }
  memset (pass2, 0, strlen (pass2));
  free (pass2);

  return pass1;
}

#else

#include <termios.h>

static char *
get_pass (grub_uint8_t *buf, char *bufhex, grub_uint8_t *salt, char *salthex)
{
  char *pass1, *pass2;
  struct termios s, t;
  ssize_t nr;
  FILE *in, *out;
  int tty_changed;

    /* Disable echoing. Based on glibc.  */
  in = fopen ("/dev/tty", "w+c");
  if (in == NULL)
    {
      in = stdin;
      out = stderr;
    }
  else
    out = in;

  if (tcgetattr (fileno (in), &t) == 0)
    {
      /* Save the old one. */
      s = t;
      /* Tricky, tricky. */
      t.c_lflag &= ~(ECHO|ISIG);
      tty_changed = (tcsetattr (fileno (in), TCSAFLUSH, &t) == 0);
    }
  else
    tty_changed = 0;

  printf ("Enter password: ");
  pass1 = NULL;
  {
    grub_size_t n;
    nr = getline (&pass1, &n, stdin);
  }
  if (nr < 0 || !pass1)
    {
      free (buf);
      free (bufhex);
      free (salthex);
      free (salt);
      /* Restore the original setting.  */
      if (tty_changed)
	(void) tcsetattr (fileno (in), TCSAFLUSH, &s);
      grub_util_error ("failure to read password");
    }
  if (nr >= 1 && pass1[nr-1] == '\n')
    pass1[nr-1] = 0;

  printf ("\nReenter password: ");
  pass2 = NULL;
  {
    grub_size_t n;
    nr = getline (&pass2, &n, stdin);
  }
  /* Restore the original setting.  */
  if (tty_changed)
    (void) tcsetattr (fileno (in), TCSAFLUSH, &s);
  printf ("\n");

  if (nr < 0 || !pass2)
    {
      memset (pass1, 0, strlen (pass1));
      free (pass1);
      free (buf);
      free (bufhex);
      free (salthex);
      free (salt);
      grub_util_error ("failure to read password");
    }
  if (nr >= 1 && pass2[nr-1] == '\n')
    pass2[nr-1] = 0;

  if (strcmp (pass1, pass2) != 0)
    {
      memset (pass1, 0, strlen (pass1));
      memset (pass2, 0, strlen (pass2));
      free (pass1);
      free (pass2);
      free (buf);
      free (bufhex);
      free (salthex);
      free (salt);
      grub_util_error ("passwords don't match");
    }
  memset (pass2, 0, strlen (pass2));
  free (pass2);

  return pass1;
}
#endif

int
main (int argc, char *argv[])
{
  unsigned int c = 10000, buflen = 64, saltlen = 64;
  char *pass1;
  char *bufhex, *salthex;
  gcry_err_code_t gcry_err;
  grub_uint8_t *buf, *salt;
  char *output = NULL;

  set_program_name (argv[0]);

  grub_util_init_nls ();

  /* Check for options.  */
  while (1)
    {
      int c = getopt_long (argc, argv, "c:l:s:o:hvV", options, 0);

      if (c == -1)
	break;

      switch (c)
	{
	case 'c':
	  c = strtoul (optarg, NULL, 0);
	  break;

	case 'l':
	  buflen = strtoul (optarg, NULL, 0);
	  break;

	case 's':
	  saltlen = strtoul (optarg, NULL, 0);
	  break;

	case 'o':
	    if (output)
	      free (output);

	    output = xstrdup (optarg);
	    break;

	case 'h':
	  usage (0);
	  return 0;

	case 'V':
	  printf ("%s (%s) %s\n", program_name,
		  PACKAGE_NAME, PACKAGE_VERSION);
	  return 0;

	default:
	  usage (1);
	  return 1;
	}
    }

  bufhex = malloc (buflen * 2 + 1);
  if (!bufhex)
    grub_util_error ("out of memory");
  buf = malloc (buflen);
  if (!buf)
    {
      free (bufhex);
      grub_util_error ("out of memory");
    }

  salt = malloc (saltlen);
  if (!salt)
    {
      free (bufhex);
      free (buf);
      grub_util_error ("out of memory");
    }
  salthex = malloc (saltlen * 2 + 1);
  if (!salthex)
    {
      free (salt);
      free (bufhex);
      free (buf);
      grub_util_error ("out of memory");
    }

  pass1 = get_pass (buf, bufhex, salt, salthex);

#ifdef __MINGW32__
 {
   unsigned i;
   time_t tm;

   srand (time (&tm));
   for (i = 0; i < saltlen; i++)
     salt[i] = rand ();
 }
#else
#if ! defined (__linux__) && ! defined (__FreeBSD__)
  printf ("WARNING: your random generator isn't known to be secure\n");
#else
#endif

  {
    int fd, rd, len, first;
    grub_uint8_t *ptr;

    fd = open ("/dev/random", O_RDONLY | O_BINARY | O_NONBLOCK);
    if (fd < 0)
      {
	memset (pass1, 0, strlen (pass1));
	free (pass1);
	free (buf);
	free (bufhex);
	free (salthex);
	free (salt);
	grub_util_error ("couldn't retrieve random data for salt");
      }

    ptr = salt;
    len = saltlen;
    first = 1;
    while (len > 0)
      {
	rd = read (fd, ptr, len);
	if ((rd < 0) && (errno != EAGAIN))
	  {
	    close (fd);
	    memset (pass1, 0, strlen (pass1));
	    free (pass1);
	    free (buf);
	    free (bufhex);
	    free (salthex);
	    free (salt);
	    grub_util_error ("couldn't retrieve random data for salt");
	  }

	if (rd == len)
	  break;
	else if (first)
	  {
	    int flags;

	    printf ("Please do some other work (type on keyboard, move mouse, etc)"
		    "\nto give the OS a chance to collect more entropy.\n");
	    first = 0;
	    flags = fcntl (fd, F_GETFL);
	    flags &= ~O_NONBLOCK;
	    fcntl (fd, F_SETFL, flags);
	  }

	if (rd > 0)
	  {
	    ptr += rd;
	    len -= rd;
	  }
      }
    close (fd);
  }
#endif

  gcry_err = grub_crypto_pbkdf2 (GRUB_MD_SHA512,
				 (grub_uint8_t *) pass1, strlen (pass1),
				 salt, saltlen,
				 c, buf, buflen);
  memset (pass1, 0, strlen (pass1));
  free (pass1);

  if (gcry_err)
    {
      memset (buf, 0, buflen);
      memset (bufhex, 0, 2 * buflen);
      free (buf);
      free (bufhex);
      memset (salt, 0, saltlen);
      memset (salthex, 0, 2 * saltlen);
      free (salt);
      free (salthex);
      grub_util_error ("cryptographic error number %d", gcry_err);
    }

  hexify (bufhex, buf, buflen);
  hexify (salthex, salt, saltlen);

  if (output)
    {
      FILE *fp;
      fp = fopen (output, "wb");
      if (! fp)
	{
	  memset (buf, 0, buflen);
	  memset (bufhex, 0, 2 * buflen);
	  free (buf);
	  free (bufhex);
	  memset (salt, 0, saltlen);
	  memset (salthex, 0, 2 * saltlen);
	  free (salt);
	  free (salthex);
	  grub_util_error ("cannot open %s", output);
	}
      else
	{
	  fprintf (fp, "grub.pbkdf2.sha512.%d.%s.%s", c, salthex, bufhex);
	  fclose (fp);
	}
      free  (output);
    }
  else
    printf ("Your PBKDF2 is grub.pbkdf2.sha512.%d.%s.%s\n", c, salthex, bufhex);
  memset (buf, 0, buflen);
  memset (bufhex, 0, 2 * buflen);
  free (buf);
  free (bufhex);
  memset (salt, 0, saltlen);
  memset (salthex, 0, 2 * saltlen);
  free (salt);
  free (salthex);

  return 0;
}

/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 1999,2000,2001,2002,2003,2004,2005,2007  Free Software Foundation, Inc.
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

#include <grub/normal_menu.h>
#include <grub/misc.h>
#include <grub/term.h>
#include <grub/err.h>
#include <grub/types.h>
#include <grub/mm.h>
#include <grub/partition.h>
#include <grub/disk.h>
#include <grub/file.h>
#include <grub/env.h>
#include <grub/lib.h>
#include <grub/i18n.h>
#include <grub/charset.h>

static grub_uint32_t *kill_buf;

static grub_size_t
strlen_ucs4 (const grub_uint32_t *s)
{
  const grub_uint32_t *p = s;

  while (*p)
    p++;

  return p - s;
}
/* A completion hook to print items.  */
static void
print_completion (const char *item, grub_completion_type_t type, int count)
{
  if (count == 0)
    {
      /* If this is the first time, print a label.  */

      grub_puts ("");
      switch (type)
	{
	case GRUB_COMPLETION_TYPE_COMMAND:
	  grub_puts_ (N_("Possible commands are:"));
	  break;
	case GRUB_COMPLETION_TYPE_DEVICE:
	  grub_puts_ (N_("Possible devices are:"));
	  break;
	case GRUB_COMPLETION_TYPE_FILE:
	  grub_puts_ (N_("Possible files are:"));
	  break;
	case GRUB_COMPLETION_TYPE_PARTITION:
	  grub_puts_ (N_("Possible partitions are:"));
	  break;
	case GRUB_COMPLETION_TYPE_ARGUMENT:
	  grub_puts_ (N_("Possible arguments are:"));
	  break;
	default:
	  grub_puts_ (N_("Possible things are:"));
	  break;
	}
      grub_puts ("");
    }

  if (type == GRUB_COMPLETION_TYPE_PARTITION)
    {
      grub_print_device_info (item);
      grub_errno = GRUB_ERR_NONE;
    }
  else
    grub_printf (" %s", item);
}

struct cmdline_term
{
  unsigned xpos, ypos, ystart, width, height;
  struct grub_term_output *term;
};

struct grub_cmdline_get_closure
{
  grub_size_t lpos, llen;
  grub_size_t plen;
  grub_uint32_t *buf;
  grub_size_t max_len;
  struct cmdline_term *cl_terms;
  unsigned nterms;
};

static void
cl_set_pos (struct cmdline_term *cl_term, struct grub_cmdline_get_closure *c)
{
  cl_term->xpos = (c->plen + c->lpos) % (cl_term->width - 1);
  cl_term->ypos = cl_term->ystart + (c->plen + c->lpos) / (cl_term->width - 1);
  grub_term_gotoxy (cl_term->term, cl_term->xpos, cl_term->ypos);
}

static void
cl_set_pos_all (struct grub_cmdline_get_closure *c)
{
  unsigned i;
  for (i = 0; i < c->nterms; i++)
    cl_set_pos (&c->cl_terms[i], c);
}

static inline void __attribute__ ((always_inline))
cl_print (struct cmdline_term *cl_term, int pos, grub_uint32_t c,
	  struct grub_cmdline_get_closure *cc)
{
  grub_uint32_t *p;

  for (p = cc->buf + pos; p < cc->buf + cc->llen; p++)
    {
      if (c)
	grub_putcode (c, cl_term->term);
      else
	grub_putcode (*p, cl_term->term);
      cl_term->xpos++;
      if (cl_term->xpos >= cl_term->width - 1)
	{
	  cl_term->xpos = 0;
	  if (cl_term->ypos >= (unsigned) (cl_term->height - 1))
	    cl_term->ystart--;
	  else
	    cl_term->ypos++;
	  grub_putcode ('\n', cl_term->term);
	}
    }
}

static void
cl_print_all (int pos, grub_uint32_t c, struct grub_cmdline_get_closure *cc)
{
  unsigned i;
  for (i = 0; i < cc->nterms; i++)
    cl_print (&cc->cl_terms[i], pos, c, cc);
}

static void
cl_insert (const grub_uint32_t *str, struct grub_cmdline_get_closure *c)
{
  grub_size_t len = strlen_ucs4 (str);

  if (len + c->llen >= c->max_len)
    {
      grub_uint32_t *nbuf;
      c->max_len *= 2;
      nbuf = grub_realloc (c->buf, sizeof (grub_uint32_t) * c->max_len);
      if (nbuf)
	c->buf = nbuf;
      else
	{
	  grub_print_error ();
	  grub_errno = GRUB_ERR_NONE;
	  c->max_len /= 2;
	}
    }

  if (len + c->llen < c->max_len)
    {
      grub_memmove (c->buf + c->lpos + len, c->buf + c->lpos,
		    (c->llen - c->lpos + 1) * sizeof (grub_uint32_t));
      grub_memmove (c->buf + c->lpos, str, len * sizeof (grub_uint32_t));

      c->llen += len;
      cl_set_pos_all (c);
      cl_print_all (c->lpos, 0, c);
      c->lpos += len;
      cl_set_pos_all (c);
    }
}

static void
cl_delete (unsigned len, struct grub_cmdline_get_closure *c)
{
  if (c->lpos + len <= c->llen)
    {
      grub_size_t saved_lpos = c->lpos;

      c->lpos = c->llen - len;
      cl_set_pos_all (c);
      cl_print_all (c->lpos, ' ', c);
      c->lpos = saved_lpos;
      cl_set_pos_all (c);

      grub_memmove (c->buf + c->lpos, c->buf + c->lpos + len,
		    sizeof (grub_uint32_t) * (c->llen - c->lpos + 1));
      c->llen -= len;
      cl_print_all (c->lpos, 0, c);
      cl_set_pos_all (c);
    }
}

static void
init_clterm (struct cmdline_term *cl_term_cur, struct grub_cmdline_get_closure *c)
{
  cl_term_cur->xpos = c->plen;
  cl_term_cur->ypos = (grub_term_getxy (cl_term_cur->term) & 0xFF);
  cl_term_cur->ystart = cl_term_cur->ypos;
  cl_term_cur->width = grub_term_width (cl_term_cur->term);
  cl_term_cur->height = grub_term_height (cl_term_cur->term);
}

static void
init_clterm_all (struct grub_cmdline_get_closure *c)
{
  unsigned i;
  for (i = 0; i < c->nterms; i++)
    init_clterm (&c->cl_terms[i], c);
}

/* Get a command-line. If ESC is pushed, return zero,
   otherwise return command line.  */
/* FIXME: The dumb interface is not supported yet.  */
char *
grub_cmdline_get (const char *prompt)
{
  int key;
  int histpos = 0;
  const char *prompt_translated = _(prompt);
  char *ret;
  struct grub_cmdline_get_closure c;

  c.max_len = 256;

  c.buf = grub_malloc (c.max_len * sizeof (grub_uint32_t));
  if (!c.buf)
    return 0;

  c.plen = grub_strlen (prompt_translated) + sizeof (" ") - 1;
  c.lpos = c.llen = 0;
  c.buf[0] = '\0';

  {
    grub_term_output_t term;

    FOR_ACTIVE_TERM_OUTPUTS(term)
      if ((grub_term_getxy (term) >> 8) != 0)
	grub_putcode ('\n', term);
  }
  grub_printf ("%s ", prompt_translated);

  {
    struct cmdline_term *cl_term_cur;
    struct grub_term_output *cur;
    c.nterms = 0;
    FOR_ACTIVE_TERM_OUTPUTS(cur)
      c.nterms++;

    c.cl_terms = grub_malloc (sizeof (c.cl_terms[0]) * c.nterms);
    if (!c.cl_terms)
      return 0;
    cl_term_cur = c.cl_terms;
    FOR_ACTIVE_TERM_OUTPUTS(cur)
    {
      cl_term_cur->term = cur;
      init_clterm (cl_term_cur, &c);
      cl_term_cur++;
    }
  }

  if (grub_history_used () == 0)
    grub_history_add (c.buf, c.llen);

  grub_refresh ();

  while ((key = GRUB_TERM_ASCII_CHAR (grub_getkey ())) != '\n' && key != '\r')
    {
      switch (key)
	{
	case 1:	/* Ctrl-a */
	  c.lpos = 0;
	  cl_set_pos_all (&c);
	  break;

	case 2:	/* Ctrl-b */
	  if (c.lpos > 0)
	    {
	      c.lpos--;
	      cl_set_pos_all (&c);
	    }
	  break;

	case 5:	/* Ctrl-e */
	  c.lpos = c.llen;
	  cl_set_pos_all (&c);
	  break;

	case 6:	/* Ctrl-f */
	  if (c.lpos < c.llen)
	    {
	      c.lpos++;
	      cl_set_pos_all (&c);
	    }
	  break;

	case 9:	/* Ctrl-i or TAB */
	  {
	    int restore;
	    char *insertu8;
	    char *bufu8;
	    grub_uint32_t cc;

	    cc = c.buf[c.lpos];
	    c.buf[c.lpos] = '\0';

	    bufu8 = grub_ucs4_to_utf8_alloc (c.buf, c.lpos);
	    c.buf[c.lpos] = cc;
	    if (!bufu8)
	      {
		grub_print_error ();
		grub_errno = GRUB_ERR_NONE;
		break;
	      }

	    insertu8 = grub_complete (bufu8, &restore, print_completion);
	    grub_free (bufu8);

	    if (restore)
	      {
		/* Restore the prompt.  */
		grub_printf ("\n%s ", prompt_translated);
		init_clterm_all (&c);
		cl_print_all (0, 0, &c);
	      }

	    if (insertu8)
	      {
		grub_size_t insertlen;
		grub_ssize_t t;
		grub_uint32_t *insert;

		insertlen = grub_strlen (insertu8);
		insert = grub_malloc ((insertlen + 1) * sizeof (grub_uint32_t));
		if (!insert)
		  {
		    grub_free (insertu8);
		    grub_print_error ();
		    grub_errno = GRUB_ERR_NONE;
		    break;
		  }
		t = grub_utf8_to_ucs4 (insert, insertlen,
				       (grub_uint8_t *) insertu8,
				       insertlen, 0);
		if (t > 0)
		  {
		    if (insert[t-1] == ' ' && c.buf[c.lpos] == ' ')
		      {
			insert[t-1] = 0;
			if (t != 1)
			  cl_insert (insert, &c);
			c.lpos++;
		      }
		    else
		      {
			insert[t] = 0;
			cl_insert (insert, &c);
		      }
		  }

		grub_free (insertu8);
		grub_free (insert);
	      }
	    cl_set_pos_all (&c);
	  }
	  break;

	case 11:	/* Ctrl-k */
	  if (c.lpos < c.llen)
	    {
	      if (kill_buf)
		grub_free (kill_buf);

	      kill_buf = grub_malloc ((c.llen - c.lpos + 1)
				      * sizeof (grub_uint32_t));
	      if (grub_errno)
		{
		  grub_print_error ();
		  grub_errno = GRUB_ERR_NONE;
		}
	      else
		{
		  grub_memcpy (kill_buf, c.buf + c.lpos,
			       (c.llen - c.lpos + 1) * sizeof (grub_uint32_t));
		  kill_buf[c.llen - c.lpos] = 0;
		}

	      cl_delete (c.llen - c.lpos, &c);
	    }
	  break;

	case 14:	/* Ctrl-n */
	case GRUB_TERM_DOWN:
	  {
	    grub_uint32_t *hist;

	    c.lpos = 0;

	    if (histpos > 0)
	      {
		grub_history_replace (histpos, c.buf, c.llen);
		histpos--;
	      }

	    cl_delete (c.llen, &c);
	    hist = grub_history_get (histpos);
	    cl_insert (hist, &c);

	    break;
	  }
	case 16:	/* Ctrl-p */
	case GRUB_TERM_UP:
	  {
	    grub_uint32_t *hist;

	    c.lpos = 0;

	    if (histpos < grub_history_used () - 1)
	      {
		grub_history_replace (histpos, c.buf, c.llen);
		histpos++;
	      }

	    cl_delete (c.llen, &c);
	    hist = grub_history_get (histpos);

	    cl_insert (hist, &c);
	  }
	  break;

	case 21:	/* Ctrl-u */
	  if (c.lpos > 0)
	    {
	      grub_size_t n = c.lpos;

	      if (kill_buf)
		grub_free (kill_buf);

	      kill_buf = grub_malloc (n + 1);
	      if (grub_errno)
		{
		  grub_print_error ();
		  grub_errno = GRUB_ERR_NONE;
		}
	      if (kill_buf)
		{
		  grub_memcpy (kill_buf, c.buf, n);
		  kill_buf[n] = '\0';
		}

	      c.lpos = 0;
	      cl_set_pos_all (&c);
	      cl_delete (n, &c);
	    }
	  break;

	case 25:	/* Ctrl-y */
	  if (kill_buf)
	    cl_insert (kill_buf, &c);
	  break;

	case '\e':
	  return 0;

	case '\b':
	  if (c.lpos > 0)
	    {
	      c.lpos--;
	      cl_set_pos_all (&c);
	    }
          else
            break;
	  /* fall through */

	case 4:	/* Ctrl-d */
	  if (c.lpos < c.llen)
	    cl_delete (1, &c);
	  break;

	default:
	  if (grub_isprint (key))
	    {
	      grub_uint32_t str[2];

	      str[0] = key;
	      str[1] = '\0';
	      cl_insert (str, &c);
	    }
	  break;
	}

      grub_refresh ();
    }

  grub_putchar ('\n');
  grub_refresh ();

  /* Remove leading spaces.  */
  c.lpos = 0;
  while (c.buf[c.lpos] == ' ')
    c.lpos++;

  histpos = 0;
  if (strlen_ucs4 (c.buf) > 0)
    {
      grub_uint32_t empty[] = { 0 };
      grub_history_replace (histpos, c.buf, c.llen);
      grub_history_add (empty, 0);
    }

  ret = grub_ucs4_to_utf8_alloc (c.buf + c.lpos, c.llen - c.lpos + 1);
  grub_free (c.buf);
  return ret;
}

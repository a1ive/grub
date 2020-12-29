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

#include <grub/dl.h>
#include <grub/term.h>
#include <grub/misc.h>
#include <grub/loader.h>
#include <grub/mm.h>
#include <grub/time.h>
#include <grub/env.h>
#include <grub/lib.h>
#include <grub/auth.h>
#include <grub/reader.h>
#include <grub/parser.h>
#include <grub/normal_menu.h>
#include <grub/menu_viewer.h>
#include <grub/i18n.h>
#include <grub/charset.h>
#include <grub/controller.h>
#include <grub/normal.h>

/* Initialize the screen.  */
void
grub_normal_init_page (struct grub_term_output *term)
{
  int msg_len;
  int posx;
  const char *msg = _("%s  version %s");
  char *msg_formatted;
  grub_uint32_t *unicode_msg;
  grub_uint32_t *last_position;

  grub_term_cls (term);

  msg_formatted = grub_xasprintf (msg, PACKAGE_NAME, PACKAGE_VERSION);
  if (!msg_formatted)
    return;

  msg_len = grub_utf8_to_ucs4_alloc (msg_formatted,
  				     &unicode_msg, &last_position);
  grub_free (msg_formatted);

  if (msg_len < 0)
    {
      return;
    }

  posx = grub_getstringwidth (unicode_msg, last_position, term);
  posx = (grub_term_width (term) - posx) / 2;
  grub_term_gotoxy (term, posx, 1);

  grub_print_ucs4 (unicode_msg, last_position, term);
  grub_printf("\n\n");
  grub_free (unicode_msg);
}

static int
grub_username_get (char buf[], unsigned buf_size)
{
  unsigned cur_len = 0;
  int key;

  while (1)
    {
      key = GRUB_TERM_ASCII_CHAR (grub_getkey ());
      if (key == '\n' || key == '\r')
	break;

      if (key == '\e')
	{
	  cur_len = 0;
	  break;
	}

      if (key == '\b')
	{
	  cur_len--;
	  grub_printf ("\b");
	  continue;
	}

      if (!grub_isprint (key))
	continue;

      if (cur_len + 2 < buf_size)
	{
	  buf[cur_len++] = key;
	  grub_putchar (key);
	}
    }

  grub_memset (buf + cur_len, 0, buf_size - cur_len);

  grub_putchar ('\n');
  grub_refresh ();

  return (key != '\e');
}

grub_err_t
grub_normal_check_authentication (const char *userlist)
{
  char login[1024];
  char entered[GRUB_AUTH_MAX_PASSLEN];

  grub_memset (login, 0, sizeof (login));

  if (grub_auth_check_password (userlist, 0, 0))
    return GRUB_ERR_NONE;

  grub_puts_ (N_("Enter username: "));

  if (!grub_username_get (login, sizeof (login) - 1))
    return GRUB_ACCESS_DENIED;

  grub_puts_ (N_("Enter password: "));

  if (!grub_password_get (entered, GRUB_AUTH_MAX_PASSLEN))
    return GRUB_ACCESS_DENIED;

  return (grub_auth_check_password (userlist, login, entered)) ?
    GRUB_ERR_NONE : GRUB_ACCESS_DENIED;
}

static grub_err_t
grub_normal_reader_init (int nested)
{
  struct grub_term_output *term;
  const char *msg = _("Minimal BASH-like line editing is supported. For "
		      "the first word, TAB lists possible command completions. Anywhere "
		      "else TAB lists possible device or file completions. %s");
  const char *msg_esc = _("ESC at any time exits.");
  char *msg_formatted;

  msg_formatted = grub_xasprintf (msg, nested ? msg_esc : "");
  if (!msg_formatted)
    return grub_errno;

  FOR_ACTIVE_TERM_OUTPUTS(term)
  {
    grub_normal_init_page (term);
    grub_term_setcursor (term, 1);

    grub_print_message_indented (msg_formatted, 3, STANDARD_MARGIN, term);
    grub_puts ("\n");
  }
  grub_free (msg_formatted);

  return 0;
}


static grub_err_t
grub_normal_read_line_real (char **line, int cont, int nested)
{
  grub_parser_t parser = grub_parser_get_current ();
  char *prompt;

  if (cont)
    prompt = grub_xasprintf (">");
  else
    prompt = grub_xasprintf ("%s>", parser->name);

  if (!prompt)
    return grub_errno;

  while (1)
    {
      *line = grub_cmdline_get (prompt);
      if (*line)
	break;

      if (cont || nested)
	{
	  grub_free (*line);
	  grub_free (prompt);
	  *line = 0;
	  return grub_errno;
	}
    }

  grub_free (prompt);

  return 0;
}

static grub_err_t
grub_normal_read_line (char **line, int cont,
		       void *closure __attribute__((unused)))
{
  return grub_normal_read_line_real (line, cont, 0);
}

void
grub_normal_cmdline_run (int nested)
{
  grub_err_t err = GRUB_ERR_NONE;

  err = grub_normal_check_authentication (NULL);

  if (err)
    {
      grub_print_error ();
      grub_errno = GRUB_ERR_NONE;
      return;
    }

  grub_normal_reader_init (nested);

  while (1)
    {
      char *line;

      if (grub_normal_exit_level)
	break;

      /* Print an error, if any.  */
      grub_print_error ();
      grub_errno = GRUB_ERR_NONE;

      grub_normal_read_line_real (&line, 0, nested);
      if (! line)
	break;

      grub_parser_get_current ()->parse_line (line, grub_normal_read_line, 0);
      grub_free (line);
    }
}

static char *
grub_env_write_pager (struct grub_env_var *var __attribute__ ((unused)),
		      const char *val)
{
  grub_set_more ((*val == '1'));
  return grub_strdup (val);
}

static grub_err_t
show_menu (grub_menu_t menu, int nested)
{
  if (menu)
    grub_normal_show_menu (menu, nested);
  else if (! nested)
    grub_normal_cmdline_run (0);

  return 0;
}

static struct grub_controller grub_normal_controller =
  {
    .name = "normal",
    .show_menu = show_menu,
  };

GRUB_MOD_INIT(nmenu)
{
  grub_controller_register ("normal", &grub_normal_controller);
  grub_register_variable_hook ("pager", 0, grub_env_write_pager);

  /* Reload terminal colors when these variables are written to.  */
  grub_register_variable_hook ("color_normal", NULL,
			       grub_env_write_color_normal);
  grub_register_variable_hook ("color_highlight", NULL,
			       grub_env_write_color_highlight);

  /* Preserve hooks after context changes.  */
  grub_env_export ("color_normal");
  grub_env_export ("color_highlight");
}

GRUB_MOD_FINI(nmenu)
{
  grub_controller_unregister (&grub_normal_controller);

  grub_register_variable_hook ("pager", 0, 0);
  grub_register_variable_hook ("color_normal", 0, 0);
  grub_register_variable_hook ("color_highlight", 0, 0);
}

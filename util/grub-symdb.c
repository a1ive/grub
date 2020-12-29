/* grub-symdb.c - manage symbol database  */
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
#include <sys/stat.h>

#define _GNU_SOURCE	1
#include <getopt.h>

#include "progname.h"

int
grub_strcmp (const char *s1, const char *s2)
{
  return strcmp (s1, s2);
}

struct grub_symbol_list
{
  struct grub_symbol_list *next;
  char *name;
  struct grub_named_list *defs;
  struct grub_named_list *unds;
};

static struct grub_symbol_list *symbol_list;

struct grub_update_list
{
  struct grub_update_list *next;
  char *name;
  struct grub_named_list *add_mods;
  struct grub_named_list *del_mods;
  struct grub_named_list *cur_mods;
  struct grub_named_list *mod_attr;
};

static struct grub_update_list *update_list;

struct grub_mod_syms
{
  struct grub_named_list *defs;
  struct grub_named_list *unds;
};


void *
grub_sort_list_find (grub_named_list_t head, const char *name)
{
  while (head)
    {
      int r;

      r = strcmp (name, head->name);
      if (! r)
	return head;

      if (r < 0)
	break;

      head = head->next;
    }

  return 0;
}

static int
grub_sort_list_insert_test (grub_named_list_t new_item, grub_named_list_t item,
			    void *closure __attribute__ ((unused)))
{
  return (strcmp (new_item->name, item->name) < 0);
}

static void
grub_sort_list_insert (grub_named_list_t *head, grub_named_list_t item)
{
  grub_list_insert (GRUB_AS_LIST_P (head), GRUB_AS_LIST (item),
		    (grub_list_test_t) grub_sort_list_insert_test, 0);
}

static void
free_named_list (grub_named_list_t *head)
{
  grub_named_list_t cur = *head;

  while (cur)
    {
      grub_named_list_t tmp;

      tmp = cur;
      cur = cur->next;
      free ((char *) tmp->name);
      free (tmp);
    }

  *head = 0;
}

static int
remove_string (grub_named_list_t *head, char *name)
{
  grub_named_list_t p;

  p = grub_sort_list_find (*head, name);
  if (p)
    {
      grub_list_remove (GRUB_AS_LIST_P (head), GRUB_AS_LIST (p));
      free ((char *) p->name);
      free (p);

      return 1;
    }

  return 0;
}

static int
insert_string (grub_named_list_t *head, char *name)
{
  struct grub_named_list *n;

  n = grub_sort_list_find (*head, name);
  if (n)
    return 0;

  n = xmalloc (sizeof (struct grub_named_list));
  n->name = xstrdup (name);
  grub_sort_list_insert (head, n);

  return 1;
}

static struct grub_symbol_list *
insert_symbol (char *name)
{
  struct grub_symbol_list *n;

  n = grub_sort_list_find (GRUB_AS_NAMED_LIST (symbol_list), name);
  if (! n)
    {
      n = xmalloc (sizeof (struct grub_symbol_list));
      n->name = xstrdup (name);
      n->defs = 0;
      n->unds = 0;

      grub_sort_list_insert (GRUB_AS_NAMED_LIST_P (&symbol_list),
			     GRUB_AS_NAMED_LIST (n));
    }

  return n;
}

static struct grub_update_list *
insert_update (char *name)
{
  struct grub_update_list *n;

  n = grub_sort_list_find (GRUB_AS_NAMED_LIST (update_list), name);
  if (! n)
    {
      n = xmalloc (sizeof (struct grub_update_list));
      n->name = xstrdup (name);
      n->add_mods = 0;
      n->del_mods = 0;
      n->cur_mods = 0;
      n->mod_attr = 0;

      grub_sort_list_insert (GRUB_AS_NAMED_LIST_P (&update_list),
			     GRUB_AS_NAMED_LIST (n));
    }

  return n;
}

static void
add_update (char *m1, char *m2, int is_add)
{
  struct grub_update_list *u;

  u = insert_update (m2);
  if (is_add)
    {
      remove_string (&u->del_mods, m1);
      insert_string (&u->add_mods, m1);
    }
  else
    insert_string (&u->del_mods, m1);
}

static void
read_symdb (char *path)
{
  FILE *fp;
  char line[512];
  struct grub_symbol_list *sym = 0;

  fp = fopen (path, "r");
  if (! fp)
    return;

  while (fgets (line, sizeof (line), fp))
    {
      char *p;

      p = line + strlen (line) - 1;
      while ((p >= line) && ((*p == '\r') || (*p == '\n') || (*p == ' ')))
	p--;

      if (p < line)
	continue;

      *(p + 1) = 0;

      p = line;
      while (*p == ' ')
	p++;

      if (*p == '#')
	continue;

      if ((*p == '+') || (*p == '-'))
	{
	  if (! sym)
	    grub_util_error ("No current symbol.");

	  insert_string ((*p == '+') ? &sym->defs : &sym->unds, p + 1);
	}
      else
	sym = insert_symbol (p);
    }

  fclose (fp);
}

static void
write_symdb (char *path)
{
  FILE *fp;
  struct grub_symbol_list *sym;

  fp = fopen (path, "w");
  if (! fp)
    grub_util_error ("Can\'t write to ", path);

  sym = symbol_list;
  while (sym)
    {
      struct grub_named_list *mod;

      fprintf (fp, "%s\n", sym->name);
      mod = sym->defs;
      while (mod)
	{
	  fprintf (fp, "+%s\n", mod->name);
	  mod = mod->next;
	}
      mod = sym->unds;
      while (mod)
	{
	  fprintf (fp, "-%s\n", mod->name);
	  mod = mod->next;
	}

      sym = sym->next;
    }

  fclose (fp);
}

static void
check_symdb (void)
{
  struct grub_symbol_list *sym;

  sym = symbol_list;
  while (sym)
    {
      if (! sym->defs)
	printf ("undefined: %s\n", sym->name);
      else if (sym->defs->next)
	printf ("duplicate: %s\n", sym->name);

      sym = sym->next;
    }
}

static void
read_mod_syms (struct grub_mod_syms *mod_syms, char *path)
{
  struct grub_util_obj *obj;
  char *image;
  size_t size;
  struct grub_util_obj_symbol *sym;
  struct grub_util_obj_reloc *rel;

  mod_syms->defs = 0;
  mod_syms->unds = 0;

  image = grub_util_read_image (path);
  size = grub_util_get_image_size (path);
  obj = grub_obj_load (image, size, 0);

  sym = obj->symbols;
  while (sym)
    {
      insert_string (&mod_syms->defs, sym->name);
      sym = sym->next;
    }

  rel = obj->relocs;
  while (rel)
    {
      if (rel->symbol_name)
	insert_string (&mod_syms->unds, rel->symbol_name);
      rel = rel->next;
    }

  grub_obj_free (obj);
  free (image);
}

static void
update_mods (char *mods[], const char *dir)
{
  for (; mods[0]; mods++)
    {
      char *mod_name, *mod_path;
      struct grub_mod_syms mod_syms;
      struct grub_named_list *m;

      mod_name = grub_util_get_module_name (mods[0]);
      mod_path = grub_util_get_module_path (dir, mod_name);

      if (strstr (mod_name, "-symdb"))
	{
	  free (mod_name);
	  free (mod_path);
	  continue;
	}

      insert_update (mod_name);
      read_mod_syms (&mod_syms, mod_path);

      m = mod_syms.defs;
      while (m)
	{
	  struct grub_symbol_list *sym;
	  struct grub_named_list *n;

	  sym = insert_symbol ((char *) m->name);
	  insert_string (&sym->defs, mod_name);

	  n = sym->unds;
	  while (n)
	    {
	      add_update ((char *) mod_name, (char *) n->name, 1);
	      n = n->next;
	    }

	  m = m->next;
	}

      m = mod_syms.unds;
      while (m)
	{
	  struct grub_symbol_list *sym;
	  struct grub_named_list *n;

	  sym = insert_symbol ((char *) m->name);
	  insert_string (&sym->unds, mod_name);

	  n = sym->defs;
	  while (n)
	    {
	      add_update ((char *) n->name, (char *) mod_name, 1);
	      n = n->next;
	    }

	  m = m->next;
	}

      free (mod_name);
      free (mod_path);
    }
}

static void
remove_mods (char *mods[])
{
  for (; mods[0]; mods++)
    {
      char *mod_name;
      struct grub_symbol_list *sym;

      mod_name = grub_util_get_module_name (mods[0]);

      sym = symbol_list;
      while (sym)
	{
	  struct grub_named_list *m, *n;

	  m = sym->defs;
	  while (m)
	    {
	      int r;

	      r = strcmp (mod_name, m->name);
	      if (! r)
		break;

	      if (r < 0)
		{
		  m = 0;
		  break;
		}

	      m = m->next;
	    }

	  n = sym->unds;
	  while (n)
	    {
	      if (m)
		{
		  add_update ((char *) m->name, (char *) n->name, 0);
		}
	      else
		{
		  int r;

		  r = strcmp (mod_name, n->name);
		  if (! r)
		    {
		      m = sym->defs;
		      while (m)
			{
			  add_update ((char *) m->name, (char *) n->name, 0);
			  m = m->next;
			}

		      break;
		    }

		  if (r < 0)
		    break;
		}

	      n = n->next;
	    }

	  sym = sym->next;
	}

      free (mod_name);
    }
}

static void
dump_update (void)
{
  struct grub_update_list *u;

  u = update_list;
  while (u)
    {
      struct grub_named_list *n;

      printf ("%s:" , u->name);
      n = u->add_mods;
      while (n)
	{
	  printf (" +%s", n->name);
	  n = n->next;
	}

      n = u->del_mods;
      while (n)
	{
	  printf (" -%s", n->name);
	  n = n->next;
	}

      printf ("\n");
      u = u->next;
    }
}

static void
update_deps (struct grub_update_list *u, char *path)
{
  struct grub_obj_header *hdr;
  struct grub_obj_segment *seg;
  struct grub_named_list *n;
  char *image, *p;
  int size, modified;

  image = grub_util_read_image (path);
  hdr = (struct grub_obj_header *) image;

  seg = &hdr->segments[0];
  while (seg->type != GRUB_OBJ_SEGMENT_END)
    seg++;

  insert_string (&u->mod_attr, "fs");
  insert_string (&u->mod_attr, "partmap");
  insert_string (&u->mod_attr, "parttool");
  insert_string (&u->mod_attr, "command");
  insert_string (&u->mod_attr, "handler");
  insert_string (&u->mod_attr, "terminal");
  insert_string (&u->mod_attr, "video");

  p = image + grub_target_to_host32 (seg->offset);
  while (*p)
    {
      insert_string (&u->mod_attr, (char *) p);
      p += strlen (p) + 1;
    }

  size = grub_target_to_host32 (hdr->mod_deps);
  size += strlen (image + size) + 1;

  p = image + size;
  while (*p)
    {
      insert_string (&u->cur_mods, (char *) p);
      p += strlen (p) + 1;
    }

  modified = 0;
  n = u->del_mods;
  while (n)
    {
      modified |= remove_string (&u->cur_mods, (char *) n->name);
      n = n->next;
    }
  n = u->add_mods;
  while (n)
    {
      if (strcmp (n->name, "kernel"))
	modified |= insert_string (&u->cur_mods, (char *) n->name);
      n = n->next;
    }

  if (modified)
    {
      FILE *fp;

      fp = fopen (path, "wb");
      if (! fp)
	grub_util_error ("Can\'t write to %s", path);

      grub_util_write_image (image, size, fp);

      p = image;
      n = u->cur_mods;
      while (n)
	{
	  strcpy (p, n->name);
	  p += strlen (p) + 1;
	  n = n->next;
	}
      *(p++) = 0;

      grub_util_write_image (image, p - image, fp);

      fclose (fp);
    }

  free (image);
}

static void
write_moddep (struct grub_update_list *u, FILE *fp)
{
  struct grub_named_list *n;

  if (! u->cur_mods)
    return;

  fprintf (fp, "%s:", u->name);
  n = u->cur_mods;
  while (n)
    {
      fprintf (fp, " %s", n->name);
      n = n->next;
    }

  fprintf (fp, "\n");
  free_named_list (&u->cur_mods);
}

static void
write_modattr (struct grub_update_list *u, char *attr, FILE *fp)
{
  struct grub_named_list *n, *c;
  int len;

  if (! u->mod_attr)
    return;

  n = grub_sort_list_find (u->mod_attr, attr);
  if (! n)
    return;

  len = strlen (attr);
  c = n->next;
  while (c)
    {
      grub_named_list_t t;

      if ((memcmp (c->name, attr, len)) || (c->name[len] != ':'))
	break;

      if (c->name[len + 1])
	fprintf (fp, "%s: %s\n", c->name + len + 1, u->name);
      else
	fprintf (fp, "%s\n", u->name);

      t = c;
      c = c->next;
      free ((char *) t->name);
      free (t);
    }

  n->next = c;
}

static void
update_lists (char *dir, char *name, int is_moddep, int has_value)
{
  FILE *fp;
  struct stat st;
  char *path, *image;
  struct grub_update_list *u;
  char buf[strlen(name) + 5];  

  sprintf (buf, "%s.lst", name);
  path = grub_util_get_path (dir, buf);
  if (stat (path, &st) == 0)
    {
      int size;

      size = grub_util_get_image_size (path);
      image = xmalloc (size + 1);
      grub_util_load_image (path, image);
      image[size] = 0;
    }
  else
    image = 0;

  fp = fopen (path, "w");
  if (! fp)
    grub_util_error ("Can\'t write to ", path);

  if (image)
    {
      char *line;

      line = image;
      while (*line)
	{
	  char *p, *c, *saved;
	  int n;

	  n = strcspn (line, "\r\n");
	  p = line;

	  line += n;
	  while ((*line == '\r') || (*line == '\n'))
	    line++;

	  saved = p;
	  *(p + n) = 0;

	  c = strchr (p, ':');
	  if (is_moddep)
	    {
	      if (! c)
		continue;

	      *c = 0;
	    }
	  else
	    {
	      if (has_value)
		{
		  if (! c)
		    continue;

		  p = c + 1;
		  while (*p == ' ')
		    p++;
		}

	      c = 0;
	    }

	  u = update_list;
	  while (u)
	    {
	      int r;

	      r = strcmp (p, u->name);
	      if (! r)
		break;

	      if (r < 0)
		{
		  u = 0;
		  break;
		}

	      u = u->next;
	    }

	  if (c)
	    *c = ':';

	  if (u)
	    (is_moddep) ? write_moddep (u, fp) : write_modattr (u, name, fp);
	  else
	    fprintf (fp, "%s\n", saved);
	}
    }

  u = update_list;
  while (u)
    {
      (is_moddep) ? write_moddep (u, fp) : write_modattr (u, name, fp);
      u = u->next;
    }

  fclose (fp);
  free (path);
  free (image);
}

static void
apply_update (char *dir)
{
  struct grub_update_list *u;

  u = update_list;
  while (u)
    {
      char *mod_path;

      mod_path = grub_util_get_module_path (dir, u->name);
      update_deps (u, mod_path);
      free (mod_path);
      u = u->next;
    }

  update_lists (dir, "moddep", 1, 0);
  update_lists (dir, "fs", 0, 0);
  update_lists (dir, "partmap", 0, 0);
  update_lists (dir, "parttool", 0, 1);
  update_lists (dir, "command", 0, 1);
  update_lists (dir, "handler", 0, 1);
  update_lists (dir, "terminal", 0, 1);
  update_lists (dir, "video", 0, 0);
}

static struct option options[] =
  {
    {"directory", required_argument, 0, 'd'},
    {"test", no_argument, 0, 't'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {"verbose", no_argument, 0, 'v'},
    {0, 0, 0, 0}
  };

static void
usage (int status)
{
  if (status)
    fprintf (stderr, _("Try `%s --help' for more information.\n"), program_name);
  else
    printf (_("\
Usage: %s [OPTION]... COMMAND\n\
\n\
Manage the symbol database of GRUB.\n\
\nCommands:\n\
  update MODULES          add/update modules to the symbol database\n\
  remove MODULES          remove modules from the symbol databsae\n\
  check                   check for duplicate and unresolved symbols\n\
\n\
  -d, --directory=DIR     use images and modules under DIR [default=%s]\n\
  -t, --test              test mode\n\
  -h, --help              display this message and exit\n\
  -V, --version           print version information and exit\n\
  -v, --verbose           print verbose messages\n\
\n\
Report bugs to <%s>.\n\
"), program_name, GRUB_LIBDIR, PACKAGE_BUGREPORT);

  exit (status);
}

int
main (int argc, char *argv[])
{
  char *dir = NULL;
  char *path;
  int test_mode = 0;

  set_program_name (argv[0]);

  grub_util_init_nls ();

  while (1)
    {
      int c = getopt_long (argc, argv, "d:thVv", options, 0);

      if (c == -1)
	break;
      else
	switch (c)
	  {
	  case 'd':
	    if (dir)
	      free (dir);

	    dir = xstrdup (optarg);
	    break;

	  case 't':
	    test_mode++;
	    break;

	  case 'h':
	    usage (0);
	    break;

	  case 'V':
	    printf ("%s (%s) %s\n", program_name, PACKAGE_NAME, PACKAGE_VERSION);
	    return 0;

	  case 'v':
	    verbosity++;
	    break;

	  default:
	    usage (1);
	    break;
	  }
    }

  if (! dir)
    dir = xstrdup (GRUB_LIBDIR);

  path = grub_util_get_path (dir, "modsym.lst");

  argv += optind;
  argc -= optind;
  if (argc == 0)
    grub_util_error ("No command specified.");

  read_symdb (path);
  if (! strcmp (argv[0], "update"))
    {
      remove_mods (argv + 1);
      update_mods (argv + 1, dir);
      if (test_mode)
	dump_update ();
      else
	{
	  apply_update (dir);
	  write_symdb (path);
	}
    }
  else if (! strcmp (argv[0], "remove"))
    {
      remove_mods (argv + 1);
      if (test_mode)
	dump_update ();
      else
	{
	  apply_update (dir);
	  write_symdb (path);
	}
    }
  else if (! strcmp (argv[0], "check"))
    {
      check_symdb ();
    }
  else
    grub_util_error ("Unkown command %s.", argv[0]);

  free (path);
  free (dir);

  return 0;
}

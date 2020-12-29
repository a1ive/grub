/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2009  Free Software Foundation, Inc.
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

#include <grub/mm.h>
#include <grub/env.h>
#include <grub/misc.h>
#include <grub/file.h>
#include <grub/uitree.h>
#include <grub/parser.h>

GRUB_EXPORT (grub_uitree_dump);
GRUB_EXPORT (grub_uitree_find);
GRUB_EXPORT (grub_uitree_find_id);
GRUB_EXPORT (grub_uitree_create_node);
GRUB_EXPORT (grub_uitree_load_string);
GRUB_EXPORT (grub_uitree_load_file);
GRUB_EXPORT (grub_uitree_clone);
GRUB_EXPORT (grub_uitree_free);
GRUB_EXPORT (grub_uitree_root);
GRUB_EXPORT (grub_uitree_set_prop);
GRUB_EXPORT (grub_uitree_get_prop);

struct grub_uitree grub_uitree_root;

#define GRUB_UITREE_TOKEN_MAXLEN	1024

#define TOKEN_TYPE_ERROR	0
#define TOKEN_TYPE_STRING	1
#define TOKEN_TYPE_EQUAL	2
#define TOKEN_TYPE_LBRACKET	3
#define TOKEN_TYPE_RBRACKET	4

static int
read_token (char *buf, char *pre, int size, int *count, int *subst)
{
  int escape = 0;
  int string = 0;
  int comment = 0;
  int prefix = 0;
  int pos = 0, ofs = 0;

  *subst = 0;
  *count = 0;
  if (*pre)
    {
      char c = *pre;

      *pre = 0;
      if (c == '=')
	return TOKEN_TYPE_EQUAL;
      else if (c == '{')
	return TOKEN_TYPE_LBRACKET;
      else
	return TOKEN_TYPE_RBRACKET;
    }

  while (1)
    {
      char c;

      if (ofs >= size)
	return 0;

      c = buf[ofs++];

      if (c == '\r')
	continue;

      if (c == '\t')
	c = ' ';

      if (comment)
	{
	  if (c == '\n')
	    {
	      comment = 0;
	      if (pos)
		break;
	    }
	  continue;
	}

      if (string)
	{
	  if (escape)
	    {
	      escape = 0;

	      if (c == 'n')
		c = '\n';
	    }
	  else
	    {
	      if (c == '\"')
		break;

	      if (c == '\\')
		{
		  escape = 1;
		  continue;
		}
	    }

	  if (c == '$')
	    {
	      prefix++;
	      if (prefix == 2)
		{
		  (*subst)++;
		  prefix = 0;
		}
	    }
	  else
	    prefix = 0;
	}
      else
	{
	  if ((c == ' ') || (c == '\n'))
	    {
	      if (pos)
		break;
	      else
		continue;
	    }
	  else if (c == '\"')
	    {
	      string = 1;
	      continue;
	    }
	  else if (c == '#')
	    {
	      comment = 1;
	      continue;
	    }
	  else if ((c == '=') || (c == '{') || (c == '}'))
	    {
	      if (pos)
		*pre = c;
	      else
		{
		  *count = ofs;
		  if (c == '=')
		    return TOKEN_TYPE_EQUAL;
		  else if (c == '{')
		    return TOKEN_TYPE_LBRACKET;
		  else
		    return TOKEN_TYPE_RBRACKET;
		}

	      break;
	    }
	}

      buf[pos++] = c;
    }

  buf[pos] = 0;
  *count = ofs;
  return TOKEN_TYPE_STRING;
}

static void
grub_uitree_dump_padding (int level)
{
  while (level > 0)
    {
      grub_printf ("  ");
      level--;
    }
}

static void
grub_uitree_dump_node (grub_uitree_t node, int level)
{
  grub_uiprop_t p = node->prop;

  grub_uitree_dump_padding (level);
  grub_printf ("%s", node->name);
  while (p)
    {
      grub_printf (" %s=\"%s\"", p->name, p->value);
      p = p->next;
    }
  grub_printf ("\n");

  level++;
  node = node->child;
  while (node)
    {
      grub_uitree_dump_node (node, level);
      node = node->next;
    }
}

void
grub_uitree_dump (grub_uitree_t node)
{
  grub_uitree_dump_node (node, 0);
}

grub_uitree_t
grub_uitree_find (grub_uitree_t node, const char *name)
{
  node = node->child;
  while (node)
    {
      if (! grub_strcmp (node->name, name))
	break;
      node = node->next;
    }

  return node;
}

grub_uitree_t
grub_uitree_find_id (grub_uitree_t node, const char *name)
{
  grub_uitree_t child;

  child = node;
  while (child)
    {
      const char *id;

      id = grub_uitree_get_prop (child, "id");
      if ((id) && (! grub_strcmp (id, name)))
	return child;

      child = grub_tree_next_node (GRUB_AS_TREE (node), GRUB_AS_TREE (child));
    }
  return 0;
}

grub_uitree_t
grub_uitree_create_node (const char *name)
{
  grub_uitree_t n;

  n = grub_zalloc (sizeof (struct grub_uitree) + grub_strlen (name) + 1);
  if (! n)
    return 0;

  grub_strcpy (n->name, name);
  return n;
}

static void
grub_uitree_reset_node (grub_uitree_t node)
{
  grub_uitree_t child;
  grub_uiprop_t p;

  child = node->child;
  while (child)
    {
      grub_uitree_t c;

      c = child;
      child = child->next;
      grub_uitree_free (c);
    }
  node->child = 0;

  p = node->prop;
  while (p)
    {
      grub_uiprop_t c;

      c = p;
      p = p->next;
      grub_free (c);
    }
  node->prop = 0;
}

grub_uitree_t
grub_uitree_clone (grub_uitree_t node)
{
  grub_uitree_t n, child;
  grub_uiprop_t *ptr, prop;

  n = grub_uitree_create_node (node->name);
  if (! n)
    return 0;

  ptr = &n->prop;
  prop = node->prop;
  while (prop)
    {
      grub_uiprop_t p;
      int len;

      len = (prop->value - (char *) prop) + grub_strlen (prop->value) + 1;
      p = grub_malloc (len);
      if (! p)
	{
	  grub_uitree_free (n);
	  return 0;
	}
      grub_memcpy (p, prop, len);
      *ptr = p;
      ptr = &(p->next);
      prop = prop->next;
    }

  child = node->child;
  while (child)
    {
      grub_uitree_t c;

      c = grub_uitree_clone (child);
      if (! c)
	{
	  grub_uitree_free (n);
	  return 0;
	}
      grub_tree_add_child (GRUB_AS_TREE (n), GRUB_AS_TREE (c), -1);
      child = child->next;
    }

  return n;
}

#define METHOD_APPEND	0
#define METHOD_MERGE	1
#define METHOD_REPLACE	2

static grub_uitree_t
grub_uitree_load_buf (const char *prefix, int prefix_len, grub_uitree_t root,
		      char *buf, int size, int flags)
{
  int type, count, sub;
  char pre;
  grub_uitree_t node;

  if (((grub_uint8_t) buf[0] == 0xef) &&
      ((grub_uint8_t) buf[1] == 0xbb) &&
      ((grub_uint8_t) buf[2] == 0xbf))
    {
      buf += 3;
      size -= 3;
    }

  node = root;
  pre = 0;
  while ((type = read_token (buf, &pre, size, &count, &sub))
	 != TOKEN_TYPE_ERROR)
    {
      if (type == TOKEN_TYPE_STRING)
	{
	  char *str, *str2;

	  str = buf;
	  buf += count;
	  size -= count;
	  type = read_token (buf, &pre, size, &count, &sub);
	  str2 = buf;
	  buf += count;
	  size -= count;
	  if (type == TOKEN_TYPE_EQUAL)
	    {
	      char *p;

	      if (read_token (buf, &pre, size, &count, &sub)
		  != TOKEN_TYPE_STRING)
		break;

	      if (sub)
		{
		  char *p1, *p2;
		  
		  p = grub_malloc (grub_strlen (buf) + (prefix_len - 2) * sub
				   + 1);
		  if (! p)
		    break;

		  p1 = buf;
		  p2 = p;
		  while (1)
		    {
		      char *t;

		      t = grub_strstr (p1, "$$");
		      if (t)
			{
			  grub_memcpy (p2, p1, t - p1);
			  p2 += (t - p1);
			  grub_memcpy (p2, prefix, prefix_len);
			  p2 += prefix_len;
			  p1 = t + 2;
			}
		      else
			{
			  grub_strcpy (p2, p1);
			  break;
			}
		    }
		}
	      else
		p = buf;		
		
	      if (grub_uitree_set_prop (node, str, p))
		break;

	      if (sub)
		grub_free (p);

	      buf += count;
	      size -= count;
	    }
	  else if (type == TOKEN_TYPE_LBRACKET)
	    {
	      grub_uitree_t p;
	      int method;

	      if ((flags & GRUB_UITREE_LOAD_FLAG_ROOT) && (node == root))
		method = METHOD_REPLACE;
	      else
		method = METHOD_APPEND;

	      if (*str == '+')
		{
		  method = METHOD_MERGE;
		  str++;
		}
	      else if (*str == '-')
		{
		  method = METHOD_REPLACE;
		  str++;
		}

	      if (! *str)
		break;

	      if (flags & GRUB_UITREE_LOAD_FLAG_SINGLE)
		method = METHOD_APPEND;

	      p = (method == METHOD_APPEND) ? 0 : grub_uitree_find (node, str);
	      if (p)
		{
		  if (method == METHOD_REPLACE)
		    grub_uitree_reset_node (p);
		  node = p;
		}
	      else
		{
		  grub_uitree_t n;

		  n = grub_uitree_create_node (str);
		  if (! n)
		    break;

		  grub_tree_add_child (GRUB_AS_TREE (node), GRUB_AS_TREE (n),
				       -1);
		  node = n;
		}
	    }
	  else if ((type == TOKEN_TYPE_STRING) &&
		   ((! grub_strcmp (str, "include")) ||
		    (! grub_strcmp (str, "-include"))))
	    {
	      int ignore_error;
	      grub_uitree_t n;
	      int b;
	      int pos;
	      char *p;

	      ignore_error = (str[0] == '-');
	      pos = prefix_len;
	      while  ((str2[0] == '.') && (str2[1] == '.'))
		{
		  str2 += 2;
		  if (*str2 == '/')
		    str2++;

		  while ((pos > 0) && (prefix[pos - 1] != '/'))
		    pos--;
		  if (pos > 0)
		    pos--;
		}
		
	      p = grub_malloc (pos + grub_strlen (str2) + 2);
	      if (! p)
		break;

	      grub_memcpy (p, prefix, pos);
	      p[pos++] = '/';
	      grub_strcpy (p + pos, str2);

	      b = (node == root) ? flags : 0;
	      n = grub_uitree_load_file (node, p, b);
	      grub_free (p);
	      if (ignore_error)
		grub_errno = 0;
	      else if (grub_errno)
		break;

	      if ((b & GRUB_UITREE_LOAD_FLAG_SINGLE) && (n != NULL))
		return n;
	    }
	  else
	    break;
	}
      else if (type == TOKEN_TYPE_RBRACKET)
	{
	  buf += count;
	  size -= count;
	  if (node == root)
	    break;

	  if ((flags & GRUB_UITREE_LOAD_FLAG_SINGLE) && (node->parent == root))
	    return node;

	  node = node->parent;
	}
      else
	break;
    }

  return 0;
}

grub_uitree_t
grub_uitree_load_string (grub_uitree_t root, char *buf, int flags)
{
  char *prefix;

  prefix = grub_env_get ("prefix");
  if (! prefix)
    prefix = "";
  return grub_uitree_load_buf (prefix, grub_strlen (prefix),
			       root, buf, grub_strlen (buf), flags);
}

grub_uitree_t
grub_uitree_load_file (grub_uitree_t root, const char *name, int flags)
{
  grub_uitree_t result = 0;
  grub_file_t file;
  int size;
  const char *prefix;

  file = grub_file_open (name);
  if (! file)
    return 0;
  
  prefix = grub_strrchr (name, '/');
  if (! prefix)
    prefix = name;

  size = file->size;
  if (size)
    {
      char *buf;

      buf = grub_malloc (size);
      if (buf)
	{
	  grub_file_read (file, buf, size);
	  result = grub_uitree_load_buf (name, prefix - name, root,
					 buf, size, flags);
	}
      grub_free (buf);
    }

  grub_file_close (file);

  return result;
}

void
grub_uitree_free (grub_uitree_t node)
{
  if (node)
    {
      grub_uitree_reset_node (node);
      grub_free (node);
    }
}

grub_err_t
grub_uitree_set_prop (grub_uitree_t node, const char *name, const char *value)
{
  int ofs, len;
  grub_uiprop_t p, *ptr, next;

  ofs = grub_strlen (name) + 1;
  len = ofs + grub_strlen (value) + 1;

  next = 0;
  ptr = &node->prop;
  while (*ptr)
    {
      if (! grub_strcmp ((*ptr)->name, name))
	{
	  next = (*ptr)->next;
	  break;
	}
      ptr = &((*ptr)->next);
    }

  p = grub_realloc (*ptr, sizeof (struct grub_uiprop) + len);
  if (! p)
    return grub_errno;

  *ptr = p;
  p->next = next;
  grub_strcpy (p->name, name);
  grub_strcpy (p->name + ofs, value);
  p->value = &p->name[ofs];

  return grub_errno;
}

char *
grub_uitree_get_prop (grub_uitree_t node, const char *name)
{
  grub_uiprop_t prop = node->prop;

  while (prop)
    {
      if (! grub_strcmp (prop->name, name))
	return prop->value;
      prop = prop->next;
    }

  return 0;
}

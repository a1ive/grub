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
#include <grub/misc.h>
#include <grub/kernel.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void
grub_obj_reverse (struct grub_util_obj *obj)
{
  obj->segments = grub_list_reverse (GRUB_AS_LIST (obj->segments));
  obj->symbols = grub_list_reverse (GRUB_AS_LIST (obj->symbols));
  obj->relocs = grub_list_reverse (GRUB_AS_LIST (obj->relocs));
}

void
grub_obj_sort_segments (struct grub_util_obj *obj)
{
  grub_list_t n;
  int i;

  n = 0;
  for (i = GRUB_OBJ_SEG_TEXT; i <= GRUB_OBJ_SEG_INFO; i++)
    {
      struct grub_util_obj_segment **p, *q;

      for (p = &obj->segments, q = *p; q; q = *p)
	if (q->segment.type == i)
	  {
	    *p = q->next;
	    grub_list_push (&n, GRUB_AS_LIST (q));
	  }
	else
	  p = &(q->next);
    }

  obj->segments = grub_list_reverse (n);
}

static int
check_merge (struct grub_util_obj_segment *s1,
	     struct grub_util_obj_segment *s2,
	     int merge)
{
  if (! s2)
    return 0;

  switch (merge)
    {
    case GRUB_OBJ_MERGE_NONE:
      return (s1 == s2);

    case GRUB_OBJ_MERGE_SAME:
      return (s1->segment.type == s2->segment.type);

    case GRUB_OBJ_MERGE_ALL:
      return 1;
    }

  return 0;
}

static inline grub_uint32_t
align_segment (grub_uint32_t offset, grub_uint32_t align)
{
  return offset = (offset + (align - 1)) & ~(align - 1);
}

void
grub_obj_merge_segments (struct grub_util_obj *obj, int align, int merge)
{
  struct grub_util_obj_segment *p, *first, *prev;
  grub_uint32_t offset;

  if (merge == GRUB_OBJ_MERGE_NONE)
    return;

  first = 0;
  prev = 0;
  offset = 0;
  p = obj->segments;
  while (p)
    {
      if (check_merge (p, first, merge))
	{
	  int cur_align;

	  if (p->segment.align > first->segment.align)
	    first->segment.align = p->segment.align;

	  cur_align = p->segment.align;
	  if ((p->segment.type != prev->segment.type) && (align > cur_align))
	    cur_align = align;

	  offset = align_segment (offset, cur_align);
	  p->segment.offset = offset;
	  offset += p->segment.size;
	}
      else
	{
	  first = p;
	  offset = p->segment.size;
	}

      prev = p;
      p = p->next;
    }
}

void
grub_obj_reloc_symbols (struct grub_util_obj *obj, int merge)
{
  struct grub_util_obj_reloc *rel;

  for (rel = obj->relocs; rel; rel = rel->next)
    {
      char *addr;
      int type;

      if (! rel->segment)
	continue;

      if (! rel->segment->data)
	grub_util_error ("can\'t relocate in .bss segment");

      addr = rel->segment->data + rel->reloc.offset;
      type = rel->reloc.type & GRUB_OBJ_REL_TYPE_MASK;

      if (! rel->symbol_segment)
	{
	  struct grub_util_obj_symbol *sym;

	  sym = grub_named_list_find (GRUB_AS_NAMED_LIST (obj->symbols),
				      rel->symbol_name);
	  if (sym)
	    {
#ifdef GRUB_TARGET_USE_ADDEND
	      rel->reloc.addend += sym->symbol.offset;
#else
	      if (type == GRUB_OBJ_REL_TYPE_32)
		{
		  grub_uint32_t v;

		  v = grub_target_to_host32 (*((grub_uint32_t *) addr));
		  *((grub_uint32_t *) addr) =
		    grub_host_to_target32 (v + sym->symbol.offset);
		}
	      else if (type == GRUB_OBJ_REL_TYPE_16)
		{
		  grub_uint16_t v;

		  v = grub_target_to_host16 (*((grub_uint16_t *) addr));
		  *((grub_uint16_t *) addr) =
		    grub_host_to_target16 (v + sym->symbol.offset);
		}
	      else if (type == GRUB_OBJ_REL_TYPE_64)
		{
		  grub_uint64_t v;

		  v = grub_target_to_host64 (*((grub_uint64_t *) addr));
		  *((grub_uint64_t *) addr) =
		    grub_host_to_target64 (v + sym->symbol.offset);
		}
	      else
		grub_util_error ("invalid relocation type %d", type);
#endif
	      rel->symbol_segment = sym->segment;
	    }
	}

      if (rel->symbol_segment)
	{
	  grub_target_addr_t delta;

	  delta = rel->symbol_segment->segment.offset;
	  if ((check_merge (rel->segment, rel->symbol_segment, merge)) &&
	      (rel->reloc.type & GRUB_OBJ_REL_FLAG_REL))
	    {
	      delta -= rel->segment->segment.offset + rel->reloc.offset;
	      rel->segment = 0;
	    }

#ifdef GRUB_TARGET_USE_ADDEND
	  rel->reloc.addend += delta;
	  if (rel->reloc.type & GRUB_OBJ_REL_FLAG_REL)
	    {
	      if (type == GRUB_OBJ_REL_TYPE_32)
		{
		  *((grub_uint32_t *) addr) =
		    grub_host_to_target32 (rel->reloc.addend);
		}
	      else if (type == GRUB_OBJ_REL_TYPE_16)
		{
		  *((grub_uint16_t *) addr) =
		    grub_host_to_target16 (rel->reloc.addend);
		}
#if defined(GRUB_TARGET_POWERPC)
	      else if (type == GRUB_OBJ_REL_TYPE_16HI)
		{
		  *((grub_uint16_t *) addr) =
		    grub_host_to_target16 (rel->reloc.addend >> 16);
		}
	      else if (type == GRUB_OBJ_REL_TYPE_16HA)
		{
		  *((grub_uint16_t *) addr) =
		    grub_host_to_target16 ((rel->reloc.addend + 0x8000) >> 16);
		}
	      else if (type == GRUB_OBJ_REL_TYPE_24)
		{
		  grub_uint32_t v;
		  grub_int32_t a;

		  v = grub_target_to_host32 (*((grub_uint32_t *) addr));
		  a = rel->reloc.addend;

		  if (a << 6 >> 6 != a)
		    grub_util_error ("relocation overflow");

		  v = (v & 0xfc000003) | (rel->reloc.addend & 0x3fffffc);
		  *((grub_uint32_t *) addr) = grub_host_to_target32 (v);
		}
#elif defined(GRUB_TARGET_SPARC64)
	      else if (type == GRUB_OBJ_REL_TYPE_30)
		{
		  grub_uint32_t v;
		  grub_int32_t a;

		  v = grub_target_to_host32 (*((grub_uint32_t *) addr));
		  a = rel->reloc.addend;

		  if (a << 2 >> 2 != a)
		    grub_util_error ("relocation overflow");

		  v = ((v & 0xc0000000) |
		       ((rel->reloc.addend >> 2) & 0x3fffffff));
		  *((grub_uint32_t *) addr) = grub_host_to_target32 (v);
		}
#endif
	      else
		grub_util_error ("invalid relocation type %d", type);
	    }
#else
	  if (type == GRUB_OBJ_REL_TYPE_32)
	    {
	      grub_uint32_t v;

	      v = grub_target_to_host32 (*((grub_uint32_t *) addr));
	      *((grub_uint32_t *) addr) = grub_host_to_target32 (v + delta);
	    }
	  else if (type == GRUB_OBJ_REL_TYPE_16)
	    {
	      grub_uint16_t v;

	      v = grub_target_to_host16 (*((grub_uint16_t *) addr));
	      *((grub_uint16_t *) addr) = grub_host_to_target16 (v + delta);
	    }
	  else if (type == GRUB_OBJ_REL_TYPE_64)
	    {
	      grub_uint64_t v;

	      v = grub_target_to_host64 (*((grub_uint64_t *) addr));
	      *((grub_uint64_t *) addr) = grub_host_to_target64 (v + delta);
	    }
	  else
	    grub_util_error ("invalid relocation type %d", type);
#endif
	}
    }
}

struct grub_strtab
{
  struct grub_strtab *next;
  char *name;
  int len;
};
typedef struct grub_strtab *grub_strtab_t;

static int
grub_strtab_find (grub_strtab_t head, char *name)
{
  int index = 1;
  int len = strlen (name);

  while (head)
    {
      if (head->len >= len)
	{
	  int ofs;

	  ofs = head->len - len;
	  if (! strcmp (head->name + ofs, name))
	    {
	      index += ofs;
	      return index;
	    }
	}

      index += head->len + 1;
      head = head->next;
    }

  return -index;
}

static int
grub_strtab_insert_test (grub_strtab_t new_item, grub_strtab_t item,
			 void *closure __attribute__ ((unused)))
{
  return (strcmp (new_item->name, item->name) < 0);
}

static void
grub_strtab_insert (grub_strtab_t *head, char *name)
{
  grub_strtab_t nitem;

  if (grub_strtab_find (*head, name) > 0)
    return;

  nitem = xmalloc (sizeof (*nitem));
  nitem->name = name;
  nitem->len = strlen (name);

  grub_list_insert (GRUB_AS_LIST_P (head), GRUB_AS_LIST (nitem),
		    (grub_list_test_t) grub_strtab_insert_test, 0);
}

#define GRUB_OBJ_HEADER_MAX	0xffff
#define ALIGN_BUF_SIZE		2048

void
grub_obj_save (struct grub_util_obj *obj, char *mod_name, FILE *fp)
{
  char *buf, *p;
  struct grub_obj_header *hdr;
  struct grub_util_obj_segment *seg;
  struct grub_util_obj_symbol *sym;
  struct grub_util_obj_reloc *rel;
  int idx;
  grub_uint32_t offset, raw_size;
  grub_strtab_t strtab;
  int strtab_size;

  if ((! obj->segments) || (obj->segments->segment.offset))
    grub_util_error ("invalid segment");

  buf = xmalloc (GRUB_OBJ_HEADER_MAX);
  hdr = (struct grub_obj_header *) buf;

  hdr->magic = grub_host_to_target32 (GRUB_OBJ_HEADER_MAGIC);
  hdr->version = grub_host_to_target16 (GRUB_OBJ_HEADER_VERSION);
  hdr->init_func = grub_host_to_target16 (GRUB_OBJ_FUNC_NONE);
  hdr->fini_func = grub_host_to_target16 (GRUB_OBJ_FUNC_NONE);

  idx = 0;
  offset = 0;
  raw_size = 0;
  hdr->segments[0].offset = 0;
  seg = obj->segments;
  while (seg)
    {
      struct grub_util_obj_segment *cur;
      grub_uint32_t size;
      int is_last;

      cur = seg;
      seg = seg->next;

      if (! cur->segment.offset)
	{
	  if (idx >= GRUB_OBJ_SEGMENT_END)
	    grub_util_error ("too many segments");

	  hdr->segments[idx].type = cur->segment.type;
	  hdr->segments[idx].align = cur->segment.align;
	  hdr->segments[idx].size = 0;
	  raw_size = 0;
	}

      cur->index = idx;
      size = align_segment (grub_target_to_host32 (hdr->segments[idx].size),
			    cur->segment.align);
      size += cur->segment.size;
      hdr->segments[idx].size = grub_host_to_target32 (size);

      is_last = ((! seg) || (! seg->segment.offset));

      if (cur->segment.type != GRUB_OBJ_SEG_BSS)
	{
	  raw_size = align_segment (raw_size, cur->segment.align);
	  raw_size += (is_last) ? cur->raw_size : cur->segment.size;
	}

      if (is_last)
	{
	  offset += raw_size;
	  idx++;
	  hdr->segments[idx].offset = offset;
	}
    }

  hdr->segments[idx].type = GRUB_OBJ_SEGMENT_END;
  p = ((char *) &hdr->segments[idx]) + 5;

  strtab = 0;
  sym = obj->symbols;
  while (sym)
    {
      if (sym->segment)
	{
	  grub_uint32_t ofs;

	  ofs = sym->symbol.offset + sym->segment->segment.offset;
	  if (! strcmp (sym->name, "grub_mod_init"))
	    {
	      if ((ofs >= GRUB_OBJ_HEADER_MAX) || (sym->segment->index))
		grub_util_error ("init function too far");

	      hdr->init_func = grub_host_to_target16 (ofs);
	    }
	  else if (! strcmp (sym->name, "grub_mod_fini"))
	    {
	      if ((ofs >= GRUB_OBJ_HEADER_MAX) || (sym->segment->index))
		grub_util_error ("fini function too far");

	      hdr->fini_func = grub_host_to_target16 (ofs);
	    }

	  if (! sym->exported)
	    {
	      sym->segment = 0;
	    }
	  else
	    grub_strtab_insert (&strtab, sym->name);
	}
      sym = sym->next;
    }

  rel = obj->relocs;
  while (rel)
    {
      if ((rel->segment) && (! rel->symbol_segment))
	grub_strtab_insert (&strtab, rel->symbol_name);
      rel = rel->next;
    }

  strtab_size = - grub_strtab_find (strtab, "?");
  if (strtab_size >= GRUB_OBJ_HEADER_MAX)
    grub_util_error ("string table too large");

  hdr->symbol_table = grub_host_to_target16 (p - buf);
  sym = obj->symbols;
  while (sym)
    {
      if (sym->segment)
	{
	  struct grub_obj_symbol *s;

	  s = (struct grub_obj_symbol *) p;
	  p += sizeof (struct grub_obj_symbol);
	  if (p - buf >= GRUB_OBJ_HEADER_MAX)
	    grub_util_error ("symbol table too large");

	  s->segment = sym->segment->index;
	  s->name =
	    grub_host_to_target16 (grub_strtab_find (strtab, sym->name));
	  s->offset = grub_host_to_target32 (sym->symbol.offset
					     + sym->segment->segment.offset);
	}
      sym = sym->next;
    }
  *(p++) = GRUB_OBJ_SEGMENT_END;
  if (p - buf >= GRUB_OBJ_HEADER_MAX)
    grub_util_error ("symbol table too large");

  hdr->reloc_table = grub_host_to_target16 (p - buf);
  rel = obj->relocs;
  while (rel)
    {
      if (rel->segment)
	{
	  struct grub_obj_reloc_extern *r;

	  r = (struct grub_obj_reloc_extern *) p;
	  p += ((rel->symbol_segment) ? sizeof (struct grub_obj_reloc) :
		sizeof (struct grub_obj_reloc_extern));
	  if (p - buf >= GRUB_OBJ_HEADER_MAX)
	    grub_util_error ("symbol table too large");

	  r->segment = rel->segment->index;
	  r->type = rel->reloc.type;
	  r->offset = grub_host_to_target32 (rel->reloc.offset
					     + rel->segment->segment.offset);
#ifdef GRUB_TARGET_USE_ADDEND
	  r->addend = grub_host_to_target32 (rel->reloc.addend);
#endif

	  if (rel->symbol_segment)
	    {
	      r->symbol_segment = rel->symbol_segment->index;
	    }
	  else
	    {
	      r->symbol_segment = GRUB_OBJ_SEGMENT_END;
	      r->symbol_name =
		grub_host_to_target16 (grub_strtab_find (strtab,
							 rel->symbol_name));
	    }
	}
      rel = rel->next;
    }
  *(p++) = GRUB_OBJ_SEGMENT_END;
  if (p - buf >= GRUB_OBJ_HEADER_MAX)
    grub_util_error ("symbol table too large");

  hdr->string_table = grub_host_to_target16 (p - buf);
  offset = strtab_size + grub_target_to_host16 (hdr->string_table);
  idx = 0;
  while (1)
    {
      hdr->segments[idx].offset =
	grub_host_to_target32 (hdr->segments[idx].offset + offset);
      if (hdr->segments[idx].type == GRUB_OBJ_SEGMENT_END)
	break;
      idx++;
    }
  hdr->mod_deps =
    grub_host_to_target32 (grub_target_to_host32 (hdr->segments[idx].offset)
			   + obj->attr_len + 1);

  grub_util_write_image (buf, grub_target_to_host16 (hdr->string_table), fp);
  free (buf);

  buf = xmalloc (strtab_size);
  p = buf;
  *(p++) = 0;

  while (strtab)
    {
      grub_strtab_t cur;

      cur = strtab;
      strtab = strtab->next;

      strcpy (p, cur->name);
      p += cur->len + 1;
      free (cur);
    }

  grub_util_write_image (buf, strtab_size, fp);
  free (buf);

  buf = xmalloc_zero (ALIGN_BUF_SIZE);

  seg = obj->segments;
  raw_size = 0;
  while (seg)
    {
      struct grub_util_obj_segment *cur;

      cur = seg;
      seg = seg->next;

      if (! cur->segment.offset)
	raw_size = 0;

      if (cur->segment.type != GRUB_OBJ_SEG_BSS)
	{
	  grub_uint32_t size;
	  int is_last;

	  size = align_segment (raw_size, cur->segment.align);
	  if (size != raw_size)
	    {
	      if (size - raw_size > ALIGN_BUF_SIZE)
		grub_util_error ("alignment too large");

	      grub_util_write_image (buf, size - raw_size, fp);
	    }

	  raw_size = size;
	  is_last = ((! seg) || (! seg->segment.offset));
	  size = (is_last) ? cur->raw_size : cur->segment.size;
	  grub_util_write_image (cur->data, size, fp);
	  raw_size += size;
	}
      else
	break;
    }

  if (obj->attr_len)
    grub_util_write_image (obj->attr, obj->attr_len, fp);

  strcpy (buf + 1, mod_name);
  grub_util_write_image (buf, strlen (mod_name) + 3, fp);
  free (buf);
}

struct grub_util_obj *
grub_obj_load (char *image, int size, int load_data)
{
  struct grub_util_obj *obj;
  struct grub_obj_header *hdr;
  struct grub_obj_symbol *sym;
  struct grub_obj_reloc_extern *rel;
  char *strtab;
  struct grub_util_obj_segment **segments;
  int i;

  hdr = (struct grub_obj_header *) image;

  if ((size <= (int) sizeof (*hdr)) ||
      (grub_target_to_host32 (hdr->magic) != GRUB_OBJ_HEADER_MAGIC))
    grub_util_error ("invalid module file");

  if (grub_target_to_host16 (hdr->version) != GRUB_OBJ_HEADER_VERSION)
    grub_util_error ("version number not match");

  obj = xmalloc_zero (sizeof (*obj));
  segments = xmalloc_zero (256 * sizeof (segments[0]));

  for (i = 0; hdr->segments[i].type != GRUB_OBJ_SEGMENT_END; i++)
    {
      struct grub_util_obj_segment *p;

      p = xmalloc_zero (sizeof (*p));
      p->segment.type = hdr->segments[i].type;
      p->segment.align = hdr->segments[i].align;
      p->segment.size = grub_target_to_host32 (hdr->segments[i].size);
      p->file_off = grub_target_to_host32 (hdr->segments[i].offset);
      p->raw_size =
	grub_target_to_host32 (hdr->segments[i + 1].offset) - p->file_off;
      p->index = i;

      if ((p->raw_size) && (load_data))
	{
	  p->data = xmalloc_zero (p->segment.size);
	  memcpy (p->data, image + p->file_off, p->raw_size);
	}

      segments[i] = p;
      grub_list_push (GRUB_AS_LIST_P (&obj->segments), GRUB_AS_LIST (p));
    }

  obj->mod_attr = grub_target_to_host32 (hdr->segments[i].offset);

  strtab = image + grub_target_to_host16 (hdr->string_table);
  for (sym = (struct grub_obj_symbol *)
	 (image + grub_target_to_host16 (hdr->symbol_table));
       sym->segment != GRUB_OBJ_SEGMENT_END; sym++)
    {
      struct grub_util_obj_symbol *p;

      p = xmalloc_zero (sizeof (*p));
      p->name = xstrdup (strtab + grub_target_to_host16 (sym->name));
      p->segment = segments[sym->segment];
      p->symbol.offset = grub_target_to_host32 (sym->offset);

      grub_list_push (GRUB_AS_LIST_P (&obj->symbols), GRUB_AS_LIST (p));
    }

  for (rel = (struct grub_obj_reloc_extern *)
	 (image + grub_target_to_host16 (hdr->reloc_table));
       rel->segment != GRUB_OBJ_SEGMENT_END;)
    {
      struct grub_util_obj_reloc *p;

      p = xmalloc_zero (sizeof (*p));
      p->segment = segments[rel->segment];
      p->reloc.type = rel->type;
      p->reloc.offset = grub_target_to_host32 (rel->offset);
#ifdef GRUB_TARGET_USE_ADDEND
      p->reloc.addend = grub_target_to_host32 (rel->addend);
#endif

      if (rel->symbol_segment == GRUB_OBJ_SEGMENT_END)
	{
	  p->symbol_name =
	    xstrdup (strtab + grub_target_to_host16 (rel->symbol_name));
	  rel++;
	}
      else
	{
	  p->symbol_segment = segments[rel->symbol_segment];
	  rel = (struct grub_obj_reloc_extern *)
	    ((char *) rel + sizeof (struct grub_obj_reloc));
	}

      grub_list_push (GRUB_AS_LIST_P (&obj->relocs), GRUB_AS_LIST (p));
    }

  free (segments);
  grub_obj_reverse (obj);
  return obj;
}

void
grub_obj_free (struct grub_util_obj *obj)
{
  struct grub_util_obj_segment *seg;
  struct grub_util_obj_symbol *sym;
  struct grub_util_obj_reloc *rel;

  seg = obj->segments;
  while (seg)
    {
      struct grub_util_obj_segment *p;

      p = seg;
      seg = seg->next;

      if (p->data)
	free (p->data);

      if (p->modname)
	free (p->modname);

      free (p);
    }

  sym = obj->symbols;
  while (sym)
    {
      struct grub_util_obj_symbol *p;

      p = sym;
      sym = sym->next;

      if (p->name)
	free (p->name);

      free (p);
    }

  rel = obj->relocs;
  while (rel)
    {
      struct grub_util_obj_reloc *p;

      p = rel;
      rel = rel->next;

      if (p->symbol_name)
	free (p->symbol_name);

      free (p);
    }

  free (obj->attr);
}

void
grub_obj_link (struct grub_util_obj *obj, grub_uint32_t base)
{
  struct grub_util_obj_segment *seg;
  struct grub_util_obj_reloc *rel;

  seg = obj->segments;
  while (seg)
    {
      seg->segment.offset += base;
      seg = seg->next;
    }

  rel = obj->relocs;
  while (rel)
    {
      if (rel->segment)
	{
	  char *addr;
#ifdef GRUB_TARGET_USE_ADDEND
	  grub_uint32_t addend;

	  addend = rel->reloc.addend + base;
#endif

	  if (! rel->segment->data)
	    grub_util_error ("can\'t relocate in .bss segment");

	  if (! rel->symbol_segment)
	    grub_util_error ("unresolved symbol %s", rel->symbol_name);

	  addr = rel->segment->data + rel->reloc.offset;
	  switch (rel->reloc.type)
	    {
#ifdef GRUB_TARGET_USE_ADDEND

	    case GRUB_OBJ_REL_TYPE_16:
	      *((grub_uint16_t *) addr) = grub_host_to_target16 (addend);
	      break;

	    case GRUB_OBJ_REL_TYPE_32:
	      *((grub_uint32_t *) addr) = grub_host_to_target32 (addend);
	      break;

	    case GRUB_OBJ_REL_TYPE_64:
	      *((grub_uint64_t *) addr) = grub_host_to_target64 (addend);
	      break;

#if defined(GRUB_TARGET_POWERPC)
	    case GRUB_OBJ_REL_TYPE_16HI:
	      *((grub_uint16_t *) addr) = grub_host_to_target16 (addend >> 16);
	      break;

	    case GRUB_OBJ_REL_TYPE_16HA:
	      *((grub_uint16_t *) addr) =
		grub_host_to_target16 ((addend + 0x8000)>> 16);
	      break;

#elif defined(GRUB_TARGET_SPARC64)
	    case GRUB_OBJ_REL_TYPE_LO10:
	      {
		grub_uint32_t v;

		v = grub_target_to_host32 (*((grub_uint32_t *) addr));
		v = (v & ~0x3ff) | (addend & 0x3ff);
		*((grub_uint32_t *) addr) = grub_host_to_target32 (v);
		break;
	      }

	    case GRUB_OBJ_REL_TYPE_HI22:
	      {
		grub_uint32_t v;

		v = grub_target_to_host32 (*((grub_uint32_t *) addr));
		v = (v & ~0x3fffff) | ((addend >> 10) & 0x3fffff);
		*((grub_uint32_t *) addr) = grub_host_to_target32 (v);
		break;
	      }

	    case GRUB_OBJ_REL_TYPE_HH22:
	    case GRUB_OBJ_REL_TYPE_HM10:
	      break;
#endif
#else
	    case GRUB_OBJ_REL_TYPE_32:
	      {
		grub_uint32_t v;

		v = grub_target_to_host32 (*((grub_uint32_t *) addr));
		*((grub_uint32_t *) addr) =
		  grub_host_to_target32 (v + base);
		break;
	      }

	    case GRUB_OBJ_REL_TYPE_16:
	      {
		grub_uint16_t v;

		v = grub_target_to_host16 (*((grub_uint16_t *) addr));
		*((grub_uint16_t *) addr) =
		  grub_host_to_target16 (v + base);
		break;
	      }

	    case GRUB_OBJ_REL_TYPE_64:
	      {
		grub_uint64_t v;

		v = grub_target_to_host64 (*((grub_uint64_t *) addr));
		*((grub_uint64_t *) addr) =
		  grub_host_to_target64 (v + base);
		break;
	      }

#endif

	    default:
	      grub_util_error ("invalid reloc type %d", rel->reloc.type);
	    }
	}

      rel = rel->next;
    }
}

const char *
grub_obj_map_symbol (const char *name)
{
  if ((! strcmp (name, "memcpy")) || (! strcmp (name, "memmove")))
    return "grub_memmove";
  if (! strcmp (name, "memset"))
    return "grub_memset";
  if (! strcmp (name, "memcmp"))
    return "grub_memcmp";
  else
    return name;
}

void
grub_obj_add_attr (struct grub_util_obj *obj, const char *start, int len)
{
  const char *p;

  p = start + len - 1;
  while ((p >= start) && (*p == 0))
    p--;

  if (p >= start)
    {
      len = (p - start) + 2;
      obj->attr = xrealloc (obj->attr, obj->attr_len + len);
      memcpy (obj->attr + obj->attr_len, start, len);
      obj->attr_len += len;
    }
}

void
grub_obj_filter_symbols (struct grub_util_obj *obj)
{
  char *c, *p;

  if (! obj->attr)
    return;

  c = p = obj->attr;
  while ((c - obj->attr) < obj->attr_len)
    {
      int len;

      len = strlen (c) + 1;

      if (! memcmp (c, "export:", 7))
	{
	  struct grub_util_obj_symbol *sym;

	  sym = grub_named_list_find (GRUB_AS_NAMED_LIST (obj->symbols),
				      c + 7);
	  if (! sym)
	    grub_util_error ("export symbol %s not found", c + 7);

	  sym->exported = 1;
	}
      else
	{
	  if (c != p)
	    strcpy (p, c);

	  p += len;
	}

      c += len;
    }

  obj->attr_len = (p - obj->attr);
}

static char *
add_module (struct grub_util_obj *obj, const char *path,
	    char *info, int *offset)
{
  char *image, *mod_name, *p, *pn;
  int size, info_size, symbol_name, symbol_value;
  struct grub_util_obj *mod;
  struct grub_obj_header *mod_header;
  struct grub_module_header *header;
  struct grub_module_object *modobj;
  struct grub_util_obj_segment *seg;
  struct grub_util_obj_symbol *sym;
  struct grub_util_obj_reloc *rel;

  image = grub_util_read_image (path);
  size = grub_util_get_image_size (path);

  mod = grub_obj_load (image, size, 1);

  mod_header = (struct grub_obj_header *) image;
  mod_name = image + grub_target_to_host32 (mod_header->mod_deps);

  info_size = (sizeof (struct grub_module_header) +
	       sizeof (struct grub_module_object) + 1);

  p = mod_name;
  while (*p)
    {
      int len;

      len = strlen (p) + 1;
      info_size += len;
      p += len;
    }

  symbol_name = (info_size - sizeof (struct grub_module_header) -
		 sizeof (struct grub_module_object));
  symbol_value = 0;
  info_size++;
  sym = mod->symbols;
  while (sym)
    {
      info_size += strlen (sym->name) + 1;
      symbol_value += sizeof (grub_uint32_t);
      sym = sym->next;
    }
  info_size = ALIGN_UP (info_size, GRUB_TARGET_MIN_ALIGN) + symbol_value;

  info = xrealloc (info, *offset + info_size);
  header = (struct grub_module_header *) (info + *offset);
  memset (header, 0, info_size);
  header->type = OBJ_TYPE_OBJECT;
  header->size = grub_host_to_target32 (info_size);
  modobj = (struct grub_module_object *) (header + 1);

  if (! strcmp (mod_name, "kernel"))
    {
      modobj->init_func = GRUB_OBJ_FUNC_NONE;
      modobj->fini_func = GRUB_OBJ_FUNC_NONE;
    }
  else
    {
      modobj->init_func = grub_target_to_host16 (mod_header->init_func);
      modobj->fini_func = grub_target_to_host16 (mod_header->fini_func);
    }
  modobj->symbol_name = grub_host_to_target16 (symbol_name);
  modobj->symbol_value =
    grub_host_to_target16 (info_size - symbol_value
			   - sizeof (struct grub_module_header)
			   - sizeof (struct grub_module_object));
  pn = modobj->name;
  p = mod_name;
  while (*p)
    {
      int len;

      len = strlen (p) + 1;
      strcpy (pn, p);
      p += len;
      pn += len;
    }
  pn++;

  seg = mod->segments;
  if ((seg) && ((modobj->init_func != GRUB_OBJ_FUNC_NONE)
		|| (modobj->fini_func != GRUB_OBJ_FUNC_NONE)))
    seg->modname = xstrdup (mod_name);

  while (seg)
    {
      struct grub_util_obj_segment *tmp;

      tmp = seg;
      seg = seg->next;

      grub_list_push (GRUB_AS_LIST_P (&obj->segments),
		      GRUB_AS_LIST (tmp));
    }

  sym = mod->symbols;
  while (sym)
    {
      struct grub_util_obj_symbol *tmp;

      tmp = sym;
      sym = sym->next;

      strcpy (pn, tmp->name);
      pn += strlen (pn) + 1;
      grub_list_push (GRUB_AS_LIST_P (&obj->symbols),
		      GRUB_AS_LIST (tmp));
    }

  rel = mod->relocs;
  while (rel)
    {
      struct grub_util_obj_reloc *tmp;

      tmp = rel;
      rel = rel->next;

      grub_list_push (GRUB_AS_LIST_P (&obj->relocs),
		      GRUB_AS_LIST (tmp));
    }

  mod->segments = 0;
  mod->symbols = 0;
  mod->relocs = 0;
  grub_obj_free (mod);

  free (image);

  *offset += info_size;
  return info;
}

static char *
read_config_file (char *filename, size_t *pack_size)
{
  FILE *f;
  char *data, *p;
  int size;

  *pack_size = 0;
  f = fopen (filename, "r");
  if (! f)
    return 0;

  size = grub_util_get_image_size (filename) + 2;
  p = data = xmalloc (size);
  while (fgets (p, size, f))
    {
      int len, org_len;

      if (p[0] == '#')
	continue;

      if (((grub_uint8_t) p[0] == 0xef) &&
	  ((grub_uint8_t) p[1] == 0xbb) &&
	  ((grub_uint8_t) p[2] == 0xbf))
	strcpy (p, p + 3);

      len = org_len = strlen (p);
      while ((len > 0) && ((p[len - 1] == '\n') || (p[len - 1] == '\r')))
	len--;

      if (len != org_len)
	p[len++] = '\n';

      p += len;
      size -= len;
    }

  *p = 0;
  *pack_size = p + 1 - data;

  fclose (f);
  return data;
}

struct grub_util_obj_segment *
grub_obj_add_modinfo (struct grub_util_obj *obj, const char *dir,
		      struct grub_util_path_list *path_list, int as_info,
		      char *memdisk_path, char *config_path)
{
  size_t memdisk_size = 0, config_size = 0;
  char *kernel_path, *info, *config_data;
  int offset, total_module_size;
  struct grub_util_obj_segment *seg;

  kernel_path = grub_util_get_path (dir, "kernel.mod");
  offset = sizeof (struct grub_module_info);
  info = add_module (obj, kernel_path, 0, &offset);
  while (path_list)
    {
      fflush (stdout);
      if (strcmp (path_list->name, kernel_path))
	info = add_module (obj, path_list->name, info, &offset);
      path_list = path_list->next;
    }
  free (kernel_path);

  total_module_size = offset;
  if (memdisk_path)
    {
      memdisk_size = ALIGN_UP(grub_util_get_image_size (memdisk_path), 512);
      grub_util_info ("the size of memory disk is 0x%x", memdisk_size);
      total_module_size += memdisk_size + sizeof (struct grub_module_header);
    }

  if (config_path)
    {
      config_data = read_config_file (config_path, &config_size);
      grub_util_info ("the size of config file is 0x%x", config_size);
      total_module_size += config_size + sizeof (struct grub_module_header);
    }

  grub_util_info ("the total module size is 0x%x", total_module_size);

  info = xrealloc (info, total_module_size);
  if (memdisk_path)
    {
      struct grub_module_header *header;

      header = (struct grub_module_header *) (info + offset);
      memset (header, 0, sizeof (struct grub_module_header));
      header->type = OBJ_TYPE_MEMDISK;
      header->size = grub_host_to_target32 (memdisk_size + sizeof (*header));
      offset += sizeof (*header);

      grub_util_load_image (memdisk_path, info + offset);
      offset += memdisk_size;
    }

  if (config_path)
    {
      struct grub_module_header *header;

      header = (struct grub_module_header *) (info + offset);
      memset (header, 0, sizeof (struct grub_module_header));
      header->type = OBJ_TYPE_CONFIG;
      header->size = grub_host_to_target32 (config_size + sizeof (*header));
      offset += sizeof (*header);

      memcpy (info + offset, config_data, config_size);
      offset += config_size;
      free (config_data);
    }

  ((struct grub_module_info *) info)->magic =
    grub_host_to_target32 (GRUB_MODULE_MAGIC);
  ((struct grub_module_info *) info)->size = grub_host_to_target32 (offset);

  /* Insert the modinfo segment.  */
  seg = xmalloc_zero (sizeof (*seg));
  seg->segment.type = (as_info) ? GRUB_OBJ_SEG_INFO : GRUB_OBJ_SEG_RDATA;
  seg->segment.align = GRUB_TARGET_MIN_ALIGN;
  seg->segment.size = offset;
  seg->raw_size = offset;
  seg->data = info;
  grub_list_push (GRUB_AS_LIST_P (&obj->segments), GRUB_AS_LIST (seg));

  grub_obj_reverse (obj);

  return seg;
}

int
grub_obj_add_kernel_symbols (struct grub_util_obj *obj,
			     struct grub_util_obj_segment *modinfo,
			     grub_uint32_t offset)
{
  struct grub_util_obj_segment *seg, *first;
  int data_size, bss_size;
  struct grub_util_obj_symbol *sym;
  struct grub_module_info *info;
  char *p;

  seg = obj->segments;
  data_size = 0;
  bss_size = 0;
  first = 0;
  while (seg)
    {
      if (seg->segment.type == GRUB_OBJ_SEG_BSS)
	{
	  if (! first)
	    {
	      first = seg;
	      sym = xmalloc_zero (sizeof (*sym));
	      sym->name = xstrdup ("grub_bss_start");
	      sym->segment = seg;
	      sym->symbol.offset = 0;
	      grub_list_push (GRUB_AS_LIST_P (&obj->symbols),
			      GRUB_AS_LIST (sym));
	    }

	  bss_size = seg->segment.offset + seg->segment.size;
	}
      else if (seg->segment.type != GRUB_OBJ_SEG_INFO)
	data_size = seg->segment.offset + seg->segment.size;

      seg = seg->next;
    }

  if (first)
    {
      sym = xmalloc_zero (sizeof (*sym));
      sym->name = xstrdup ("grub_bss_end");
      sym->segment = first;
      sym->symbol.offset = bss_size - first->segment.offset;
      grub_list_push (GRUB_AS_LIST_P (&obj->symbols), GRUB_AS_LIST (sym));
    }

  if (offset)
    offset -= modinfo->segment.offset - data_size;

  info = (struct grub_module_info *) modinfo->data;
  p = modinfo->data + sizeof (struct grub_module_info);
  while (p < modinfo->data + grub_target_to_host32 (info->size))
    {
      struct grub_module_header *h;

      h = (struct grub_module_header *) p;
      if (h->type == OBJ_TYPE_OBJECT)
	{
	  struct grub_module_object *o;
	  char *pn;
	  grub_uint32_t *pv;

	  o = (struct grub_module_object *) (h + 1);
	  if ((o->init_func != GRUB_OBJ_FUNC_NONE) ||
	      (o->fini_func != GRUB_OBJ_FUNC_NONE))
	    {
	      struct grub_util_obj_segment *s;

	      s = obj->segments;
	      while (s)
		{
		  if ((s->modname) && (! strcmp (s->modname, o->name)))
		    break;
		  s = s->next;
		}

	      if (! s)
		grub_util_error ("can't find segment %s", o->name);

	      if (o->init_func != GRUB_OBJ_FUNC_NONE)
		o->init_func += s->segment.offset;
	      else
		o->init_func = 0;

	      if (o->fini_func != GRUB_OBJ_FUNC_NONE)
		o->fini_func += s->segment.offset;
	      else
		o->fini_func = 0;
	    }
	  else
	    o->init_func = o->fini_func = 0;

	  o->init_func = grub_host_to_target32 (o->init_func);
	  o->fini_func = grub_host_to_target32 (o->fini_func);

	  pn = o->name + grub_target_to_host16 (o->symbol_name);
	  pv = (grub_uint32_t *)
	    (o->name + grub_target_to_host16 (o->symbol_value));
	  while (*pn)
	    {
	      struct grub_util_obj_symbol *s;

	      s = grub_named_list_find (GRUB_AS_NAMED_LIST (obj->symbols), pn);
	      if (! s)
		grub_util_error ("can't find symbol %s", pn);

	      pn += strlen (pn) + 1;
	      *(pv++) = grub_host_to_target32 (s->symbol.offset
					       + s->segment->segment.offset);
	    }
	}

      p += grub_target_to_host32 (h->size);
    }

  /* Insert the grub_modinfo symbol.  */
  sym = xmalloc_zero (sizeof (*sym));
  sym->name = xstrdup ("grub_modinfo");
  sym->segment = modinfo;
  sym->symbol.offset = offset;
  grub_list_push (GRUB_AS_LIST_P (&obj->symbols), GRUB_AS_LIST (sym));

  return modinfo->segment.offset - data_size;
}

void
grub_obj_add_csym (struct grub_util_obj *obj, const char *name, int size)
{
  struct grub_util_obj_csym *csym;

  if (! size)
    return;

  csym = grub_named_list_find (GRUB_AS_NAMED_LIST (obj->csyms), name);
  if (csym)
    grub_util_error ("common symbol %s exists", name);

  csym = xmalloc (sizeof (*csym));
  csym->name = xstrdup (name);
  csym->size = size;
  grub_list_push (GRUB_AS_LIST_P (&obj->csyms), GRUB_AS_LIST (csym));
}

void
grub_obj_csym_done (struct grub_util_obj *obj)
{
  struct grub_util_obj_segment *seg;
  struct grub_util_obj_csym *csym;
  grub_uint32_t size;

  if (! obj->csyms)
    return;

  seg = xmalloc_zero (sizeof (*seg));
  seg->segment.type = GRUB_OBJ_SEG_BSS;
  seg->segment.align = GRUB_TARGET_MIN_ALIGN;

  size = 0;
  csym = grub_list_reverse (GRUB_AS_LIST (obj->csyms));
  while (csym)
    {
      struct grub_util_obj_csym *c;
      struct grub_util_obj_symbol *s;

      c = csym;
      csym = csym->next;

      s = xmalloc_zero (sizeof (*s));
      s->segment = seg;
      if ((c->size & (GRUB_TARGET_SIZEOF_LONG - 1)) == 0)
	size = ALIGN_UP (size, GRUB_TARGET_SIZEOF_LONG);
      s->symbol.offset = size;
      s->name = c->name;
      size += c->size;

      grub_list_push (GRUB_AS_LIST_P (&obj->symbols), GRUB_AS_LIST (s));
      free (c);
    }

  seg->segment.size = size;
  seg->raw_size = size;

  grub_list_push (GRUB_AS_LIST_P (&obj->segments), GRUB_AS_LIST (seg));
}

grub_uint32_t
grub_obj_add_got (struct grub_util_obj *obj, const char *name)
{
  struct grub_util_obj_got *got;

  if (! obj->got_segment)
    obj->got_segment = xmalloc_zero (sizeof (*obj->got_segment));

  got = grub_named_list_find (GRUB_AS_NAMED_LIST (obj->gots), name);
  if (! got)
    {
      got = xmalloc (sizeof (*got));
      got->offset = obj->got_size;
      got->name = xstrdup (name);
      grub_list_push (GRUB_AS_LIST_P (&obj->gots), GRUB_AS_LIST (got));
      obj->got_size += sizeof (grub_target_addr_t);
    }

  return got->offset;
}

void
grub_obj_got_done (struct grub_util_obj *obj)
{
  struct grub_util_obj_segment *seg;
  struct grub_util_obj_got *got;

  if (! obj->got_segment)
    return;

  seg = obj->got_segment;
  seg->segment.type = GRUB_OBJ_SEG_TEXT;
  seg->segment.align = GRUB_TARGET_MIN_ALIGN;
  seg->segment.size = obj->got_size;
  seg->raw_size = obj->got_size;
  seg->data = xmalloc_zero (obj->got_size);

  got = grub_list_reverse (GRUB_AS_LIST (obj->gots));
  while (got)
    {
      struct grub_util_obj_got *g;
      struct grub_util_obj_reloc *r;

      g = got;
      got = got->next;

      r = xmalloc_zero (sizeof (*r));
      r->segment = seg;
      r->reloc.offset = g->offset;
      r->reloc.type = ((GRUB_TARGET_SIZEOF_VOID_P == 8) ?
		       GRUB_OBJ_REL_TYPE_64 : GRUB_OBJ_REL_TYPE_32);
      r->symbol_name = g->name;

      grub_list_push (GRUB_AS_LIST_P (&obj->relocs), GRUB_AS_LIST (r));
      free (g);
    }

  grub_list_push (GRUB_AS_LIST_P (&obj->segments), GRUB_AS_LIST (seg));
  obj->got_segment = 0;
}

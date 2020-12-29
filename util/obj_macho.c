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
#include <grub/macho.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if GRUB_TARGET_SIZEOF_VOID_P == 4

#define grub_target_to_host	grub_target_to_host32
#define grub_host_to_target	grub_host_to_target32

#define grub_macho_header	grub_macho_header32
#define grub_macho_segment	grub_macho_segment32
#define grub_macho_section	grub_macho_section32
#define grub_macho_nlist	grub_macho_nlist32

#define GRUB_MACHO_MAGIC	GRUB_MACHO_MAGIC32
#define GRUB_MACHO_CMD_SEGMENT	GRUB_MACHO_CMD_SEGMENT32

#elif GRUB_TARGET_SIZEOF_VOID_P == 8

#define grub_target_to_host	grub_target_to_host64
#define grub_host_to_target	grub_host_to_target64

#define grub_macho_header	grub_macho_header64
#define grub_macho_segment	grub_macho_segment64
#define grub_macho_section	grub_macho_section64
#define grub_macho_nlist	grub_macho_nlist64

#define GRUB_MACHO_MAGIC	GRUB_MACHO_MAGIC64
#define GRUB_MACHO_CMD_SEGMENT	GRUB_MACHO_CMD_SEGMENT64

#endif

static const char *
get_symbol_name (const char *name)
{
  if (*name == '_')
    name++;

  return grub_obj_map_symbol (name);
}

static void
add_segments (struct grub_util_obj *obj,
	      struct grub_util_obj_segment **segments,
	      char *image,
	      struct grub_macho_segment *cmd, int num_cmds)
{
  int i, seg_idx;

  seg_idx = 1;
  for (i = 0; i < num_cmds ; i++,
	 cmd = (struct grub_macho_segment *)
	 (((char *) cmd) + grub_target_to_host32 (cmd->cmdsize)))
    {
      int j;
      struct grub_macho_section *s;

      if (grub_target_to_host32 (cmd->cmd) != GRUB_MACHO_CMD_SEGMENT)
	continue;

      s = (struct grub_macho_section *) (cmd + 1);

      for (j = 0; j < (int) grub_target_to_host32 (cmd->nsects);
	   j++, s++, seg_idx++)
	{
	  int type, jump_table, jump_pointers;

	  if (! strcmp (s->segname, "modattr"))
	    {
	      grub_obj_add_attr (obj,
				 image + grub_target_to_host32 (s->offset),
				 grub_target_to_host (s->size));
	      continue;
	    }

	  jump_table = 0;
	  jump_pointers = 0;

	  if (! strcmp (s->segname, "__IMPORT"))
	    {
	      if (! strcmp (s->sectname, "__jump_table"))
		jump_table = 1;
	      else if (! strcmp (s->sectname, "__pointers"))
		jump_pointers = 1;
	      else
		continue;
	    }

	  /* The sectname and segname are joined together.  */
	  if (! strcmp (s->sectname, "__picsymbolstub1__TEXT"))
	    jump_table = 1;

	  if ((! strcmp (s->segname, "__DATA")) &&
	      ((! strcmp (s->sectname, "__la_symbol_ptr")) ||
	       (! strcmp (s->sectname, "__nl_symbol_ptr"))))
	      jump_pointers = 1;

	  if ((jump_table) || (jump_pointers))
	    {
	      struct grub_util_obj_segment *p;

	      p = xmalloc_zero (sizeof (*p));
	      p->segment.type = GRUB_OBJ_SEG_INFO;
	      p->segment.size = grub_target_to_host (s->size);
	      p->segment.align = 1;
	      p->vaddr = grub_target_to_host (s->addr);
	      p->index = grub_target_to_host32 (s->reserved1);
	      p->raw_size = grub_target_to_host32 (s->reserved2);
	      p->is_jmptab = jump_table;
	      segments[seg_idx] = p;
	      continue;
	    }

	  if (! strcmp (s->segname, "__TEXT"))
	    {
	      if (! strcmp (s->sectname, "__eh_frame"))
		continue;

	      type = ((! strcmp (s->sectname, "__text")) ?
		      GRUB_OBJ_SEG_TEXT : GRUB_OBJ_SEG_RDATA);
	    }
	  else if (! strcmp (s->segname, "__DATA"))
	    {
	      type = ((! strcmp (s->sectname, "__bss")) ?
		      GRUB_OBJ_SEG_BSS : GRUB_OBJ_SEG_DATA);
	    }
	  else
	    type = 0;

	  if ((type) && (s->size))
	    {
	      struct grub_util_obj_segment *p;

	      p = xmalloc_zero (sizeof (*p));
	      p->segment.type = type;
	      p->segment.align = 1 << grub_target_to_host32 (s->align);
	      p->segment.size = grub_target_to_host (s->size);
	      p->vaddr = grub_target_to_host (s->addr);
	      segments[seg_idx] = p;

	      if (type != GRUB_OBJ_SEG_BSS)
		{
		  p->raw_size = p->segment.size;
		  p->data = xmalloc (p->raw_size);
		  memcpy (p->data, image + grub_target_to_host32 (s->offset),
			  p->raw_size);
		}

	      grub_list_push (GRUB_AS_LIST_P (&obj->segments),
			      GRUB_AS_LIST (p));
	    }
	}
    }
}

static void
add_symbols (struct grub_util_obj *obj,
	     struct grub_util_obj_segment **segments,
	     char *image,
	     struct grub_macho_symtab *sym)
{
  int i;
  struct grub_macho_nlist *n;
  char *strtab;

  strtab = image + grub_target_to_host32 (sym->stroff);
  n = (struct grub_macho_nlist *) (image +
				   grub_target_to_host32 (sym->symoff));

  for (i = 0; i < (int) grub_target_to_host32 (sym->nsyms); i++, n++)
    {
      const char *name;
      int type;

      if ((n->n_sect) && (! segments[n->n_sect]))
	continue;

      name = get_symbol_name (strtab + grub_target_to_host32 (n->n_strx));
      if ((! (n->n_type & GRUB_MACHO_N_EXT)) &&
	  (strcmp (name, "grub_mod_init")) &&
	  (strcmp (name, "grub_mod_fini")))
	continue;

      type = (n->n_type & GRUB_MACHO_N_TYPE);
      if (type == GRUB_MACHO_N_UNDEF)
	grub_obj_add_csym (obj, name, grub_target_to_host (n->n_value));
      else if (type == GRUB_MACHO_N_SECT)
	{
	  struct grub_util_obj_symbol *p;

	  p = xmalloc_zero (sizeof (*p));
	  p->name = xstrdup (name);
	  p->segment = segments[n->n_sect];
	  p->symbol.offset = (grub_target_to_host (n->n_value)
			      - p->segment->vaddr);

	  grub_list_push (GRUB_AS_LIST_P (&obj->symbols), GRUB_AS_LIST (p));
	}
    }
}

#if defined(GRUB_TARGET_I386) || defined(GRUB_TARGET_POWERPC)

static struct grub_util_obj_segment *
find_segment (struct grub_util_obj_segment **segments, int num_segs,
	      grub_uint32_t vaddr)
{
  int i;

  for (i = 1; i < num_segs; i++)
    {
      if ((segments[i]) &&
	  (vaddr >= segments[i]->vaddr) &&
	  (vaddr < segments[i]->vaddr + segments[i]->segment.size))
	return segments[i];
    }

  /* Try including the end address.  */
  for (i = 1; i < num_segs; i++)
    {
      if ((segments[i]) &&
	  (vaddr >= segments[i]->vaddr) &&
	  (vaddr <= segments[i]->vaddr + segments[i]->segment.size))
	return segments[i];
    }

  grub_util_error ("can\'t locate segment for address %x", vaddr);
  return 0;
}

static grub_uint32_t
handle_abs_symbol (struct grub_util_obj_reloc *p, grub_uint32_t addr,
		   grub_uint32_t v, grub_uint32_t value,
		   int scattered, int rel, int ext, int rt,
		   struct grub_util_obj_segment **segments, int num_segs,
		   struct grub_macho_nlist *symtab, char *strtab,
		   grub_uint32_t *istab)
{
  struct grub_macho_nlist *n;
  grub_uint32_t addend, d;

  if (rt == GRUB_OBJ_REL_TYPE_32)
    d = 4;
  else if (rt == GRUB_OBJ_REL_TYPE_16)
    d = 2;
  else if (rt == GRUB_OBJ_REL_TYPE_24)
    d = 0;
  else
    grub_util_error ("invalid relocation %d", rt);

  addend = (rel) ? -d : 0;

  if (! ext)
    {
      struct grub_util_obj_segment *import_seg;
      int o;

      if (scattered)
	import_seg = find_segment (segments, num_segs, value);
      else
	import_seg = segments[value];

      if (import_seg->segment.type == GRUB_OBJ_SEG_INFO)
	{
	  if (! rel)
	    grub_util_error ("This should be relative");

	  if (! import_seg->is_jmptab)
	    grub_util_error ("This should be jump table");

	  if (! istab)
	    grub_util_error ("indirect symbol table empty");

	  o = (((p->segment->vaddr + addr + v + d
		 - import_seg->vaddr) / import_seg->raw_size)
	       + import_seg->index);

	  n = symtab + grub_target_to_host32 (istab[o]);
	}
      else
	{
	  if (rel)
	    v += p->segment->vaddr + addr + d;

	  p->symbol_segment = find_segment (segments, num_segs, v);
	  addend = v - p->symbol_segment->vaddr;
	  if (rel)
	    addend -= d;

	  n = 0;
	}
    }
  else
    {
      n = symtab + value;
      addend = v;
      if (rel)
	addend += addr;
    }

  if (n)
    {
      const char *name;

      name = get_symbol_name (strtab + grub_target_to_host32 (n->n_strx));
      p->symbol_name = xstrdup (name);
    }

  return addend;
}

static grub_uint32_t
handle_sdiff_symbol (struct grub_util_obj_reloc *p, grub_uint32_t addr,
		     grub_uint32_t v, grub_uint32_t value,
		     int scattered, int rel,
		     struct grub_util_obj *obj,
		     struct grub_macho_scattered_relocation_info *r,
		     struct grub_util_obj_segment **segments, int num_segs,
		     struct grub_macho_nlist *symtab, char *strtab,
		     grub_uint32_t *istab)
{
  grub_uint32_t addend, info, nv;

  if (rel)
    grub_util_error ("this relocation shouldn't be relative");

  if (! scattered)
    grub_util_error ("this relocation should be scattered");

  info = grub_target_to_host32 (r->r_info);
  if (! GRUB_MACHO_SREL_SCATTERED (info))
    grub_util_error ("This should be scattered");

  if (GRUB_MACHO_SREL_TYPE (info) != GRUB_MACHO_RELOC_PAIR)
    grub_util_error ("This should be relocation pair");

  nv = grub_target_to_host32 (r->r_value);
  if (find_segment (segments, num_segs, nv) != p->segment)
    grub_util_error ("current segment not matched");

  v += nv;

  p->symbol_segment = find_segment (segments, num_segs, value);
  p->reloc.type |= GRUB_OBJ_REL_FLAG_REL;
  if (p->symbol_segment->segment.type == GRUB_OBJ_SEG_INFO)
    {
      struct grub_macho_nlist *n;
      struct grub_util_obj_segment *seg;
      const char *name;
      int idx;

      if (p->symbol_segment->is_jmptab)
	grub_util_error ("This should be pointer");

      if (v != value)
	grub_util_error ("Offset not matched %x %x", v, value);

      seg = p->symbol_segment;
      idx = (v - seg->vaddr) / sizeof (grub_target_addr_t);
      n = symtab + grub_target_to_host32 (istab[seg->index + idx]);
      name = strtab + grub_target_to_host32 (n->n_strx);
      addend = grub_obj_add_got (obj, get_symbol_name (name));
      addend += addr - (nv - p->segment->vaddr);
      p->symbol_segment = obj->got_segment;
    }
  else
    addend = v - p->symbol_segment->vaddr + addr - (nv - p->segment->vaddr);

  return addend;
}

#endif

static void
add_relocs (struct grub_util_obj *obj,
	    struct grub_util_obj_segment **segments, int num_segs,
	    char *image,
	    struct grub_macho_segment *cmd, int num_cmds,
	    struct grub_macho_symtab *sym, grub_uint32_t *istab)
{
  int i, seg_idx;
  struct grub_macho_nlist *symtab;
  char *strtab;

  (void) num_segs;
  (void) istab;

  if (sym)
    {
      strtab = image + grub_target_to_host32 (sym->stroff);
      symtab =
	(struct grub_macho_nlist *) (image +
				     grub_target_to_host32 (sym->symoff));
    }
  else
    {
      strtab = 0;
      symtab = 0;
    }

  seg_idx = 1;
  for (i = 0; i < num_cmds ; i++,
	 cmd = (struct grub_macho_segment *)
	 (((char *) cmd) + grub_target_to_host32 (cmd->cmdsize)))
    {
      int j;
      struct grub_macho_section *s;

      if (grub_target_to_host32 (cmd->cmd) != GRUB_MACHO_CMD_SEGMENT)
	continue;

      s = (struct grub_macho_section *) (cmd + 1);
      for (j = 0; j < (int) grub_target_to_host32 (cmd->nsects);
	   j++, s++, seg_idx++)
	{
	  struct grub_macho_scattered_relocation_info *r;
	  int k;

	  if ((! segments[seg_idx]) ||
	      (segments[seg_idx]->segment.type == GRUB_OBJ_SEG_INFO) ||
	      (! s->nreloc))
	    continue;

	  r = (struct grub_macho_scattered_relocation_info *)
	    (image + grub_target_to_host32 (s->reloff));

	  for (k = 0; k < (int) grub_target_to_host32 (s->nreloc); r++, k++)
	    {
	      grub_uint32_t addr, value, info;
	      grub_target_addr_t addend, v;
	      int scattered, type, len, rel, ext, rt;
	      struct grub_util_obj_reloc *p;

	      info = grub_target_to_host32 (r->r_info);
	      if (GRUB_MACHO_SREL_SCATTERED (info))
		{
		  scattered = 1;
		  addr = GRUB_MACHO_SREL_ADDRESS (info);
		  value = grub_target_to_host32 (r->r_value);
		  type = GRUB_MACHO_SREL_TYPE (info);
		  len = GRUB_MACHO_SREL_LENGTH (info);
		  rel = GRUB_MACHO_SREL_PCREL (info);
		  ext = 0;
		}
	      else
		{
		  struct grub_macho_relocation_info *r1;

		  r1 = (struct grub_macho_relocation_info *) r;

		  scattered = 0;
		  addr = info;
		  info = grub_target_to_host32 (r1->r_info);
		  value = GRUB_MACHO_REL_SYMBOLNUM (info);
		  type = GRUB_MACHO_REL_TYPE (info);
		  len = GRUB_MACHO_REL_LENGTH (info);
		  rel = GRUB_MACHO_REL_PCREL (info);
		  ext = GRUB_MACHO_REL_EXTERN (info);
		}

	      p = xmalloc_zero (sizeof (*p));
	      p->segment = segments[seg_idx];

	      addend = 0;
	      if (len == 2)
		{
		  rt = GRUB_OBJ_REL_TYPE_32;
		  v = grub_target_to_host32 (*((grub_uint32_t *)
					       (p->segment->data + addr)));
		}
	      else if (len == 1)
		{
		  rt = GRUB_OBJ_REL_TYPE_16;
		  v = grub_target_to_host16 (*((grub_uint16_t *)
					       (p->segment->data + addr)));
		}
#ifdef GRUB_TARGET_X86_64
	      else if (len == 3)
		{
		  rt = GRUB_OBJ_REL_TYPE_64;
		  v = grub_target_to_host64 (*((grub_uint64_t *)
					       (p->segment->data + addr)));
		}
#endif
	      else
		grub_util_error ("invalid relocation length %d", len);

#ifdef GRUB_TARGET_X86_64
	      if (rel)
		{
		  if (len == 1)
		    addend = -2LL;
		  else if (len == 2)
		    addend = -4LL;
		  else if (len == 3)
		    addend = -8LL;
		}
#endif

	      p->reloc.type = rt;
	      if (rel)
		p->reloc.type |= GRUB_OBJ_REL_FLAG_REL;

	      switch (type)
		{
#if defined(GRUB_TARGET_I386)
		case GRUB_MACHO_RELOC_VANILLA:
		  addend = handle_abs_symbol (p, addr, v, value, scattered,
					      rel, ext, rt, segments, num_segs,
					      symtab, strtab, istab);
		  break;

		case GRUB_MACHO_RELOC_SECTDIFF:
		case GRUB_MACHO_RELOC_LOCAL_SECTDIFF:
		  r++;
		  k++;
		  addend = handle_sdiff_symbol (p, addr, v, value, scattered,
						rel, obj, r, segments, num_segs,
						symtab, strtab, istab);
		  break;

#elif defined(GRUB_TARGET_POWERPC)
		case GRUB_MACHO_RELOC_VANILLA:
		  addend = handle_abs_symbol (p, addr, v, value, scattered,
					      rel, ext, GRUB_OBJ_REL_TYPE_24,
					      segments, num_segs,
					      symtab, strtab, istab);
		  break;

		case GRUB_MACHO_PPC_RELOC_SDIFF:
		case GRUB_MACHO_PPC_RELOC_LOCAL_SDIFF:
		  r++;
		  k++;
		  addend = handle_sdiff_symbol (p, addr, v, value, scattered,
						rel, obj, r, segments, num_segs,
						symtab, strtab, istab);
		  break;

		case GRUB_MACHO_PPC_RELOC_BR24:
		  v &= 0x3fffffc;
		  if (v & 0x2000000)
		    v |= 0xfc000000;

		  rt = GRUB_OBJ_REL_TYPE_24;
		  p->reloc.type = rt | GRUB_OBJ_REL_FLAG_REL;
		  addend = handle_abs_symbol (p, addr, v, value, scattered,
					      rel, ext, rt, segments, num_segs,
					      symtab, strtab, istab);
		  break;

		case GRUB_MACHO_PPC_RELOC_LO16:
		case GRUB_MACHO_PPC_RELOC_HI16:
		case GRUB_MACHO_PPC_RELOC_HA16:
		  if (len != 2)
		    grub_util_error ("invalid length");

		  v = grub_target_to_host16 (*((grub_uint16_t *)
					       (p->segment->data + addr + 2)));

		  r++;
		  k++;
		  info = grub_target_to_host32 (r->r_info);
		  if ((scattered) || (GRUB_MACHO_SREL_SCATTERED (info)))
		    grub_util_error ("this relocation shouldn't be scattered");

		  info = grub_target_to_host32 (r->r_value);
		  if (GRUB_MACHO_REL_TYPE (info) != GRUB_MACHO_RELOC_PAIR)
		    grub_util_error ("This should be relocation pair");

		  /* info field is actually r_address.  */
		  if (type == GRUB_MACHO_PPC_RELOC_LO16)
		    {
		      p->reloc.type = GRUB_OBJ_REL_TYPE_16;
		      v |= grub_target_to_host32 (r->r_info) << 16;
		    }
		  else if (type == GRUB_MACHO_PPC_RELOC_HI16)
		    {
		      p->reloc.type = GRUB_OBJ_REL_TYPE_16HI;
		      v = (v << 16) | grub_target_to_host32 (r->r_info);
		    }
		  else
		    {
		      p->reloc.type = GRUB_OBJ_REL_TYPE_16HA;
		      v = (v << 16) | grub_target_to_host32 (r->r_info);
		      if (v & 0x8000)
			v -= 0x10000;
		    }

		  addend = handle_abs_symbol (p, addr, v, value, scattered,
					      rel, ext,	GRUB_OBJ_REL_TYPE_24,
					      segments, num_segs,
					      symtab, strtab, istab);
		  addr += 2;
		  if (rel)
		    addend += 2;
		  break;

		case GRUB_MACHO_PPC_RELOC_LO16_SDIFF:
		case GRUB_MACHO_PPC_RELOC_HI16_SDIFF:
		case GRUB_MACHO_PPC_RELOC_HA16_SDIFF:
		  if (len != 2)
		    grub_util_error ("invalid length");

		  v = grub_target_to_host16 (*((grub_uint16_t *)
					       (p->segment->data + addr + 2)));

		  r++;
		  k++;
		  info = grub_target_to_host32 (r->r_info);

		  if (type == GRUB_MACHO_PPC_RELOC_LO16_SDIFF)
		    {
		      p->reloc.type = GRUB_OBJ_REL_TYPE_16;
		      v |= GRUB_MACHO_SREL_ADDRESS (info) << 16;
		    }
		  else if (type == GRUB_MACHO_PPC_RELOC_HI16_SDIFF)
		    {
		      p->reloc.type = GRUB_OBJ_REL_TYPE_16HI;
		      v = (v << 16) | GRUB_MACHO_SREL_ADDRESS (info);
		    }
		  else
		    {
		      p->reloc.type = GRUB_OBJ_REL_TYPE_16HA;
		      v = (v << 16) | GRUB_MACHO_SREL_ADDRESS (info);
		      if (v & 0x8000)
			v -= 0x10000;
		    }

		  addend = handle_sdiff_symbol (p, addr, v, value, scattered,
						rel, obj, r, segments, num_segs,
						symtab, strtab, istab);

		  if (addr == 0x374)
		    {
		      p->segment->file_off = 0x1234;
		    }

		  addr += 2;
		  addend += 2;
		  break;

#elif defined(GRUB_TARGET_X86_64)
		case GRUB_MACHO_X86_64_RELOC_UNSIGNED:
		case GRUB_MACHO_X86_64_RELOC_SIGNED:
		case GRUB_MACHO_X86_64_RELOC_BRANCH:
		case GRUB_MACHO_X86_64_RELOC_SIGNED1:
		case GRUB_MACHO_X86_64_RELOC_SIGNED2:
		case GRUB_MACHO_X86_64_RELOC_SIGNED4:
		  if (scattered)
		    grub_util_error ("This shouldn't be scattered");

		  if (! ext)
		    {
		      p->symbol_segment = segments[value];
		      if (rel)
			v += p->segment->vaddr + addr - addend;

		      v -= p->symbol_segment->vaddr;
		    }
		  else
		    {
		      struct grub_macho_nlist *n;

		      n = symtab + value;
		      if ((n->n_type & GRUB_MACHO_N_TYPE) == GRUB_MACHO_N_SECT)
			{
			  p->symbol_segment = segments[n->n_sect];
			  addend += (grub_target_to_host (n->n_value)
				     - p->symbol_segment->vaddr);
			}
		      else
			{
			  const char *name;

			  name = strtab + grub_target_to_host32 (n->n_strx);
			  p->symbol_name = xstrdup (get_symbol_name (name));
			}
		    }

		  addend += v;
		  break;

		case GRUB_MACHO_X86_64_RELOC_GOT_LD:
		case GRUB_MACHO_X86_64_RELOC_GOT:
		  {
		    struct grub_macho_nlist *n;
		    const char *name;

		    if (! rel)
		      grub_util_error ("This should be relative");

		    if (! ext)
		      grub_util_error ("This should be extern");

		    n = symtab + value;
		    name = strtab + grub_target_to_host32 (n->n_strx);
		    addend += grub_obj_add_got (obj, get_symbol_name (name));
		    p->symbol_segment = obj->got_segment;
		    break;
		  }

		case GRUB_MACHO_X86_64_RELOC_SUB:
		  {
		    struct grub_macho_nlist *n1;
		    struct grub_macho_nlist *n2;

		    r++;
		    k++;
		    info = grub_target_to_host32 (r->r_info);
		    if ((scattered) || (GRUB_MACHO_SREL_SCATTERED (info)))
		      grub_util_error ("This shouldn't be scattered");

		    info = grub_target_to_host32 (r->r_value);
		    if (GRUB_MACHO_REL_TYPE (info)
			!= GRUB_MACHO_X86_64_RELOC_UNSIGNED)
		      grub_util_error ("no matching relocation pair");

		    if ((rel) || (GRUB_MACHO_REL_PCREL (info)))
		      grub_util_error ("This shouldn't be relative");

		    if ((! ext) || (! GRUB_MACHO_REL_EXTERN (info)))
		      grub_util_error ("This should be extern");

		    n1 = symtab + value;
		    n2 = symtab + GRUB_MACHO_REL_SYMBOLNUM (info);
		    if (((n1->n_type & GRUB_MACHO_N_TYPE) != GRUB_MACHO_N_SECT)
			|| (n1->n_sect != seg_idx))
		      grub_util_error ("Can\'t substract from other section");

		    addend = (v + addr - grub_target_to_host (n1->n_value)
			      + p->segment->vaddr);

		    if ((n2->n_type & GRUB_MACHO_N_TYPE) == GRUB_MACHO_N_SECT)
		      {
			p->symbol_segment = segments[n2->n_sect];
			addend += (grub_target_to_host (n2->n_value)
				   - p->symbol_segment->vaddr);
		      }
		    else
		      {
			const char *name;

			name = strtab + grub_target_to_host32 (n2->n_strx);
			p->symbol_name = xstrdup (get_symbol_name (name));
		      }
		    p->reloc.type |= GRUB_OBJ_REL_FLAG_REL;
		    break;
		  }
#endif

		default:
		  grub_util_error ("invalid mach-o relocation type %d", type);
		}

	      p->reloc.offset = addr;
#ifdef GRUB_TARGET_USE_ADDEND
	      p->reloc.addend = addend;
#else
	      if (rt == GRUB_OBJ_REL_TYPE_32)
		*((grub_uint32_t *) (p->segment->data + addr)) =
		  grub_host_to_target32 (addend);
	      else if (rt == GRUB_OBJ_REL_TYPE_16)
		*((grub_uint16_t *) (p->segment->data + addr)) =
		  grub_host_to_target16 (addend);
	      else if (rt == GRUB_OBJ_REL_TYPE_64)
		*((grub_uint64_t *) (p->segment->data + addr)) =
		  grub_host_to_target64 (addend);
	      else
		grub_util_error ("invalid relocation type %d", rt);
#endif

	      grub_list_push (GRUB_AS_LIST_P (&obj->relocs),
			      GRUB_AS_LIST (p));
	    }
	}
    }
}

int
grub_obj_import_macho (struct grub_util_obj *obj, char *image, int size)
{
  struct grub_macho_header *head;
  struct grub_macho_segment *cmd;
  struct grub_macho_symtab *sym;
  struct grub_macho_dysymtab *dsym;
  grub_uint32_t *istab;
  struct grub_util_obj_segment **segments;
  int i, num_cmds, num_segs;

  head = (struct grub_macho_header *) image;

  if ((size < (int) sizeof (*head)) ||
      (grub_target_to_host32 (head->magic) != GRUB_MACHO_MAGIC))
    return 0;

  cmd = (struct grub_macho_segment *) (head + 1);
  num_cmds = grub_target_to_host32 (head->ncmds);
  num_segs = 1;

  sym = 0;
  dsym = 0;
  for (i = 0; i < num_cmds ; i++,
	 cmd = (struct grub_macho_segment *)
	 (((char *) cmd) + grub_target_to_host32 (cmd->cmdsize)))
    {
      if (grub_target_to_host32 (cmd->cmd) == GRUB_MACHO_CMD_SEGMENT)
	num_segs += grub_target_to_host32 (cmd->nsects);
      else if (grub_target_to_host32 (cmd->cmd) == GRUB_MACHO_CMD_SYMTAB)
	sym = (struct grub_macho_symtab *) cmd;
      else if (grub_target_to_host32 (cmd->cmd) == GRUB_MACHO_CMD_DYSYMTAB)
	dsym = (struct grub_macho_dysymtab *) cmd;
    }

  istab = (dsym) ? (grub_uint32_t *)
    (image + grub_target_to_host32 (dsym->indirectsymoff)) : 0;

  segments = xmalloc_zero (num_segs * sizeof (segments[0]));

  cmd = (struct grub_macho_segment *) (head + 1);
  add_segments (obj, segments, image, cmd, num_cmds);

  if (sym)
    add_symbols (obj, segments, image, sym);

  add_relocs (obj, segments, num_segs, image, cmd, num_cmds, sym, istab);

  for (i = 1; i < num_segs; i++)
    if ((segments[i]) && (segments[i]->segment.type == GRUB_OBJ_SEG_INFO))
      free (segments[i]);

  free (segments);
  return 1;
}

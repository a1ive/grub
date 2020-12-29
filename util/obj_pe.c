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
#include <grub/efi/pe32.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
add_segments (struct grub_util_obj *obj,
	      struct grub_util_obj_segment **segments,
	      char *image,
	      struct grub_pe32_section_table *pe_shdr, int num_secs)
{
  int i;

  for (i = 0; i < num_secs; i++, pe_shdr++)
    {
      int type;

      if (! strcmp (pe_shdr->name, "modattr"))
	{
	  grub_obj_add_attr (obj,
			     image + pe_shdr->raw_data_offset,
			     pe_shdr->raw_data_size);
	  continue;
	}

      if (! strcmp (pe_shdr->name, ".text"))
	type = GRUB_OBJ_SEG_TEXT;
      else if (! strcmp (pe_shdr->name, ".data"))
	type = GRUB_OBJ_SEG_DATA;
      else if (! strcmp (pe_shdr->name, ".rdata"))
	type = GRUB_OBJ_SEG_RDATA;
      else if (! strcmp (pe_shdr->name, ".bss"))
	type = GRUB_OBJ_SEG_BSS;
      else
	type = 0;

      if ((type) && (pe_shdr->raw_data_size))
	{
	  struct grub_util_obj_segment *p;

	  p = xmalloc_zero (sizeof (*p));
	  p->segment.type = type;
	  p->segment.align = 1 << (((pe_shdr->characteristics >>
				     GRUB_PE32_SCN_ALIGN_SHIFT) &
				    GRUB_PE32_SCN_ALIGN_MASK) - 1);
	  p->segment.size = pe_shdr->raw_data_size;
	  segments[i + 1] = p;

	  if (type != GRUB_OBJ_SEG_BSS)
	    {
	      p->raw_size = p->segment.size;
	      p->data = xmalloc (p->raw_size);
	      memcpy (p->data, image + pe_shdr->raw_data_offset, p->raw_size);
	    }

	  grub_list_push (GRUB_AS_LIST_P (&obj->segments), GRUB_AS_LIST (p));
	}
    }
}

static const char *
get_symbol_name (struct grub_pe32_symbol *pe_sym, char *pe_strtab)
{
  static char short_name[9];
  const char *name;

  if (pe_sym->long_name[0])
    {
      strncpy (short_name, pe_sym->short_name, 8);
      short_name[8] = 0;
      name = short_name;
    }
  else
    name = pe_strtab + pe_sym->long_name[1];

  if (*name == '_')
    name++;

  return grub_obj_map_symbol (name);
}

static void
add_symbols (struct grub_util_obj *obj,
	     struct grub_util_obj_segment **segments,
	     struct grub_pe32_symbol *pe_symtab, int num_syms,
	     char *pe_strtab)
{
  int i;

  for (i = 0; i < num_syms;
       i += pe_symtab->num_aux + 1, pe_symtab += pe_symtab->num_aux + 1)
    {
      const char *name;

      name = get_symbol_name (pe_symtab, pe_strtab);

      if ((pe_symtab->section > num_syms) ||
	  ((pe_symtab->section) && (! segments[pe_symtab->section])))
	continue;

      if ((pe_symtab->storage_class != GRUB_PE32_SYM_CLASS_EXTERNAL) &&
	  (strcmp (name, "grub_mod_init")) &&
	  (strcmp (name, "grub_mod_fini")))
	continue;

      if (! pe_symtab->section)
	grub_obj_add_csym (obj, name, pe_symtab->value);
      else
	{
	  struct grub_util_obj_symbol *p;

	  p = xmalloc_zero (sizeof (*p));
	  p->name = xstrdup (name);
	  p->segment = segments[pe_symtab->section];
	  p->symbol.offset = pe_symtab->value;

	  grub_list_push (GRUB_AS_LIST_P (&obj->symbols), GRUB_AS_LIST (p));
	}
    }
}

static void
add_relocs (struct grub_util_obj *obj,
	    struct grub_util_obj_segment **segments,
	    char *image,
	    struct grub_pe32_section_table *pe_sec, int num_secs,
	    struct grub_pe32_symbol *pe_symtab, int num_syms,
	    char *pe_strtab)
{
  int i;

  for (i = 0; i < num_secs; i++, pe_sec++)
    {
      struct grub_pe32_reloc *pe_rel;
      int j;

      if (! segments[i + 1])
	continue;

      pe_rel = (struct grub_pe32_reloc *) (image + pe_sec->relocations_offset);
      for (j = 0; j < pe_sec->num_relocations; j++, pe_rel++)
	{
	  struct grub_util_obj_reloc *p;
	  struct grub_pe32_symbol *pe_sym;
	  int type;

	  pe_sym = pe_symtab + pe_rel->symtab_index;

	  if (((int) pe_rel->symtab_index >= num_syms) ||
	      ((pe_sym->section) && (! segments[pe_sym->section])))
	    grub_util_error ("invalid symbol index");

	  type = -1;
	  switch (pe_rel->type)
	    {
	    case GRUB_PE32_REL_BASED_ABSOLUTE:
	      break;

#if defined(GRUB_TARGET_I386)
	    case GRUB_PE32_REL_I386_DIR32:
	      type = GRUB_OBJ_REL_TYPE_32;
	      break;

	    case GRUB_PE32_REL_I386_REL32:
	      type = GRUB_OBJ_REL_TYPE_32 | GRUB_OBJ_REL_FLAG_REL;
	      break;

	    case 16:
	      type = GRUB_OBJ_REL_TYPE_16;
	      break;

#elif defined(GRUB_TARGET_X86_64)

	    case GRUB_PE32_REL_X86_64_ADDR32:
	      type = GRUB_OBJ_REL_TYPE_32;
	      break;

	    case GRUB_PE32_REL_X86_64_REL32:
	      type = GRUB_OBJ_REL_TYPE_32 | GRUB_OBJ_REL_FLAG_REL;
	      break;

	    case GRUB_PE32_REL_X86_64_ADDR64:
	      type = GRUB_OBJ_REL_TYPE_64;
	      break;

#endif

	    default:
	      grub_util_error ("unknown pe relocation type %d at %d:%x\n",
			       pe_rel->type, i,
			       pe_rel->offset - pe_sec->virtual_address);
	    }

	  if (type < 0)
	    continue;

	  p = xmalloc_zero (sizeof (*p));
	  p->segment = segments[i + 1];
	  p->reloc.type = type;
	  p->reloc.offset = pe_rel->offset - pe_sec->virtual_address;
	  if (pe_sym->section)
	    p->symbol_segment = segments[pe_sym->section];
	  p->symbol_name = xstrdup (get_symbol_name (pe_sym, pe_strtab));

	  if (! p->segment->data)
	    grub_util_error ("can\'t relocate in .bss segment");

	  if (type & GRUB_OBJ_REL_FLAG_REL)
	    {
	      grub_uint32_t *addr;

	      addr = (grub_uint32_t *) (p->segment->data + p->reloc.offset);
	      type &= GRUB_OBJ_REL_TYPE_MASK;

	      if (type == GRUB_OBJ_REL_TYPE_32)
		*((grub_uint32_t *) addr) -= 4;
	      else if (type == GRUB_OBJ_REL_TYPE_16)
		*((grub_uint16_t *) addr) -= 2;
	      else if (type == GRUB_OBJ_REL_TYPE_64)
		*((grub_uint64_t *) addr) -= 8;
	      else
		grub_util_error ("invalid relocation type %d", type);
	    }

	  grub_list_push (GRUB_AS_LIST_P (&obj->relocs), GRUB_AS_LIST (p));
	}
    }
}

static int
check_pe_header (struct grub_pe32_coff_header *c, size_t size)
{
  if ((size < sizeof (*c) ||
       ((grub_le_to_cpu16 (c->machine) != GRUB_PE32_MACHINE_I386) &&
	(grub_le_to_cpu16 (c->machine) != GRUB_PE32_MACHINE_X86_64))))
    return 0;

  return 1;
}

int
grub_obj_import_pe (struct grub_util_obj *obj, char *image, int size)
{
  struct grub_pe32_coff_header *pe_chdr;
  struct grub_pe32_section_table *pe_shdr;
  struct grub_pe32_symbol *pe_symtab;
  int num_secs, num_syms;
  char *pe_strtab;
  struct grub_util_obj_segment **segments;

  pe_chdr = (struct grub_pe32_coff_header *) image;
  if (! check_pe_header (pe_chdr, size))
    return 0;

  pe_shdr = (struct grub_pe32_section_table *) (pe_chdr + 1);
  num_secs = pe_chdr->num_sections;
  segments = xmalloc_zero ((num_secs + 1) * sizeof (segments[0]));
  add_segments (obj, segments, image, pe_shdr, num_secs);

  pe_symtab = (struct grub_pe32_symbol *) (image + pe_chdr->symtab_offset);
  num_syms = pe_chdr->num_symbols;
  pe_strtab = (char *) (pe_symtab + pe_chdr->num_symbols);

  add_symbols (obj, segments, pe_symtab, num_syms, pe_strtab);
  add_relocs (obj, segments, image, pe_shdr, num_secs,
	      pe_symtab, num_syms, pe_strtab);

  free (segments);
  return 1;
}

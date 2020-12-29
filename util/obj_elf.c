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
#include <grub/elf.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if GRUB_TARGET_SIZEOF_VOID_P == 4

#define grub_target_to_host	grub_target_to_host32
#define grub_host_to_target	grub_host_to_target32

#elif GRUB_TARGET_SIZEOF_VOID_P == 8

#define grub_target_to_host	grub_target_to_host64
#define grub_host_to_target	grub_host_to_target64

#endif

void
grub_obj_import (struct grub_util_obj *obj, char *image, int size)
{
  if ((! grub_obj_import_elf (obj, image, size)) &&
      (! grub_obj_import_pe (obj, image, size)) &&
      (! grub_obj_import_macho (obj, image, size)))
    grub_util_error ("Invalid object format");
}

static int
check_elf_header (Elf_Ehdr *e, size_t size)
{
  if (size < sizeof (*e)
      || e->e_ident[EI_MAG0] != ELFMAG0
      || e->e_ident[EI_MAG1] != ELFMAG1
      || e->e_ident[EI_MAG2] != ELFMAG2
      || e->e_ident[EI_MAG3] != ELFMAG3
      || e->e_ident[EI_VERSION] != EV_CURRENT
      || grub_target_to_host32 (e->e_version) != EV_CURRENT)
     return 0;

  return 1;
}

/* Return the symbol table section, if any.  */
static Elf_Shdr *
find_symtab_section (Elf_Shdr *sections,
		     Elf_Half section_entsize, Elf_Half num_sections)
{
  int i;
  Elf_Shdr *s;

  for (i = 0, s = sections;
       i < num_sections;
       i++, s = (Elf_Shdr *) ((char *) s + section_entsize))
    if (grub_target_to_host32 (s->sh_type) == SHT_SYMTAB)
      return s;

  return 0;
}

extern int sss;
int sss;

static void
add_segments (struct grub_util_obj *obj,
	      struct grub_util_obj_segment **segments,
	      char *image,
	      Elf_Shdr *sections, int section_entsize, int num_sections)
{
  Elf_Shdr *s;
  char *strtab;
  int i;

  s = (Elf_Shdr *) ((char *) sections
		    + grub_target_to_host16 (((Elf_Ehdr *) image)->e_shstrndx)
		    * section_entsize);
  strtab = image + grub_target_to_host (s->sh_offset);

  for (i = 0, s = sections;
       i < num_sections;
       i++, s = (Elf_Shdr *) ((char *) s + section_entsize))
    {
      int type;
      char *name;

      name = strtab + grub_target_to_host32 (s->sh_name);

      if (! strcmp (name, ".eh_frame"))
	continue;

      if (! strcmp (name, "modattr"))
	{
	  grub_obj_add_attr (obj,
			     image + grub_target_to_host (s->sh_offset),
			     grub_target_to_host (s->sh_size));
	  continue;
	}

      if ((grub_target_to_host (s->sh_flags) & (SHF_EXECINSTR | SHF_ALLOC))
	  == (SHF_EXECINSTR | SHF_ALLOC))
	type = GRUB_OBJ_SEG_TEXT;
      else if ((grub_target_to_host (s->sh_flags) & SHF_ALLOC)
	       && ! (grub_target_to_host (s->sh_flags) & SHF_EXECINSTR))
	{
	  if (! (grub_target_to_host (s->sh_flags) & SHF_WRITE))
	    type = GRUB_OBJ_SEG_RDATA;
	  else if (grub_target_to_host32 (s->sh_type) == SHT_NOBITS)
	    type = GRUB_OBJ_SEG_BSS;
	  else
	    type = GRUB_OBJ_SEG_DATA;
	}
      else
	type = 0;

      if ((type) && (s->sh_size))
	{
	  struct grub_util_obj_segment *p;

	  p = xmalloc_zero (sizeof (*p));
	  p->segment.type = type;
	  p->segment.align = grub_target_to_host (s->sh_addralign);
	  p->segment.size = grub_target_to_host (s->sh_size);
	  segments[i] = p;

	  if (type != GRUB_OBJ_SEG_BSS)
	    {
	      p->raw_size = p->segment.size;
	      p->data = xmalloc (p->raw_size);
	      memcpy (p->data, image + grub_target_to_host (s->sh_offset),
		      p->raw_size);
	    }

	  grub_list_push (GRUB_AS_LIST_P (&obj->segments),  GRUB_AS_LIST (p));
	}
    }
}

static void
add_symbols (struct grub_util_obj *obj,
	     struct grub_util_obj_segment **segments,
	     char *image,
	     Elf_Shdr *sections, int section_entsize, int num_sections)
{
  int i;
  Elf_Shdr *symtab_section, *str_sec;
  Elf_Sym *sym;
  int num_syms, sym_size;
  char *strtab;

  symtab_section = find_symtab_section (sections,
					section_entsize, num_sections);
  sym = (Elf_Sym *) (image + grub_target_to_host (symtab_section->sh_offset));
  sym_size = grub_target_to_host (symtab_section->sh_entsize);
  num_syms = grub_target_to_host (symtab_section->sh_size) / sym_size;
  str_sec = (Elf_Shdr *) ((char *) sections
			  + (grub_target_to_host32 (symtab_section->sh_link)
			     * section_entsize));
  strtab = image + grub_target_to_host (str_sec->sh_offset);

  for (i = 0; i < num_syms;
       i++, sym = (Elf_Sym *) ((char *) sym + sym_size))
    {
      Elf_Section index;
      const char *name;

      name = grub_obj_map_symbol (strtab +
				  grub_target_to_host32 (sym->st_name));

      if ((ELF_ST_BIND (sym->st_info) == STB_LOCAL) &&
	  (strcmp (name, "grub_mod_init")) &&
	  (strcmp (name, "grub_mod_fini")))
	continue;

      index = grub_target_to_host16 (sym->st_shndx);

      if (index == STN_UNDEF)
	continue;

      if (index == STN_COMMON)
	{
	  if (! sym->st_size)
	    grub_util_error ("empty size for common symbol");

	  grub_obj_add_csym (obj, name, grub_target_to_host (sym->st_size));
	}
      else if ((index < num_sections) && (segments[index]))
	{
	  struct grub_util_obj_symbol *p;

	  p = xmalloc_zero (sizeof (*p));
	  p->name = xstrdup (name);
	  p->segment = segments[index];
	  p->symbol.offset = grub_target_to_host (sym->st_value);

	  grub_list_push (GRUB_AS_LIST_P (&obj->symbols),
			  GRUB_AS_LIST (p));
	}
    }
}

static void
add_relocs (struct grub_util_obj *obj,
	    struct grub_util_obj_segment **segments,
	    char *image,
	    Elf_Shdr *sections, int section_entsize, int num_sections)
{
  int i;
  Elf_Shdr *s;

  for (i = 0, s = sections;
       i < num_sections;
       i++, s = (Elf_Shdr *) ((char *) s + section_entsize))
    if ((grub_target_to_host32 (s->sh_type) == SHT_REL) ||
	(grub_target_to_host32 (s->sh_type) == SHT_RELA))
      {
	Elf_Rela *r;
	Elf_Shdr *sym_sec, *str_sec;
	int sym_size;
	char *strtab;
	Elf_Word r_size, num_rs, j;
	Elf_Word target_index;

	sym_sec = (Elf_Shdr *) ((char *) sections
				+ (grub_target_to_host32 (s->sh_link)
				   * section_entsize));
	sym_size = grub_target_to_host (sym_sec->sh_entsize);

	str_sec = (Elf_Shdr *) ((char *) sections
				+ (grub_target_to_host32 (sym_sec->sh_link)
				   * section_entsize));
	strtab = image + grub_target_to_host (str_sec->sh_offset);

	target_index = grub_target_to_host32 (s->sh_info);
	if (! segments[target_index])
	  continue;

	r_size = grub_target_to_host (s->sh_entsize);
	num_rs = grub_target_to_host (s->sh_size) / r_size;

	r = (Elf_Rela *) (image + grub_target_to_host (s->sh_offset));
	for (j = 0;
	     j < num_rs;
	     j++, r = (Elf_Rela *) ((char *) r + r_size))
	  {
	    struct grub_util_obj_reloc *p;
	    Elf_Addr info, offset;
	    Elf_Sym *sym;
	    char *addr;
	    const char *name;
	    int type, is_got;
	    grub_target_addr_t addend;

	    offset = grub_target_to_host (r->r_offset);
	    if (! segments[target_index]->data)
	      grub_util_error ("can\'t relocate in .bss segment");

	    addr = segments[target_index]->data + offset;
	    info = grub_target_to_host (r->r_info);

	    if (grub_target_to_host32 (s->sh_type) == SHT_RELA)
	      addend = grub_target_to_host (r->r_addend);
	    else
	      addend = 0;

	    sym = (Elf_Sym *) (image
			       + grub_target_to_host (sym_sec->sh_offset)
			       + (ELF_R_SYM (info) * sym_size));
	    name = grub_obj_map_symbol (strtab +
					grub_target_to_host32 (sym->st_name));
	    is_got = 0;
	    type = -1;
	    switch (ELF_R_TYPE (info) & 0xff)
	      {
	      case R_386_NONE:
		break;

#if defined(GRUB_TARGET_I386)
	      case R_386_32:
		type = GRUB_OBJ_REL_TYPE_32;
		break;

	      case R_386_PC32:
		type = GRUB_OBJ_REL_TYPE_32 | GRUB_OBJ_REL_FLAG_REL;
		break;

	      case R_386_16:
		type = GRUB_OBJ_REL_TYPE_16;
		break;

	      case R_386_PC16:
		type = GRUB_OBJ_REL_TYPE_16 | GRUB_OBJ_REL_FLAG_REL;
		break;

#elif defined(GRUB_TARGET_X86_64)
	      case R_X86_64_64:
		type = GRUB_OBJ_REL_TYPE_64;
		break;

	      case R_X86_64_PC32:
	      case R_X86_64_PLT32:
		type = GRUB_OBJ_REL_TYPE_32 | GRUB_OBJ_REL_FLAG_REL;
		break;

	      case R_X86_64_32:
	      case R_X86_64_32S:
		type = GRUB_OBJ_REL_TYPE_32;
		break;

	      case R_X86_64_GOTPCREL:
		type = GRUB_OBJ_REL_TYPE_32 | GRUB_OBJ_REL_FLAG_REL;
		addend += grub_obj_add_got (obj, name);
		is_got = 1;
		break;

#elif defined(GRUB_TARGET_POWERPC)
	      case R_PPC_ADDR32:
		type = GRUB_OBJ_REL_TYPE_32;
		break;

	      case R_PPC_REL32:
		type = GRUB_OBJ_REL_TYPE_32 | GRUB_OBJ_REL_FLAG_REL;
		break;

	      case R_PPC_ADDR24:
		type = GRUB_OBJ_REL_TYPE_24;
		break;

	      case R_PPC_REL24:
		type = GRUB_OBJ_REL_TYPE_24 | GRUB_OBJ_REL_FLAG_REL;
		break;

	      case R_PPC_ADDR16:
	      case R_PPC_ADDR16_LO:
		type = GRUB_OBJ_REL_TYPE_16;
		break;

	      case R_PPC_ADDR16_HI:
		type = GRUB_OBJ_REL_TYPE_16HI;
		break;

	      case R_PPC_ADDR16_HA:
		type = GRUB_OBJ_REL_TYPE_16HA;
		break;

#elif defined(GRUB_TARGET_SPARC64)
	      case R_SPARC_LO10:
		type = GRUB_OBJ_REL_TYPE_LO10;
		break;

	      case R_SPARC_OLO10:
		type = GRUB_OBJ_REL_TYPE_LO10;
		addend += (ELF_R_TYPE (info) >> 8);
		if ((obj->relocs) &&
		    (obj->relocs->reloc.type == R_SPARC_HI22))
		  obj->relocs->reloc.addend += (ELF_R_TYPE (info) >> 8);
		break;

	      case R_SPARC_HI22:
	      case R_SPARC_LM22:
		type = GRUB_OBJ_REL_TYPE_HI22;
		break;

	      case R_SPARC_HM10:
		type = GRUB_OBJ_REL_TYPE_HM10;
		break;

	      case R_SPARC_HH22:
		type = GRUB_OBJ_REL_TYPE_HH22;
		break;

	      case R_SPARC_WDISP30:
		type = GRUB_OBJ_REL_TYPE_30 | GRUB_OBJ_REL_FLAG_REL;
		break;

	      case R_SPARC_32:
		type = GRUB_OBJ_REL_TYPE_32;
		break;

	      case R_SPARC_64:
		type = GRUB_OBJ_REL_TYPE_64;
		break;
#endif

	      default:
		grub_util_error ("unknown elf relocation type %d at %d:%x",
				 ELF_R_TYPE (info), i, offset);
	      }

	    if (type < 0)
	      continue;

	    p = xmalloc_zero (sizeof (*p));
	    p->segment = segments[target_index];
	    p->reloc.type = type;
	    p->reloc.offset = offset;
	    p->symbol_name = xstrdup (name);

#ifndef GRUB_TARGET_USE_ADDEND
	    type &= GRUB_OBJ_REL_TYPE_MASK;
	    if (addend)
	      {
		if (type == GRUB_OBJ_REL_TYPE_32)
		  {
		    addend +=
		      grub_target_to_host32 (*((grub_uint32_t *) addr));
		    *((grub_uint32_t *) addr) = grub_host_to_target32 (addend);
		  }
		else if (type == GRUB_OBJ_REL_TYPE_16)
		  {
		    addend +=
		      grub_target_to_host16 (*((grub_uint16_t *) addr));
		    *((grub_uint16_t *) addr) = grub_host_to_target16 (addend);
		  }
		else if (type == GRUB_OBJ_REL_TYPE_64)
		  {
		    addend +=
		      grub_target_to_host64 (*((grub_uint64_t *) addr));
		    *((grub_uint64_t *) addr) = grub_host_to_target64 (addend);
		  }
		else
		  grub_util_error ("invalid relocation type %d", type);
	      }
#endif

	    if (is_got)
	      p->symbol_segment = obj->got_segment;
	    else
	      {
		int sym_idx;

		sym_idx = grub_target_to_host16 (sym->st_shndx);
		if (sym_idx == STN_ABS)
		  grub_util_error ("can\'t relocate absolute symbol");

		if ((sym_idx != STN_UNDEF) && (sym_idx != STN_COMMON))
		  {
		    if (! segments[sym_idx])
		      grub_util_error ("no symbol segment");

		    p->symbol_segment = segments[sym_idx];

#ifdef GRUB_TARGET_USE_ADDEND
		    addend += grub_target_to_host (sym->st_value);
#else
		    if (type == GRUB_OBJ_REL_TYPE_32)
		      {
			grub_uint32_t v;

			v = grub_target_to_host32 (*((grub_uint32_t *) addr)) +
			  grub_target_to_host (sym->st_value);
			*((grub_uint32_t *) addr) =
			  grub_host_to_target32 (v);
		      }
		    else if (type == GRUB_OBJ_REL_TYPE_16)
		      {
			grub_uint16_t v;

			v = grub_target_to_host16 (*((grub_uint16_t *) addr)) +
			  grub_target_to_host (sym->st_value);
			*((grub_uint16_t *) addr) =
			  grub_host_to_target16 (v);
		      }
		    else if (type == GRUB_OBJ_REL_TYPE_64)
		      {
			grub_uint64_t v;

			v = grub_target_to_host64 (*((grub_uint64_t *) addr)) +
			  grub_target_to_host (sym->st_value);
			*((grub_uint64_t *) addr) =
			  grub_host_to_target64 (v);
		      }
		    else
		      grub_util_error ("invalid relocation type %d", type);
#endif
		  }
	      }

#ifdef GRUB_TARGET_USE_ADDEND
	    p->reloc.addend = addend;
#endif
	    grub_list_push (GRUB_AS_LIST_P (&obj->relocs),
			    GRUB_AS_LIST (p));
	  }
      }
}

int
grub_obj_import_elf (struct grub_util_obj *obj, char *image, int size)
{
  Elf_Ehdr *e;
  Elf_Shdr *sections;
  Elf_Off section_offset;
  Elf_Half section_entsize;
  Elf_Half num_sections;
  struct grub_util_obj_segment **segments;

  e = (Elf_Ehdr *) image;
  if (! check_elf_header (e, size))
    return 0;

  section_offset = grub_target_to_host (e->e_shoff);
  section_entsize = grub_target_to_host16 (e->e_shentsize);
  num_sections = grub_target_to_host16 (e->e_shnum);

  if (size < (int) (section_offset + section_entsize * num_sections))
    grub_util_error ("invalid ELF format");

  sections = (Elf_Shdr *) (image + section_offset);
  segments = xmalloc_zero (num_sections * sizeof (segments[0]));

  add_segments (obj, segments, image, sections, section_entsize, num_sections);
  add_symbols (obj, segments, image, sections, section_entsize, num_sections);
  add_relocs (obj, segments, image, sections, section_entsize, num_sections);

  free (segments);
  return 1;
}

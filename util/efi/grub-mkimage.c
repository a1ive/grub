/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2004,2005,2006,2007,2008,2009  Free Software Foundation, Inc.
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

#include <config.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <grub/util/obj.h>
#include <grub/util/misc.h>
#include <grub/util/resolve.h>
#include <grub/kernel.h>
#include <grub/efi/pe32.h>
#include <grub/machine/kernel.h>
#include <grub/i18n.h>

#include "progname.h"

int
grub_strcmp (const char *s1, const char *s2)
{
  return strcmp (s1, s2);
}

#define MAX_SECTIONS	6

static const grub_uint8_t stub[] = GRUB_PE32_MSDOS_STUB;

static inline grub_uint32_t
align_address (grub_uint32_t addr, unsigned alignment)
{
  return (addr + alignment - 1) & ~(alignment - 1);
}

static inline grub_uint32_t
align_pe32_section (grub_uint32_t addr)
{
  return align_address (addr, GRUB_PE32_SECTION_ALIGNMENT);
}

/* Return the starting address right after the header,
   aligned by the section alignment. Allocate 6 section tables for
   .text, .data, .rdata, .bss, .info and .reloc.  */
static grub_uint32_t
get_starting_section_address (void)
{
  return align_pe32_section (sizeof (struct grub_pe32_header)
			     + (MAX_SECTIONS
				* sizeof (struct grub_pe32_section_table)));
}

void
write_padding (FILE *out, size_t size)
{
  size_t i;

  for (i = 0; i < size; i++)
    if (fputc (0, out) == EOF)
      grub_util_error ("padding failed");
}

/* Add a PE32's fixup entry for a relocation. Return the resulting address
   after having written to the file OUT.  */
grub_uint32_t
add_fixup_entry (struct grub_pe32_fixup_block **block, grub_uint16_t type,
		 grub_uint32_t addr, int flush, grub_uint32_t current_address,
		 FILE *out)
{
  struct grub_pe32_fixup_block *b = *block;

  /* First, check if it is necessary to write out the current block.  */
  if (b)
    {
      if (flush || addr < b->page_rva || b->page_rva + 0x1000 <= addr)
	{
	  grub_uint32_t size;

	  if (flush)
	    {
	      /* Add as much padding as necessary to align the address
		 with a section boundary.  */
	      grub_uint32_t next_address;
	      unsigned padding_size;
	      size_t index;

	      next_address = current_address + b->block_size;
	      padding_size = ((align_pe32_section (next_address)
			       - next_address)
			      >> 1);
	      index = ((b->block_size - sizeof (*b)) >> 1);
	      grub_util_info ("adding %d padding fixup entries", padding_size);
	      while (padding_size--)
		{
		  b->entries[index++] = 0;
		  b->block_size += 2;
		}
	    }
	  else if (b->block_size & (8 - 1))
	    {
	      /* If not aligned with a 32-bit boundary, add
		 a padding entry.  */
	      size_t index;

	      grub_util_info ("adding a padding fixup entry");
	      index = ((b->block_size - sizeof (*b)) >> 1);
	      b->entries[index] = 0;
	      b->block_size += 2;
	    }

	  /* Flush it.  */
	  grub_util_info ("writing %d bytes of a fixup block starting at 0x%x",
			  b->block_size, b->page_rva);
	  size = b->block_size;
	  current_address += size;
	  b->page_rva = grub_cpu_to_le32 (b->page_rva);
	  b->block_size = grub_cpu_to_le32 (b->block_size);
	  if (fwrite (b, size, 1, out) != 1)
	    grub_util_error ("write failed");
	  free (b);
	  *block = b = 0;
	}
    }

  if (! flush)
    {
      grub_uint16_t entry;
      size_t index;

      /* If not allocated yet, allocate a block with enough entries.  */
      if (! b)
	{
	  *block = b = xmalloc (sizeof (*b) + 2 * 0x1000);

	  /* The spec does not mention the requirement of a Page RVA.
	     Here, align the address with a 4K boundary for safety.  */
	  b->page_rva = (addr & ~(0x1000 - 1));
	  b->block_size = sizeof (*b);
	}

      /* Sanity check.  */
      if (b->block_size >= sizeof (*b) + 2 * 0x1000)
	grub_util_error ("too many fixup entries");

      /* Add a new entry.  */
      index = ((b->block_size - sizeof (*b)) >> 1);
      entry = GRUB_PE32_FIXUP_ENTRY (type, addr - b->page_rva);
      b->entries[index] = grub_cpu_to_le16 (entry);
      b->block_size += 2;
    }

  return current_address;
}

/* Write out zeros to make space for the header.  */
static grub_uint32_t
make_header_space (FILE *out)
{
  grub_uint32_t addr;

  addr = get_starting_section_address ();
  write_padding (out, addr);

  return addr;
}

static grub_uint32_t
write_section_padding (FILE* out, grub_uint32_t offset)
{
  grub_uint32_t new_ofs;

  new_ofs = align_address (offset, GRUB_PE32_FILE_ALIGNMENT);
  if (new_ofs != offset)
    {
      grub_util_info ("padding %d bytes for file alignment",
		      new_ofs - offset);
      write_padding (out, new_ofs - offset);
      offset = new_ofs;
    }

  return offset;
}

static int
write_sections (FILE* out, struct grub_util_obj *obj,
		struct grub_pe32_section_table *sections,
		grub_uint32_t *offset)
{
  struct grub_util_obj_segment *seg;
  grub_uint32_t ofs;
  int n, last_type;

  ofs = *offset;
  n = 0;
  last_type = 0;
  sections--;
  for (seg = obj->segments; seg; seg = seg->next)
    {
      grub_uint32_t seg_ofs;

      if (seg->segment.type != last_type)
	{
	  last_type = seg->segment.type;
	  ofs = write_section_padding (out, ofs);
	  n++;
	  sections++;

	  switch (seg->segment.type)
	    {
	    case GRUB_OBJ_SEG_TEXT:
	      strcpy (sections->name, ".text");
	      sections->characteristics =
		grub_cpu_to_le32 (GRUB_PE32_SCN_CNT_CODE
				  | GRUB_PE32_SCN_MEM_EXECUTE
				  | GRUB_PE32_SCN_MEM_READ);
	      break;

	    case GRUB_OBJ_SEG_DATA:
	      strcpy (sections->name, ".data");
	      sections->characteristics =
		grub_cpu_to_le32 (GRUB_PE32_SCN_CNT_INITIALIZED_DATA
				  | GRUB_PE32_SCN_MEM_READ
				  | GRUB_PE32_SCN_MEM_WRITE);
	      break;

	    case GRUB_OBJ_SEG_RDATA:
	      strcpy (sections->name, ".rdata");
	      sections->characteristics =
		grub_cpu_to_le32 (GRUB_PE32_SCN_CNT_INITIALIZED_DATA
				  | GRUB_PE32_SCN_MEM_READ);
	      break;

	    case GRUB_OBJ_SEG_BSS:
	      /* Fill bss section to avoid a bug in EFI implementations.  */
	      strcpy (sections->name, ".bss");
	      sections->characteristics =
		grub_cpu_to_le32 (GRUB_PE32_SCN_CNT_INITIALIZED_DATA
				  | GRUB_PE32_SCN_MEM_READ
				  | GRUB_PE32_SCN_MEM_WRITE);
	      break;

	    case GRUB_OBJ_SEG_INFO:
	      strcpy (sections->name, ".info");
	      sections->characteristics =
		grub_cpu_to_le32 (GRUB_PE32_SCN_CNT_INITIALIZED_DATA
				  | GRUB_PE32_SCN_MEM_READ);
	      break;

	    default:
	      grub_util_error ("invalid segment type %d", seg->segment.type);
	    }

	  sections->virtual_address = grub_cpu_to_le32 (seg->segment.offset);
	  sections->raw_data_offset = grub_cpu_to_le32 (ofs);
	}

      seg_ofs = seg->segment.offset -
	grub_le_to_cpu32 (sections->virtual_address);
      sections->virtual_size = grub_cpu_to_le32 (seg_ofs + seg->segment.size);
      sections->raw_data_size = sections->virtual_size;
      ofs = grub_le_to_cpu32 (sections->raw_data_offset) +
	grub_le_to_cpu32 (sections->raw_data_size);

      if (seg->raw_size)
	grub_util_write_image_at (seg->data, seg->raw_size,
				  grub_le_to_cpu32 (sections->raw_data_offset)
				  + seg_ofs, out);
    }

  *offset = write_section_padding (out, ofs);
  return n;
}

/* Make a .reloc section.  */
static grub_uint32_t
make_reloc_section (FILE *out, struct grub_util_obj *obj,
		    struct grub_pe32_section_table *rel_section,
		    grub_uint32_t offset)
{
  struct grub_pe32_fixup_block *fixup_block = 0;
  struct grub_util_obj_reloc *rel;
  grub_uint32_t rva;

  rva =
    align_pe32_section (grub_le_to_cpu32 ((rel_section - 1)->virtual_address)
			+ grub_le_to_cpu32 ((rel_section - 1)->virtual_size));

  rel_section->virtual_address = grub_cpu_to_le32 (rva);
  rel_section->raw_data_offset = grub_cpu_to_le32 (offset);

  rel = obj->relocs;
  while (rel)
    {
      if (rel->segment)
	{
	  grub_uint32_t addr;

	  addr = (rel->reloc.offset + rel->segment->segment.offset);
	  switch (rel->reloc.type)
	    {
#if defined(GRUB_TARGET_I386)
	    case GRUB_OBJ_REL_TYPE_32:
	      rva = add_fixup_entry (&fixup_block,
				     GRUB_PE32_REL_BASED_HIGHLOW,
				     addr, 0, rva, out);
	      break;

#elif defined(GRUB_TARGET_X86_64)
	    case GRUB_OBJ_REL_TYPE_32:
	      rva = add_fixup_entry (&fixup_block,
				     GRUB_PE32_REL_BASED_HIGHLOW,
				     addr, 0, rva, out);
	      break;

	    case GRUB_OBJ_REL_TYPE_64:
	      rva = add_fixup_entry (&fixup_block,
				     GRUB_PE32_REL_BASED_DIR64,
				     addr, 0, rva, out);

	      break;
#endif
	    default:
	      grub_util_error ("invalid relocation type %d", rel->reloc.type);
	    }
	}
      rel = rel->next;
    }

  rva = add_fixup_entry (&fixup_block, 0, 0, 1, rva, out);
  rel_section->virtual_size =
    grub_cpu_to_le32 (rva - grub_le_to_cpu32 (rel_section->virtual_address));
  rel_section->raw_data_size = rel_section->virtual_size;

  strcpy (rel_section->name, ".reloc");
  rel_section->characteristics
    = grub_cpu_to_le32 (GRUB_PE32_SCN_CNT_INITIALIZED_DATA
			| GRUB_PE32_SCN_MEM_DISCARDABLE
			| GRUB_PE32_SCN_MEM_READ);

  return offset += grub_le_to_cpu32 (rel_section->raw_data_size);
}

static void
get_segment_size (struct grub_util_obj *obj, grub_uint32_t *code_size,
		  grub_uint32_t *data_size, grub_uint32_t *bss_size)
{
  struct grub_util_obj_segment *seg;

  *code_size = 0;
  *data_size = 0;
  *bss_size = 0;

  seg = obj->segments;
  while (seg)
    {
      switch (seg->segment.type)
	{
	case GRUB_OBJ_SEG_TEXT:
	  *code_size += seg->segment.size;
	  break;

	case GRUB_OBJ_SEG_DATA:
	case GRUB_OBJ_SEG_RDATA:
	case GRUB_OBJ_SEG_INFO:
	  *data_size += seg->segment.size;
	  break;

	case GRUB_OBJ_SEG_BSS:
	  *bss_size += seg->segment.size;
	  break;

	default:
	  grub_util_error ("invalid segment type %d", seg->segment.type);
	}

      seg = seg->next;
    }
}

/* Create the header.  */
static void
make_header (FILE *out, struct grub_util_obj *obj,
	     struct grub_pe32_section_table *sections, int num_sections,
	     grub_uint32_t size)
{
  struct grub_pe32_header header;
  struct grub_pe32_coff_header *c;
  struct grub_pe32_optional_header *o;
  grub_uint32_t code_size, data_size, bss_size;

  /* The magic.  */
  memset (&header, 0, sizeof (header));
  memcpy (header.msdos_stub, stub, sizeof (header.msdos_stub));
  memcpy (header.signature, "PE\0\0", sizeof (header.signature));

  /* The COFF file header.  */
  c = &header.coff_header;
#if GRUB_TARGET_SIZEOF_VOID_P == 4
  c->machine = grub_cpu_to_le16 (GRUB_PE32_MACHINE_I386);
#else
  c->machine = grub_cpu_to_le16 (GRUB_PE32_MACHINE_X86_64);
#endif

  c->num_sections = grub_cpu_to_le16 (num_sections);
  c->time = grub_cpu_to_le32 (time (0));
  c->optional_header_size = grub_cpu_to_le16 (sizeof (header.optional_header));
  c->characteristics = grub_cpu_to_le16 (GRUB_PE32_EXECUTABLE_IMAGE
					 | GRUB_PE32_LINE_NUMS_STRIPPED
#if GRUB_TARGET_SIZEOF_VOID_P == 4
					 | GRUB_PE32_32BIT_MACHINE
#endif
					 | GRUB_PE32_LOCAL_SYMS_STRIPPED
					 | GRUB_PE32_DEBUG_STRIPPED);

  /* The PE Optional header.  */
  o = &header.optional_header;
  o->magic = grub_cpu_to_le16 (GRUB_PE32_PE32_MAGIC);

  get_segment_size (obj, &code_size, &data_size, &bss_size);
  o->code_size = grub_cpu_to_le32 (code_size);
  o->data_size = grub_cpu_to_le32 (data_size + bss_size);
  o->bss_size = 0;

  o->entry_addr = grub_cpu_to_le32 (obj->segments->segment.offset);
  o->code_base = sections[0].virtual_address;

#if GRUB_TARGET_SIZEOF_VOID_P == 4
  o->data_base = sections[1].virtual_address;
#endif
  o->image_base = 0;
  o->section_alignment = grub_cpu_to_le32 (GRUB_PE32_SECTION_ALIGNMENT);
  o->file_alignment = grub_cpu_to_le32 (GRUB_PE32_FILE_ALIGNMENT);
  o->image_size = grub_cpu_to_le32 (size);
  o->header_size = grub_cpu_to_le32 (sections[0].virtual_address);
  o->subsystem = grub_cpu_to_le16 (GRUB_PE32_SUBSYSTEM_EFI_APPLICATION);

  /* Do these really matter? */
  o->stack_reserve_size = grub_cpu_to_le32 (0x10000);
  o->stack_commit_size = grub_cpu_to_le32 (0x10000);
  o->heap_reserve_size = grub_cpu_to_le32 (0x10000);
  o->heap_commit_size = grub_cpu_to_le32 (0x10000);

  o->num_data_directories = grub_cpu_to_le32 (GRUB_PE32_NUM_DATA_DIRECTORIES);

  o->base_relocation_table.rva = sections[num_sections - 1].virtual_address;
  o->base_relocation_table.size = sections[num_sections - 1].virtual_size;

  if (fseeko (out, 0, SEEK_SET) < 0)
    grub_util_error ("seek failed");

  if (fwrite (&header, sizeof (header), 1, out) != 1
      || fwrite (sections, sizeof (sections[0]) * num_sections, 1, out) != 1)
    grub_util_error ("write failed");
}

/* Convert an ELF relocatable object into an EFI Application (PE32).  */
static void
generate_image (const char *dir, char *prefix, FILE *out, char *mods[],
		char *memdisk_path, char *config_path)
{
  struct grub_pe32_section_table *sections;
  int num_sections;
  grub_uint32_t offset;
  struct grub_util_path_list *path_list;
  struct grub_util_obj *obj;
  struct grub_util_obj_segment *modinfo;

  path_list = grub_util_resolve_dependencies (dir, "moddep.lst", mods);

  obj = xmalloc_zero (sizeof (*obj));
  modinfo = grub_obj_add_modinfo (obj, dir, path_list, 1,
				  memdisk_path, config_path);
  grub_obj_sort_segments (obj);
  grub_obj_merge_segments (obj, GRUB_PE32_SECTION_ALIGNMENT,
			   GRUB_OBJ_MERGE_ALL);

  grub_obj_add_kernel_symbols (obj, modinfo, 0);

  /* Resolve addresses in the virtual address space.  */
  grub_obj_reloc_symbols (obj, GRUB_OBJ_MERGE_ALL);
  offset = make_header_space (out);
  grub_obj_link (obj, offset);

  if ((! obj->segments) || (obj->segments->segment.type != GRUB_OBJ_SEG_TEXT))
    grub_util_error ("invalid mod format");

  if (GRUB_KERNEL_MACHINE_PREFIX + strlen (prefix) + 1 >
      GRUB_KERNEL_MACHINE_DATA_END)
    grub_util_error ("prefix too long");

  strcpy (obj->segments->data + GRUB_KERNEL_MACHINE_PREFIX, prefix);

  sections = xmalloc_zero (sizeof (struct grub_pe32_section_table)
			   * MAX_SECTIONS);
  num_sections = write_sections (out, obj, sections, &offset);

  offset = make_reloc_section (out, obj, &sections[num_sections], offset);
  num_sections++;

  make_header (out, obj, sections, num_sections, offset);

  /* Clean up.  */
  grub_obj_free (obj);
  free (sections);
}

static struct option options[] =
  {
    {"directory", required_argument, 0, 'd'},
    {"prefix", required_argument, 0, 'p'},
    {"memdisk", required_argument, 0, 'm'},
    {"config", required_argument, 0, 'c'},
    {"output", required_argument, 0, 'o'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {"verbose", no_argument, 0, 'v'},
    { 0, 0, 0, 0 }
  };

static void
usage (int status)
{
  if (status)
    fprintf (stderr, _("Try `%s --help' for more information.\n"), program_name);
  else
    printf (_("\
Usage: %s -o FILE [OPTION]... [MODULES]\n\
\n\
Make a bootable image of GRUB.\n\
\n\
  -d, --directory=DIR     use images and modules under DIR [default=%s]\n\
  -p, --prefix=DIR        set grub_prefix directory [default=%s]\n\
  -m, --memdisk=FILE      embed FILE as a memdisk image\n\
  -c, --config=FILE       embed FILE as boot config\n\
  -o, --output=FILE       output a generated image to FILE\n\
  -h, --help              display this message and exit\n\
  -V, --version           print version information and exit\n\
  -v, --verbose           print verbose messages\n\
\n\
Report bugs to <%s>.\n\
"), program_name, GRUB_LIBDIR, DEFAULT_DIRECTORY, PACKAGE_BUGREPORT);

  exit (status);
}

int
main (int argc, char *argv[])
{
  FILE *fp;
  char *output = NULL;
  char *dir = NULL;
  char *prefix = NULL;
  char *memdisk = NULL;
  char *config = NULL;

  set_program_name (argv[0]);

  grub_util_init_nls ();

  while (1)
    {
      int c = getopt_long (argc, argv, "d:p:m:c:o:hVv", options, 0);
      if (c == -1)
	break;

      switch (c)
	{
	  case 'd':
	    if (dir)
	      free (dir);
	    dir = xstrdup (optarg);
	    break;
	  case 'h':
	    usage (0);
	    break;
	  case 'o':
	    if (output)
	      free (output);
	    output = xstrdup (optarg);
	    break;
	  case 'p':
	    if (prefix)
	      free (prefix);
	    prefix = xstrdup (optarg);
	    break;

	  case 'm':
	    if (memdisk)
	      free (memdisk);

	    memdisk = xstrdup (optarg);

	    if (prefix)
	      free (prefix);

	    prefix = xstrdup ("(memdisk)/boot/burg");
	    break;

	  case 'c':
	    if (config)
	      free (config);

	    config = xstrdup (optarg);
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

  if (! output)
    usage (1);

  fp = fopen (output, "wb");
  if (! fp)
    grub_util_error ("cannot open %s", output);

  generate_image (dir ? : GRUB_LIBDIR, prefix ? : DEFAULT_DIRECTORY, fp,
		  argv + optind, memdisk, config);

  fclose (fp);

  if (dir)
    free (dir);

  if (memdisk)
    free (memdisk);

  if (config)
    free (config);

  return 0;
}

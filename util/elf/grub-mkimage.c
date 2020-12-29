/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2004,2005,2006,2007,2008,2009,2010  Free Software Foundation, Inc.
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
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <grub/elf.h>
#include <grub/misc.h>
#include <grub/util/obj.h>
#include <grub/util/misc.h>
#include <grub/util/resolve.h>
#include <grub/kernel.h>
#include <grub/cpu/kernel.h>
#include <grub/i18n.h>

#include "progname.h"

#if GRUB_TARGET_SIZEOF_VOID_P == 4

#define grub_target_to_host	grub_target_to_host32
#define grub_host_to_target	grub_host_to_target32

#elif GRUB_TARGET_SIZEOF_VOID_P == 8

#define grub_target_to_host	grub_target_to_host64
#define grub_host_to_target	grub_host_to_target64

#endif

#define GRUB_IEEE1275_NOTE_NAME "PowerPC"
#define GRUB_IEEE1275_NOTE_TYPE 0x1275

/* These structures are defined according to the CHRP binding to IEEE1275,
   "Client Program Format" section.  */

struct grub_ieee1275_note_hdr
{
  grub_uint32_t namesz;
  grub_uint32_t descsz;
  grub_uint32_t type;
  char name[sizeof (GRUB_IEEE1275_NOTE_NAME)];
};

struct grub_ieee1275_note_desc
{
  grub_uint32_t real_mode;
  grub_uint32_t real_base;
  grub_uint32_t real_size;
  grub_uint32_t virt_base;
  grub_uint32_t virt_size;
  grub_uint32_t load_base;
};

struct grub_ieee1275_note
{
  struct grub_ieee1275_note_hdr header;
  struct grub_ieee1275_note_desc descriptor;
};

void
make_note_section (FILE *out, Elf_Phdr *phdr, grub_uint32_t offset)
{
  struct grub_ieee1275_note note;
  int note_size = sizeof (struct grub_ieee1275_note);

  grub_util_info ("adding CHRP NOTE segment");

  note.header.namesz = grub_host_to_target32 (sizeof (GRUB_IEEE1275_NOTE_NAME));
  note.header.descsz = grub_host_to_target32 (note_size);
  note.header.type = grub_host_to_target32 (GRUB_IEEE1275_NOTE_TYPE);
  strcpy (note.header.name, GRUB_IEEE1275_NOTE_NAME);
  note.descriptor.real_mode = grub_host_to_target32 (0xffffffff);
  note.descriptor.real_base = grub_host_to_target32 (0x00c00000);
  note.descriptor.real_size = grub_host_to_target32 (0xffffffff);
  note.descriptor.virt_base = grub_host_to_target32 (0xffffffff);
  note.descriptor.virt_size = grub_host_to_target32 (0xffffffff);
  note.descriptor.load_base = grub_host_to_target32 (0x00004000);

  /* Write the note data to the new segment.  */
  grub_util_write_image_at (&note, note_size, offset, out);

  /* Fill in the rest of the segment header.  */
  phdr->p_type = grub_host_to_target32 (PT_NOTE);
  phdr->p_flags = grub_host_to_target32 (PF_R);
  phdr->p_align = grub_host_to_target (GRUB_TARGET_SIZEOF_LONG);
  phdr->p_vaddr = 0;
  phdr->p_paddr = 0;
  phdr->p_filesz = grub_host_to_target (note_size);
  phdr->p_memsz = 0;
  phdr->p_offset = grub_host_to_target (offset);
}

int
grub_strcmp (const char *s1, const char *s2)
{
  return strcmp (s1, s2);
}

#define MAX_SECTIONS	2

static void
write_padding (FILE *out, size_t size)
{
  size_t i;

  for (i = 0; i < size; i++)
    if (fputc (0, out) == EOF)
      grub_util_error ("padding failed");
}

static grub_uint32_t
make_header_space (FILE *out)
{
  grub_uint32_t addr;

  addr = (ALIGN_UP (sizeof (Elf_Ehdr), GRUB_TARGET_SIZEOF_LONG) +
	  sizeof (Elf_Shdr) * MAX_SECTIONS);
  write_padding (out, addr);

  return addr;
}

static grub_uint32_t
write_section_padding (FILE* out, grub_uint32_t offset)
{
  grub_uint32_t new_ofs;

  new_ofs = ALIGN_UP (offset, GRUB_TARGET_SIZEOF_LONG);
  if (new_ofs != offset)
    {
      grub_util_info ("padding %d bytes for file alignment",
		      new_ofs - offset);
      write_padding (out, new_ofs - offset);
      offset = new_ofs;
    }

  return offset;
}

static grub_uint32_t
write_sections (FILE* out, struct grub_util_obj *obj,
		Elf_Phdr *phdr, grub_uint32_t sofs)
{
  struct grub_util_obj_segment *seg;
  grub_uint32_t ofs, vaddr;

  seg = obj->segments;
  phdr->p_type = grub_host_to_target32 (PT_LOAD);
  phdr->p_flags = grub_host_to_target32 (PF_R | PF_W | PF_X);
  phdr->p_align = grub_host_to_target (seg->segment.align);

  vaddr = seg->segment.offset;
  phdr->p_vaddr = grub_host_to_target (vaddr);
  phdr->p_paddr = phdr->p_vaddr;

  ofs = sofs;
  phdr->p_offset = grub_host_to_target (ofs);
  for (; seg; seg = seg->next)
    {
      grub_uint32_t seg_ofs;

      seg_ofs = seg->segment.offset - vaddr;
      phdr->p_memsz = grub_host_to_target (seg_ofs + seg->segment.size);
      if (seg->data)
	{
	  phdr->p_filesz = grub_host_to_target (seg_ofs + seg->raw_size);
	  ofs = sofs + grub_target_to_host (phdr->p_filesz);
	  grub_util_write_image_at (seg->data, seg->raw_size,
				    sofs + seg_ofs, out);
	}
    }

  return write_section_padding (out, ofs);
}

static void
make_header (FILE *out, Elf_Phdr *phdr, int num_sections)
{
  Elf_Ehdr ehdr;
  grub_uint32_t offset;

  memset (&ehdr, 0, sizeof (ehdr));

  ehdr.e_ident[EI_MAG0] = ELFMAG0;
  ehdr.e_ident[EI_MAG1] = ELFMAG1;
  ehdr.e_ident[EI_MAG2] = ELFMAG2;
  ehdr.e_ident[EI_MAG3] = ELFMAG3;
  ehdr.e_ident[EI_VERSION] = EV_CURRENT;
  ehdr.e_version = grub_host_to_target32 (EV_CURRENT);
  ehdr.e_type = grub_host_to_target16 (ET_EXEC);
#if GRUB_TARGET_SIZEOF_VOID_P == 4
  ehdr.e_ident[EI_CLASS] = ELFCLASS32;
#else
  ehdr.e_ident[EI_CLASS] = ELFCLASS64;
#endif
  ehdr.e_ehsize = grub_host_to_target16 (sizeof (ehdr));
  ehdr.e_entry = phdr->p_vaddr;

#if defined(GRUB_TARGET_I386)
  ehdr.e_ident[EI_DATA] = ELFDATA2LSB;
  ehdr.e_machine = grub_host_to_target16 (EM_386);
#elif defined(GRUB_TARGET_POWERPC)
  ehdr.e_ident[EI_DATA] = ELFDATA2MSB;
  ehdr.e_machine = grub_host_to_target16 (EM_PPC);
#elif defined(GRUB_TARGET_SPARC64)
  ehdr.e_ident[EI_DATA] = ELFDATA2MSB;
  ehdr.e_machine = grub_host_to_target16 (EM_SPARCV9);
#endif

  offset = ALIGN_UP (sizeof (ehdr), GRUB_TARGET_SIZEOF_LONG);
  ehdr.e_phoff = grub_host_to_target (offset);
  ehdr.e_phentsize = grub_host_to_target16 (sizeof (Elf_Phdr));
  ehdr.e_phnum = grub_host_to_target16 (num_sections);

  grub_util_write_image_at (&ehdr, sizeof (ehdr), 0, out);
  grub_util_write_image_at (phdr, sizeof (Elf_Phdr) * num_sections,
			    offset, out);
}

void
generate_image (char *dir, char *prefix, FILE *out, grub_uint32_t base,
		int chrp, char *mods[], char *memdisk_path, char *config_path)
{
  Elf_Phdr *sections;
  int num_sections;
  grub_uint32_t offset;
  struct grub_util_path_list *path_list;
  struct grub_util_obj *obj;
  struct grub_util_obj_segment *modinfo;

  path_list = grub_util_resolve_dependencies (dir, "moddep.lst", mods);

  obj = xmalloc_zero (sizeof (*obj));
  modinfo = grub_obj_add_modinfo (obj, dir, path_list, 0,
				  memdisk_path, config_path);
  grub_obj_sort_segments (obj);
  grub_obj_merge_segments (obj, GRUB_TARGET_SIZEOF_LONG, GRUB_OBJ_MERGE_ALL);
  grub_obj_add_kernel_symbols (obj, modinfo, 0);
  grub_obj_reloc_symbols (obj, GRUB_OBJ_MERGE_ALL);
  grub_obj_link (obj, base);

  if (prefix)
    {
      if ((! obj->segments) ||
	  (obj->segments->segment.type != GRUB_OBJ_SEG_TEXT))
	grub_util_error ("invalid mod format");

      if (GRUB_KERNEL_CPU_PREFIX + strlen (prefix) + 1 >
	  GRUB_KERNEL_CPU_DATA_END)
	grub_util_error ("prefix too long");

      strcpy (obj->segments->data + GRUB_KERNEL_CPU_PREFIX, prefix);
    }

  sections = xmalloc_zero (sizeof (Elf_Phdr) * MAX_SECTIONS);
  offset = make_header_space (out);
  offset = write_sections (out, obj, sections, offset);
  num_sections = 1;

  if (chrp)
    {
      make_note_section (out, &sections[num_sections], offset);
      num_sections++;
    }

  make_header (out, sections, num_sections);

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
    {"base", required_argument, 0, 'b'},
    {"help", no_argument, 0, 'h'},
    {"note", no_argument, 0, 'n'},
    {"version", no_argument, 0, 'V'},
    {"verbose", no_argument, 0, 'v'},
    { 0, 0, 0, 0 },
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
  -p, --prefix=DIR        set grub_prefix directory\n\
  -m, --memdisk=FILE      embed FILE as a memdisk image\n\
  -c, --config=FILE       embed FILE as boot config\n\
  -o, --output=FILE       output a generated image to FILE\n\
  -b, --base=ADDR         set base address\n\
  -h, --help              display this message and exit\n\
  -n, --note              add NOTE segment for CHRP Open Firmware\n\
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
  FILE *fp;
  char *output = NULL;
  char *dir = NULL;
  char *prefix = NULL;
  char *memdisk = NULL;
  char *config = NULL;
  int chrp = 0;
  grub_uint32_t base;

#if defined (GRUB_TARGET_I386) && defined (GRUB_MACHINE_IEEE1275)
  base = 0x10000;
#elif defined (GRUB_TARGET_I386) && defined (GRUB_MACHINE_COREBOOT)
  base = 0x8200;
#elif defined (GRUB_TARGET_POWERPC) && defined (GRUB_MACHINE_IEEE1275)
  base = 0x200000;
#elif defined (GRUB_TARGET_SPARC64) && defined (GRUB_MACHINE_IEEE1275)
  base = 0x200000;
#else
  base = 0;
#endif

  set_program_name (argv[0]);

  grub_util_init_nls ();

  while (1)
    {
      int c = getopt_long (argc, argv, "d:p:m:c:o:b:hVvn", options, 0);
      if (c == -1)
	break;

      switch (c)
	{
	  case 'd':
	    if (dir)
	      free (dir);
	    dir = xstrdup (optarg);
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

	  case 'h':
	    usage (0);
	    break;
	  case 'n':
	    chrp = 1;
	    break;
	  case 'o':
	    if (output)
	      free (output);
	    output = xstrdup (optarg);
	    break;
	  case 'b':
	    base = strtoul (optarg, 0, 0);
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

  if (!output)
    usage (1);

  fp = fopen (output, "wb");
  if (! fp)
    grub_util_error ("cannot open %s", output);

  generate_image (dir ? : GRUB_LIBDIR, prefix, fp, base, chrp, argv + optind,
		  memdisk, config);

  fclose (fp);

  if (dir)
    free (dir);

  if (memdisk)
    free (memdisk);

  if (config)
    free (config);

  return 0;
}

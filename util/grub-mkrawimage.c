/* grub-mkimage.c - make a bootable image */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2002,2003,2004,2005,2006,2007,2008,2009,2010  Free Software Foundation, Inc.
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
#include <grub/types.h>
#include <grub/machine/boot.h>
#include <grub/machine/kernel.h>
#include <grub/machine/memory.h>
#include <grub/i18n.h>
#include <grub/kernel.h>
#include <grub/disk.h>
#include <grub/util/misc.h>
#include <grub/util/resolve.h>
#include <grub/misc.h>
#include <grub/util/obj.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#define _GNU_SOURCE	1
#include <getopt.h>

#include "progname.h"

int
grub_strcmp (const char *s1, const char *s2)
{
  return strcmp (s1, s2);
}

#ifdef ENABLE_LZMA
#include <grub/lib/LzmaEnc.h>

static void *SzAlloc(void *p, size_t size) { p = p; return xmalloc(size); }
static void SzFree(void *p, void *address) { p = p; free(address); }
static ISzAlloc g_Alloc = { SzAlloc, SzFree };

static void
compress_kernel (char *kernel_img, size_t kernel_size,
		 char **core_img, size_t *core_size)
{
  CLzmaEncProps props;
  unsigned char out_props[5];
  size_t out_props_size = 5;

  LzmaEncProps_Init(&props);
  props.dictSize = 1 << 16;
  props.lc = 3;
  props.lp = 0;
  props.pb = 2;
  props.numThreads = 1;

  if (kernel_size < GRUB_KERNEL_MACHINE_RAW_SIZE)
    grub_util_error (_("the core image is too small"));

  *core_img = xmalloc (kernel_size);
  memcpy (*core_img, kernel_img, GRUB_KERNEL_MACHINE_RAW_SIZE);

  *core_size = kernel_size - GRUB_KERNEL_MACHINE_RAW_SIZE;
  if (LzmaEncode((unsigned char *) *core_img + GRUB_KERNEL_MACHINE_RAW_SIZE,
		 core_size,
		 (unsigned char *) kernel_img + GRUB_KERNEL_MACHINE_RAW_SIZE,
		 kernel_size - GRUB_KERNEL_MACHINE_RAW_SIZE,
		 &props, out_props, &out_props_size,
		 0, NULL, &g_Alloc, &g_Alloc) != SZ_OK)
    grub_util_error (_("cannot compress the kernel image"));

  *core_size += GRUB_KERNEL_MACHINE_RAW_SIZE;
}

#else	/* No lzma compression */

static void
compress_kernel (char *kernel_img, size_t kernel_size,
	       char **core_img, size_t *core_size)
{
  *core_img = xmalloc (kernel_size);
  memcpy (*core_img, kernel_img, kernel_size);
  *core_size = kernel_size;
}

#endif	/* No lzma compression */

#if defined(GRUB_MACHINE_PCBIOS)
#define LINK_ADDR	(GRUB_BOOT_MACHINE_KERNEL_ADDR + 0x200)
#elif defined(GRUB_MACHINE_QEMU)
#define LINK_ADDR	GRUB_KERNEL_MACHINE_LINK_ADDR
#endif

static void
generate_image (const char *dir, char *prefix, FILE *out, char *mods[],
		char *memdisk_path, char *config_path)
{
  char *kernel_img, *boot_img, *core_img;
  size_t kernel_size, boot_size, total_module_size, core_size;
  char *boot_path;
  unsigned num;
  struct grub_util_path_list *path_list;
  struct grub_util_obj *obj;
  struct grub_util_obj_segment *modinfo, *seg;
  int bss_ofs;

  path_list = grub_util_resolve_dependencies (dir, "moddep.lst", mods);

  obj = xmalloc_zero (sizeof (*obj));
#ifdef ENABLE_LZMA
  modinfo = grub_obj_add_modinfo (obj, dir, path_list, 1,
				  memdisk_path, config_path);
#else
  modinfo = grub_obj_add_modinfo (obj, dir, path_list, 0,
				  memdisk_path, config_path);
#endif
  grub_obj_sort_segments (obj);
  grub_obj_merge_segments (obj, 0, GRUB_OBJ_MERGE_ALL);

#ifdef ENABLE_LZMA
  bss_ofs =
    grub_obj_add_kernel_symbols (obj, modinfo,
				 GRUB_MEMORY_MACHINE_DECOMPRESSION_ADDR -
				 LINK_ADDR - GRUB_KERNEL_MACHINE_RAW_SIZE);
#else
  grub_obj_add_kernel_symbols (obj, modinfo, 0);
  bss_ofs = 0;
#endif

  grub_obj_reloc_symbols (obj, GRUB_OBJ_MERGE_ALL);
  grub_obj_link (obj, LINK_ADDR);

  kernel_size = modinfo->segment.offset - bss_ofs;
  total_module_size = modinfo->segment.size;
  kernel_img = xmalloc_zero (kernel_size + total_module_size);

  seg = obj->segments;
  while (seg)
    {
      if (seg->segment.type != GRUB_OBJ_SEG_BSS)
	{
	  int offset = seg->segment.offset - LINK_ADDR;

	  if (seg->segment.type == GRUB_OBJ_SEG_INFO)
	    offset -= bss_ofs;

	  memcpy (kernel_img + offset, seg->data, seg->raw_size);
	}
      seg = seg->next;
    }

  grub_obj_free (obj);

  if (GRUB_KERNEL_MACHINE_PREFIX + strlen (prefix) + 1 > GRUB_KERNEL_MACHINE_DATA_END)
    grub_util_error (_("prefix is too long"));
  strcpy (kernel_img + GRUB_KERNEL_MACHINE_PREFIX, prefix);

  grub_util_info ("kernel_img=%p, kernel_size=0x%x", kernel_img, kernel_size);
  compress_kernel (kernel_img, kernel_size + total_module_size,
		   &core_img, &core_size);

  grub_util_info ("the core size is 0x%x", core_size);

  num = ((core_size + GRUB_DISK_SECTOR_SIZE - 1) >> GRUB_DISK_SECTOR_BITS);
  if (num > 0xffff)
    grub_util_error ("the core image is too big");

#if defined(GRUB_MACHINE_PCBIOS)
  {
    unsigned num;
    num = ((core_size + GRUB_DISK_SECTOR_SIZE - 1) >> GRUB_DISK_SECTOR_BITS);
    if (num > 0xffff)
      grub_util_error (_("the core image is too big"));

    boot_path = grub_util_get_path (dir, "diskboot.img");
    boot_size = grub_util_get_image_size (boot_path);
    if (boot_size != GRUB_DISK_SECTOR_SIZE)
      grub_util_error (_("diskboot.img size must be %u bytes"), GRUB_DISK_SECTOR_SIZE);

    boot_img = grub_util_read_image (boot_path);

    {
      struct grub_boot_blocklist *block;
      block = (struct grub_boot_blocklist *) (boot_img
					 + GRUB_DISK_SECTOR_SIZE
					 - sizeof (*block));
      block->len = grub_host_to_target16 (num);

      /* This is filled elsewhere.  Verify it just in case.  */
      assert (block->segment == grub_host_to_target16 (GRUB_BOOT_MACHINE_KERNEL_SEG
						  + (GRUB_DISK_SECTOR_SIZE >> 4)));
    }

    grub_util_write_image (boot_img, boot_size, out);
    free (boot_img);
    free (boot_path);
  }
#elif defined(GRUB_MACHINE_QEMU)

  {
    char *rom_img;
    size_t rom_size;

    boot_path = grub_util_get_path (dir, "boot.img");
    boot_size = grub_util_get_image_size (boot_path);
    boot_img = grub_util_read_image (boot_path);

    /* Rom sizes must be 64k-aligned.  */
    rom_size = ALIGN_UP (core_size + boot_size, 64 * 1024);

    rom_img = xmalloc (rom_size);
    memset (rom_img, 0, rom_size);

    *((grub_int32_t *) (core_img + GRUB_KERNEL_MACHINE_CORE_ENTRY_ADDR))
      = grub_cpu_to_le32 ((grub_uint32_t) -rom_size);

    memcpy (rom_img, core_img, core_size);

    *((grub_int32_t *) (boot_img + GRUB_BOOT_MACHINE_CORE_ENTRY_ADDR))
      = grub_cpu_to_le32 ((grub_uint32_t) -rom_size);

    memcpy (rom_img + rom_size - boot_size, boot_img, boot_size);

    free (core_img);
    core_img = rom_img;
    core_size = rom_size;

    free (boot_img);
    free (boot_path);
  }

#endif

#ifdef GRUB_KERNEL_MACHINE_TOTAL_MODULE_SIZE
  *((grub_uint32_t *) (core_img + GRUB_KERNEL_MACHINE_TOTAL_MODULE_SIZE))
    = grub_cpu_to_le32 (total_module_size);
#endif
  *((grub_uint32_t *) (core_img + GRUB_KERNEL_MACHINE_KERNEL_IMAGE_SIZE))
    = grub_cpu_to_le32 (kernel_size);
#ifdef GRUB_KERNEL_MACHINE_COMPRESSED_SIZE
  *((grub_uint32_t *) (core_img + GRUB_KERNEL_MACHINE_COMPRESSED_SIZE))
    = grub_cpu_to_le32 (core_size - GRUB_KERNEL_MACHINE_RAW_SIZE);
#endif

#if defined(GRUB_KERNEL_MACHINE_INSTALL_DOS_PART) && defined(GRUB_KERNEL_MACHINE_INSTALL_BSD_PART)
  /* If we included a drive in our prefix, let GRUB know it doesn't have to
     prepend the drive told by BIOS.  */
  if (prefix[0] == '(')
    {
      *((grub_int32_t *) (core_img + GRUB_KERNEL_MACHINE_INSTALL_DOS_PART))
	= grub_cpu_to_le32 (-2);
      *((grub_int32_t *) (core_img + GRUB_KERNEL_MACHINE_INSTALL_BSD_PART))
	= grub_cpu_to_le32 (-2);
    }
#endif

#ifdef GRUB_MACHINE_PCBIOS
  if (GRUB_KERNEL_MACHINE_LINK_ADDR + core_size > GRUB_MEMORY_MACHINE_UPPER)
    grub_util_error (_("core image is too big (%p > %p)"),
 		     GRUB_KERNEL_MACHINE_LINK_ADDR + core_size, GRUB_MEMORY_MACHINE_UPPER);
#endif

  grub_util_write_image (core_img, core_size, out);
  free (kernel_img);
  free (core_img);

  while (path_list)
    {
      struct grub_util_path_list *next;

      next = path_list->next;
      free ((void *) path_list->name);
      free (path_list);
      path_list = next;
    }
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
    {0, 0, 0, 0}
  };

static void
usage (int status)
{
  if (status)
    fprintf (stderr, _("Try `%s --help' for more information.\n"), program_name);
  else
    printf (_("\
Usage: %s [OPTION]... [MODULES]\n\
\n\
Make a bootable image of GRUB.\n\
\n\
  -d, --directory=DIR     use images and modules under DIR [default=%s]\n\
  -p, --prefix=DIR        set grub_prefix directory [default=%s]\n\
  -m, --memdisk=FILE      embed FILE as a memdisk image\n\
  -c, --config=FILE       embed FILE as boot config\n\
  -o, --output=FILE       output a generated image to FILE [default=stdout]\n\
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
  char *output = NULL;
  char *dir = NULL;
  char *prefix = NULL;
  char *memdisk = NULL;
  char *config = NULL;
  FILE *fp = stdout;

  set_program_name (argv[0]);

  grub_util_init_nls ();

  while (1)
    {
      int c = getopt_long (argc, argv, "d:p:m:c:o:hVv", options, 0);

      if (c == -1)
	break;
      else
	switch (c)
	  {
	  case 'o':
	    if (output)
	      free (output);

	    output = xstrdup (optarg);
	    break;

	  case 'd':
	    if (dir)
	      free (dir);

	    dir = xstrdup (optarg);
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

	  case 'p':
	    if (prefix)
	      free (prefix);

	    prefix = xstrdup (optarg);
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

  if (output)
    {
      fp = fopen (output, "wb");
      if (! fp)
	grub_util_error (_("cannot open %s"), output);
      free (output);
    }

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

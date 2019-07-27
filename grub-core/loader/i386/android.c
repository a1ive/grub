/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2006,2007,2008,2009,2010,2019  Free Software Foundation, Inc.
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

#include <grub/loader.h>
#include <grub/memory.h>
#include <grub/normal.h>
#include <grub/file.h>
#include <grub/disk.h>
#include <grub/err.h>
#include <grub/misc.h>
#include <grub/types.h>
#include <grub/dl.h>
#include <grub/mm.h>
#include <grub/term.h>
#include <grub/cpu/linux.h>
#include <grub/video.h>
#include <grub/video_fb.h>
#include <grub/command.h>
#include <grub/i386/relocator.h>
#include <grub/i386/android.h>
#include <grub/i18n.h>
#include <grub/lib/cmdline.h>
#include <grub/linux.h>
#include <grub/i386/linux_private.h>

GRUB_MOD_LICENSE ("GPLv3+");

#ifdef GRUB_MACHINE_PCBIOS
#include <grub/i386/pc/vesa_modes_table.h>
#endif

#ifdef GRUB_MACHINE_EFI
#include <grub/efi/efi.h>
#define HAS_VGA_TEXT 0
#define DEFAULT_VIDEO_MODE "auto"
#define ACCEPTS_PURE_TEXT 0
#else
#include <grub/i386/pc/vbe.h>
#include <grub/i386/pc/console.h>
#define HAS_VGA_TEXT 1
#define DEFAULT_VIDEO_MODE "text"
#define ACCEPTS_PURE_TEXT 1
#endif

#define ALIGN(x,ps) (((x) + (ps)-1) & (~((ps)-1)))
#define GRUB_EFI_PAGE_SHIFT	12
#define BYTES_TO_PAGES(bytes)   (((bytes) + 0xfff) >> GRUB_EFI_PAGE_SHIFT)
#define PAGES_TO_BYTES(pages)	((pages) << GRUB_EFI_PAGE_SHIFT)

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

static grub_dl_t my_mod;

static grub_size_t prot_init_space;
static void *prot_mode_mem;
static grub_addr_t prot_mode_target;

static grub_size_t maximal_cmdline_size;
static char *linux_cmdline;

static void *initrd_mem;
static grub_addr_t initrd_mem_target;

static struct linux_kernel_params linux_params;

static struct grub_relocator *relocator = NULL;

static grub_size_t linux_mem_size;

static void *efi_mmap_buf;

#ifdef GRUB_MACHINE_EFI
static grub_efi_uintn_t efi_mmap_size;
#else
static const grub_size_t efi_mmap_size = 0;
#endif

struct source;
typedef grub_err_t (source_read) (struct source * src, grub_off_t offset, grub_size_t len, void *buf);
typedef grub_err_t (source_free) (struct source * src);
struct source
{
    source_read *read;
    source_free *free;
    grub_size_t size;
    void *priv;
};

static grub_err_t
disk_read (struct source *src, grub_off_t offset, grub_size_t len, void *buf)
{
    grub_disk_t disk = (grub_disk_t) src->priv;
    if (grub_disk_read (disk, 0, offset, len, buf))
    {
        if (!grub_errno)
            grub_error (GRUB_ERR_BAD_OS, N_("premature end of disk"));
        return grub_errno;
    }
    return GRUB_ERR_NONE;
}

static grub_err_t
disk_free (struct source *src)
{
    grub_disk_t disk = (grub_disk_t) src->priv;
    grub_disk_close (disk);
    return GRUB_ERR_NONE;
}

static grub_err_t
file_read (struct source *src, grub_off_t offset, grub_size_t len, void *buf)
{
    grub_file_t file = (grub_file_t) src->priv;
    grub_file_seek (file, offset);

    if (grub_file_read (file, buf, len) != (grub_ssize_t) len)
    {
        if (!grub_errno)
            grub_error (GRUB_ERR_BAD_OS, N_("premature end of file"));
        return grub_errno;
    }

    return GRUB_ERR_NONE;
}

static grub_err_t
file_free (struct source *src)
{
    grub_file_t file = (grub_file_t) src->priv;
    grub_file_close (file);
    return GRUB_ERR_NONE;
}

static void
free_pages (void)
{
  grub_relocator_unload (relocator);
  relocator = NULL;
  prot_mode_mem = initrd_mem = 0;
  prot_mode_target = initrd_mem_target = 0;
}

/* Allocate pages for the real mode code and the protected mode code
   for linux as well as a memory map buffer.  */
static grub_err_t
allocate_pages (grub_size_t prot_size, grub_size_t *align,
		grub_size_t min_align, int relocatable,
		grub_uint64_t preferred_address)
{
  grub_err_t err;

  if (prot_size == 0)
    prot_size = 1;

  prot_size = page_align (prot_size);

  /* Initialize the memory pointers with NULL for convenience.  */
  free_pages ();

  relocator = grub_relocator_new ();
  if (!relocator)
    {
      err = grub_errno;
      goto fail;
    }

  /* FIXME: Should request low memory from the heap when this feature is
     implemented.  */

  {
    grub_relocator_chunk_t ch;
    if (relocatable)
      {
	err = grub_relocator_alloc_chunk_align (relocator, &ch,
						preferred_address,
						preferred_address,
						prot_size, 1,
						GRUB_RELOCATOR_PREFERENCE_LOW,
						1);
	for (; err && *align + 1 > min_align; (*align)--)
	  {
	    grub_errno = GRUB_ERR_NONE;
	    err = grub_relocator_alloc_chunk_align (relocator, &ch,
						    0x1000000,
						    0xffffffff & ~prot_size,
						    prot_size, 1 << *align,
						    GRUB_RELOCATOR_PREFERENCE_LOW,
						    1);
	  }
	if (err)
	  goto fail;
      }
    else
      err = grub_relocator_alloc_chunk_addr (relocator, &ch,
					     preferred_address,
					     prot_size);
    if (err)
      goto fail;
    prot_mode_mem = get_virtual_current_address (ch);
    prot_mode_target = get_physical_target_address (ch);
  }

  grub_dprintf ("android", "prot_mode_mem = %p, prot_mode_target = %lx, prot_size = %x\n",
                prot_mode_mem, (unsigned long) prot_mode_target, (unsigned) prot_size);
  return GRUB_ERR_NONE;

 fail:
  free_pages ();
  return err;
}

/* Helper for grub_android_boot.  */
static int
grub_linux_boot_mmap_find (grub_uint64_t addr, grub_uint64_t size,
			   grub_memory_type_t type, void *data)
{
  struct grub_linux_boot_ctx *ctx = data;

  /* We must put real mode code in the traditional space.  */
  if (type != GRUB_MEMORY_AVAILABLE || addr > 0x90000)
    return 0;

  if (addr + size < 0x10000)
    return 0;

  if (addr < 0x10000)
    {
      size += addr - 0x10000;
      addr = 0x10000;
    }

  if (addr + size > 0x90000)
    size = 0x90000 - addr;

  if (ctx->real_size + efi_mmap_size > size)
    return 0;

  grub_dprintf ("android", "addr = %lx, size = %x, need_size = %x\n",
		(unsigned long) addr,
		(unsigned) size,
		(unsigned) (ctx->real_size + efi_mmap_size));
  ctx->real_mode_target = ((addr + size) - (ctx->real_size + efi_mmap_size));
  return 1;
}

static grub_err_t
grub_android_boot (void)
{
    grub_err_t err = 0;
    const char *modevar;
    char *tmp;
    struct grub_relocator32_state state;
    void *real_mode_mem;
    struct grub_linux_boot_ctx ctx = {
    .real_mode_target = 0
    };
    grub_size_t mmap_size;
    grub_size_t cl_offset;

    modevar = grub_env_get ("gfxpayload");

    /* Now all graphical modes are acceptable.
    May change in future if we have modes without framebuffer.  */
    if (modevar && *modevar != 0)
    {
        tmp = grub_xasprintf ("%s;" DEFAULT_VIDEO_MODE, modevar);
        if (! tmp)
        return grub_errno;
#if ACCEPTS_PURE_TEXT
        err = grub_video_set_mode (tmp, 0, 0);
#else
        err = grub_video_set_mode (tmp, GRUB_VIDEO_MODE_TYPE_PURE_TEXT, 0);
#endif
        grub_free (tmp);
    }
    else
    {
#if ACCEPTS_PURE_TEXT
        err = grub_video_set_mode (DEFAULT_VIDEO_MODE, 0, 0);
#else
        err = grub_video_set_mode (DEFAULT_VIDEO_MODE,
        GRUB_VIDEO_MODE_TYPE_PURE_TEXT, 0);
#endif
    }

    if (err)
    {
        grub_print_error ();
        grub_puts_ (N_("Booting in blind mode"));
        grub_errno = GRUB_ERR_NONE;
    }

    if (grub_linux_setup_video (&linux_params))
    {
#if defined (GRUB_MACHINE_PCBIOS) || defined (GRUB_MACHINE_QEMU)
        linux_params.have_vga = GRUB_VIDEO_LINUX_TYPE_TEXT;
        linux_params.video_mode = 0x3;
#else
        linux_params.have_vga = 0;
        linux_params.video_mode = 0;
        linux_params.video_width = 0;
        linux_params.video_height = 0;
#endif
    }

    {
        grub_term_output_t term;
        int found = 0;
        FOR_ACTIVE_TERM_OUTPUTS(term)
        if (grub_strcmp (term->name, "vga_text") == 0
            || grub_strcmp (term->name, "console") == 0
            || grub_strcmp (term->name, "ofconsole") == 0)
            {
                struct grub_term_coordinate pos = grub_term_getxy (term);
                linux_params.video_cursor_x = pos.x;
                linux_params.video_cursor_y = pos.y;
                linux_params.video_width = grub_term_width (term);
                linux_params.video_height = grub_term_height (term);
                found = 1;
                break;
            }
            if (!found)
            {
                linux_params.video_cursor_x = 0;
                linux_params.video_cursor_y = 0;
                linux_params.video_width = 80;
                linux_params.video_height = 25;
            }
    }

    mmap_size = find_mmap_size ();
    /* Make sure that each size is aligned to a page boundary.  */
    cl_offset = ALIGN_UP (mmap_size + sizeof (linux_params), 4096);

    if (cl_offset < ((grub_size_t) linux_params.setup_sects << GRUB_DISK_SECTOR_BITS))
        cl_offset = ALIGN_UP ((grub_size_t) (linux_params.setup_sects << GRUB_DISK_SECTOR_BITS), 4096);

    ctx.real_size = ALIGN_UP (cl_offset + maximal_cmdline_size, 4096);

#ifdef GRUB_MACHINE_EFI
    efi_mmap_size = grub_efi_find_mmap_size ();
    if (efi_mmap_size == 0)
        return grub_errno;
#endif

    grub_dprintf ("linux", "real_size = %x, mmap_size = %x\n",
    (unsigned) ctx.real_size, (unsigned) mmap_size);

#ifdef GRUB_MACHINE_EFI
    grub_efi_mmap_iterate (grub_linux_boot_mmap_find, &ctx, 1);
    if (! ctx.real_mode_target)
        grub_efi_mmap_iterate (grub_linux_boot_mmap_find, &ctx, 0);
#else
    grub_mmap_iterate (grub_linux_boot_mmap_find, &ctx);
#endif
    grub_dprintf ("linux", "real_mode_target = %lx, real_size = %x, efi_mmap_size = %x\n",
    (unsigned long) ctx.real_mode_target,
    (unsigned) ctx.real_size,
    (unsigned) efi_mmap_size);

    if (! ctx.real_mode_target)
        return grub_error (GRUB_ERR_OUT_OF_MEMORY, "cannot allocate real mode pages");

    {
        grub_relocator_chunk_t ch;

        err = grub_relocator_alloc_chunk_addr (
            relocator,
            &ch,
            ctx.real_mode_target,
            (ctx.real_size + efi_mmap_size)
        );

        if (err)
            return err;
        real_mode_mem = get_virtual_current_address (ch);
    }

    efi_mmap_buf = (grub_uint8_t *) real_mode_mem + ctx.real_size;

    grub_dprintf ("linux", "real_mode_mem = %p\n", real_mode_mem);

    ctx.params = real_mode_mem;

    *ctx.params = linux_params;
    ctx.params->cmd_line_ptr = ctx.real_mode_target + cl_offset;
    grub_memcpy ((char *) ctx.params + cl_offset, linux_cmdline, maximal_cmdline_size);

    grub_dprintf ("linux", "code32_start = %x\n",(unsigned) ctx.params->code32_start);

    ctx.e820_num = 0;
    if (grub_mmap_iterate (grub_linux_boot_mmap_fill, &ctx))
        return grub_errno;
    ctx.params->mmap_size = ctx.e820_num;

#ifdef GRUB_MACHINE_EFI
    {
        grub_efi_uintn_t efi_desc_size;
        grub_size_t efi_mmap_target;
        grub_efi_uint32_t efi_desc_version;
        err = grub_efi_finish_boot_services (&efi_mmap_size, efi_mmap_buf, NULL, &efi_desc_size, &efi_desc_version);
        if (err)
            return err;

        /* Note that no boot services are available from here.  */
        efi_mmap_target = ctx.real_mode_target
        + ((grub_uint8_t *) efi_mmap_buf - (grub_uint8_t *) real_mode_mem);
        /* Pass EFI parameters.  */
        if (grub_le_to_cpu16 (ctx.params->version) >= 0x0208)
        {
            ctx.params->v0208.efi_mem_desc_size = efi_desc_size;
            ctx.params->v0208.efi_mem_desc_version = efi_desc_version;
            ctx.params->v0208.efi_mmap = efi_mmap_target;
            ctx.params->v0208.efi_mmap_size = efi_mmap_size;

    #ifdef __x86_64__
            ctx.params->v0208.efi_mmap_hi = (efi_mmap_target >> 32);
    #endif
        }
        else if (grub_le_to_cpu16 (ctx.params->version) >= 0x0206)
        {
            ctx.params->v0206.efi_mem_desc_size = efi_desc_size;
            ctx.params->v0206.efi_mem_desc_version = efi_desc_version;
            ctx.params->v0206.efi_mmap = efi_mmap_target;
            ctx.params->v0206.efi_mmap_size = efi_mmap_size;
        }
        else if (grub_le_to_cpu16 (ctx.params->version) >= 0x0204)
        {
            ctx.params->v0204.efi_mem_desc_size = efi_desc_size;
            ctx.params->v0204.efi_mem_desc_version = efi_desc_version;
            ctx.params->v0204.efi_mmap = efi_mmap_target;
            ctx.params->v0204.efi_mmap_size = efi_mmap_size;
        }
    }
#endif

    /* FIXME.  */
    /*  asm volatile ("lidt %0" : : "m" (idt_desc)); */
    state.ebp = state.edi = state.ebx = 0;
    state.esi = ctx.real_mode_target;
    state.esp = ctx.real_mode_target;
    state.eip = ctx.params->code32_start;
    return grub_relocator32_boot (relocator, state, 0);
}

static grub_err_t
grub_android_unload (void)
{
  grub_dl_unref (my_mod);
  grub_free (linux_cmdline);
  linux_cmdline = 0;
  return GRUB_ERR_NONE;
}

static grub_err_t
grub_android_load (struct source *src, int argc, char *argv[])
{
    struct linux_i386_kernel_header lh;
    boot_img_hdr *hdr;

    grub_size_t offset = 0;

    grub_uint8_t setup_sects;
    grub_size_t real_size, prot_size, prot_file_size;
    grub_size_t len;
    int i;
    grub_size_t align, min_align;
    int relocatable;
    grub_uint64_t preferred_address = GRUB_LINUX_BZIMAGE_ADDR;

    grub_dprintf ("android", "Loading android\n");

    // read header
    hdr = (boot_img_hdr *) grub_malloc (sizeof (boot_img_hdr) + 100);
    if (!hdr)
    {
        grub_dprintf ("android", "alloc mem for boot_img_hdr error\n");
        goto err_out;
    }

    if (src->read (src, 0, sizeof (boot_img_hdr) + 100, hdr))
    {
        grub_dprintf ("android", "read boot_img_hdr error\n");
        goto err_free_hdr;
    }

    // check magic
    if (grub_memcmp (hdr->magic, BOOT_MAGIC, BOOT_MAGIC_SIZE))
    {
        grub_dprintf ("android", "BOOT_MAGIC error\n");
        grub_error (GRUB_ERR_BAD_ARGUMENT, N_("Invalid magic in boot header"));
        goto err_free_hdr;
    }

    // offset + one page size
    offset += ALIGN (sizeof (boot_img_hdr), hdr->page_size);

    // now here is bzImage start
    // bzImage = 512 bytes + setup sects + kernel execute file

    if (src->read(src,offset,sizeof (lh),&lh))
    {
        if (!grub_errno)
            grub_error (GRUB_ERR_BAD_OS, N_("premature end of file %s"),argv[0]);
        goto err_free_hdr;
    }

    offset += sizeof (lh);

    if (lh.boot_flag != grub_cpu_to_le16_compile_time (0xaa55))
    {
        grub_error (GRUB_ERR_BAD_OS, "invalid magic number");
        goto err_free_hdr;
    }

    if (lh.setup_sects > GRUB_LINUX_MAX_SETUP_SECTS)
    {
        grub_error (GRUB_ERR_BAD_OS, "too many setup sectors");
        goto err_free_hdr;
    }

    /* FIXME: 2.03 is not always good enough (Linux 2.4 can be 2.03 and
    still not support 32-bit boot.  */
    if (lh.header != grub_cpu_to_le32_compile_time (GRUB_LINUX_I386_MAGIC_SIGNATURE)
    || grub_le_to_cpu16 (lh.version) < 0x0203)
    {
        grub_error (GRUB_ERR_BAD_OS, "version too old for 32-bit boot"
        #ifdef GRUB_MACHINE_PCBIOS
        " (try with `linux16')"
        #endif
        );
        goto err_free_hdr;
    }

    if (! (lh.loadflags & GRUB_LINUX_FLAG_BIG_KERNEL))
    {
        grub_error (GRUB_ERR_BAD_OS, "zImage doesn't support 32-bit boot"
        #ifdef GRUB_MACHINE_PCBIOS
        " (try with `linux16')"
        #endif
        );
        goto err_free_hdr;
    }

    if (grub_le_to_cpu16 (lh.version) >= 0x0206)
        maximal_cmdline_size = grub_le_to_cpu32 (lh.cmdline_size) + 1;
    else
        maximal_cmdline_size = 256;

    if (maximal_cmdline_size < 128)
        maximal_cmdline_size = 128;

    setup_sects = lh.setup_sects;

    /* If SETUP_SECTS is not set, set it to the default (4).  */
    if (! setup_sects)
        setup_sects = GRUB_LINUX_DEFAULT_SETUP_SECTS;

    real_size = setup_sects << GRUB_DISK_SECTOR_BITS; // real_size = 0x1f * 512 = 31 * 512 = 15872 = 0x3e00
    prot_file_size = hdr->kernel_size - real_size - GRUB_DISK_SECTOR_SIZE; //prot_file_size = kernel_size - real_size - 512 = kernel_size - 0x3e00 - 0x200

    if (grub_le_to_cpu16 (lh.version) >= 0x205
    && lh.kernel_alignment != 0
    && ((lh.kernel_alignment - 1) & lh.kernel_alignment) == 0)
    {
      for (align = 0; align < 32; align++)
	if (grub_le_to_cpu32 (lh.kernel_alignment) & (1 << align))
	  break;
      relocatable = grub_le_to_cpu32 (lh.relocatable);
    }
    else
    {
        align = 0;
        relocatable = 0;
    }

    if (grub_le_to_cpu16 (lh.version) >= 0x020a)
    {
        min_align = lh.min_alignment;
        prot_size = grub_le_to_cpu32 (lh.init_size);
        prot_init_space = page_align (prot_size);
        if (relocatable)
            preferred_address = grub_le_to_cpu64 (lh.pref_address);
        else
            preferred_address = GRUB_LINUX_BZIMAGE_ADDR;
    }
    else
    {
        min_align = align;
        prot_size = prot_file_size;
        preferred_address = GRUB_LINUX_BZIMAGE_ADDR;
        /* Usually, the compression ratio is about 50%.  */
        prot_init_space = page_align (prot_size) * 3;
    }

    if (allocate_pages (
        prot_size,
        &align,
        min_align,
        relocatable,
        preferred_address))
    {
        grub_dprintf ("android", "allocate_pages error\n");
        goto err_free_hdr;
    }

    grub_memset (&linux_params, 0, sizeof (linux_params));
    grub_memcpy (&linux_params.setup_sects, &lh.setup_sects, sizeof (lh) - 0x1F1);

    linux_params.code32_start = prot_mode_target + lh.code32_start - GRUB_LINUX_BZIMAGE_ADDR;
    linux_params.kernel_alignment = (1 << align);
    linux_params.ps_mouse = linux_params.padding10 =  0;

    len = sizeof (linux_params) - sizeof (lh);
    if (src->read(src,offset,len,(char *) &linux_params + sizeof (lh)))
    {
        grub_dprintf ("android", "read params error\n");
        if (!grub_errno)
            grub_error (GRUB_ERR_BAD_OS, N_("premature end of file %s"),argv[0]);
        goto err_free_hdr;
    }

    linux_params.type_of_loader = GRUB_LINUX_BOOT_LOADER_TYPE;

    /* These two are used (instead of cmd_line_ptr) by older versions of Linux,
    and otherwise ignored.  */
    linux_params.cl_magic = GRUB_LINUX_CL_MAGIC;
    linux_params.cl_offset = 0x1000;

    linux_params.ramdisk_image = 0;
    linux_params.ramdisk_size = 0;

    linux_params.heap_end_ptr = GRUB_LINUX_HEAP_END_OFFSET;
    linux_params.loadflags |= GRUB_LINUX_FLAG_CAN_USE_HEAP;

    /* These are not needed to be precise, because Linux uses these values
    only to raise an error when the decompression code cannot find good
    space.  */
    linux_params.ext_mem = ((32 * 0x100000) >> 10);
    linux_params.alt_mem = ((32 * 0x100000) >> 10);

    /* Ignored by Linux.  */
    linux_params.video_page = 0;

    /* Only used when `video_mode == 0x7', otherwise ignored.  */
    linux_params.video_ega_bx = 0;

    linux_params.font_size = 16; /* XXX */

#ifdef GRUB_MACHINE_EFI
    #ifdef __x86_64__
    if (grub_le_to_cpu16 (linux_params.version) < 0x0208 && ((grub_addr_t) grub_efi_system_table >> 32) != 0)
        return grub_error(GRUB_ERR_BAD_OS, "kernel does not support 64-bit addressing");
    #endif

    if (grub_le_to_cpu16 (linux_params.version) >= 0x0208)
    {
        linux_params.v0208.efi_signature = GRUB_LINUX_EFI_SIGNATURE;
        linux_params.v0208.efi_system_table = (grub_uint32_t) (grub_addr_t) grub_efi_system_table;
    #ifdef __x86_64__
        linux_params.v0208.efi_system_table_hi = (grub_uint32_t) ((grub_uint64_t) grub_efi_system_table >> 32);
    #endif
    }
    else if (grub_le_to_cpu16 (linux_params.version) >= 0x0206)
    {
        linux_params.v0206.efi_signature = GRUB_LINUX_EFI_SIGNATURE;
        linux_params.v0206.efi_system_table = (grub_uint32_t) (grub_addr_t) grub_efi_system_table;
    }
    else if (grub_le_to_cpu16 (linux_params.version) >= 0x0204)
    {
        linux_params.v0204.efi_signature = GRUB_LINUX_EFI_SIGNATURE_0204;
        linux_params.v0204.efi_system_table = (grub_uint32_t) (grub_addr_t) grub_efi_system_table;
    }
#endif

    /* The other parameters are filled when booting.  */

    // position kernel start
    offset = ALIGN (sizeof (boot_img_hdr), hdr->page_size);

    // skip first 512 byte and setup data
    offset += GRUB_DISK_SECTOR_SIZE + real_size;

    /* Look for memory size and video mode specified on the command line.  */
    linux_mem_size = 0;
    for (i = 1; i < argc; i++)
#ifdef GRUB_MACHINE_PCBIOS
        if (grub_memcmp (argv[i], "vga=", 4) == 0)
        {
            /* Video mode selection support.  */
            char *val = argv[i] + 4;
            unsigned vid_mode = GRUB_LINUX_VID_MODE_NORMAL;
            struct grub_vesa_mode_table_entry *linux_mode;
            grub_err_t err;
            char *buf;

            grub_dl_load ("vbe");

            if (grub_strcmp (val, "normal") == 0)
                vid_mode = GRUB_LINUX_VID_MODE_NORMAL;
            else if (grub_strcmp (val, "ext") == 0)
                vid_mode = GRUB_LINUX_VID_MODE_EXTENDED;
            else if (grub_strcmp (val, "ask") == 0)
            {
                grub_puts_ (N_("Legacy `ask' parameter no longer supported."));

                /* We usually would never do this in a loader, but "vga=ask" means user
                requested interaction, so it can't hurt to request keyboard input.  */
                grub_wait_after_message ();

                goto err_free_hdr;
            }
            else
                vid_mode = (grub_uint16_t) grub_strtoul (val, 0, 0);

            switch (vid_mode)
            {
            case 0:
            case GRUB_LINUX_VID_MODE_NORMAL:
                grub_env_set ("gfxpayload", "text");
                grub_printf_ (N_("%s is deprecated. "
                "Use set gfxpayload=%s before "
                "linux command instead.\n"), "text",
                argv[i]);
            break;

            case 1:
            case GRUB_LINUX_VID_MODE_EXTENDED:
                /* FIXME: support 80x50 text. */
                grub_env_set ("gfxpayload", "text");
                grub_printf_ (N_("%s is deprecated. "
                "Use set gfxpayload=%s before "
                "linux command instead.\n"), "text",
                argv[i]);
            break;
            default:
                /* Ignore invalid values.  */
                if (vid_mode < GRUB_VESA_MODE_TABLE_START || vid_mode > GRUB_VESA_MODE_TABLE_END)
                {
                    grub_env_set ("gfxpayload", "text");
                    /* TRANSLATORS: "x" has to be entered in, like an identifier,
                    so please don't use better Unicode codepoints.  */
                    grub_printf_ (N_("%s is deprecated. VGA mode %d isn't recognized. "
                     "Use set gfxpayload=WIDTHxHEIGHT[xDEPTH] "
                     "before linux command instead.\n"),
                     argv[i], vid_mode);
                    break;
                }

                linux_mode = &grub_vesa_mode_table[vid_mode - GRUB_VESA_MODE_TABLE_START];

                buf = grub_xasprintf ("%ux%ux%u,%ux%u",
                 linux_mode->width, linux_mode->height,
                 linux_mode->depth,
                 linux_mode->width, linux_mode->height);
                if (! buf)
                    goto err_free_hdr;

                grub_printf_ (N_("%s is deprecated. "
                 "Use set gfxpayload=%s before "
                 "linux command instead.\n"),
                argv[i], buf);

                err = grub_env_set ("gfxpayload", buf);
                grub_free (buf);
                if (err)
                    goto err_free_hdr;
            }
        }
        else
#endif /* GRUB_MACHINE_PCBIOS */
        if (grub_memcmp (argv[i], "mem=", 4) == 0)
        {
            char *val = argv[i] + 4;

            linux_mem_size = grub_strtoul (val, &val, 0);

            if (grub_errno)
            {
                grub_errno = GRUB_ERR_NONE;
                linux_mem_size = 0;
            }
            else
            {
                int shift = 0;

                switch (grub_tolower (val[0]))
                {
                case 'g':
                    shift += 10;
                /* FALLTHROUGH */
                case 'm':
                    shift += 10;
                /* FALLTHROUGH */
                case 'k':
                    shift += 10;
                /* FALLTHROUGH */
                default:
                break;
                }

                /* Check an overflow.  */
                if (linux_mem_size > (~0UL >> shift))
                    linux_mem_size = 0;
                else
                    linux_mem_size <<= shift;
            }
        }
        else if (grub_memcmp (argv[i], "quiet", sizeof ("quiet") - 1) == 0)
        {
            linux_params.loadflags |= GRUB_LINUX_FLAG_QUIET;
        }
    /* end argc for*/

    /* Create kernel command line.  */
    linux_cmdline = grub_zalloc (maximal_cmdline_size + 1);
    if (!linux_cmdline)
    {
        grub_dprintf ("android", "linux_cmdline error\n");
        goto err_free_hdr;
    }

    // copy cmdline
    grub_memcpy (linux_cmdline, hdr->cmdline, BOOT_ARGS_SIZE - 1);

    // copy extra cmdline if maximal_cmdline_size perfect and extra_cmdline has value
    if (hdr->cmdline[BOOT_ARGS_SIZE - 2] && (maximal_cmdline_size <= (BOOT_ARGS_SIZE + BOOT_EXTRA_ARGS_SIZE)))
    {
        grub_memcpy(linux_cmdline + (BOOT_ARGS_SIZE - 1),hdr->extra_cmdline,BOOT_EXTRA_ARGS_SIZE);
    }

    len = prot_file_size;
    if (src->read(src,offset,len,(char *) prot_mode_mem))
    {
        grub_dprintf ("android", "read prot error\n");
        grub_error (GRUB_ERR_BAD_OS, N_("premature end of file %s"),argv[0]);
    }

    // offset point to ramdisk ,skip first page,skip all kernel pages
    offset = ALIGN (sizeof (boot_img_hdr), hdr->page_size);
    offset += ALIGN (hdr->kernel_size, hdr->page_size);

    grub_size_t size = 0, aligned_size = 0;
    grub_addr_t addr_min, addr_max;
    grub_addr_t addr;
    grub_err_t err;

    // ramdisk size
    size = ALIGN (hdr->ramdisk_size, hdr->page_size);

    // paged ramdisk size
    aligned_size = ALIGN_UP (size, 4096);


    /* Get the highest address available for the initrd.  */
    if (grub_le_to_cpu16 (linux_params.version) >= 0x0203)
    {
        addr_max = grub_cpu_to_le32 (linux_params.initrd_addr_max);

        /* XXX in reality, Linux specifies a bogus value, so
        it is necessary to make sure that ADDR_MAX does not exceed
        0x3fffffff.  */
        if (addr_max > GRUB_LINUX_INITRD_MAX_ADDRESS)
            addr_max = GRUB_LINUX_INITRD_MAX_ADDRESS;
    }
    else
        addr_max = GRUB_LINUX_INITRD_MAX_ADDRESS;

    if (linux_mem_size != 0 && linux_mem_size < addr_max)
        addr_max = linux_mem_size;

    /* Linux 2.3.xx has a bug in the memory range check, so avoid
    the last page.
    Linux 2.2.xx has a bug in the memory range check, which is
    worse than that of Linux 2.3.xx, so avoid the last 64kb.  */
    addr_max -= 0x10000;

    addr_min = (grub_addr_t) prot_mode_target + prot_init_space;

    /* Put the initrd as high as possible, 4KiB aligned.  */
    addr = (addr_max - aligned_size) & ~0xFFF;

    if (addr < addr_min)
    {
        grub_error (GRUB_ERR_OUT_OF_RANGE, "the initrd is too big");
        goto err_free_hdr;
    }

    {
        grub_relocator_chunk_t ch;
        err = grub_relocator_alloc_chunk_align (
            relocator,
            &ch,
            addr_min,
            addr,
            aligned_size,
            0x1000,
            GRUB_RELOCATOR_PREFERENCE_HIGH,
            1);
        if (err)
        {
            grub_error (GRUB_ERR_BAD_OS, N_("grub_relocator_alloc_chunk_align error"));
            goto err_free_hdr;
        }

        initrd_mem = get_virtual_current_address (ch);
        initrd_mem_target = get_physical_target_address (ch);
    }

    if (src->read(src,offset,size,initrd_mem))
    {
        grub_dprintf ("android", "read ramdisk error\n");
        grub_error (GRUB_ERR_BAD_OS, N_("premature end of file %s"),argv[0]);
        goto err_free_hdr;
    }

    grub_dprintf ("android", "Initrd, addr=0x%x, size=0x%x\n", (unsigned) addr, (unsigned) size);

    linux_params.ramdisk_image = initrd_mem_target;
    linux_params.ramdisk_size = size;
    linux_params.root_dev = 0x0100; /* XXX */

err_free_hdr:
    grub_free (hdr);
err_out:
    grub_dprintf ("android","grub_errno=%d\n",grub_errno);
    if (grub_errno)
        grub_error (GRUB_ERR_BUG, N_("%s: Unknown error."), __func__);
    return grub_errno;
}



static grub_err_t
grub_cmd_android (grub_command_t cmd __attribute__ ((unused)), int argc, char *argv[])
{
    grub_dl_ref (my_mod);
    struct source src;
    int namelen;

    if (argc <= 0)
    {
        grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected %s"),argv[0]);
        goto dl_unref;
    }

    namelen = grub_strlen (argv[0]);
    if ((argv[0][0] == '(') && (argv[0][namelen - 1] == ')'))
    {
        // open disk
        argv[0][namelen - 1] = 0;
        grub_disk_t disk = grub_disk_open (&argv[0][1]);
        if (!disk)
            goto dl_unref;
        src.read = &disk_read;
        src.free = &disk_free;
        src.size = grub_disk_get_size (disk) * GRUB_DISK_SECTOR_SIZE;
        grub_dprintf ("android", "device size = %zd\n",src.size);
        src.priv = disk;
    }
    else
    {
        // open file
        grub_file_t file = grub_file_open (argv[0], GRUB_FILE_TYPE_LINUX_KERNEL);
        if (!file)
            goto dl_unref;
        src.read = &file_read;
        src.free = &file_free;
        src.size = grub_file_size (file);
        grub_dprintf ("android", "file size = %zd\n",src.size);
        src.priv = file;
    }

    // load android image
    if (grub_android_load(&src, argc, argv))
    {
        goto source_free;
    }

    // close source
    src.free (&src);

    if (grub_errno == GRUB_ERR_NONE)
    {
        grub_loader_set (grub_android_boot, grub_android_unload,
        0 /* set noreturn=0 in order to avoid grub_console_fini() */);
    }
    return GRUB_ERR_NONE;
source_free:
    src.free (&src);
dl_unref:
    grub_dl_unref (my_mod);
    if (!grub_errno)
        grub_error (GRUB_ERR_BUG, N_("%s: Unknown error."), __func__);
    return grub_errno;
}


static grub_command_t cmd_android;

GRUB_MOD_INIT(android)
{
    cmd_android = grub_register_command ("android", grub_cmd_android,
                        0, N_("Load android."));
    my_mod = mod;
}

GRUB_MOD_FINI(android)
{
    grub_unregister_command (cmd_android);
}

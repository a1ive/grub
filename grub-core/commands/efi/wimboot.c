/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2019  Free Software Foundation, Inc.
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

#include <grub/file.h>
#include <grub/err.h>
#include <grub/env.h>
#include <grub/device.h>
#include <grub/disk.h>
#include <grub/misc.h>
#include <grub/types.h>
#include <grub/term.h>
#include <grub/dl.h>
#include <grub/extcmd.h>
#include <grub/mm.h>
#include <grub/efi/efi.h>
#include <grub/efi/api.h>
#include <grub/i386/efi/wimboot.h>

GRUB_MOD_LICENSE ("GPLv3+");

  /* wimboot @:boot.wim:/test.wim @:bootx64.efi:/test.efi @:bcd:/bcd
   * params:
   *         --gui     -g
   *         --rawbcd  -b
   *         --rawwim  -w
   *         --pause   -p
   *         --index   -i  --index=n
   */

static const struct grub_arg_option options_wimboot[] = {
  {"gui", 'g', 0, N_("Display graphical boot messages."), 0, 0},
  {"rawbcd", 'b', 0, N_("Disable rewriting .exe to .efi in the BCD file."), 0, 0},
  {"rawwim", 'w', 0, N_("Disable patching the wim file."), 0, 0},
  {"index", 'i', 0, N_("Use WIM image index n."), N_("n"), ARG_TYPE_INT},
  {"pause", 'p', 0, N_("Show info and wait for keypress."), 0, 0},
  {"inject", 'j', 0, N_("Set inject dir."), N_("PATH"), ARG_TYPE_STRING},
  {0, 0, 0, 0, 0, 0}
};

enum options_wimboot
{
  WIMBOOT_GUI,
  WIMBOOT_RAWBCD,
  WIMBOOT_RAWWIM,
  WIMBOOT_INDEX,
  WIMBOOT_PAUSE,
  WIMBOOT_INJECT
};

struct newc_head
{
  char magic[6];
  char ino[8];
  char mode[8];
  char uid[8];
  char gid[8];
  char nlink[8];
  char mtime[8];
  char filesize[8];
  char devmajor[8];
  char devminor[8];
  char rdevmajor[8];
  char rdevminor[8];
  char namesize[8];
  char check[8];
} GRUB_PACKED;

struct dir
{
  char *name;
  struct dir *next;
  struct dir *child;
};

static char
hex (grub_uint8_t val)
{
  if (val < 10)
    return '0' + val;
  return 'a' + val - 10;
}

static void
set_field (char *var, grub_uint32_t val)
{
  int i;
  char *ptr = var;
  for (i = 28; i >= 0; i -= 4)
    *ptr++ = hex((val >> i) & 0xf);
}

static grub_uint8_t *
make_header (grub_uint8_t *ptr,
	     const char *name, grub_size_t len,
	     grub_uint32_t mode,
	     grub_off_t fsize)
{
  struct newc_head *head = (struct newc_head *) ptr;
  grub_uint8_t *optr;
  grub_size_t oh = 0;
  grub_memcpy (head->magic, "070701", 6);
  set_field (head->ino, 0);
  set_field (head->mode, mode);
  set_field (head->uid, 0);
  set_field (head->gid, 0);
  set_field (head->nlink, 1);
  set_field (head->mtime, 0);
  set_field (head->filesize, fsize);
  set_field (head->devmajor, 0);
  set_field (head->devminor, 0);
  set_field (head->rdevmajor, 0);
  set_field (head->rdevminor, 0);
  set_field (head->namesize, len);
  set_field (head->check, 0);
  optr = ptr;
  ptr += sizeof (struct newc_head);
  grub_memcpy (ptr, name, len);
  ptr += len;
  oh = ALIGN_UP_OVERHEAD (ptr - optr, 4);
  grub_memset (ptr, 0, oh);
  ptr += oh;
  return ptr;
}

static void
free_dir (struct dir *root)
{
  if (!root)
    return;
  free_dir (root->next);
  free_dir (root->child);
  grub_free (root->name);
  grub_free (root);
}

static grub_size_t
insert_dir (const char *name, struct dir **root,
	    grub_uint8_t *ptr)
{
  struct dir *cur, **head = root;
  const char *cb, *ce = name;
  grub_size_t size = 0;
  while (1)
    {
      for (cb = ce; *cb == '/'; cb++);
      for (ce = cb; *ce && *ce != '/'; ce++);
      if (!*ce)
	break;

      for (cur = *root; cur; cur = cur->next)
	if (grub_memcmp (cur->name, cb, ce - cb)
	    && cur->name[ce - cb] == 0)
	  break;
      if (!cur)
	{
	  struct dir *n;
	  n = grub_zalloc (sizeof (*n));
	  if (!n)
	    return 0;
	  n->next = *head;
	  n->name = grub_strndup (cb, ce - cb);
	  if (ptr)
	    {
	      grub_printf ("...creating directory %s, %s\n", name, ce);
	      ptr = make_header (ptr, name, ce - name,
				 040777, 0);
	    }
	  size += ALIGN_UP ((ce - (char *) name)
			    + sizeof (struct newc_head), 4);
	  *head = n;
	  cur = n;
	}
      root = &cur->next;
    }
  return size;
}

static void
grub_wimboot_close (struct grub_wimboot_context *wimboot_ctx)
{
  int i;
  if (!wimboot_ctx->components)
    return;
  for (i = 0; i < wimboot_ctx->nfiles; i++)
    {
      grub_free (wimboot_ctx->components[i].file_name);
      grub_file_close (wimboot_ctx->components[i].file);
    }
  grub_free (wimboot_ctx->components);
  wimboot_ctx->components = 0;
}

static grub_err_t
grub_wimboot_init (int argc, char *argv[],
          struct grub_wimboot_context *wimboot_ctx)
{
  int i;
  int newc = 0;
  struct dir *root = 0;

  wimboot_ctx->nfiles = 0;
  wimboot_ctx->components = 0;

  wimboot_ctx->components = grub_zalloc (argc
                    * sizeof (wimboot_ctx->components[0]));
  if (!wimboot_ctx->components)
    return grub_errno;

  wimboot_ctx->size = 0;

  for (i = 0; i < argc; i++)
    {
      const char *fname = argv[i];
      
      wimboot_ctx->size = ALIGN_UP (wimboot_ctx->size, 4);

      if (grub_memcmp (argv[i], "@:", 2) == 0)
    {
      const char *ptr, *eptr;
      ptr = argv[i] + 2;
      while (*ptr == '/')
        ptr++;
      eptr = grub_strchr (ptr, ':');
      if (eptr)
        {
          wimboot_ctx->components[i].file_name = grub_strndup (ptr, eptr - ptr);
          if (!wimboot_ctx->components[i].file_name)
        {
          grub_wimboot_close (wimboot_ctx);
          return grub_errno;
        }
          wimboot_ctx->size
		+= ALIGN_UP (sizeof (struct newc_head)
			    + grub_strlen (wimboot_ctx->components[i].file_name),
			     4);
	      wimboot_ctx->size += insert_dir (wimboot_ctx->components[i].file_name,
					      &root, 0);
	      newc = 1;
          fname = eptr + 1;
        }
    }
      else if (newc)
	{
	  wimboot_ctx->size += ALIGN_UP (sizeof (struct newc_head)
					+ sizeof ("TRAILER!!!") - 1, 4);
	  free_dir (root);
	  root = 0;
	  newc = 0;
	}
      wimboot_ctx->components[i].file = grub_file_open (fname,
                               GRUB_FILE_TYPE_LINUX_INITRD
                               | GRUB_FILE_TYPE_NO_DECOMPRESS);
      if (!wimboot_ctx->components[i].file)
    {
      grub_wimboot_close (wimboot_ctx);
      return grub_errno;
    }
      wimboot_ctx->nfiles++;
      wimboot_ctx->components[i].size
    = grub_file_size (wimboot_ctx->components[i].file);
      wimboot_ctx->size += wimboot_ctx->components[i].size;
      grub_printf ("file %d: %s path: %s\n", wimboot_ctx->nfiles, wimboot_ctx->components[i].file_name, fname);
    }
    
  if (newc)
    {
      wimboot_ctx->size = ALIGN_UP (wimboot_ctx->size, 4);
      wimboot_ctx->size += ALIGN_UP (sizeof (struct newc_head)
				    + sizeof ("TRAILER!!!") - 1, 4);
      free_dir (root);
      root = 0;
    }
  
  return GRUB_ERR_NONE;
}

static grub_size_t
grub_get_wimboot_size (struct grub_wimboot_context *wimboot_ctx)
{
  return wimboot_ctx->size;
}

static grub_err_t
grub_wimboot_load (struct grub_wimboot_context *wimboot_ctx,
		  char *argv[], void *target)
{
  grub_uint8_t *ptr = target;
  int i;
  int newc = 0;
  struct dir *root = 0;
  grub_ssize_t cursize = 0;

  for (i = 0; i < wimboot_ctx->nfiles; i++)
    {
      grub_memset (ptr, 0, ALIGN_UP_OVERHEAD (cursize, 4));
      ptr += ALIGN_UP_OVERHEAD (cursize, 4);

      if (wimboot_ctx->components[i].file_name)
	{
	  ptr += insert_dir (wimboot_ctx->components[i].file_name,
			     &root, ptr);
	  ptr = make_header (ptr, wimboot_ctx->components[i].file_name,
			     grub_strlen (wimboot_ctx->components[i].file_name),
			     0100777,
			     wimboot_ctx->components[i].size);
	  newc = 1;
	}
      else if (newc)
	{
	  ptr = make_header (ptr, "TRAILER!!!", sizeof ("TRAILER!!!") - 1,
			     0, 0);
	  free_dir (root);
	  root = 0;
	  newc = 0;
	}

      cursize = wimboot_ctx->components[i].size;
      if (grub_file_read (wimboot_ctx->components[i].file, ptr, cursize)
	  != cursize)
	{
	  if (!grub_errno)
	    grub_error (GRUB_ERR_FILE_READ_ERROR, N_("premature end of file %s"),
			argv[i]);
	  grub_wimboot_close (wimboot_ctx);
	  return grub_errno;
	}
      ptr += cursize;
    }
  if (newc)
    {
      grub_memset (ptr, 0, ALIGN_UP_OVERHEAD (cursize, 4));
      ptr += ALIGN_UP_OVERHEAD (cursize, 4);
      ptr = make_header (ptr, "TRAILER!!!", sizeof ("TRAILER!!!") - 1, 0, 0);
    }
  free_dir (root);
  root = 0;
  return GRUB_ERR_NONE;
}

static grub_err_t
grub_wimboot_boot (int argc, char *argv[])
{
  grub_efi_boot_services_t *b;
  b = grub_efi_system_table->boot_services;

  grub_efi_status_t status;
  void *boot_image = 0;
  grub_efi_char16_t *cmdline;
  grub_efi_physical_address_t address;
  grub_efi_handle_t image_handle;
  grub_efi_uintn_t pages;
  grub_efi_loaded_image_t *loaded_image;

  pages = (((grub_efi_uintn_t) wimboot_bin_len + ((1 << 12) - 1)) >> 12);
  status = efi_call_4 (b->allocate_pages, GRUB_EFI_ALLOCATE_ANY_PAGES,
			      GRUB_EFI_LOADER_CODE,
			      pages, &address);
  if (status != GRUB_EFI_SUCCESS)
    {
      grub_error (GRUB_ERR_OUT_OF_MEMORY, N_("out of memory"));
      goto fail;
    }
  grub_script_execute_sourcecode ("terminal_output console");
  grub_printf (".. load wimboot image\n");

  boot_image = (void *) ((grub_addr_t) address);
  
  grub_memcpy (boot_image, wimboot_bin, wimboot_bin_len);

  status = efi_call_6 (b->load_image, 0, grub_efi_image_handle, NULL,
		       boot_image, wimboot_bin_len,
		       &image_handle);
  if (status != GRUB_EFI_SUCCESS)
    {
      if (status == GRUB_EFI_OUT_OF_RESOURCES)
	grub_error (GRUB_ERR_OUT_OF_MEMORY, "out of resources");
      else
	grub_error (GRUB_ERR_BAD_OS, "cannot load image");

      goto fail;
    }
  loaded_image = grub_efi_get_loaded_image (image_handle);
  if (! loaded_image)
    {
      grub_error (GRUB_ERR_BAD_OS, "no loaded image available");
      goto fail;
    }

  if (argc > 1)
    {
      int i, len;
      grub_efi_char16_t *p16;

      for (i = 1, len = 0; i < argc; i++)
        len += grub_strlen (argv[i]) + 1;

      len *= sizeof (grub_efi_char16_t);
      cmdline = p16 = grub_malloc (len);
      if (! cmdline)
        goto fail;

      for (i = 1; i < argc; i++)
        {
          char *p8;
          grub_printf ("arg[%d] : %s\n", i, argv[i]);
          p8 = argv[i];
          while (*p8)
            *(p16++) = *(p8++);

          *(p16++) = ' ';
        }
      *(--p16) = 0;

      loaded_image->load_options = cmdline;
      loaded_image->load_options_size = len;
      grub_printf ("cmdline len: %d", len);
    }

  efi_call_3 (b->start_image, image_handle, NULL, NULL);
  
  efi_call_1 (b->unload_image, image_handle);
  efi_call_2 (b->free_pages, address, pages);
  grub_free (cmdline);
  grub_error (GRUB_ERR_BAD_OS, "unknown error");
fail:
  return grub_errno;

}

static grub_err_t
grub_cmd_wimboot (grub_extcmd_context_t ctxt,
              int argc, char *argv[])
{
  struct grub_arg_list *state = ctxt->state;
  struct grub_wimboot_context wimboot_ctx = { 0, 0, 0};
  grub_size_t size = 0;
  void *wimboot_mem = NULL;
  grub_efi_status_t status;
  grub_efi_boot_services_t *b;
  const char *progress = grub_env_get ("enable_progress_indicator");

  b = grub_efi_system_table->boot_services;
  
  if (argc == 0)
    {
      grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));
      goto fail;
    }
  
  if (grub_wimboot_init (argc, argv, &wimboot_ctx))
    goto fail;

  size = grub_get_wimboot_size (&wimboot_ctx);
  grub_printf (".. allocate memory\n");

  status = efi_call_3 (b->allocate_pool, GRUB_EFI_RESERVED_MEMORY_TYPE,
			      size, &wimboot_mem);
  
  if (status != GRUB_EFI_SUCCESS)
    {
      grub_error (GRUB_ERR_OUT_OF_MEMORY, N_("out of memory"));
      goto fail;
    }
  grub_env_set ("enable_progress_indicator", "1");
  if (grub_wimboot_load (&wimboot_ctx, argv, wimboot_mem))
    goto fail;

  char **wim_args;
  int i = 3;
  char wim_addr[64];
  char wim_size[64];
  grub_sprintf(wim_addr, "addr=%ld", (unsigned long) wimboot_mem);
  grub_sprintf(wim_size, "size=%ld", (unsigned long) size);
  wim_args = (char **)grub_malloc(9 * sizeof(char **));
  wim_args[0] = (char *)"\\wimboot";
  wim_args[1] = wim_addr;
  wim_args[2] = wim_size;
  
  if (state[WIMBOOT_GUI].set)
  {
    wim_args[i] = (char *)"gui";
    i++;
  }
  if (state[WIMBOOT_RAWBCD].set)
  {
    wim_args[i] = (char *)"rawbcd";
    i++;
  }
  if (state[WIMBOOT_RAWWIM].set)
  {
    wim_args[i] = (char *)"rawwim";
    i++;
  }
  if (state[WIMBOOT_PAUSE].set)
  {
    wim_args[i] = (char *)"pause";
    i++;
  }
  
  if (state[WIMBOOT_INDEX].set)
  {
	wim_args[i] = (char *)"index=";
	grub_strcat(wim_args[i], state[WIMBOOT_INDEX].arg);
    i++;
  }
  
  if (state[WIMBOOT_INJECT].set)
  {
	wim_args[i] = (char *)"inject=";
    grub_strcat(wim_args[i], state[WIMBOOT_INJECT].arg);
    i++;
  }

  grub_wimboot_boot (i, wim_args);
  if (!progress)
    grub_env_unset ("enable_progress_indicator");
  else
    grub_env_set ("enable_progress_indicator", progress);
  grub_free(wim_args);
fail:
  grub_wimboot_close (&wimboot_ctx);
  if (wimboot_mem)
    efi_call_1 (b->free_pool, &wimboot_mem);
  return grub_errno;
}

static grub_extcmd_t cmd;

GRUB_MOD_INIT(wimboot)
{
  cmd = grub_register_extcmd ("wimboot", grub_cmd_wimboot, 0,
    N_("[--rawbcd] [--index=n] [--pause] @:NAME:PATH"),
    N_("Windows Imaging Format bootloader"),
    options_wimboot);
}

GRUB_MOD_FINI(wimboot)
{
  grub_unregister_extcmd (cmd);
}

/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2002,2005,2006,2007,2008,2009  Free Software Foundation, Inc.
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

#ifndef GRUB_KERNEL_HEADER
#define GRUB_KERNEL_HEADER	1

#include <grub/types.h>
#include <grub/symbol.h>

enum
{
  OBJ_TYPE_OBJECT,
  OBJ_TYPE_MEMDISK,
  OBJ_TYPE_CONFIG,
  OBJ_TYPE_FONT
};

/* The module header.  */
struct grub_module_header
{
  /* The type of object.  */
  grub_uint8_t type;
  /* The size of object (including this header).  */
  grub_uint32_t size;
} __attribute__((packed));

struct grub_module_object
{
  grub_uint32_t init_func;
  grub_uint32_t fini_func;
  grub_uint16_t symbol_name;
  grub_uint16_t symbol_value;
  char name[0];
} __attribute__((packed));

/* "gmim" (GRUB Module Info Magic).  */
#define GRUB_MODULE_MAGIC 0x676d696d

struct grub_module_info
{
  /* Magic number so we know we have modules present.  */
  grub_uint32_t magic;
  /* The size of all modules plus this header.  */
  grub_uint32_t size;
} __attribute__((packed));

extern char * grub_arch_menu_addr (void);

extern void grub_module_iterate (int (*hook) (struct grub_module_header *));

grub_addr_t grub_modules_get_end (void);

/* The start point of the C code.  */
void grub_main (void);

/* The machine-specific initialization. This must initialize memory.  */
void grub_machine_init (void);

/* The machine-specific finalization.  */
void grub_machine_fini (void);

/* The machine-specific prefix initialization.  */
void grub_machine_set_prefix (void);

/* Register all the exported symbols. This is automatically generated.  */
void grub_register_exported_symbols (void);

/* These symbols are inserted by grub-mkimage.  */
extern struct grub_module_info grub_modinfo;
extern char grub_bss_start[];
extern char grub_bss_end[];

/* This is defined in startup code.  */
extern char grub_code_start[];

#endif /* ! GRUB_KERNEL_HEADER */

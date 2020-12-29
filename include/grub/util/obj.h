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

#ifndef GRUB_UTIL_OBJ_HEADER
#define GRUB_UTIL_OBJ_HEADER	1

#include <config.h>
#include <grub/types.h>
#include <grub/obj.h>
#include <grub/list.h>
#include <grub/util/resolve.h>

#include <stdio.h>

struct grub_util_obj_segment
{
  struct grub_util_obj_segment *next;
  struct grub_obj_segment segment;
  char *data;
  grub_uint32_t raw_size;
  grub_uint32_t file_off;
  int index;
  char *modname;
  grub_uint32_t vaddr;
  int is_jmptab;
};

struct grub_util_obj_symbol
{
  struct grub_util_obj_symbol *next;
  char *name;
  struct grub_util_obj_segment *segment;
  struct grub_obj_symbol symbol;
  int exported;
};

struct grub_util_obj_reloc
{
  struct grub_util_obj_reloc *next;
  struct grub_util_obj_segment *segment;
  struct grub_obj_reloc reloc;
  struct grub_util_obj_segment *symbol_segment;
  grub_uint32_t symbol_offset;
  char *symbol_name;
};

struct grub_util_obj_csym
{
  struct grub_util_obj_csym *next;
  char *name;
  grub_uint32_t size;
};

struct grub_util_obj_got
{
  struct grub_util_obj_got *next;
  char *name;
  grub_uint32_t offset;
};

struct grub_util_obj
{
  struct grub_util_obj_segment *segments;
  struct grub_util_obj_symbol *symbols;
  struct grub_util_obj_reloc *relocs;
  grub_uint32_t mod_attr;
  char *attr;
  int attr_len;
  struct grub_util_obj_segment *got_segment;
  struct grub_util_obj_csym *csyms;
  struct grub_util_obj_got *gots;
  int got_size;
};

#define GRUB_OBJ_MERGE_NONE	0
#define GRUB_OBJ_MERGE_SAME	1
#define GRUB_OBJ_MERGE_ALL	2

void grub_obj_reverse (struct grub_util_obj *obj);
void grub_obj_sort_segments (struct grub_util_obj *obj);
void grub_obj_merge_segments (struct grub_util_obj *obj, int align, int merge);
void grub_obj_reloc_symbols (struct grub_util_obj *obj, int merge);
void grub_obj_save (struct grub_util_obj *obj, char *mod_name, FILE *fp);
struct grub_util_obj *grub_obj_load (char *image, int size, int load_data);
void grub_obj_free (struct grub_util_obj *obj);
void grub_obj_link (struct grub_util_obj *obj, grub_uint32_t base);
const char *grub_obj_map_symbol (const char *name);
void grub_obj_add_attr (struct grub_util_obj *obj, const char *start, int len);
void grub_obj_filter_symbols (struct grub_util_obj *obj);
struct grub_util_obj_segment *
grub_obj_add_modinfo (struct grub_util_obj *obj, const char *dir,
		      struct grub_util_path_list *path_list, int as_info,
		      char *memdisk_path, char *config_path);
int grub_obj_add_kernel_symbols (struct grub_util_obj *obj,
				 struct grub_util_obj_segment *modinfo,
				 grub_uint32_t offset);
void grub_obj_add_csym (struct grub_util_obj *obj, const char *name, int size);
void grub_obj_csym_done (struct grub_util_obj *obj);
grub_uint32_t grub_obj_add_got (struct grub_util_obj *obj, const char *name);
void grub_obj_got_done (struct grub_util_obj *obj);

void grub_obj_dump_segments (struct grub_util_obj *obj);
void grub_obj_dump_symbols (struct grub_util_obj *obj);
void grub_obj_dump_relocs (struct grub_util_obj *obj);

void grub_obj_import (struct grub_util_obj *obj, char *image, int size);
int grub_obj_import_elf (struct grub_util_obj *obj, char *image, int size);
int grub_obj_import_pe (struct grub_util_obj *obj, char *image, int size);
int grub_obj_import_macho (struct grub_util_obj *obj, char *image, int size);

#endif /* ! GRUB_UTIL_OBJ_HEADER */

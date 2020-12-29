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

#ifndef GRUB_OBJ_H
#define GRUB_OBJ_H 1

#include <grub/types.h>

#define GRUB_OBJ_HEADER_MAGIC	0x4a424f47	/* GOBJ  */
#define GRUB_OBJ_HEADER_VERSION	1

#define GRUB_OBJ_SEG_END	0
#define GRUB_OBJ_SEG_TEXT	1
#define GRUB_OBJ_SEG_DATA	2
#define GRUB_OBJ_SEG_RDATA	3
#define GRUB_OBJ_SEG_BSS	4
#define GRUB_OBJ_SEG_INFO	5

#define GRUB_OBJ_FUNC_NONE	0xffff

struct grub_obj_segment
{
  grub_uint8_t type;
  grub_uint32_t offset;
  grub_uint8_t align;
  grub_uint32_t size;
} __attribute__((packed));

struct grub_obj_header
{
  grub_uint32_t magic;
  grub_uint16_t version;
  grub_uint16_t symbol_table;
  grub_uint16_t reloc_table;
  grub_uint16_t string_table;
  grub_uint16_t init_func;
  grub_uint16_t fini_func;
  grub_uint32_t mod_deps;
  struct grub_obj_segment segments[1];
} __attribute__((packed));

#define GRUB_OBJ_SEGMENT_END	0xff

#define GRUB_OBJ_REL_TYPE_32	0
#define GRUB_OBJ_REL_TYPE_16	1
#define GRUB_OBJ_REL_TYPE_8	2
#define GRUB_OBJ_REL_TYPE_64	3
#define GRUB_OBJ_REL_TYPE_14	4
#define GRUB_OBJ_REL_TYPE_24	5
#define GRUB_OBJ_REL_TYPE_16HI	6
#define GRUB_OBJ_REL_TYPE_16HA	7
#define GRUB_OBJ_REL_TYPE_LO10	8
#define GRUB_OBJ_REL_TYPE_HI22	9
#define GRUB_OBJ_REL_TYPE_30	10
#define GRUB_OBJ_REL_TYPE_HM10	11
#define GRUB_OBJ_REL_TYPE_HH22	12

#define GRUB_OBJ_REL_TYPE_MASK	0x7f
#define GRUB_OBJ_REL_FLAG_REL	0x80

struct grub_obj_symbol
{
  grub_uint8_t segment;
  grub_uint16_t name;
  grub_uint32_t offset;
} __attribute__((packed));

struct grub_obj_reloc
{
  grub_uint8_t segment;
  grub_uint8_t type;
  grub_uint32_t offset;
#ifdef GRUB_TARGET_USE_ADDEND
  grub_uint32_t addend;
#endif
  grub_uint8_t symbol_segment;
} __attribute__((packed));

struct grub_obj_reloc_extern
{
  grub_uint8_t segment;
  grub_uint8_t type;
  grub_uint32_t offset;
#ifdef GRUB_TARGET_USE_ADDEND
  grub_uint32_t addend;
#endif
  grub_uint8_t symbol_segment;
  grub_uint16_t symbol_name;
} __attribute__((packed));

#endif /* ! GRUB_OBJ_H */

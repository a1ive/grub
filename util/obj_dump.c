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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char name_buf[16];

static char *
get_segment_name (int type)
{
  switch (type)
    {
    case GRUB_OBJ_SEG_TEXT:
      return ".text";

    case GRUB_OBJ_SEG_DATA:
      return ".data";

    case GRUB_OBJ_SEG_RDATA:
      return ".rdata";

    case GRUB_OBJ_SEG_BSS:
      return ".bss";

    case GRUB_OBJ_SEG_INFO:
      return ".info";
    }

  sprintf (name_buf, "%d", type);
  return name_buf;
}

static char *
get_reloc_type (int type)
{
  switch (type)
    {
    case GRUB_OBJ_REL_TYPE_32:
      return "dir32";

    case GRUB_OBJ_REL_TYPE_32 | GRUB_OBJ_REL_FLAG_REL:
      return "rel32";

    case GRUB_OBJ_REL_TYPE_64:
      return "dir64";

    case GRUB_OBJ_REL_TYPE_64 | GRUB_OBJ_REL_FLAG_REL:
      return "rel64";

    case GRUB_OBJ_REL_TYPE_16:
      return "dir16";

    case GRUB_OBJ_REL_TYPE_16 | GRUB_OBJ_REL_FLAG_REL:
      return "rel16";

    case GRUB_OBJ_REL_TYPE_24 | GRUB_OBJ_REL_FLAG_REL:
      return "rel24";

    case GRUB_OBJ_REL_TYPE_16HI:
      return "dir16hi";

    case GRUB_OBJ_REL_TYPE_16HI | GRUB_OBJ_REL_FLAG_REL:
      return "rel16hi";

    case GRUB_OBJ_REL_TYPE_16HA:
      return "dir16ha";

    case GRUB_OBJ_REL_TYPE_16HA | GRUB_OBJ_REL_FLAG_REL:
      return "rel16ha";

    case GRUB_OBJ_REL_TYPE_LO10:
      return "dirlo10";

    case GRUB_OBJ_REL_TYPE_HI22:
      return "dirhi22";

    case GRUB_OBJ_REL_TYPE_HM10:
      return "dirhm10";

    case GRUB_OBJ_REL_TYPE_HH22:
      return "dirhh22";

    case GRUB_OBJ_REL_TYPE_30 | GRUB_OBJ_REL_FLAG_REL:
      return "rel30";
    }

  sprintf (name_buf, "%x", type);
  return name_buf;
}

static int
dump_segments_hook (struct grub_util_obj_segment *obj,
		    void *closure __attribute__ ((unused)))
{
  printf ("%-10s%08x  %08x  %08x  %08x  %d\n",
	  get_segment_name (obj->segment.type),
	  obj->segment.offset, obj->segment.size, obj->raw_size,
	  obj->file_off, obj->segment.align);

  return 0;
}

void
grub_obj_dump_segments (struct grub_util_obj *obj)
{
  printf ("Segments:\n"
	  "Segment   Offset    Size      Raw Size  File Off  Align\n");
  grub_list_iterate (GRUB_AS_LIST (obj->segments),
		     (grub_list_hook_t) dump_segments_hook, 0);
}

static int
dump_symbols_hook (struct grub_util_obj_symbol *obj,
		   void *closure __attribute__ ((unused)))
{
  if (obj->segment)
    printf ("%-10s%08x  %s\n",
	    get_segment_name (obj->segment->segment.type),
	    obj->symbol.offset + obj->segment->segment.offset, obj->name);

  return 0;
}

void
grub_obj_dump_symbols (struct grub_util_obj *obj)
{
  printf ("Symbols:\n"
	  "Segment   Offset    Name\n");
  grub_list_iterate (GRUB_AS_LIST (obj->symbols),
		     (grub_list_hook_t) dump_symbols_hook, 0);
}

static int
dump_reloc_hook (struct grub_util_obj_reloc *obj,
		 void *closure __attribute__ ((unused)))
{
  if (obj->segment)
    {
      grub_uint32_t value;

#ifdef GRUB_TARGET_USE_ADDEND
      value = obj->reloc.addend;
#else
      if (obj->segment->data)
	value =
	  grub_target_to_host32 (*((grub_uint32_t *) (obj->segment->data
						      + obj->reloc.offset)));
      else
	value = 0;
#endif

      printf ("%-10s%08x  %08x  %-10s%s\n",
	      get_segment_name (obj->segment->segment.type),
	      obj->reloc.offset + obj->segment->segment.offset, value,
	      get_reloc_type (obj->reloc.type),
	      ((! obj->symbol_segment) ? obj->symbol_name :
	       get_segment_name (obj->symbol_segment->segment.type)));
    }

  return 0;
}

void
grub_obj_dump_relocs (struct grub_util_obj *obj)
{
  printf ("Relocs:\n"
	  "Segment   Offset    Value     Type      Name\n");
  grub_list_iterate (GRUB_AS_LIST (obj->relocs),
		     (grub_list_hook_t) dump_reloc_hook, 0);
}

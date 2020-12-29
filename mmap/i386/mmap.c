/* Mmap management. */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2009  Free Software Foundation, Inc.
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

#include <grub/machine/memory.h>
#include <grub/memory.h>
#include <grub/err.h>
#include <grub/misc.h>
#include <grub/mm.h>

GRUB_EXPORT(grub_mmap_free_and_unregister);
GRUB_EXPORT(grub_mmap_malign_and_register);

#ifndef GRUB_MMAP_REGISTER_BY_FIRMWARE

struct grub_mmap_malign_and_register_closure
{
  grub_uint64_t align;
  grub_uint64_t size;
  grub_uint64_t min;
  grub_uint64_t max;
  grub_uint64_t highestlow;
};

static int
find_hook (grub_uint64_t start, grub_uint64_t rangesize,
	   grub_uint32_t memtype, void *closure)
{
  struct grub_mmap_malign_and_register_closure *c = closure;
  grub_uint64_t end = start + rangesize;
  grub_uint64_t addr;
  if (memtype != GRUB_MACHINE_MEMORY_AVAILABLE)
    return 0;
  if (end > c->max)
    end = c->max;
  if (end < c->size)
    return 0;
  if (start < c->min)
    start = c->min;
  addr = (end - c->size) - ((end - c->size) & (c->align - 1));
  if (addr >= start && c->highestlow < addr)
    c->highestlow = addr;
  return 0;
}

void *
grub_mmap_malign_and_register (grub_uint64_t align, grub_uint64_t size,
			       int *handle, int type, int flags)
{
  void *ret;

  if (flags & (GRUB_MMAP_MALLOC_LOW | GRUB_MMAP_MALLOC_HIGH))
    {
      struct grub_mmap_malign_and_register_closure c;

      c.align = align;
      c.size = size;
      c.highestlow = 0;
      if (flags & GRUB_MMAP_MALLOC_LOW)
	{
	  c.min = 0;
	  c.max = 0x100000;
	}
      else
	{
	  c.min = grub_mmap_high;
	  c.max = 0x100000000ll;
	}
      /* FIXME: use low-memory mm allocation once it's available. */
      grub_mmap_iterate (find_hook, &c);
      ret = UINT_TO_PTR (c.highestlow);
    }
  else
    ret = grub_memalign (align, size);

  if (! ret)
    {
      *handle = 0;
      return 0;
    }

  *handle = grub_mmap_register (PTR_TO_UINT64 (ret), size, type);
  if (! *handle)
    {
      grub_free (ret);
      return 0;
    }

  return ret;
}

void
grub_mmap_free_and_unregister (int handle)
{
  struct grub_mmap_region *cur;
  grub_uint64_t addr;

  for (cur = grub_mmap_overlays; cur; cur = cur->next)
    if (cur->handle == handle)
      break;

  if (! cur)
    return;

  addr = cur->start;

  grub_mmap_unregister (handle);

  if (addr >= 0x100000)
    grub_free (UINT_TO_PTR (addr));
}

#endif

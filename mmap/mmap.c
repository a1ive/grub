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
#include <grub/command.h>
#include <grub/dl.h>
#include <grub/i18n.h>

GRUB_EXPORT(grub_mmap_register);
GRUB_EXPORT(grub_mmap_unregister);
GRUB_EXPORT(grub_mmap_iterate);

#ifndef GRUB_MMAP_REGISTER_BY_FIRMWARE

struct grub_mmap_region *grub_mmap_overlays = 0;
static int curhandle = 1;

#endif

struct grub_mmap_iterate_closure
{
  int i;
  struct grub_mmap_scan *scanline_events;
  int *priority;
};

static int
count_hook (grub_uint64_t addr __attribute__ ((unused)),
	    grub_uint64_t size __attribute__ ((unused)),
	    grub_uint32_t type __attribute__ ((unused)), void *closure)
{
  int *mmap_num = closure;

  (*mmap_num)++;
  return 0;
}

/* Scanline events. */
struct grub_mmap_scan
{
  /* At which memory address. */
  grub_uint64_t pos;
  /* 0 = region starts, 1 = region ends. */
  int type;
  /* Which type of memory region? */
  int memtype;
};

static int
fill_hook (grub_uint64_t addr, grub_uint64_t size, grub_uint32_t type,
	   void *closure)
{
  struct grub_mmap_iterate_closure *c = closure;
  struct grub_mmap_scan *scanline_events = c->scanline_events;

  scanline_events[c->i].pos = addr;
  scanline_events[c->i].type = 0;
  if (type <= GRUB_MACHINE_MEMORY_MAX_TYPE && c->priority[type])
    scanline_events[c->i].memtype = type;
  else
    {
      grub_dprintf ("mmap", "Unknown memory type %d. Assuming unusable\n",
		    type);
      scanline_events[c->i].memtype = GRUB_MACHINE_MEMORY_RESERVED;
    }
  c->i++;

  scanline_events[c->i].pos = addr + size;
  scanline_events[c->i].type = 1;
  scanline_events[c->i].memtype = scanline_events[c->i - 1].memtype;
  c->i++;

  return 0;
}

grub_err_t
grub_mmap_iterate (int (*hook) (grub_uint64_t, grub_uint64_t,
				grub_uint32_t, void *), void *closure)
{
  /* This function resolves overlapping regions and sorts the memory map.
     It uses scanline (sweeping) algorithm.
  */
  /* If same page is used by multiple types it's resolved
     according to priority:
     1 - free memory
     2 - memory usable by firmware-aware code
     3 - unusable memory
     4 - a range deliberately empty
  */
  int priority[GRUB_MACHINE_MEMORY_MAX_TYPE + 2] =
    {
#ifdef GRUB_MACHINE_MEMORY_AVAILABLE
      [GRUB_MACHINE_MEMORY_AVAILABLE] = 1,
#endif
#if defined (GRUB_MACHINE_MEMORY_RESERVED) && GRUB_MACHINE_MEMORY_RESERVED != GRUB_MACHINE_MEMORY_HOLE
      [GRUB_MACHINE_MEMORY_RESERVED] = 3,
#endif
#ifdef GRUB_MACHINE_MEMORY_ACPI
      [GRUB_MACHINE_MEMORY_ACPI] = 2,
#endif
#ifdef GRUB_MACHINE_MEMORY_CODE
      [GRUB_MACHINE_MEMORY_CODE] = 3,
#endif
#ifdef GRUB_MACHINE_MEMORY_NVS
      [GRUB_MACHINE_MEMORY_NVS] = 3,
#endif
      [GRUB_MACHINE_MEMORY_HOLE] = 4,
    };

  int i, k, done;

  struct grub_mmap_scan *scanline_events;
  struct grub_mmap_scan t;

  /* Previous scanline event. */
  grub_uint64_t lastaddr;
  int lasttype;
  /* Current scanline event. */
  int curtype;
  /* How many regions of given type overlap at current location? */
  int present[GRUB_MACHINE_MEMORY_MAX_TYPE + 2];
  /* Number of mmap chunks. */
  int mmap_num;
  struct grub_mmap_iterate_closure c;

#ifndef GRUB_MMAP_REGISTER_BY_FIRMWARE
  struct grub_mmap_region *cur;
#endif

  mmap_num = 0;

#ifndef GRUB_MMAP_REGISTER_BY_FIRMWARE
  for (cur = grub_mmap_overlays; cur; cur = cur->next)
    mmap_num++;
#endif

  grub_machine_mmap_iterate (count_hook, &mmap_num);

  /* Initialize variables. */
  grub_memset (present, 0, sizeof (present));
  scanline_events = (struct grub_mmap_scan *)
    grub_malloc (sizeof (struct grub_mmap_scan) * 2 * mmap_num);

  if (! scanline_events)
    {
      return grub_error (GRUB_ERR_OUT_OF_MEMORY,
			 "couldn't allocate space for new memory map");
    }

  i = 0;
#ifndef GRUB_MMAP_REGISTER_BY_FIRMWARE
  /* Register scanline events. */
  for (cur = grub_mmap_overlays; cur; cur = cur->next)
    {
      scanline_events[i].pos = cur->start;
      scanline_events[i].type = 0;
      if (cur->type == GRUB_MACHINE_MEMORY_HOLE
	  || (cur->type >= 0 && cur->type <= GRUB_MACHINE_MEMORY_MAX_TYPE
	      && priority[cur->type]))
	scanline_events[i].memtype = cur->type;
      else
	scanline_events[i].memtype = GRUB_MACHINE_MEMORY_RESERVED;
      i++;

      scanline_events[i].pos = cur->end;
      scanline_events[i].type = 1;
      scanline_events[i].memtype = scanline_events[i - 1].memtype;
      i++;
    }
#endif /* ! GRUB_MMAP_REGISTER_BY_FIRMWARE */

  c.i = i;
  c.scanline_events = scanline_events;
  c.priority = priority;
  grub_machine_mmap_iterate (fill_hook, &c);

  /* Primitive bubble sort. It has complexity O(n^2) but since we're
     unlikely to have more than 100 chunks it's probably one of the
     fastest for one purpose. */
  done = 1;
  while (done)
    {
      done = 0;
      for (i = 0; i < 2 * mmap_num - 1; i++)
	if (scanline_events[i + 1].pos < scanline_events[i].pos
	    || (scanline_events[i + 1].pos == scanline_events[i].pos
		&& scanline_events[i + 1].type == 0
		&& scanline_events[i].type == 1))
	  {
	    t = scanline_events[i + 1];
	    scanline_events[i + 1] = scanline_events[i];
	    scanline_events[i] = t;
	    done = 1;
	  }
    }

  lastaddr = scanline_events[0].pos;
  lasttype = scanline_events[0].memtype;
  for (i = 0; i < 2 * mmap_num; i++)
    {
      /* Process event. */
      if (scanline_events[i].type)
	present[scanline_events[i].memtype]--;
      else
	present[scanline_events[i].memtype]++;

      /* Determine current region type. */
      curtype = -1;
      for (k = 0; k <= GRUB_MACHINE_MEMORY_MAX_TYPE + 1; k++)
	if (present[k] && (curtype == -1 || priority[k] > priority[curtype]))
	  curtype = k;

      /* Announce region to the hook if necessary. */
      if ((curtype == -1 || curtype != lasttype)
	  && lastaddr != scanline_events[i].pos
	  && lasttype != -1
	  && lasttype != GRUB_MACHINE_MEMORY_HOLE
	  && hook (lastaddr, scanline_events[i].pos - lastaddr, lasttype,
		   closure))
	{
	  grub_free (scanline_events);
	  return GRUB_ERR_NONE;
	}

      /* Update last values if necessary. */
      if (curtype == -1 || curtype != lasttype)
	{
	  lasttype = curtype;
	  lastaddr = scanline_events[i].pos;
	}
    }

  grub_free (scanline_events);
  return GRUB_ERR_NONE;
}

#ifndef GRUB_MMAP_REGISTER_BY_FIRMWARE
int
grub_mmap_register (grub_uint64_t start, grub_uint64_t size, int type)
{
  struct grub_mmap_region *cur;

  grub_dprintf ("mmap", "registering\n");

  cur = (struct grub_mmap_region *)
    grub_malloc (sizeof (struct grub_mmap_region));
  if (! cur)
    {
      grub_error (GRUB_ERR_OUT_OF_MEMORY,
		  "couldn't allocate memory map overlay");
      return 0;
    }

  cur->next = grub_mmap_overlays;
  cur->start = start;
  cur->end = start + size;
  cur->type = type;
  cur->handle = curhandle++;
  grub_mmap_overlays = cur;

  if (grub_machine_mmap_register (start, size, type, curhandle))
    {
      grub_mmap_overlays = cur->next;
      grub_free (cur);
      return 0;
    }

  return cur->handle;
}

grub_err_t
grub_mmap_unregister (int handle)
{
  struct grub_mmap_region *cur, *prev;

  for (cur = grub_mmap_overlays, prev = 0; cur; prev= cur, cur = cur->next)
    if (handle == cur->handle)
      {
	grub_err_t err;
	if ((err = grub_machine_mmap_unregister (handle)))
	  return err;

	if (prev)
	  prev->next = cur->next;
	else
	  grub_mmap_overlays = cur->next;
	grub_free (cur);
	return GRUB_ERR_NONE;
      }
  return grub_error (GRUB_ERR_BAD_ARGUMENT, "mmap overlay not found");
}

#endif /* ! GRUB_MMAP_REGISTER_BY_FIRMWARE */

#define CHUNK_SIZE	0x400

static inline grub_uint64_t
fill_mask (grub_uint64_t addr, grub_uint64_t mask, grub_uint64_t iterator)
{
  int i, j;
  grub_uint64_t ret = (addr & mask);

  /* Find first fixed bit. */
  for (i = 0; i < 64; i++)
    if ((mask & (1ULL << i)) != 0)
      break;
  j = 0;
  for (; i < 64; i++)
    if ((mask & (1ULL << i)) == 0)
      {
	if ((iterator & (1ULL << j)) != 0)
	  ret |= 1ULL << i;
	j++;
      }
  return ret;
}

struct grub_cmd_badram_closure
{
  grub_uint64_t badaddr, badmask;
};

static int
grub_cmd_badram_hook (grub_uint64_t addr, grub_uint64_t size,
		      grub_uint32_t type __attribute__ ((unused)),
		      void *closure)
{
  struct grub_cmd_badram_closure *c = closure;
  grub_uint64_t iterator, low, high, cur;
  int tail, var;
  int i;
  grub_dprintf ("badram", "hook %llx+%llx\n", (unsigned long long) addr,
		(unsigned long long) size);

  /* How many trailing zeros? */
  for (tail = 0; ! (c->badmask & (1ULL << tail)); tail++);

  /* How many zeros in mask? */
  var = 0;
  for (i = 0; i < 64; i++)
    if (! (c->badmask & (1ULL << i)))
      var++;

  if (fill_mask (c->badaddr, c->badmask, 0) >= addr)
    iterator = 0;
  else
    {
      low = 0;
      high = ~0ULL;
      /* Find starting value. Keep low and high such that
	 fill_mask (low) < addr and fill_mask (high) >= addr;
      */
      while (high - low > 1)
	{
	  cur = (low + high) / 2;
	  if (fill_mask (c->badaddr, c->badmask, cur) >= addr)
	    high = cur;
	  else
	    low = cur;
	}
      iterator = high;
    }

  for (; iterator < (1ULL << (var - tail))
	 && (cur = fill_mask (c->badaddr, c->badmask, iterator)) < addr + size;
       iterator++)
    {
      grub_dprintf ("badram", "%llx (size %llx) is a badram range\n",
		    (unsigned long long) cur, (1ULL << tail));
      grub_mmap_register (cur, (1ULL << tail), GRUB_MACHINE_MEMORY_HOLE);
    }
  return 0;
}

static grub_err_t
grub_cmd_badram (grub_command_t cmd __attribute__ ((unused)),
		 int argc, char **args)
{
  char * str;

  if (argc != 1)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "badram string required");

  grub_dprintf ("badram", "executing badram\n");

  str = args[0];

  while (1)
    {
      struct grub_cmd_badram_closure c;

      /* Parse address and mask.  */
      c.badaddr = grub_strtoull (str, &str, 16);
      if (*str == ',')
	str++;
      c.badmask = grub_strtoull (str, &str, 16);
      if (*str == ',')
	str++;

      if (grub_errno == GRUB_ERR_BAD_NUMBER)
	{
	  grub_errno = 0;
	  return GRUB_ERR_NONE;
	}

      /* When part of a page is tainted, we discard the whole of it.  There's
	 no point in providing sub-page chunks.  */
      c.badmask &= ~(CHUNK_SIZE - 1);

      grub_dprintf ("badram", "badram %llx:%llx\n",
		    (unsigned long long) c.badaddr,
		    (unsigned long long) c.badmask);

      grub_mmap_iterate (grub_cmd_badram_hook, &c);
    }
}

static grub_command_t cmd;


GRUB_MOD_INIT(mmap)
{
  cmd = grub_register_command ("badram", grub_cmd_badram,
			       N_("ADDR1,MASK1[,ADDR2,MASK2[,...]]"),
			       N_("Declare memory regions as badram."));
}

GRUB_MOD_FINI(mmap)
{
  grub_unregister_command (cmd);
}


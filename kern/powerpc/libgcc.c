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

/* The following code comes from gcc-4.3.  */

#include <grub/types.h>
#include <grub/symbol.h>
#include <grub/misc.h>
#include <grub/cache.h>

GRUB_EXPORT(__ashldi3);
GRUB_EXPORT(__ashrdi3);
GRUB_EXPORT(__lshrdi3);
GRUB_EXPORT(__ucmpdi2);

#define DWtype		long long
#define UDWtype		unsigned long long
#define Wtype		int
#define UWtype		unsigned int
#define BITS_PER_UNIT	8
#define shift_count_type int
#define cmp_return_type int

#ifdef GRUB_CPU_WORDS_BIGENDIAN
struct DWstruct {Wtype high, low;};
#else
struct DWstruct {Wtype low, high;};
#endif

typedef union
{
  struct DWstruct s;
  DWtype ll;
} DWunion;

DWtype __ashldi3 (DWtype u, shift_count_type b);
DWtype __ashrdi3 (DWtype u, shift_count_type b);
DWtype __lshrdi3 (DWtype u, shift_count_type b);
void __trampoline_setup (grub_uint32_t* stack, int size,
			void* func, void* local_ptr);
cmp_return_type __ucmpdi2 (DWtype a, DWtype b);

DWtype
__ashldi3 (DWtype u, shift_count_type b)
{
  if (b == 0)
    return u;

  const DWunion uu = {.ll = u};
  const shift_count_type bm = (sizeof (Wtype) * BITS_PER_UNIT) - b;
  DWunion w;

  if (bm <= 0)
    {
      w.s.low = 0;
      w.s.high = (UWtype) uu.s.low << -bm;
    }
  else
    {
      const UWtype carries = (UWtype) uu.s.low >> bm;

      w.s.low = (UWtype) uu.s.low << b;
      w.s.high = ((UWtype) uu.s.high << b) | carries;
    }

  return w.ll;
}

DWtype
__ashrdi3 (DWtype u, shift_count_type b)
{
  if (b == 0)
    return u;

  const DWunion uu = {.ll = u};
  const shift_count_type bm = (sizeof (Wtype) * BITS_PER_UNIT) - b;
  DWunion w;

  if (bm <= 0)
    {
      /* w.s.high = 1..1 or 0..0 */
      w.s.high = uu.s.high >> (sizeof (Wtype) * BITS_PER_UNIT - 1);
      w.s.low = uu.s.high >> -bm;
    }
  else
    {
      const UWtype carries = (UWtype) uu.s.high << bm;

      w.s.high = uu.s.high >> b;
      w.s.low = ((UWtype) uu.s.low >> b) | carries;
    }

  return w.ll;
}

DWtype
__lshrdi3 (DWtype u, shift_count_type b)
{
  if (b == 0)
    return u;

  const DWunion uu = {.ll = u};
  const shift_count_type bm = (sizeof (Wtype) * BITS_PER_UNIT) - b;
  DWunion w;

  if (bm <= 0)
    {
      w.s.high = 0;
      w.s.low = (UWtype) uu.s.high >> -bm;
    }
  else
    {
      const UWtype carries = (UWtype) uu.s.high << bm;

      w.s.high = (UWtype) uu.s.high >> b;
      w.s.low = ((UWtype) uu.s.low >> b) | carries;
    }

  return w.ll;
}

#if 0
void
__trampoline_setup (grub_uint32_t* stack, int size,
		    void* func, void* local_ptr)
{
  if (size < 40)
    grub_abort();

  /* create trampoline */
  stack[0] = 0x7c0802a6;    /* mflr r0 */
  stack[1] = 0x4800000d;    /* bl Lbase */
  stack[2] = (grub_uint32_t) func;
  stack[3] = (grub_uint32_t) local_ptr;
  stack[4] = 0x7d6802a6;    /* Lbase: mflr r11 */
  stack[5] = 0x818b0000;    /* lwz    r12,0(r11) */
  stack[6] = 0x7c0803a6;    /* mtlr r0 */
  stack[7] = 0x7d8903a6;    /* mtctr r12 */
  stack[8] = 0x816b0004;    /* lwz    r11,4(r11) */
  stack[9] = 0x4e800420;    /* bctr */

  grub_arch_sync_caches (stack, 40);
}
#endif

cmp_return_type
__ucmpdi2 (DWtype a, DWtype b)
{
  const DWunion au = {.ll = a};
  const DWunion bu = {.ll = b};

  if ((UWtype) au.s.high < (UWtype) bu.s.high)
    return 0;
  else if ((UWtype) au.s.high > (UWtype) bu.s.high)
    return 2;
  if ((UWtype) au.s.low < (UWtype) bu.s.low)
    return 0;
  else if ((UWtype) au.s.low > (UWtype) bu.s.low)
    return 2;
  return 1;
}

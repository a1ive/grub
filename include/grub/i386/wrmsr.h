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

#ifndef GRUB_CPU_MSR_WRITE_HEADER
#define GRUB_CPU_MSR_WRITE_HEADER 1
#endif

#ifdef __x86_64__

extern __inline void grub_msr_write(grub_uint64_t msr, grub_uint64_t value)
{
    grub_uint32_t low = value, high = value >> 32;

    asm volatile ( "wrmsr" : : "c"(msr), "a"(low), "d"(high) );
}

#else

extern __inline void grub_msr_write(grub_uint64_t msr_id, grub_uint64_t msr_value)
{
    /* We use uint64 in msr_id just to keep the same function signature. */
    grub_uint32_t low_id = msr_id, low_value = msr_value;

    asm volatile ( "wrmsr" : : "c" (low_id), "A" (low_value) );
}

#endif

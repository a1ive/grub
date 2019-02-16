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

/*
 * grub msr module
 * Allows reading and writing to model-specific registers.
 * Jesús Diéguez Fernández
 *
 * Main structure inspired by memrw.c and cpuid.c, assembly code adapted from:
 *
 *	https://wiki.osdev.org/Inline_Assembly/Examples#RDMSR
 *	https://wiki.osdev.org/Inline_Assembly/Examples#WRMSR
 *
 */

#ifndef GRUB_CPU_MSR_HEADER
#define GRUB_CPU_MSR_HEADER 1
#endif

#ifdef __x86_64__

extern __inline grub_uint64_t grub_msr_read (grub_uint64_t msr)
{
    grub_uint32_t low;
    grub_uint32_t high;
    __asm__ __volatile__ ( "rdmsr"
                           : "=a"(low), "=d"(high)
                           : "c"(msr)
                           );
    return ((grub_uint64_t)high << 32) | low;
}

extern __inline void grub_msr_write(grub_uint64_t msr, grub_uint64_t value)
{
    grub_uint32_t low = value & 0xFFFFFFFF;
    grub_uint32_t high = value >> 32;
    __asm__ __volatile__ (
                "wrmsr"
                :
                : "c"(msr), "a"(low), "d"(high)
                );
}

#else

extern __inline grub_uint64_t grub_msr_read (grub_uint64_t msr_id)
{
    /* We use uint64 in msr_id just to keep the same function signature */
    grub_uint32_t low_id = msr_id & 0xFFFFFFFF;
    grub_uint64_t msr_value;
    __asm__ __volatile__ ( "rdmsr" : "=A" (msr_value) : "c" (low_id) );
    return msr_value;
}

extern __inline void grub_msr_write(grub_uint64_t msr_id, grub_uint64_t msr_value)
{
    /* We use uint64 in msr_id just to keep the same function signature */
    grub_uint32_t low_id = msr_id & 0xFFFFFFFF;
    grub_uint32_t low_value = msr_value & 0xFFFFFFFF;
    __asm__ __volatile__ ( "wrmsr" : : "c" (low_id), "A" (low_value) );
}

#endif


/* -*-Asm-*- */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 1999,2000,2001,2002,2005,2006,2007,2008,2009  Free Software Foundation, Inc.
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

#include <grub/symbol.h>
#include <grub/machine/boot.h>

/*
 *  defines for the code go here
 */

	/* Print message string */
#define MSG(x)	movw $x, %si; call LOCAL(message)
#define ERR(x)	movw $x, %si; jmp LOCAL(error_message)

	.macro floppy
part_start:

LOCAL(probe_values):
	.byte	36, 18, 15, 9, 0

LOCAL(floppy_probe):
	pushw	%dx
/*
 *  Perform floppy probe.
 */
#ifdef __APPLE__
	LOCAL(probe_values_minus_one) = LOCAL(probe_values) - 1
	movw	MACRO_DOLLAR(LOCAL(probe_values_minus_one)), %si
#else
	movw	MACRO_DOLLAR(LOCAL(probe_values)) - 1, %si
#endif

LOCAL(probe_loop):
	/* reset floppy controller INT 13h AH=0 */
	xorw	%ax, %ax
	int	MACRO_DOLLAR(0x13)

	incw	%si
	movb	(%si), %cl

	/* if number of sectors is 0, display error and die */
	testb	%cl, %cl
	jnz	1f

/*
 * Floppy disk probe failure.
 */
	MSG(fd_probe_error_string)
	jmp	LOCAL(general_error)

/* "Floppy" */
fd_probe_error_string:	.asciz "Floppy"

1:
	/* perform read */
	movw	MACRO_DOLLAR(GRUB_BOOT_MACHINE_BUFFER_SEG), %bx
	movw	%bx, %es
	xorw	%bx, %bx
	movw	MACRO_DOLLAR(0x201), %ax
	movb	MACRO_DOLLAR(0), %ch
	movb	MACRO_DOLLAR(0), %dh
	int	MACRO_DOLLAR(0x13)

	/* if error, jump to "LOCAL(probe_loop)" */
	jc	LOCAL(probe_loop)

	/* %cl is already the correct value! */
	movb	MACRO_DOLLAR(1), %dh
	movb	MACRO_DOLLAR(79), %ch

	jmp	LOCAL(final_init)
	.endm

	.macro scratch

	/* scratch space */
mode:
	.byte	0
disk_address_packet:
sectors:
	.long	0
heads:
	.long	0
cylinders:
	.word	0
sector_start:
	.byte	0
head_start:
	.byte	0
cylinder_start:
	.word	0
	/* more space... */
	.endm

	.file	"boot.S"

	.text

	/* Tell GAS to generate 16-bit instructions so that this code works
	   in real mode. */
	.code16

.globl _start, start;
_start:
start:
	/*
	 * _start is loaded at 0x7c00 and is jumped to with CS:IP 0:0x7c00
	 */

	/*
	 * Beginning of the sector is compatible with the FAT/HPFS BIOS
	 * parameter block.
	 */

	jmp	LOCAL(after_BPB)
	nop	/* do I care about this ??? */

#ifdef HYBRID_BOOT
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop

	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop

	nop
	nop
	jmp	LOCAL(after_BPB)
#else
	/*
	 * This space is for the BIOS parameter block!!!!  Don't change
	 * the first jump, nor start the code anywhere but right after
	 * this area.
	 */

	. = _start + GRUB_BOOT_MACHINE_BPB_START
	. = _start + 4
#endif
#ifdef HYBRID_BOOT
	floppy
#else
	scratch
#endif

	. = _start + GRUB_BOOT_MACHINE_BPB_END
	/*
	 * End of BIOS parameter block.
	 */

kernel_address:
	.word	GRUB_BOOT_MACHINE_KERNEL_ADDR

#ifndef HYBRID_BOOT
	. = _start + GRUB_BOOT_MACHINE_KERNEL_SECTOR
kernel_sector:
	.long	1
kernel_sector_high:
	.long	0
#endif

	. = _start + GRUB_BOOT_MACHINE_BOOT_DRIVE
boot_drive:
	.byte 0xff	/* the disk to load kernel from */
			/* 0xff means use the boot drive */

LOCAL(after_BPB):

/* general setup */
	cli		/* we're not safe here! */

        /*
         * This is a workaround for buggy BIOSes which don't pass boot
         * drive correctly. If GRUB is installed into a HDD, check if
         * DL is masked correctly. If not, assume that the BIOS passed
         * a bogus value and set DL to 0x80, since this is the only
         * possible boot drive. If GRUB is installed into a floppy,
         * this does nothing (only jump).
         */
	. = _start + GRUB_BOOT_MACHINE_DRIVE_CHECK
boot_drive_check:
        jmp     3f	/* grub-setup may overwrite this jump */
        testb   $0x80, %dl
        jz      2f
3:
	/* Ignore %dl different from 0-0x0f and 0x80-0x8f.  */
	testb   $0x70, %dl
	jz      1f
2:	
        movb    $0x80, %dl
1:
	/*
	 * ljmp to the next instruction because some bogus BIOSes
	 * jump to 07C0:0000 instead of 0000:7C00.
	 */
	ljmp	$0, $real_start

real_start:

	/* set up %ds and %ss as offset from 0 */
	xorw	%ax, %ax
	movw	%ax, %ds
	movw	%ax, %ss

	/* set up the REAL stack */
	movw	$GRUB_BOOT_MACHINE_STACK_SEG, %sp

	sti		/* we're safe again */

	movw	$(20), %cx
	movw	$start, %di
	movw	$0, %ax
	movw	%ax, %es
	call LOCAL(hexdump)

/* go here when you need to stop the machine hard after an error condition */
        /* tell the BIOS a boot failure, which may result in no effect */
        int	$0x18
LOCAL(stop):
	jmp	LOCAL(stop)

#include "hexdump.S"


	. = _start + GRUB_BOOT_MACHINE_PART_END
	
/* the last 2 bytes in the sector 0 contain the signature */
	.word	GRUB_BOOT_MACHINE_SIGNATURE

/*
 *  BURG - Brand-new Universal loadeR from GRUB
 *  Copyright 2010 Bean Lee - All Rights Reserved
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

#include <grub/symbol.h>
#include <grub/i386/pc/memory.h>

	.code32
	.p2align	2

VARIABLE(grub_relocator16_trampoline_start)

LOCAL(csip_saved):
	.long	0

	pushl	%eax
	pushl	%ebx
	call	LOCAL(start)

LOCAL(start):
	popl	%ebx
	leal	(LOCAL(gdt) - LOCAL(start))(%ebx), %eax
	movl	%eax, (LOCAL(gdtdesc) - LOCAL(start) + 2)(%ebx)
	lgdt	(LOCAL(gdtdesc) - LOCAL(start))(%ebx)
	leal	(LOCAL(tmpcseg) - LOCAL(start))(%ebx), %eax
	movw	%ax, (LOCAL(tmpcseg_var) - LOCAL(start))(%ebx)
	shrl	$16, %eax
	movb	%al, (LOCAL(gdt_code_base) - LOCAL(start))(%ebx)
	movl	(LOCAL(csip_saved) - LOCAL(start))(%ebx), %eax
	movl	%eax, (LOCAL(csip) - LOCAL(start))(%ebx)

	popl	%ebx

	/* set up segment limits */
	movw	$GRUB_MEMORY_MACHINE_PSEUDO_REAL_DSEG, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	movw	%ax, %ss

	/* this might be an extra step */
	/* jump to a 16 bit segment */
	/* ljmp $GRUB_MEMORY_MACHINE_PSEUDO_REAL_CSEG, $tmpcseg */
	.byte	0xea
LOCAL(tmpcseg_var):
	.long	0
	.short	GRUB_MEMORY_MACHINE_PSEUDO_REAL_CSEG

LOCAL(tmpcseg):
	.code16

	/* clear the PE bit of CR0 */
	movl	%cr0, %eax
	andl 	$(~GRUB_MEMORY_CPU_CR0_PE_ON), %eax
	movl	%eax, %cr0

	/* we are in real mode now
	 * set up the real mode segment registers : DS, SS, ES
	 */
	xorw	%ax, %ax

	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	movw	%ax, %ss

	popl	%eax

	/* restore interrupts */
	cld

	/* return on new stack! */
	/* ljmp	$CS, $IP  */
	.byte	0xea
LOCAL(csip):
	.long	0

	.code32

	.p2align	2

LOCAL(gdt):
	.word	0, 0
	.byte	0, 0, 0, 0

	/* -- code segment --
	 * base = 0x00000000, limit = 0xFFFFF (4 KiB Granularity), present
	 * type = 32bit code execute/read, DPL = 0
	 */
	.word	0xFFFF, 0
	.byte	0, 0x9A, 0xCF, 0

	/* -- data segment --
	 * base = 0x00000000, limit 0xFFFFF (4 KiB Granularity), present
	 * type = 32 bit data read/write, DPL = 0
	 */
	.word	0xFFFF, 0
	.byte	0, 0x92, 0xCF, 0

	/* -- 16 bit real mode CS --
	 * base = 0x00000000, limit 0xFFFFF (1 B Granularity), present
	 * type = 16 bit code execute/read only/conforming, DPL = 0
	 */
	.word	0xFFFF, 0
LOCAL(gdt_code_base):
	.byte	0, 0x9E, 0x0, 0

	/* -- 16 bit real mode DS --
	 * base = 0x00000000, limit 0xFFFFF (1 B Granularity), present
	 * type = 16 bit data read/write, DPL = 0
	 */
	.word	0xFFFF, 0
	.byte	0, 0x92, 0x0, 0

LOCAL(gdtdesc):
	.word	0x27			/* limit */
	.long	0			/* addr */

VARIABLE(grub_relocator16_trampoline_end)

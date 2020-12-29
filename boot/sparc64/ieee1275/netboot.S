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

#include <grub/machine/boot.h>
#include <grub/machine/kernel.h>

	.text
	.align	4

	/* a.out header.  */
	.word	GRUB_BOOT_AOUT_MAGIC
	.word	0
	.word	0, 0, 0
	.word	0x4000
	.word	0, 0

_start:
pic_base:
	call	boot_continue
	 mov	%o4, CIF_REG

boot_continue:
	mov	%o7, PIC_REG
	GET_ABS(kernel_start, %o2)
	sethi	%hi(GRUB_BOOT_MACHINE_IMAGE_ADDRESS), %o3
	or	%o3, %lo(GRUB_BOOT_MACHINE_IMAGE_ADDRESS), %o3
	LDUW_ABS(kernel_start, GRUB_KERNEL_MACHINE_TOTAL_MODULE_SIZE, %o4)
	LDUW_ABS(kernel_start, GRUB_KERNEL_MACHINE_KERNEL_IMAGE_SIZE, %o5)
	add	%o4, %o5, %o4
	add	%o4, 3, %o4
	srl	%o4, 2, %o4
	sll	%o4, 2, %o4
	add	%o2, %o4, %o2
	add	%o3, %o4, %o3
	sub	%o2, 4, %o2
1:
	sub	%o3, 4, %o3
	lduw	[%o2], %o5
	stw	%o5, [%o3]
	subcc	%o4, 4, %o4
	bgu,pt	%icc, 1b
	 sub	%o2, 4, %o2

	jmpl	%o3, %o7
	 mov	CIF_REG, %o0

1:	ba,a	1b
	 nop

	. = _start + GRUB_BOOT_MACHINE_CODE_END
	.word	GRUB_BOOT_MACHINE_SIGNATURE

kernel_start:


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

#include <grub/symbol.h>

#define DS(x) ((x) - LOCAL (segstart))

LOCAL (segstart):
VARIABLE(grub_machine_mmaphook_start)
	.code16
VARIABLE(grub_machine_mmaphook_int15)
	cmpw $0xe801, %ax
	jz LOCAL (e801)
	cmpw $0xe881, %ax
	jz LOCAL (e881)	
	cmpw $0xe820, %ax
	jz LOCAL (e820)
	cmpb $0x88, %ah
	jz LOCAL (h88)
	/* ljmp */
LOCAL (chain_old_int15):
	.byte	0xea
VARIABLE (grub_machine_mmaphook_int15offset)
	.word	0
VARIABLE (grub_machine_mmaphook_int15segment)
	.word	0

LOCAL (e881):
	xorl %eax, %eax
	xorl %ebx, %ebx
	xorl %ecx, %ecx
	xorl %edx, %edx
	
LOCAL (e801):
	push %ds
	push %cs
	pop %ds
	movw DS (LOCAL (kbin16mb)), %ax
	movw DS (LOCAL (m64kbin4gb)), %bx
	movw %ax, %cx
	movw %bx, %dx

done:
	clc
fail:
	popw	%ds
	pushw	%bp
	movw	%sp, %bp
	pushfw
	andb	$0xfe, 6(%bp)	
	popfw
	adcb	$0, 6(%bp)
	popw	%bp
	iret

LOCAL (h88):
	push %ds
	push %cs
	pop %ds
	movw DS (LOCAL (int1588mem)), %ax
	jmp done

LOCAL (e820):
	cmp $0x534d4150, %edx
	jnz LOCAL (chain_old_int15)	
	push %ds
	push %cs
	pop %ds
	cmpl $20, %ecx
	jb LOCAL (errexit)
	cmpl DS (LOCAL (mmap_num)), %ebx
	jae LOCAL (errexit)
	pushl %edx
	movl $20, %ecx
	pushw %cx
	push %si
	push %di
	movw $(DS(LOCAL (mmaphook_mmap))), %si
	add %bx, %si
	cld
	rep movsb
	pop %di
	pop %si
	popw %cx
	addw %cx, %bx
	cmpw DS(LOCAL (mmap_num)), %bx
	jb LOCAL (noclean)
	xor %ebx, %ebx
LOCAL (noclean):
	pop %eax
	jmp done
LOCAL (errexit):
	mov $0x86, %ah
	stc
	jmp fail
	
VARIABLE(grub_machine_mmaphook_mmap_num)
LOCAL (mmap_num):
	.word 0
VARIABLE (grub_machine_mmaphook_kbin16mb)
LOCAL (kbin16mb):
	.word 0
VARIABLE (grub_machine_mmaphook_64kbin4gb)
LOCAL (m64kbin4gb):
	.word 0
VARIABLE (grub_machine_mmaphook_int1588mem)
LOCAL (int1588mem):
	.word 0	
LOCAL (mmaphook_mmap):
	/* Memory map is placed just after the interrupt handlers. */
VARIABLE(grub_machine_mmaphook_end)
	.byte 0

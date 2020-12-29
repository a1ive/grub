/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2003,2007  Free Software Foundation, Inc.
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

	.file	"setjmp.S"

	.text

GRUB_EXPORT_START
GRUB_EXPORT(grub_setjmp);
GRUB_EXPORT(grub_longjmp);
GRUB_EXPORT_END

/*
 *  jmp_buf:
 *   rbx rsp rbp r12 r13 r14 r15 rip
 *   0   8   16  24  32  40  48  56
 */

/*
 * int grub_setjmp (grub_jmp_buf env)
 */
FUNCTION(grub_setjmp)
#ifdef __MINGW32__
	pop	%rdx		/* Return address, and adjust the stack */
	xorq	%rax, %rax
	movq	%rbx, 0(%rcx)	/* RBX */
	movq	%rsp, 8(%rcx)   /* RSP */
	push	%rdx
	movq	%rbp, 16(%rcx)	/* RBP */
	movq	%r12, 24(%rcx)	/* R12 */
	movq	%r13, 32(%rcx)	/* R13 */
	movq	%r14, 40(%rcx)	/* R14 */
	movq	%r15, 48(%rcx)	/* R15 */
	movq	%rdx, 56(%rcx)	/* RIP */
	ret
#else
	pop	%rsi		/* Return address, and adjust the stack */
	xorq	%rax, %rax
	movq	%rbx, 0(%rdi)	/* RBX */
	movq	%rsp, 8(%rdi)   /* RSP */
	push	%rsi
	movq	%rbp, 16(%rdi)	/* RBP */
	movq	%r12, 24(%rdi)	/* R12 */
	movq	%r13, 32(%rdi)	/* R13 */
	movq	%r14, 40(%rdi)	/* R14 */
	movq	%r15, 48(%rdi)	/* R15 */
	movq	%rsi, 56(%rdi)	/* RIP */
	ret
#endif

/*
 * int grub_longjmp (grub_jmp_buf env, int val)
 */
FUNCTION(grub_longjmp)
#ifdef __MINGW32__
	movl	%edx, %eax
	orl	%eax, %eax
	jnz	1f
	incl	%eax
1:

	movq	(%rcx), %rbx
	movq	8(%rcx), %rsp
	movq	16(%rcx), %rbp
	movq	24(%rcx), %r12
	movq    32(%rcx), %r13
	movq    40(%rcx), %r14
	movq    48(%rcx), %r15
	jmp	*56(%rcx)
#else
	movl	%esi, %eax
	orl	%eax, %eax
	jnz	1f
	incl	%eax
1:

	movq	(%rdi), %rbx
	movq	8(%rdi), %rsp
	movq	16(%rdi), %rbp
	movq	24(%rdi), %r12
	movq    32(%rdi), %r13
	movq    40(%rdi), %r14
	movq    48(%rdi), %r15
	jmp	*56(%rdi)
#endif

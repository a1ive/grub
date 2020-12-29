/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2004,2007  Free Software Foundation, Inc.
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

#ifdef APPLE_CC

/*
 * int grub_setjmp (grub_jmp_buf env)
 */
FUNCTION(grub_setjmp)
	stw	r1, 0(r3)
	stw	r14, 4(r3)
	stw	r15, 8(r3)
	stw	r16, 12(r3)
	stw	r17, 16(r3)
	stw	r18, 20(r3)
	stw	r19, 24(r3)
	stw	r20, 28(r3)
	stw	r21, 32(r3)
	stw	r22, 36(r3)
	stw	r23, 40(r3)
	stw	r24, 44(r3)
	stw	r25, 48(r3)
	stw	r26, 52(r3)
	stw	r27, 56(r3)
	stw	r28, 60(r3)
	stw	r29, 64(r3)
	stw	r30, 68(r3)
	mflr	r4
	stw	r4, 72(r3)
	mfcr	r4
	stw	r4, 76(r3)
	li	r3, 0
	blr

/*
 * int grub_longjmp (grub_jmp_buf env, int val)
 */
FUNCTION(grub_longjmp)
	lwz	r1, 0(r3)
	lwz	r14, 4(r3)
	lwz	r15, 8(r3)
	lwz	r16, 12(r3)
	lwz	r17, 16(r3)
	lwz	r18, 20(r3)
	lwz	r19, 24(r3)
	lwz	r20, 28(r3)
	lwz	r21, 32(r3)
	lwz	r22, 36(r3)
	lwz	r23, 40(r3)
	lwz	r24, 44(r3)
	lwz	r25, 48(r3)
	lwz	r26, 52(r3)
	lwz	r27, 56(r3)
	lwz	r28, 60(r3)
	lwz	r29, 64(r3)
	lwz	r30, 68(r3)
	lwz	r5, 72(r3)
	mtlr	r5
	lwz	r5, 76(r3)
	mtcr	r5
	mr.	r3, r4
	bne	1f
	li	r3, 1
1:	blr

#else

/*
 * int grub_setjmp (grub_jmp_buf env)
 */
FUNCTION(grub_setjmp)
	stw	1, 0(3)
	stw	14, 4(3)
	stw	15, 8(3)
	stw	16, 12(3)
	stw	17, 16(3)
	stw	18, 20(3)
	stw	19, 24(3)
	stw	20, 28(3)
	stw	21, 32(3)
	stw	22, 36(3)
	stw	23, 40(3)
	stw	24, 44(3)
	stw	25, 48(3)
	stw	26, 52(3)
	stw	27, 56(3)
	stw	28, 60(3)
	stw	29, 64(3)
	stw	30, 68(3)
	mflr	4
	stw	4, 72(3)
	mfcr	4
	stw	4, 76(3)
	li	3, 0
	blr

/*
 * int grub_longjmp (grub_jmp_buf env, int val)
 */
FUNCTION(grub_longjmp)
	lwz	1, 0(3)
	lwz	14, 4(3)
	lwz	15, 8(3)
	lwz	16, 12(3)
	lwz	17, 16(3)
	lwz	18, 20(3)
	lwz	19, 24(3)
	lwz	20, 28(3)
	lwz	21, 32(3)
	lwz	22, 36(3)
	lwz	23, 40(3)
	lwz	24, 44(3)
	lwz	25, 48(3)
	lwz	26, 52(3)
	lwz	27, 56(3)
	lwz	28, 60(3)
	lwz	29, 64(3)
	lwz	30, 68(3)
	lwz	5, 72(3)
	mtlr	5
	lwz	5, 76(3)
	mtcr	5
	mr.	3, 4
	bne	1f
	li	3, 1
1:	blr

#endif

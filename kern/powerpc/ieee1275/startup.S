/* startup.S - Startup code for the PowerPC.  */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2003,2004,2005,2007,2008  Free Software Foundation, Inc.
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
#include <grub/cpu/kernel.h>

	.text
	.align	2

	.globl	_start, grub_code_start
_start:
grub_code_start:
	b	LOCAL(codestart)

	. = _start + GRUB_KERNEL_CPU_PREFIX

VARIABLE(grub_prefix)
	/* to be filled by grub-mkelfimage */

	/*
	 *  Leave some breathing room for the prefix.
	 */

	. = _start + GRUB_KERNEL_CPU_DATA_END

LOCAL(codestart):

#ifdef APPLE_CC

	li      r2, 0
	li      r13, 0

	/* Stage1 won't zero BSS for us. In other cases, why not do it again?  */
	lis	r6, hi16(grub_bss_start - 4)
	ori	r6, r6, lo16(grub_bss_start - 4)
	lis	r7, hi16(grub_bss_end - 4)
	ori	r7, r7, lo16(grub_bss_end - 4)
	subf	r7, r6, r7
	srwi	r7, r7, 2 /* We store 4 bytes at a time.  */
	mtctr	r7
2:	stwu	r2, 4(r6) /* We know r2 is already 0 from above.  */
	bdnz	2b

	/* Store r5 in grub_ieee1275_entry_fn.  */
	lis	r9, ha16(grub_ieee1275_entry_fn)
	stw	r5, lo16(grub_ieee1275_entry_fn)(r9)

	bl	grub_main
1:	b	1b

#else

	li      2, 0
	li      13, 0

	/* Stage1 won't zero BSS for us. In other cases, why not do it again?  */
	lis	6, (grub_bss_start - 4)@h
	ori	6, 6, (grub_bss_start - 4)@l
	lis	7, (grub_bss_end - 4)@h
	ori	7, 7, (grub_bss_end - 4)@l
	subf	7, 6, 7
	srwi	7, 7, 2 /* We store 4 bytes at a time.  */
	mtctr	7
2:	stwu	2, 4(6) /* We know r2 is already 0 from above.  */
	bdnz	2b

	/* Store r5 in grub_ieee1275_entry_fn.  */
	lis	9, grub_ieee1275_entry_fn@ha
	stw	5, grub_ieee1275_entry_fn@l(9)

	bl	grub_main
1:	b	1b

#endif

#if defined(__GNUC__) && (__GNUC__ > 3) && (__GNUC__ > 4 || __GNUC_MINOR__ >= 4)

/* Routines for saving integer registers, called by the compiler.  */
/* Called with r11 pointing to the stack header word of the caller of the */
/* function, just beyond the end of the integer save area.  */

GRUB_EXPORT_START
GRUB_EXPORT(_savegpr_14)
GRUB_EXPORT(_savegpr_15)
GRUB_EXPORT(_savegpr_16)
GRUB_EXPORT(_savegpr_17)
GRUB_EXPORT(_savegpr_18)
GRUB_EXPORT(_savegpr_19)
GRUB_EXPORT(_savegpr_20)
GRUB_EXPORT(_savegpr_21)
GRUB_EXPORT(_savegpr_22)
GRUB_EXPORT(_savegpr_23)
GRUB_EXPORT(_savegpr_24)
GRUB_EXPORT(_savegpr_25)
GRUB_EXPORT(_savegpr_26)
GRUB_EXPORT(_savegpr_27)
GRUB_EXPORT(_savegpr_28)
GRUB_EXPORT(_savegpr_29)
GRUB_EXPORT(_savegpr_30)
GRUB_EXPORT(_savegpr_31)

GRUB_EXPORT(_restgpr_14)
GRUB_EXPORT(_restgpr_15)
GRUB_EXPORT(_restgpr_16)
GRUB_EXPORT(_restgpr_17)
GRUB_EXPORT(_restgpr_18)
GRUB_EXPORT(_restgpr_19)
GRUB_EXPORT(_restgpr_20)
GRUB_EXPORT(_restgpr_21)
GRUB_EXPORT(_restgpr_22)
GRUB_EXPORT(_restgpr_23)
GRUB_EXPORT(_restgpr_24)
GRUB_EXPORT(_restgpr_25)
GRUB_EXPORT(_restgpr_26)
GRUB_EXPORT(_restgpr_27)
GRUB_EXPORT(_restgpr_28)
GRUB_EXPORT(_restgpr_29)
GRUB_EXPORT(_restgpr_30)
GRUB_EXPORT(_restgpr_31)

GRUB_EXPORT(_restgpr_14_x)
GRUB_EXPORT(_restgpr_15_x)
GRUB_EXPORT(_restgpr_16_x)
GRUB_EXPORT(_restgpr_17_x)
GRUB_EXPORT(_restgpr_18_x)
GRUB_EXPORT(_restgpr_19_x)
GRUB_EXPORT(_restgpr_20_x)
GRUB_EXPORT(_restgpr_21_x)
GRUB_EXPORT(_restgpr_22_x)
GRUB_EXPORT(_restgpr_23_x)
GRUB_EXPORT(_restgpr_24_x)
GRUB_EXPORT(_restgpr_25_x)
GRUB_EXPORT(_restgpr_26_x)
GRUB_EXPORT(_restgpr_27_x)
GRUB_EXPORT(_restgpr_28_x)
GRUB_EXPORT(_restgpr_29_x)
GRUB_EXPORT(_restgpr_30_x)
GRUB_EXPORT(_restgpr_31_x)
GRUB_EXPORT_END

FUNCTION(_savegpr_14)
	stw	14,-72(11)	/* save gp registers */
FUNCTION(_savegpr_15)
	stw	15,-68(11)
FUNCTION(_savegpr_16)
	stw	16,-64(11)
FUNCTION(_savegpr_17)
	stw	17,-60(11)
FUNCTION(_savegpr_18)
	stw	18,-56(11)
FUNCTION(_savegpr_19)
	stw	19,-52(11)
FUNCTION(_savegpr_20)
	stw	20,-48(11)
FUNCTION(_savegpr_21)
	stw	21,-44(11)
FUNCTION(_savegpr_22)
	stw	22,-40(11)
FUNCTION(_savegpr_23)
	stw	23,-36(11)
FUNCTION(_savegpr_24)
	stw	24,-32(11)
FUNCTION(_savegpr_25)
	stw	25,-28(11)
FUNCTION(_savegpr_26)
	stw	26,-24(11)
FUNCTION(_savegpr_27)
	stw	27,-20(11)
FUNCTION(_savegpr_28)
	stw	28,-16(11)
FUNCTION(_savegpr_29)
	stw	29,-12(11)
FUNCTION(_savegpr_30)
	stw	30,-8(11)
FUNCTION(_savegpr_31)
	stw	31,-4(11)
	blr

/* Routines for restoring integer registers, called by the compiler.  */
/* Called with r11 pointing to the stack header word of the caller of the */
/* function, just beyond the end of the integer restore area.  */

FUNCTION(_restgpr_14)
	lwz	14,-72(11)	/* restore gp registers */
FUNCTION(_restgpr_15)
	lwz	15,-68(11)
FUNCTION(_restgpr_16)
	lwz	16,-64(11)
FUNCTION(_restgpr_17)
	lwz	17,-60(11)
FUNCTION(_restgpr_18)
	lwz	18,-56(11)
FUNCTION(_restgpr_19)
	lwz	19,-52(11)
FUNCTION(_restgpr_20)
	lwz	20,-48(11)
FUNCTION(_restgpr_21)
	lwz	21,-44(11)
FUNCTION(_restgpr_22)
	lwz	22,-40(11)
FUNCTION(_restgpr_23)
	lwz	23,-36(11)
FUNCTION(_restgpr_24)
	lwz	24,-32(11)
FUNCTION(_restgpr_25)
	lwz	25,-28(11)
FUNCTION(_restgpr_26)
	lwz	26,-24(11)
FUNCTION(_restgpr_27)
	lwz	27,-20(11)
FUNCTION(_restgpr_28)
	lwz	28,-16(11)
FUNCTION(_restgpr_29)
	lwz	29,-12(11)
FUNCTION(_restgpr_30)
	lwz	30,-8(11)
FUNCTION(_restgpr_31)
	lwz	31,-4(11)
	blr

/* Routines for restoring integer registers, called by the compiler.  */
/* Called with r11 pointing to the stack header word of the caller of the */
/* function, just beyond the end of the integer restore area.  */

FUNCTION(_restgpr_14_x)
	lwz	14,-72(11)	/* restore gp registers */
FUNCTION(_restgpr_15_x)
	lwz	15,-68(11)
FUNCTION(_restgpr_16_x)
	lwz	16,-64(11)
FUNCTION(_restgpr_17_x)
	lwz	17,-60(11)
FUNCTION(_restgpr_18_x)
	lwz	18,-56(11)
FUNCTION(_restgpr_19_x)
	lwz	19,-52(11)
FUNCTION(_restgpr_20_x)
	lwz	20,-48(11)
FUNCTION(_restgpr_21_x)
	lwz	21,-44(11)
FUNCTION(_restgpr_22_x)
	lwz	22,-40(11)
FUNCTION(_restgpr_23_x)
	lwz	23,-36(11)
FUNCTION(_restgpr_24_x)
	lwz	24,-32(11)
FUNCTION(_restgpr_25_x)
	lwz	25,-28(11)
FUNCTION(_restgpr_26_x)
	lwz	26,-24(11)
FUNCTION(_restgpr_27_x)
	lwz	27,-20(11)
FUNCTION(_restgpr_28_x)
	lwz	28,-16(11)
FUNCTION(_restgpr_29_x)
	lwz	29,-12(11)
FUNCTION(_restgpr_30_x)
	lwz	30,-8(11)
FUNCTION(_restgpr_31_x)
	lwz	0,4(11)
	lwz	31,-4(11)
	mtlr    0
	mr	 1,11
	blr

#endif

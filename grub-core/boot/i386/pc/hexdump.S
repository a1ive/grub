/* Copyright 2010 Miray Software */

/*
 * Hexdump function that outputs over bios interrupts.
 * define SERIAL_DEBUG for output over com1
 * output buffer needs to be in %di
 * length in 16 byte units (output lines) in %cx  
 *
 */

#define N_SERIAL_DEBUG  1

#ifndef LOCAL
#define LOCAL(x) L_ ## x
#endif


/* Serial Port parameters
 *       AH = 00h
 *       AL = initializing parameters
 *           7 - 6 - 5          4 - 3     2    1 - 0
 *           -BAUD RATE-        PARITY   STOP   WORD
 *                                       BITS  LENGTH
 *           000 110/19200 bd   00 none  0: 1  00: 5
 *           001 150/38400 bd   01 odd   1: 2  01: 6
 *           010       300 bd   11 even        10: 7
 *           011       600 bd                  11: 8
 *           100      1200 bd
 *           101      2400 bd
 *           110      4800 bd
 *           111      9600 bd (4800 on PCjr)
 *       DX = port number (0-3 or FFh if only performing non-I/O setup)
 */

/*
 * Debug Hexdump method
 */
LOCAL(hexdump):
#ifdef SERIAL_DEBUG
	movw $0x00c7, %ax /* 4800, no parity, 1 stop, 8 data*/
	movw $0, %dx
	int $0x14
#endif

/* 	inc %cx */
1:
	movb $16, %dl	
	xor %dh, %dh
2:
	movb %es:(%di), %bl
	shr  $4,%bl
	and $0xf, %bl
	call LOCAL(charout_idx)
	movb %es:(%di), %bl
	and $0xf, %bl
	call LOCAL(charout_idx)
	movb $' ', %al
	call LOCAL(charout_chr)
	inc %di
	dec %dl
	jnz 2b

	movb $' ', %al
	call LOCAL(charout_chr)

	movw $16, %dx
	subw %dx, %di
3:
	movb %es:(%di), %al
	cmp $0x20, %al
	jge 4f
	movb $'.', %al
4:
	cmp $0x7f, %al
	jl 5f
	movb $'.', %al
5:
	call LOCAL(charout_chr)
#if 0
#ifdef SERIAL_DEBUG
	movw %dx, %bx
	movw	$0x0, %dx
	movb	$0x1, %ah
	int	$0x14
	movw %bx, %dx
#else
	movw	$0x0001, %bx
	movb	$0xe, %ah
	int	$0x10		/* display a byte */
#endif 
#endif
	inc %di
	dec %dl
	jnz 3b

	
	/* Print newline*/
	movb $'\r', %al
	call LOCAL(charout_chr)
	movb $'\n', %al
	call LOCAL(charout_chr)
6:
	loop 1b
	ret

LOCAL(charout_idx):
 	movw	$hexdump_characters, %si
	xor 	%bh, %bh
	movb    (%bx,%si), %al
LOCAL(charout_chr):
#ifdef SERIAL_DEBUG
	movw %dx, %bx
	movw	$0x0, %dx
	movb	$0x1, %ah
	int	$0x14
	movw %bx, %dx
#else
	movw	$0x0001, %bx
	movb	$0xe, %ah
	int	$0x10		/* display a byte */
#endif
	ret

hexdump_characters:	.asciz "0123456789ABCDEF"


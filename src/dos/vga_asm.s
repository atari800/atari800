/*
 * vga_asm.s - x86-assembler routines for DOS/VGA port
 *
 * Copyright (c) 1996 Ivo van Poorten
 * Copyright (c) 1996-2003 Atari800 development team (see DOC/CREDITS)
 *
 * This file is part of the Atari800 emulator project which emulates
 * the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.
 *
 * Atari800 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Atari800 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Atari800; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* read analog joystick 0 from atari_vga.c */
/* int joystick0(int *x, int *y) */
/* stack frame:
esp:	old_ebx
esp+4:	ret_addr
esp+8:	x
esp+12:	y
 */

.globl _joystick0
_joystick0:
	pushl %ebx
	cli
	movw $0x201, %dx
	xorl %ebx, %ebx
	xorl %ecx, %ecx
	outb %al, %dx
__jloop:
	inb %dx, %al
	testb $1, %al
	jz x_ok
	incl %ecx
x_ok:
	testb $2, %al
	jz y_ok
	incl %ebx
	jmp __jloop
y_ok:
	testb $1, %al
	jnz __jloop
	sti
	shrb $4, %al
	movl 8(%esp), %edx
	movl %ecx, (%edx)
	movl 12(%esp), %edx
	movl %ebx, (%edx)
	andl $3, %eax
	popl %ebx
	ret

/* from vga_gfx.c ---------------------------------------------------------- */

/* void x_open_asm(int mode) */
/* stack frame:
esp:	old_es
esp+2:	old_ebx
esp+6:	old_esi
esp+10:	old_edi
esp+14:	ret_addr
esp+18:	mode
 */
.globl _x_open_asm
_x_open_asm:
	pushl %edi
	pushl %esi
	pushl %ebx
	pushw %es

	movl 18(%esp), %ebx
	movw __go32_info_block+26, %es	/* __dos_ds */
	movw $0x03c4, %dx
	movb _x_clocktab(%ebx), %cl
	orb %cl, %cl
	je x_Skip_Reset

	movw $0x100, %ax
	outw %ax, %dx

	movw $0x3c2, %dx
	movb %cl, %al
	outb %al, %dx

	movw $0x3c4, %dx
	movw $0x300, %ax
	outw %ax, %dx
x_Skip_Reset:
	movw $0x604, %ax
	outw %ax, %dx

	cld
	movl $0xa0000, %edi
	xorl %eax, %eax
	movl $0x8000, %ecx
	rep
	stosw

	movw $0x3d4, %dx
	xorl %esi, %esi
	movw $3, %cx
	cmpb $5, %bl
	jbe x_CRT1
	movw $10, %cx
x_CRT1:
	movw _x_regtab(,%esi,2), %ax
	outw %ax, %dx
	incl %esi
	decw %cx
	jne x_CRT1

	movl _x_verttab(,%ebx,4),%esi
	cmpl $0, %esi
	je x_noerr
	cmpl $1, %esi
	je x_Set400

	movw $8, %cx
x_CRT2:
	movw (%esi), %ax
	outw %ax, %dx
	incl %esi
	incl %esi
	decl %ecx
	jne x_CRT2
	jmp x_noerr

x_Set400:
	movw $0x4009, %ax
	outw %ax, %dx
x_noerr:

	popw %es
	popl %ebx
	popl %esi
	popl %edi
	ret

/* bitmap blitting function */
/* void VESA_blit(void *mem,ULONG width,ULONG height,ULONG bitmapline,ULONG videoline,UWORD selector) */
/* stack frame:
esp:	old_es
esp+2:	old_esi
esp+6:	old_edi
esp+10:	ret_addr
esp+14:	mem
esp+18:	width
esp+22:	height
esp+26:	bitmapline
esp+30:	videoline
esp+34:	selector
 */
.globl _VESA_blit
_VESA_blit:
	pushl %edi
	pushl %esi
	pushw %es
	movw 34(%esp), %es
	movl 14(%esp), %esi
	xorl %edi, %edi
	movl 18(%esp), %edx
	subl %edx, 26(%esp)
	subl %edx, 30(%esp)
	shrl $4, %edx
	movl 22(%esp), %ecx
0:
	movl %edx, %eax
1:
	movsl
	movsl
	movsl
	movsl
	decl %eax
	jnz 1b
	addl 30(%esp), %edi
	addl 26(%esp), %esi
	decl %ecx
	jnz 0b
	popw %es
	popl %esi
	popl %edi
	ret

/* interlaced bitmap blitting function */
/* void VESA_i_blit(void *mem,ULONG width,ULONG height,ULONG bitmapline,ULONG videoline,UWORD selector) */
/* stack frame:
esp:	old_es
esp+2:	old_ebx
esp+6:	old_esi
esp+10:	old_edi
esp+14:	ret_addr
esp+18:	mem
esp+22:	width
esp+26:	height
esp+30:	bitmapline
esp+34:	videoline
esp+38:	selector
 */
.globl _VESA_i_blit
_VESA_i_blit:
	pushl %edi
	pushl %esi
	pushl %ebx
	pushw %es
	movw 38(%esp), %es
	movl 18(%esp), %esi
	xorl %edi, %edi
	movl 22(%esp), %ebx
	movl 34(%esp), %edx
	subl $4, %edx
	shll $1, 34(%esp)
	subl %ebx, 30(%esp)
	subl %ebx, 34(%esp)
	shrl $3, %ebx
	movl 26(%esp), %ecx
	shll $16, %ecx
	orl %ebx, %ecx
0:
	movb %cl, %ch
1:
	lodsl
	stosl
	movl %eax, %ebx
	andl $0xf0f0f0f0, %eax
	shrl $1, %ebx
	andl $0x07070707, %ebx
	orl %ebx, %eax
	movl %eax, %es:(%edi,%edx)
	lodsl
	stosl
	movl %eax, %ebx
	andl $0xf0f0f0f0, %eax
	shrl $1, %ebx
	andl $0x07070707, %ebx
	orl %ebx, %eax
	movl %eax, %es:(%edi,%edx)
	decb %ch
	jnz 1b
	addl 34(%esp), %edi
	addl 30(%esp), %esi
	subl $0x10000, %ecx
	testl $0xffff0000, %ecx
	jnz 0b
	popw %es
	popl %ebx
	popl %esi
	popl %edi
	ret

/* Procedure for copying atari screen to x-mode videomemory */
/* void x_blit(void *mem,UBYTE lines,ULONG change,ULONG videoline) */
/* stack frame:
esp:	old_es
esp+2:	old_ebx
esp+6:	old_esi
esp+10:	old_edi
esp+14:	ret_addr
esp+18:	mem
esp+22:	lines
esp+26:	change
esp+30:	videoline
 */
.globl _x_blit
_x_blit:
	pushl %edi
	pushl %esi
	pushl %ebx
	pushw %es
	movl 18(%esp), %esi
	movw __go32_info_block+26, %es	/* __dos_ds */
	subl $4, 26(%esp)

	movb $2, %bl
	movw $0x3c4, %dx
	cld

	movl $0xa0000, %edi
.ALIGN 4
1:
	movb $0x11, %bh
2:
	movw %bx, %ax
	outw %ax, %dx
	movl $20, %ecx
.ALIGN 4
3:
	movb (%esi), %al
	movb 4(%esi), %ah
	shrd $16, %eax, %eax
	movb 8(%esi), %al
	movb 12(%esi), %ah
	shrd $16, %eax, %eax
	addl $16, %esi
	stosl

	decl %ecx
	jne 3b

	subl $319, %esi
	subl $80, %edi
	addb %bh, %bh
	jnc 2b

	addl 26(%esp), %esi
	addl 30(%esp), %edi
	decl 22(%esp)
	jne 1b

	popw %es
	popl %ebx
	popl %esi
	popl %edi
	ret

/* Procedure for copying atari screen to x-mode videomemory interlaced with darker-lines (-video 3)*/
/* void x_blit(void *mem,UBYTE lines,ULONG change,ULONG videoline) */
/* stack frame:
esp:	old_es
esp+2:	old_ebx
esp+6:	old_esi
esp+10:	old_edi
esp+14:	ret_addr
esp+18:	mem
esp+22:	lines
esp+26:	change
esp+30:	videoline
 */
.globl _x_blit_i2
_x_blit_i2:
	pushl %edi
	pushl %esi
	pushl %ebx
	pushw %es
	movl 18(%esp), %esi
	movw __go32_info_block+26, %es	/* __dos_ds */
	subl $4, 26(%esp)

	movb $2, %bl
	cld

	movl $0xa0000, %edi
.ALIGN 4
1:
	movb $0x11, %bh
2:
	movw $0x3c4, %dx
	movw %bx, %ax
	outw %ax, %dx
	movl $20, %ecx
.ALIGN 4
3:
	movb (%esi), %al
	movb 4(%esi), %ah
	shrd $16, %eax, %eax
	movb 8(%esi), %al
	movb 12(%esi), %ah
	shrd $16, %eax, %eax
	addl $16, %esi
	movl %eax, %edx
	stosl
	shrl $1, %edx
	andl $0xf0f0f0f0, %eax
	andl $0x07070707, %edx
	orl %edx, %eax
/*	.BYTE 0x26 */
	movl %eax, %es:76(%edi)

	decl %ecx
	jne 3b

	subl $319, %esi
	subl $80, %edi
	addb %bh, %bh
	jnc 2b

	addl 26(%esp), %esi
	addl 30(%esp), %edi
	decl 22(%esp)
	jne 1b

	popw %es
	popl %ebx
	popl %esi
	popl %edi
	ret

/* this routine makes darker copy of buffer 'source' in buffer 'target' */
/* void make_darker(void *target,void *source,int bytes) */
/* stack frame:
esp:	old_esi
esp+4:	old_edi
esp+8:	ret_addr
esp+12:	target
esp+16:	source
esp+20:	bytes
 */
/* ALLEGRO support has been removed
.globl _make_darker
_make_darker:
	pushl %edi
	pushl %esi
	movl 12(%esp), %edi
	movl 16(%esp), %esi
	movl 20(%esp), %ecx
	subl $4, %edi
	shr $2, %ecx
0:
	movl (%esi), %eax
	movl %eax, %edx
	andl $0xf0f0f0f0, %eax
	shrl $1, %edx
	addl $4, %esi
	addl $4, %edi
	andl $0x07070707, %edx
	orl %edx, %eax
	decl %ecx
	movl %eax, (%edi)
	jnz 0b
	popl %esi
	popl %edi
	ret
*/

/* Vertical retrace control */
/* void v_ret(void) */
.globl _v_ret
_v_ret:
	movw $0x03da, %dx
1:
	inb %dx, %al
	testb $8, %al
	jnz	1b
2:
	inb %dx, %al
	testb $8, %al
	jz 2b
	ret

;  c2p_uni.asm - Atari Falcon specific port code
;
;  Copyright (c) 1997-1998 Petr Stehlik and Karel Rous
;  Copyright (c) 1998-2003 Atari800 development team (see DOC/CREDITS)
;  Copyright (c) 2004 Mikael Kalms
;
;  This file is part of the Atari800 emulator project which emulates
;  the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.
;
;  Atari800 is free software; you can redistribute it and/or modify
;  it under the terms of the GNU General Public License as published by
;  the Free Software Foundation; either version 2 of the License, or
;  (at your option) any later version.
;
;  Atari800 is distributed in the hope that it will be useful,
;  but WITHOUT ANY WARRANTY; without even the implied warranty of
;  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;  GNU General Public License for more details.
;
;  You should have received a copy of the GNU General Public License
;  along with Atari800; if not, write to the Free Software
;  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*-------------------------------------------------------*

	OPT		P=68040,L1,O+,W-
	output		c2p_uni.o
*-------------------------------------------------------*

*-------------------------------------------------------*
*	external variables				*
*-------------------------------------------------------*

	xref		_screenw,_screenh
	xref		_vramw,_vramh
	xref		_odkud,_kam

*-------------------------------------------------------*
*	General functions				*
*-------------------------------------------------------*

	xdef		_rplanes

pushall	macro
	movem.l		d2-d7/a2-a6,-(sp)
	endm

popall	macro
	movem.l		(sp)+,d2-d7/a2-a6
	endm

_rplanes:
*-------------------------------------------------------*
	pushall
*-------------------------------------------------------*
	move.l		sp,old_sp

	move.l		_odkud,a0
	move.l		_kam,a1
	clr.l		d0

; centering of view at screen
	move.w		#384,d0		; width of Atari800 emulated screen
	sub.w		_screenw,d0	; width  of active screen area
	move.l		d0,src_line_offset
	lsr.w		#1,d0
	add.l		d0,a0

; centering of screen in videoram in horizontal axis
	move.w		_vramw,d0	; width of screen area
	sub.w		_screenw,d0	; width  of active screen area
	move.l		d0,dst_line_offset
	lsr.w		#1,d0
	and.b		#%11110000,d0	; make sure intial offset % 16 == 0
	add.l		d0,a1

; centering of screen in videoram in vertical axis
	move.w		_vramh,d1	; height of screen area
	sub.w		_screenh,d1	; height of active screen area
	lsr.w		#1,d1
	mulu.w		_vramw,d1	; width of screen area
	add.l		d1,a1

; precompute end of line addresses
	move.w		_screenw,d0	; width  of active screen area
	lea		(a0,d0.l),a2	; end of src line
	lea		(a1,d0.l),a7	; end of dst line

; precompute src end address
	move.l		#384*240,d0
	add.l		a0,d0
	move.l		d0,src_end_address

	move.l	#$0f0f0f0f,d4
	move.l	#$00ff00ff,d5
	move.l	#$55555555,d6

	move.l	(a0)+,d0
	move.l	(a0)+,d1
	move.l	(a0)+,d2
	move.l	(a0)+,d3

	move.l	d1,d7
	lsr.l	#4,d7
	eor.l	d0,d7
	and.l	d4,d7
	eor.l	d7,d0
	lsl.l	#4,d7
	eor.l	d7,d1
	move.l	d3,d7
	lsr.l	#4,d7
	eor.l	d2,d7
	and.l	d4,d7
	eor.l	d7,d2
	lsl.l	#4,d7
	eor.l	d7,d3

	move.l	d2,d7
	lsr.l	#8,d7
	eor.l	d0,d7
	and.l	d5,d7
	eor.l	d7,d0
	lsl.l	#8,d7
	eor.l	d7,d2
	move.l	d3,d7
	lsr.l	#8,d7
	eor.l	d1,d7
	and.l	d5,d7
	eor.l	d7,d1
	lsl.l	#8,d7
	eor.l	d7,d3

	bra.s	.start

.pix16:	move.l	(a0)+,d0
	move.l	(a0)+,d1
	move.l	(a0)+,d2
	move.l	(a0)+,d3

	move.l	d1,d7
	lsr.l	#4,d7
	move.l	a3,(a1)+
	eor.l	d0,d7
	and.l	d4,d7
	eor.l	d7,d0
	lsl.l	#4,d7
	eor.l	d7,d1
	move.l	d3,d7
	lsr.l	#4,d7
	eor.l	d2,d7
	and.l	d4,d7
	eor.l	d7,d2
	move.l	a4,(a1)+
	lsl.l	#4,d7
	eor.l	d7,d3

	move.l	d2,d7
	lsr.l	#8,d7
	eor.l	d0,d7
	and.l	d5,d7
	eor.l	d7,d0
	move.l	a5,(a1)+
	lsl.l	#8,d7
	eor.l	d7,d2
	move.l	d3,d7
	lsr.l	#8,d7
	eor.l	d1,d7
	and.l	d5,d7
	eor.l	d7,d1
	move.l	a6,(a1)+
	lsl.l	#8,d7
	eor.l	d7,d3

	cmp.l	a1,a7			; end of dst line?
	bne.b	.start

	add.l	(dst_line_offset,pc),a1
	adda.w	_vramw,a7

.start:	move.l	d2,d7
	lsr.l	#1,d7
	eor.l	d0,d7
	and.l	d6,d7
	eor.l	d7,d0
	add.l	d7,d7
	eor.l	d7,d2
	move.l	d3,d7
	lsr.l	#1,d7
	eor.l	d1,d7
	and.l	d6,d7
	eor.l	d7,d1
	add.l	d7,d7
	eor.l	d7,d3

	move.w	d2,d7
	move.w	d0,d2
	swap	d2
	move.w	d2,d0
	move.w	d7,d2
	move.w	d3,d7
	move.w	d1,d3
	swap	d3
	move.w	d3,d1
	move.w	d7,d3

	move.l	d2,d7
	lsr.l	#2,d7
	eor.l	d0,d7
	and.l	#$33333333,d7
	eor.l	d7,d0
	lsl.l	#2,d7
	eor.l	d7,d2
	move.l	d3,d7
	lsr.l	#2,d7
	eor.l	d1,d7
	and.l	#$33333333,d7
	eor.l	d7,d1
	lsl.l	#2,d7
	eor.l	d7,d3

	swap	d0
	swap	d1
	swap	d2
	swap	d3

	move.l	d0,a6
	move.l	d2,a5
	move.l	d1,a4
	move.l	d3,a3

	cmp.l	a0,a2			; end of src line?
	bne	.pix16

	add.l	(src_line_offset,pc),a0
	lea	(384,a2),a2

	cmp.l	(src_end_address,pc),a0
	bne	.pix16

	move.l	a3,(a1)+
	move.l	a4,(a1)+
	move.l	a5,(a1)+
	move.l	a6,(a1)+

	move.l	old_sp,sp
*-------------------------------------------------------*
	popall
*-------------------------------------------------------*
	rts

; don't put these into data/bss as it can be more than 32K away
			cnop	0,16
src_line_offset:	ds.l	1
dst_line_offset:	ds.l	1
src_end_address:	ds.l	1

*-------------------------------------------------------*
			bss
*-------------------------------------------------------*
old_sp:			ds.l	1
*-------------------------------------------------------*

;  c2p_unid.asm - Atari Falcon specific port code
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
	output		c2p_unid.o
*-------------------------------------------------------*

*-------------------------------------------------------*
*	external variables				*
*-------------------------------------------------------*

	xref		_screenw,_screenh
	xref		_vramw,_vramh
	xref		_odkud,_kam
	xref		_oldscreen

*-------------------------------------------------------*
*	General functions				*
*-------------------------------------------------------*

	xdef		_rplanes_delta

pushall	macro
	movem.l		d2-d7/a2-a6,-(sp)
	endm

popall	macro
	movem.l		(sp)+,d2-d7/a2-a6
	endm

_rplanes_delta:
*-------------------------------------------------------*
	pushall
*-------------------------------------------------------*
	move.l		_odkud,a0
	move.l		_kam,a1
	move.l		_oldscreen,a2
	clr.l		d0

; centering of view at screen
	move.w		#384,d0		; width of Atari800 emulated screen
	sub.w		_screenw,d0	; width  of active screen area
	move.l		d0,a3
	lsr.w		#1,d0
	add.l		d0,a0
	add.l		d0,a2

; centering of screen in videoram in horizontal axis
	move.w		_vramw,d0	; width of screen area
	sub.w		_screenw,d0	; width  of active screen area
	move.l		d0,a4
	lsr.w		#1,d0
	and.b		#%11110000,d0	; make sure intial offset % 16 == 0
	add.l		d0,a1

; centering of screen in videoram in vertical axis
	move.w		_vramh,d1	; height of screen area
	sub.w		_screenh,d1	; height of active screen area
	lsr.w		#1,d1
	mulu.w		_vramw,d1	; width of screen area
	add.l		d1,a1

; precompute end of line address
	move.w		_screenw,d0	; width  of active screen area
	lea		(a0,d0.l),a5

; precompute src end address
	lea		(384*240.l,a0),a6

	move.l	#$0f0f0f0f,d4
	move.l	#$00ff00ff,d5
	move.l	#$55555555,d6

.pix16:	move.l	(a0)+,d0
	cmp.l	(a2)+,d0
	bne.b	.doit3
	move.l	(a0)+,d1
	cmp.l	(a2)+,d1
	bne.b	.doit2
	move.l	(a0)+,d2
	cmp.l	(a2)+,d2
	bne.b	.doit1
	move.l	(a0)+,d3
	cmp.l	(a2)+,d3
	bne.b	.doit0

	lea	(16,a1),a1
	bra	.done

.doit3:	move.l	(a0)+,d1
	addq.l	#4,a2
.doit2:	move.l	(a0)+,d2
	addq.l	#4,a2
.doit1:	move.l	(a0)+,d3
	addq.l	#4,a2
.doit0:
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

	move.l	d2,d7
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

	swap	d3
	move.l	d3,(a1)+
	swap	d1
	move.l	d1,(a1)+
	swap	d2
	move.l	d2,(a1)+
	swap	d0
	move.l	d0,(a1)+

.done:	cmp.l	a0,a5			; end of line?
	bne	.pix16

	add.l	a3,a0
	add.l	a4,a1
	add.l	a3,a2
	lea	(384,a5),a5

	cmp.l	a0,a6			; end of screen?
	bne	.pix16
*-------------------------------------------------------*
	popall
*-------------------------------------------------------*
	rts

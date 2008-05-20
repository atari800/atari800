|  c2p_unid.S - Atari Falcon chunky to planar DELTA conversion
|
|  Copyright (c) 1996 Douglas Little
|  Copyright (c) 1997-1998 Petr Stehlik and Karel Rous
|  Copyright (c) 1998-2008 Atari800 development team (see DOC/CREDITS)
|
|  This file is part of the Atari800 emulator project which emulates
|  the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.
|
|  Atari800 is free software; you can redistribute it and/or modify
|  it under the terms of the GNU General Public License as published by
|  the Free Software Foundation; either version 2 of the License, or
|  (at your option) any later version.
|
|  Atari800 is distributed in the hope that it will be useful,
|  but WITHOUT ANY WARRANTY; without even the implied warranty of
|  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
|  GNU General Public License for more details.
|
|  You should have received a copy of the GNU General Public License
|  along with Atari800; if not, write to the Free Software
|  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*-------------------------------------------------------*

*-------------------------------------------------------*
*	external variables				*
*-------------------------------------------------------*

	.globl		_screenw,_screenh
	.globl		_vramw,_vramh
	.globl		_odkud,_kam
	.globl		_oldscreen

*-------------------------------------------------------*
*	General functions				*
*-------------------------------------------------------*

	.globl		_rplanes_delta_init
	.globl		_rplanes_delta

*-------------------------------------------------------*
	.include	"falcon/c2pmac.S"
*-------------------------------------------------------*

*-------------------------------------------------------*
*	Initialise rendering display			*
*-------------------------------------------------------*
_rplanes_delta_init:
*-------------------------------------------------------*
	rts
*-------------------------------------------------------*
*-------------------------------------------------------*
*	display	conversion				*
*-------------------------------------------------------*
_rplanes_delta:
*-------------------------------------------------------*
	movem.l		d2-d7/a2-a5,-(sp)
*-------------------------------------------------------*

	move.l		_odkud,a0
	move.l		_kam,a1
	move.l		_oldscreen,a2

| centering of view at screen 
	move.w		#384,d0		| width of Atari800 emulated screen
	sub.w		_screenw,d0	| width of displayed screen
	movea.w		d0,a3
	lsr.w		#1,d0		| centering
	lea		(a0,d0.w),a0	| offset 24 or 32 pixels
	lea		(a2,d0.w),a2	| offset 24 or 32 pixels

| centering of screen in videoram in horizontal axis
	move.w		_vramw,d0
	sub.w		_screenw,d0
	movea.w		d0,a4
	lsr.w		#1,d0
	neg.w		d0
	lea		(a1,d0.w),a1	| negative pre-offset (will be OK at rplanes_delta_ylp)

| centering of screen in videoram in vertical axis
	move.w		_vramh,d0
	sub.w		_screenh,d0
	lsr.w		#1,d0
	move.w		_vramw,d1
	mulu		d1,d0
	lea		(a1,d0.l),a1

| precompute line width in long words
	move.w		_screenw,d0
	lsr.w		#4,d0
	subq.w		#1,d0
	movea.w		d0,a5

*-------------------------------------------------------*
	move.w		_screenh,d6
	subq.w		#1,d6
	move.w		a5,d5
	nop			| for cache alignment	
*-------------------------------------------------------*
rplanes_delta_xlp:	
	move.l		(a0)+,d4
	move.l		(a0)+,d3
	move.l		(a0)+,d2
	move.l		(a0)+,d1
*-------------------------------------------------------*
	moveq		#12,d0
	cmp.l		(a2)+,d4
	bne.s		rplanes_delta_doit
	moveq		#8,d0
	cmp.l		(a2)+,d3
	bne.s		rplanes_delta_doit
	moveq		#4,d0
	cmp.l		(a2)+,d2
	bne.s		rplanes_delta_doit
	cmp.l		(a2)+,d1
	bne.s		rplanes_delta_doit2
	lea		16(a1),a1
	dbra		d5,rplanes_delta_xlp
	adda.l		a3,a0
	adda.l		a3,a2
	adda.l		a4,a1
	move.w		a5,d5
	dbra		d6,rplanes_delta_xlp
	bra		rplanes_delta_none
rplanes_delta_doit:
	adda.l		d0,a2
rplanes_delta_doit2:
*-------------------------------------------------------*
	move.l		#0x00FF00FF,d0	| 4
	splice 8	d4 d2 d0 d7	| 18
	splice 8	d3 d1 d0 d7	| 18
*-------------------------------------------------------*
	move.l		#0x0F0F0F0F,d0	| 4
	splice 4	d4 d3 d0 d7	| 18
	splice 4	d2 d1 d0 d7	| 18
*-------------------------------------------------------*
	swap		d3		| 4(4:0)
	swap		d1		| 4(4:0)
	eor.w		d4,d3		| 2(2:0)
	eor.w		d2,d1		| 2(2:0)
	eor.w		d3,d4		| 2(2:0)
	eor.w		d1,d2		| 2(2:0)
	eor.w		d4,d3		| 2(2:0)
	eor.w		d2,d1		| 2(2:0)
	swap		d3		| 4(4:0)
	swap		d1		| 4(4:0)
*-------------------------------------------------------*
	move.l		#0x33333333,d0	| 4
	splice 2	d4 d3 d0 d7	| 18
	splice 2	d2 d1 d0 d7	| 18
*-------------------------------------------------------*
	move.l		#0x55555555,d0	| 4
	splice 1	d4 d2 d0 d7	| 18
	splice 1	d3 d1 d0 d7	| 18
*-------------------------------------------------------*
*	32-bit destination				*
*-------------------------------------------------------*
	swap		d1		| 4(4:0)
	eor.w		d3,d1		| 2(2:0)
	eor.w		d1,d3		| 2(2:0)
	eor.w		d3,d1		| 2(2:0)
	swap		d3		| 4(4:0)
	swap		d2		| 4(4:0)
	eor.w		d4,d2		| 2(2:0)
	eor.w		d2,d4		| 2(2:0)
	eor.w		d4,d2		| 2(2:0)
	swap		d4		| 4(4:0)
*-------------------------------------------------------*
	move.l		d1,(a1)+
 	move.l		d2,(a1)+
	move.l		d3,(a1)+
	move.l		d4,(a1)+
*-------------------------------------------------------*
	dbra		d5,rplanes_delta_xlp
	adda.l		a3,a0
	adda.l		a3,a2
	adda.l		a4,a1
	move.w		a5,d5
	dbra		d6,rplanes_delta_xlp
*-------------------------------------------------------*
rplanes_delta_none:
*-------------------------------------------------------*
	movem.l		(sp)+,d2-d7/a2-a5
*-------------------------------------------------------*
	rts
*-------------------------------------------------------*

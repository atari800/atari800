|  c2p_uni.S - Atari Falcon chunky to planar conversion
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

*-------------------------------------------------------*
*	Initialisation functions			*
*-------------------------------------------------------*

	.globl		_screenw,_screenh
	.globl		_vramw,_vramh
	.globl		_odkud,_kam

*-------------------------------------------------------*
*	General functions				*
*-------------------------------------------------------*

	.globl		_rplanes

*-------------------------------------------------------*
	.include	"falcon/c2pmac.S"
*-------------------------------------------------------*

*-------------------------------------------------------*
*	Initialise rendering display			*
*-------------------------------------------------------*
_rplanes:
*-------------------------------------------------------*
	movem.l		d0-a6,-(sp)
*-------------------------------------------------------*
			
*-------------------------------------------------------*
|	.set	rplanes_local_regs,0+0
*-------------------------------------------------------*
|	.set	rplanes_local_rts,0+60
*-------------------------------------------------------*
	move.l		_odkud,a0
	move.l		_kam,a1

| centering of view at screen 
	move.w		#384,d0		| width of Atari800 emulated screen
	sub.w		_screenw,d0	| width of displayed screen
	move.w		d0,src_line_offset
	lsr.w		#1,d0		| centering
	lea		(a0,d0.w),a0	| offset 24 or 32 pixels

| centering of screen in videoram in horizontal axis
	move.w		_vramw,d0
	sub.w		_screenw,d0
	move.w		d0,dst_line_offset
	lsr.w		#1,d0
	neg.w		d0
	lea		(a1,d0.w),a1	| negative pre-offset (will be OK at rplanes_ylp)

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
	move.w		d0,line_long_width

*-------------------------------------------------------*
	movem.l		(a0)+,d1-d4
*-------------------------------------------------------*
	move.l		#0x00FF00FF,d0	| 4
	splice 8	d1 d3 d0 d7	| 18
	splice 8	d2 d4 d0 d7	| 18
*-------------------------------------------------------*
	move.l		#0x0F0F0F0F,d0	| 4
	splice 4	d1 d2 d0 d7	| 18
	splice 4	d3 d4 d0 d7	| 18
*-------------------------------------------------------*
	swap		d2		| 4(4:0)
	swap		d4		| 4(4:0)
	eor.w		d1,d2		| 2(2:0)
	eor.w		d3,d4		| 2(2:0)
	eor.w		d2,d1		| 2(2:0)
	eor.w		d4,d3		| 2(2:0)
	eor.w		d1,d2		| 2(2:0)
	eor.w		d3,d4		| 2(2:0)
	swap		d2		| 4(4:0)
	swap		d4		| 4(4:0)
*-------------------------------------------------------*
	move.l		#0x33333333,d0	| 4
	splice 2	d1 d2 d0 d7	| 18
	splice 2	d3 d4 d0 d7	| 18
*-------------------------------------------------------*
	move.l		#0x55555555,d0	| 4
	splice 1	d1 d3 d0 d7	| 18
	splice 1	d2 d4 d0 d7	| 18
*-------------------------------------------------------*
*	32-bit destination				*
*-------------------------------------------------------*
	swap		d4		| 4(4:0)
	eor.w		d2,d4		| 2(2:0)
	eor.w		d4,d2		| 2(2:0)
	eor.w		d2,d4		| 2(2:0)
	swap		d2		| 4(4:0)
	swap		d3		| 4(4:0)
	eor.w		d1,d3		| 2(2:0)
	eor.w		d3,d1		| 2(2:0)
	eor.w		d1,d3		| 2(2:0)
	swap		d1		| 4(4:0)
*-------------------------------------------------------*
	move.l		d4,a2
	move.l		d3,a3
	move.l		d2,a4
	move.l		d1,a5
*-------------------------------------------------------*
	move.w		_screenh,d6
	subq.w		#1,d6
*-------------------------------------------------------*
rplanes_ylp:	move.w		line_long_width,d5
	move.w		dst_line_offset,d0
	lea		(a1,d0.w),a1
*-------------------------------------------------------*
rplanes_xlp:	tst.w		d5
	bne.s		rplanes_nono
	move.w		src_line_offset,d0
	lea		(a0,d0.w),a0	| offset D0 pixels to beginning of next line
rplanes_nono:	movem.l		(a0)+,d1-d4
*-------------------------------------------------------*
	move.l		#0x00FF00FF,d0	| 4
	splice 8	d1 d3 d0 d7	| 18
	splice 8	d2 d4 d0 d7	| 18
*-------------------------------------------------------*
	move.l		a2,(a1)+
*-------------------------------------------------------*
	move.l		#0x0F0F0F0F,d0	| 4
	splice 4	d1 d2 d0 d7	| 18
	splice 4	d3 d4 d0 d7	| 18
*-------------------------------------------------------*
	move.l		a3,(a1)+
*-------------------------------------------------------*
	swap		d2		| 4(4:0)
	swap		d4		| 4(4:0)
	eor.w		d1,d2		| 2(2:0)
	eor.w		d3,d4		| 2(2:0)
	eor.w		d2,d1		| 2(2:0)
	eor.w		d4,d3		| 2(2:0)
	eor.w		d1,d2		| 2(2:0)
	eor.w		d3,d4		| 2(2:0)
	swap		d2		| 4(4:0)
	swap		d4		| 4(4:0)
*-------------------------------------------------------*
	move.l		#0x33333333,d0	| 4
	splice 2	d1 d2 d0 d7	| 18
	splice 2	d3 d4 d0 d7	| 18
*-------------------------------------------------------*
	move.l		a4,(a1)+
*-------------------------------------------------------*
	move.l		#0x55555555,d0	| 4
	splice 1	d1 d3 d0 d7	| 18
	splice 1	d2 d4 d0 d7	| 18
*-------------------------------------------------------*
	move.l		a5,(a1)+
*-------------------------------------------------------*
*	32-bit destination				*
*-------------------------------------------------------*
	swap		d4		| 4(4:0)
	eor.w		d2,d4		| 2(2:0)
	eor.w		d4,d2		| 2(2:0)
	eor.w		d2,d4		| 2(2:0)
	swap		d2		| 4(4:0)
	swap		d3		| 4(4:0)
	eor.w		d1,d3		| 2(2:0)
	eor.w		d3,d1		| 2(2:0)
	eor.w		d1,d3		| 2(2:0)
	swap		d1		| 4(4:0)
*-------------------------------------------------------*
	move.l		d4,a2
	move.l		d3,a3
	move.l		d2,a4
	move.l		d1,a5
*-------------------------------------------------------*
	dbra		d5,rplanes_xlp
|	tst.w		d6
|	beq.s		rplanes_none
	dbra		d6,rplanes_ylp
*-------------------------------------------------------*
|rplanes_none:	move.l		a2,(a1)+
|	move.l		a3,(a1)+
|	move.l		a4,(a1)+
|	move.l		a5,(a1)+
*-------------------------------------------------------*
	movem.l		(sp)+,d0-a6
*-------------------------------------------------------*
	rts

*-------------------------------------------------------*
			.bss
*-------------------------------------------------------*
src_line_offset:	ds.w	1
dst_line_offset:	ds.w	1
line_long_width:	ds.w	1
*-------------------------------------------------------*

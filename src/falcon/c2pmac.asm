;  c2pmac.asm - Atari Falcon specific port code
;
;  Copyright (c) 1997-1998 Petr Stehlik and Karel Rous
;  Copyright (c) 1998-2003 Atari800 development team (see DOC/CREDITS)
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

splice1	macro				; data1,data2,mask,temp1,temp2
	move.l		\1,\5  		; 2(2:0) (1:1) AABBCCDD
	move.l		\2,\4  		; 2(2:0) (1:1) EEFFGGHH
	and.l		\3,\2 		; 2(2:0) (1:1) ..FF..HH
	eor.l		\2,\4 		; 2(2:0) (1:1) EE..GG..
	lsr.l		#\0,\4 		; 4(4:0) (1:2) ..EE..GG
	eor.l		\4,\1  		; 2(2:0) (1:1) AABBCCDD^..EE..GG
	and.l		\3,\5  		; 2(2:0) (1:1) ..BB..DD
	eor.l		\5,\1  		; 2(2:0) (1:1) AAEECCGG (AABBCCDD^..EE..GG^..BB..DD)
	lsl.l		#\0,\5		; 4(4:0) (1:2) BB..DD..
	add.l		\5,\2  		; 2(2:0) (1:1) BBFFDDHH
	endm				; 10 ops / 20 bytes / 68030=24(24:0) / 68040=(10:12)

splice2	macro				; data1,data2,mask,temp
	ror.l		#\0,\2 		; 6(4:0) (1:3) AABBCCDD HHEEFFGG
	move.l		\2,\4		; 2(2:0) (1:1) HHEEFFGG
	eor.l		\1,\4		; 2(2:0) (1:1) HHEEFFGG^AABBCCDD
	and.l		\3,\4		; 2(2:0) (1:1) ..EE..GG^..BB..DD
	eor.l		\4,\1		; 2(2:0) (1:1) AAEECCGG (AABBCCDD^..EE..GG^..BB..DD)
	eor.l		\4,\2		; 2(2:0) (1:1) HHBBFFDD (HHEEFFGG^..EE..GG^..BB..DD)
	rol.l		#\0,\2		; 6(4:0) (1:3) BBFFDDHH
	endm				; 7 ops / 14 bytes / 68030=22(18:0) / 68040=(7:11)

splice3	macro				; data1,data2,mask,temp
	move.l		\2,\4		; 2(2:0) (1:1) EEFFGGHH
	lsr.l		#\0,\4 		; 4(4:0) (1:2) ..EEFFGG
	eor.l		\1,\4		; 2(2:0) (1:1) ..EEFFGG^AABBCCDD
	and.l		\3,\4		; 2(2:0) (1:1) ..EE..GG^..BB..DD
	eor.l		\4,\1		; 2(2:0) (1:1) AAEECCGG (AABBCCDD^..EE..GG^..BB..DD)
	lsl.l		#\0,\4 		; 4(4:0) (1:2) EE..GG..^BB..DD..
	eor.l		\4,\2		; 2(2:0) (1:1) BBFFDDHH (EEFFGGHH^EE..GG..^BB..DD..)
	endm				; 7 ops / 14 bytes / 68030=18(18:0) / 68040=(7:9)

splice	macro				; data1,data2,mask,temp1[,temp2]
	splice3.\0	\1,\2,\3,\4
	endm

*-------------------------------------------------------*
*	Byte-per-pixel -> bitplane transform macro	*
*-------------------------------------------------------*
*	Converts 16 pixels per instance			*
*-------------------------------------------------------*
chars2planes		macro
*-------------------------------------------------------*
	movem.l		\1,d1-d4
*-------------------------------------------------------*
	move.l		#$00FF00FF,d0	; 4
	splice.8	d1,d3,d0,d7	; 18
	splice.8	d2,d4,d0,d7	; 18
*-------------------------------------------------------*
	move.l		#$0F0F0F0F,d0	; 4
	splice.4	d1,d2,d0,d7	; 18
	splice.4	d3,d4,d0,d7	; 18
*-------------------------------------------------------*
	swap		d2		; 4(4:0)
	swap		d4		; 4(4:0)
	eor.w		d1,d2		; 2(2:0)
	eor.w		d3,d4		; 2(2:0)
	eor.w		d2,d1		; 2(2:0)
	eor.w		d4,d3		; 2(2:0)
	eor.w		d1,d2		; 2(2:0)
	eor.w		d3,d4		; 2(2:0)
	swap		d2		; 4(4:0)
	swap		d4		; 4(4:0)
*-------------------------------------------------------*
	move.l		#$33333333,d0	; 4
	splice.2	d1,d2,d0,d7	; 18
	splice.2	d3,d4,d0,d7	; 18
*-------------------------------------------------------*
	move.l		#$55555555,d0	; 4
	splice.1	d1,d3,d0,d7	; 18
	splice.1	d2,d4,d0,d7	; 18
*-------------------------------------------------------*
	ifeq		'\0'-'w'
*-------------------------------------------------------*
*	16-bit destination				*
*-------------------------------------------------------*
	move.w		d4,\2
	swap		d4		; 4(4:0)
	move.w		d2,\2
	swap		d2		; 4(4:0)
	move.w		d3,\2
	swap		d3		; 4(4:0)
	move.w		d1,\2
	swap		d1		; 4(4:0)
	move.w		d4,\2
	move.w		d2,\2
	move.w		d3,\2
	move.w		d1,\2
*-------------------------------------------------------*
	elseif
*-------------------------------------------------------*
*	32-bit destination				*
*-------------------------------------------------------*
	swap		d4		; 4(4:0)
	eor.w		d2,d4		; 2(2:0)
	eor.w		d4,d2		; 2(2:0)
	eor.w		d2,d4		; 2(2:0)
	move.l		d4,\2
	swap		d3		; 4(4:0)
	eor.w		d1,d3		; 2(2:0)
	eor.w		d3,d1		; 2(2:0)
	eor.w		d1,d3		; 2(2:0)
	move.l		d3,\2
	swap		d2		; 4(4:0)
	move.l		d2,\2
	swap		d1		; 4(4:0)
	move.l		d1,\2
*-------------------------------------------------------*
	endc
*-------------------------------------------------------*
	endm
*-------------------------------------------------------*

*-------------------------------------------------------*
*	Byte-per-pixel -> bitplane transform macro	*
*-------------------------------------------------------*
*	Converts 16 pixels per instance			*
*-------------------------------------------------------*
chars2planesr		macro
*-------------------------------------------------------*
	move.l		#$00FF00FF,d0	; 4
	splice.8	d1,d3,d0,d7	; 18
	splice.8	d2,d4,d0,d7	; 18
*-------------------------------------------------------*
	move.l		#$0F0F0F0F,d0	; 4
	splice.4	d1,d2,d0,d7	; 18
	splice.4	d3,d4,d0,d7	; 18
*-------------------------------------------------------*
	swap		d2		; 4(4:0)
	swap		d4		; 4(4:0)
	eor.w		d1,d2		; 2(2:0)
	eor.w		d3,d4		; 2(2:0)
	eor.w		d2,d1		; 2(2:0)
	eor.w		d4,d3		; 2(2:0)
	eor.w		d1,d2		; 2(2:0)
	eor.w		d3,d4		; 2(2:0)
	swap		d2		; 4(4:0)
	swap		d4		; 4(4:0)
*-------------------------------------------------------*
	move.l		#$33333333,d0	; 4
	splice.2	d1,d2,d0,d7	; 18
	splice.2	d3,d4,d0,d7	; 18
*-------------------------------------------------------*
	move.l		#$55555555,d0	; 4
	splice.1	d1,d3,d0,d7	; 18
	splice.1	d2,d4,d0,d7	; 18
*-------------------------------------------------------*
	endm
*-------------------------------------------------------*

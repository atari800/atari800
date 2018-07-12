/*
 *  c2pmac.asm - Atari Falcon specific port code
 *
 *  Copyright (c) 1997-1998 Petr Stehlik and Karel Rous
 *  Copyright (c) 1998-2003 Atari800 development team (see DOC/CREDITS)
 *
 *  This file is part of the Atari800 emulator project which emulates
 *  the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.
 *
 *  Atari800 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Atari800 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Atari800; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define splice1(shift,data1,data2,mask,temp1,temp2) \
	move.l		data1,temp2  	/* 2(2:0) (1:1) AABBCCDD */ \
	move.l		data2,temp1  	/* 2(2:0) (1:1) EEFFGGHH */ \
	and.l		mask,data2 	/* 2(2:0) (1:1) ..FF..HH */ \
	eor.l		data2,temp1 	/* 2(2:0) (1:1) EE..GG.. */ \
	lsr.l		shift,temp1 	/* 4(4:0) (1:2) ..EE..GG */ \
	eor.l		temp1,data1  	/* 2(2:0) (1:1) AABBCCDD^..EE..GG */ \
	and.l		mask,temp2  	/* 2(2:0) (1:1) ..BB..DD */ \
	eor.l		temp2,data1  	/* 2(2:0) (1:1) AAEECCGG (AABBCCDD^..EE..GG^..BB..DD) */ \
	lsl.l		shift,temp2	/* 4(4:0) (1:2) BB..DD.. */ \
	add.l		temp2,data2  	/* 2(2:0) (1:1) BBFFDDHH */ \
					/* 10 ops / 20 bytes / 68030=24(24:0) / 68040=(10:12) */

#define splice2(shift,data1,data2,mask,temp) \
	ror.l		shift,data2 	/* 6(4:0) (1:3) AABBCCDD HHEEFFGG */ \
	move.l		data2,temp1	/* 2(2:0) (1:1) HHEEFFGG */ \
	eor.l		data1,temp1	/* 2(2:0) (1:1) HHEEFFGG^AABBCCDD */ \
	and.l		mask,temp1	/* 2(2:0) (1:1) ..EE..GG^..BB..DD */ \
	eor.l		temp1,data1	/* 2(2:0) (1:1) AAEECCGG (AABBCCDD^..EE..GG^..BB..DD) */ \
	eor.l		temp1,data2	/* 2(2:0) (1:1) HHBBFFDD (HHEEFFGG^..EE..GG^..BB..DD) */ \
	rol.l		shift,data2	/* 6(4:0) (1:3) BBFFDDHH */ \
					/* 7 ops / 14 bytes / 68030=22(18:0) / 68040=(7:11) */ \

#define splice3(shift,data1,data2,mask,temp1) \
	move.l		data2,temp1;	/* 2(2:0) (1:1) EEFFGGHH */ \
	lsr.l		shift,temp1; 	/* 4(4:0) (1:2) ..EEFFGG */ \
	eor.l		data1,temp1;	/* 2(2:0) (1:1) ..EEFFGG^AABBCCDD */ \
	and.l		mask,temp1;	/* 2(2:0) (1:1) ..EE..GG^..BB..DD */ \
	eor.l		temp1,data1;	/* 2(2:0) (1:1) AAEECCGG (AABBCCDD^..EE..GG^..BB..DD) */ \
	lsl.l		shift,temp1; 	/* 4(4:0) (1:2) EE..GG..^BB..DD.. */ \
	eor.l		temp1,data2;	/* 2(2:0) (1:1) BBFFDDHH (EEFFGGHH^EE..GG..^BB..DD..) */ \
					/* 7 ops / 14 bytes / 68030=18(18:0) / 68040=(7:9) */

#define splice(shift,data1,data2,mask,temp1) \
	splice3(shift,data1,data2,mask,temp1)

/*-------------------------------------------------------*/
/*      Byte-per-pixel -> bitplane transform macro       */
/*-------------------------------------------------------*/
/*      Converts 16 pixels per instance                  */
/*-------------------------------------------------------*/
#define chars2planes_pre(input) \
/*-------------------------------------------------------*/ \
	movem.l		input,d1-d4 \
/*-------------------------------------------------------*/ \
	move.l		#0x00FF00FF,d0	/* 4 */ \
	splice		8,d1,d3,d0,d7	/* 18 */ \
	splice		8,d2,d4,d0,d7	/* 18 */ \
/*-------------------------------------------------------*/ \
	move.l		#0x0F0F0F0F,d0	/* 4 */ \
	splice		4,d1,d2,d0,d7	/* 18 */ \
	splice		4,d3,d4,d0,d7	/* 18 */ \
/*-------------------------------------------------------*/ \
	swap		d2		/* 4(4:0) */ \
	swap		d4		/* 4(4:0) */ \
	eor.w		d1,d2		/* 2(2:0) */ \
	eor.w		d3,d4		/* 2(2:0) */ \
	eor.w		d2,d1		/* 2(2:0) */ \
	eor.w		d4,d3		/* 2(2:0) */ \
	eor.w		d1,d2		/* 2(2:0) */ \
	eor.w		d3,d4		/* 2(2:0) */ \
	swap		d2		/* 4(4:0) */ \
	swap		d4		/* 4(4:0) */ \
/*-------------------------------------------------------*/ \
	move.l		#0x33333333,d0	/* 4 */ \
	splice		2,d1,d2,d0,d7	/* 18 */ \
	splice		2,d3,d4,d0,d7	/* 18 */ \
/*-------------------------------------------------------*/ \
	move.l		#0x55555555,d0	/* 4 */ \
	splice		1,d1,d3,d0,d7	/* 18 */ \
	splice		1,d2,d4,d0,d7	/* 18 */ \
/*-------------------------------------------------------*/ \

/*-------------------------------------------------------*/
/*      16-bit destination                               */
/*-------------------------------------------------------*/
#define chars2planes_16(input,dest) \
	chars2planes_pre(input) \
	move.w		d4,dest \
	swap		d4		/* 4(4:0) */ \
	move.w		d2,dest \
	swap		d2		/* 4(4:0) */ \
	move.w		d3,dest \
	swap		d3		/* 4(4:0) */ \
	move.w		d1,dest \
	swap		d1		/* 4(4:0) */ \
	move.w		d4,dest \
	move.w		d2,dest \
	move.w		d3,dest \
	move.w		d1,dest

/*-------------------------------------------------------*/
/*      32-bit destination                               */
/*-------------------------------------------------------*/
#define chars2planes_32(input,dest) \
	chars2planes_pre(input) \
	swap		d4		/* 4(4:0) */ \
	eor.w		d2,d4		/* 2(2:0) */ \
	eor.w		d4,d2		/* 2(2:0) */ \
	eor.w		d2,d4		/* 2(2:0) */ \
	move.l		d4,dest \
	swap		d3		/* 4(4:0) */ \
	eor.w		d1,d3		/* 2(2:0) */ \
	eor.w		d3,d1		/* 2(2:0) */ \
	eor.w		d1,d3		/* 2(2:0) */ \
	move.l		d3,dest \
	swap		d2		/* 4(4:0) */ \
	move.l		d2,dest \
	swap		d1		/* 4(4:0) */ \
	move.l		d1,dest

/*-------------------------------------------------------*/
/*      Byte-per-pixel -> bitplane transform macro       */
/*-------------------------------------------------------*/
/*      Converts 16 pixels per instance                  */
/*-------------------------------------------------------*/
#define chars2planesr() \
/*-------------------------------------------------------*/ \
	move.l		#0x00FF00FF,d0	/* 4 */ \
	splice		8,d1,d3,d0,d7	/* 18 */ \
	splice		8,d2,d4,d0,d7	/* 18 */ \
/*-------------------------------------------------------*/ \
	move.l		#0x0F0F0F0F,d0	/* 4 */ \
	splice		4,d1,d2,d0,d7	/* 18 */ \
	splice		4,d3,d4,d0,d7	/* 18 */ \
/*-------------------------------------------------------*/ \
	swap		d2		/* 4(4:0) */ \
	swap		d4		/* 4(4:0) */ \
	eor.w		d1,d2		/* 2(2:0) */ \
	eor.w		d3,d4		/* 2(2:0) */ \
	eor.w		d2,d1		/* 2(2:0) */ \
	eor.w		d4,d3		/* 2(2:0) */ \
	eor.w		d1,d2		/* 2(2:0) */ \
	eor.w		d3,d4		/* 2(2:0) */ \
	swap		d2		/* 4(4:0) */ \
	swap		d4		/* 4(4:0) */ \
/*-------------------------------------------------------*/ \
	move.l		#0x33333333,d0	/* 4 */ \
	splice		2,d1,d2,d0,d7	/* 18 */ \
	splice		2,d3,d4,d0,d7	/* 18 */ \
/*-------------------------------------------------------*/ \
	move.l		#0x55555555,d0	/* 4 */ \
	splice		1,d1,d3,d0,d7	/* 18 */ \
	splice		1,d2,d4,d0,d7	/* 18 */ \
/*-------------------------------------------------------*/

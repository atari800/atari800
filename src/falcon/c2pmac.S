|  c2pmac.S - Atari Falcon chunky to planar conversion helper macro
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

	.macro	splice p0 p1 p2 p3 p4	| data1,data2,mask,temp
	move.l		\p2,\p4		| 2(2:0) (1:1) EEFFGGHH
	lsr.l		#\p0,\p4 	| 4(4:0) (1:2) plice3_.EEFFGG
	eor.l		\p1,\p4		| 2(2:0) (1:1) plice3_.EEFFGG^AABBCCDD
	and.l		\p3,\p4		| 2(2:0) (1:1) plice3_.EE..GG^plice3_.BB..DD
	eor.l		\p4,\p1		| 2(2:0) (1:1) AAEECCGG (AABBCCDD^plice3_.EE..GG^plice3_.BB..DD)
	lsl.l		#\p0,\p4 	| 4(4:0) (1:2) EE..GG..^BB..DD..
	eor.l		\p4,\p2		| 2(2:0) (1:1) BBFFDDHH (EEFFGGHH^EE..GG..^BB..DD..)
	.endm				| 7 ops / 14 bytes / 68030=18(18:0) / 68040=(7:9)

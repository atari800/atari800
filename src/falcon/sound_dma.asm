;  sound_dma.asm - Atari Falcon specific port code
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

	xdef	_timer_A
	xref	_timer_A_v_C

_timer_A:
	tst.b	semafor
	bne.s	konec
	st	semafor

	movem.l	d0-d7/a0-a6,zaloha_registru
	jsr	_timer_A_v_C
	movem.l	zaloha_registru,d0-d7/a0-a6

	sf	semafor
konec	rte

	section	bss
zaloha_registru:
	ds.l	15
zaloha_SR:
	ds.w	1
semafor	ds.b	1

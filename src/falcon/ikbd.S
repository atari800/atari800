|  ikbd.asm - Atari Falcon specific port code
|
|  Copyright (c) 1997-1998 Petr Stehlik and Karel Rous
|  Copyright (c) 1998-2003 Atari800 development team (see DOC/CREDITS)
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

	.globl	_init_kb
	.globl	_rem_kb
	.globl	_key_buf
	.globl	_joy0
	.globl	_joy1
	.globl	_buttons
	.globl	_kbhead
	.globl	_keybuf
*=======================================================*
*	IKBD routines: latest update 25/03/96		*
*=======================================================*
*	Handle keyboard & mouse				*
*=======================================================*

	.set	ikbd,0x118		| keyboard vector
	.set	key_ctl,0xfffffc00	| keyboard control register
	.set	key_dat,0xfffffc02	| keyboard data register

*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*
* Initialise custom keyboard packet handler		*
*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*
_init_kb:
initialise_ikbd:
*-------------------------------------------------------*
	bsr	empty_buffer
	lea	key_buffer,a0
	moveq	#(128/4)-1,d7
nitialise_ikbd_clear_keybd:
	clr.l	(a0)+
	dbf	d7,nitialise_ikbd_clear_keybd
	move.l	ikbd.w,old_ikbd
	move.l	#ikbd_handler,ikbd.w
	bsr	flush_ikbd
	rts

*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*
* Remove custom keyboard packet handler			*
*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*
_rem_kb:
remove_ikbd:
*-------------------------------------------------------*
	move.l	old_ikbd,ikbd.w
	bsr	flush_ikbd
	bsr	empty_buffer
	rts

*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*
* Remove unread data from ikbd chip			*
*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*
flush_ikbd:
*-------------------------------------------------------*
lush_ikbd_wl:	tst.b	key_dat.w
	bpl.s	lush_ikbd_wl
	move.b	key_ctl.w,d0
	btst	#0,d0
	bne.s	lush_ikbd_read
	rts
lush_ikbd_read:	move.b	key_dat.w,d0
	bra.s	flush_ikbd

*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*
* Keyboard Packet handler				*
*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*
ikbd_handler:
*-------------------------------------------------------*
	movem.l	a0/d0-d1,-(sp)

ikbd_loop:
	move.b	key_ctl.w,d0

	btst	#7,d0
	beq	ikbd_aq2

	btst	#4,d0
	bne	ik_err
	btst	#5,d0
	bne	ik_err
	btst	#6,d0
	bne	ik_err
	btst	#0,d0
	beq	ik_err

	move.b	key_dat.w,d0

	cmp.b	#0xff,d0			| joystick 1 packet
	beq.s	do_joy1
	cmp.b	#0xfe,d0			| joystick 0 packet
	beq.s	do_joy0

mouse_event:
	move.b	d0,d1
	and.b	#0xfc,d1			| 0b111110xx = mouse packet
	cmp.b	#0xf8,d1
	bne.s	key_press
	and.b	#3,d0
	move.b	d0,buttons
	move.l	#get_dx,ikbd.w
	bra.s	ikbd_aq

do_joy0:
	move.l	#get_joy0,ikbd.w
	bra.s	ikbd_aq

do_joy1:
	move.l	#get_joy1,ikbd.w
	bra.s	ikbd_aq


key_press:
	move.l	_kbhead,d1
	lea	_keybuf,a0
	move.b	d0,(a0,d1.l)
	addq.l	#1,d1
	and.w	#256-1,d1
	move.l	d1,_kbhead

	btst	#7,d0			| test release bit
	seq	d1			| d1.b cleared if release bit set
	and.w	#0x7f,d0			| mask off release bit
	lea	key_buffer,a0
	move.b	d1,(a0,d0.w)

ikbd_aq:

	btst	#0,key_ctl.w
	bne	ikbd_loop

ikbd_aq2:

	bclr	#6,0xfffffa11.w
	movem.l	(sp)+,a0/d0-d1
	rte


ik_err:	move.l	#ikbd_handler,ikbd.w
k_err_ikl:	btst	#0,key_ctl.w
	beq.s	k_err_clr
	move.b	key_dat.w,d0
	bra.s	k_err_ikl
k_err_clr:	bra.s	ikbd_aq

get_dx:
	movem.l	a0/d0-d1,-(sp)

	move.b	key_ctl.w,d0
	btst	#4,d0
	bne.s	ik_err
	btst	#5,d0
	bne.s	ik_err
	btst	#6,d0
	bne.s	ik_err
	btst	#0,d0
	beq.s	ik_err

	move.b	key_dat.w,d0
	ext.w	d0
	add	d0,mouse_dx
	move.l	#get_dy,ikbd.w

	bra.s	ikbd_aq

get_dy:
	movem.l	a0/d0-d1,-(sp)

	move.b	key_ctl.w,d0
	btst	#4,d0
	bne.s	ik_err
	btst	#5,d0
	bne.s	ik_err
	btst	#6,d0
	bne.s	ik_err
	btst	#0,d0
	beq.s	ik_err

	move.b	key_dat.w,d0
	ext.w	d0
	add	d0,mouse_dy
	move.l	#ikbd_handler,ikbd.w

	bra	ikbd_aq

get_joy1:
	move.b	key_dat.w,joy0
	move.l	#ikbd_handler,ikbd.w
	bclr	#6,0xfffffa11.w
	rte

get_joy0:
	move.b	key_dat.w,joy1
	move.l	#ikbd_handler,ikbd.w
	bclr	#6,0xfffffa11.w
	rte

*-------------------------------------------------------*
*	Flush keyboard buffer				*
*-------------------------------------------------------*
empty_buffer:
*-------------------------------------------------------*
mpty_buffer_bk:	move.w		#11,-(sp)
	trap		#1
	addq		#2,sp
	tst.w		d0
	beq.s		mpty_buffer_ot
	move.w		#7,-(sp)
	trap		#1
	addq		#2,sp
	bra.s		mpty_buffer_bk
mpty_buffer_ot:	rts

*-------------------------------------------------------*
			.bss
*-------------------------------------------------------*

old_ikbd:		ds.l	1
old_mousevec:		ds.l	1
key_recptr:		ds.l	1

last_key:		ds.w	1
mouse_dx:		ds.w	1
mouse_dy:		ds.w	1

_joy0:
joy0:			ds.b	1
_joy1:
joy1:			ds.b	1
_buttons:
buttons:		ds.b	1

_key_buf:
key_buffer:		ds.w	64

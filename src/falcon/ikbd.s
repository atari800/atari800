	xdef	_init_kb
	xdef	_rem_kb
	xdef	_key_buf
	xdef	_joy0
	xdef	_joy1
	xdef	_buttons
	xref	_kbhead
	xref	_keybuf
*=======================================================*
*	IKBD routines: latest update 25/03/96		*
*=======================================================*
*	Handle keyboard & mouse				*
*=======================================================*

ikbd		=	$118		; keyboard vector
key_ctl		=	$fffffc00	; keyboard control register
key_dat		=	$fffffc02	; keyboard data register

*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*
* Initialise custom keyboard packet handler		*
*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*
_init_kb:
initialise_ikbd:
*-------------------------------------------------------*
	bsr	empty_buffer
	lea	key_buffer,a0
	moveq	#(128/4)-1,d7
.clear_keybd:
	clr.l	(a0)+
	dbf	d7,.clear_keybd
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
.wl:	tst.b	key_dat.w
	bpl.s	.wl
	move.b	key_ctl.w,d0
	btst	#0,d0
	bne.s	.read
	rts
.read	move.b	key_dat.w,d0
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

	cmp.b	#$ff,d0			; joystick 1 packet
	beq.s	do_joy1
	cmp.b	#$fe,d0			; joystick 0 packet
	beq.s	do_joy0

mouse_event:
	move.b	d0,d1
	and.b	#$fc,d1			; %111110xx = mouse packet
	cmp.b	#$f8,d1
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

	btst	#7,d0			; test release bit
	seq	d1			; d1.b cleared if release bit set
	and.w	#$7f,d0			; mask off release bit
	lea	key_buffer,a0
	move.b	d1,(a0,d0.w)

ikbd_aq:

	btst	#0,key_ctl.w
	bne	ikbd_loop

ikbd_aq2:

	bclr	#6,$fffffa11.w
	movem.l	(sp)+,a0/d0-d1
	rte


ik_err:	move.l	#ikbd_handler,ikbd.w
.ikl:	btst	#0,key_ctl.w
	beq.s	.clr
	move.b	key_dat.w,d0
	bra.s	.ikl
.clr:	bra.s	ikbd_aq

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
	bclr	#6,$fffffa11.w
	rte

get_joy0:
	move.b	key_dat.w,joy1
	move.l	#ikbd_handler,ikbd.w
	bclr	#6,$fffffa11.w
	rte

*-------------------------------------------------------*
*	Flush keyboard buffer				*
*-------------------------------------------------------*
empty_buffer:
*-------------------------------------------------------*
.bk	move.w		#11,-(sp)
	trap		#1
	addq		#2,sp
	tst.w		d0
	beq.s		.ot
	move.w		#7,-(sp)
	trap		#1
	addq		#2,sp
	bra.s		.bk
.ot	rts

*-------------------------------------------------------*
			bss
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
			even
_key_buf:
key_buffer:		ds.b	128

*-------------------------------------------------------*
			text
*-------------------------------------------------------*

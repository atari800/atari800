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

	xdef		_rplanes_delta_init
	xdef		_rplanes_delta

*-------------------------------------------------------*
	include		c2pmac.s
*-------------------------------------------------------*

pushall	macro
	movem.l		d2-d7/a2-a5,-(sp)
	endm

popall	macro
	movem.l		(sp)+,d2-d7/a2-a5
	endm

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
	pushall
*-------------------------------------------------------*

	move.l		_odkud,a0
	move.l		_kam,a1
	move.l		_oldscreen,a2

; centering of view at screen 
	move.w		#384,d0		; width of Atari800 emulated screen
	sub.w		_screenw,d0	; width of displayed screen
	movea.w		d0,a3
	lsr.w		#1,d0		; centering
	lea		(a0,d0),a0	; offset 24 or 32 pixels
	lea		(a2,d0),a2	; offset 24 or 32 pixels

; centering of screen in videoram in horizontal axis
	move.w		_vramw,d0
	sub.w		_screenw,d0
	movea.w		d0,a4
	lsr.w		#1,d0
	neg.w		d0
	lea		(a1,d0),a1	; negative pre-offset (will be OK at .ylp)

; centering of screen in videoram in vertical axis
	move.w		_vramh,d0
	sub.w		_screenh,d0
	lsr.w		#1,d0
	move.w		_vramw,d1
	mulu		d1,d0
	lea		(a1,d0.l),a1

; precompute line width in long words
	move.w		_screenw,d0
	lsr.w		#4,d0
	subq.w		#1,d0
	movea.w		d0,a5

*-------------------------------------------------------*
	move.w		_screenh,d6
	subq.w		#1,d6
	move.w		a5,d5
	nop			; for cache alignment	
*-------------------------------------------------------*
.xlp:	
	move.l		(a0)+,d4
	move.l		(a0)+,d3
	move.l		(a0)+,d2
	move.l		(a0)+,d1
*-------------------------------------------------------*
	moveq		#12,d0
	cmp.l		(a2)+,d4
	bne.s		.doit
	moveq		#8,d0
	cmp.l		(a2)+,d3
	bne.s		.doit
	moveq		#4,d0
	cmp.l		(a2)+,d2
	bne.s		.doit
	cmp.l		(a2)+,d1
	bne.s		.doit2
	lea		16(a1),a1
	dbra		d5,.xlp
	adda.l		a3,a0
	adda.l		a3,a2
	adda.l		a4,a1
	move.w		a5,d5
	dbra		d6,.xlp
	bra		.none
.doit:
	adda.l		d0,a2
.doit2:
*-------------------------------------------------------*
	move.l		#$00FF00FF,d0	; 4
	splice.8	d4,d2,d0,d7	; 18
	splice.8	d3,d1,d0,d7	; 18
*-------------------------------------------------------*
	move.l		#$0F0F0F0F,d0	; 4
	splice.4	d4,d3,d0,d7	; 18
	splice.4	d2,d1,d0,d7	; 18
*-------------------------------------------------------*
	swap		d3		; 4(4:0)
	swap		d1		; 4(4:0)
	eor.w		d4,d3		; 2(2:0)
	eor.w		d2,d1		; 2(2:0)
	eor.w		d3,d4		; 2(2:0)
	eor.w		d1,d2		; 2(2:0)
	eor.w		d4,d3		; 2(2:0)
	eor.w		d2,d1		; 2(2:0)
	swap		d3		; 4(4:0)
	swap		d1		; 4(4:0)
*-------------------------------------------------------*
	move.l		#$33333333,d0	; 4
	splice.2	d4,d3,d0,d7	; 18
	splice.2	d2,d1,d0,d7	; 18
*-------------------------------------------------------*
	move.l		#$55555555,d0	; 4
	splice.1	d4,d2,d0,d7	; 18
	splice.1	d3,d1,d0,d7	; 18
*-------------------------------------------------------*
*	32-bit destination				*
*-------------------------------------------------------*
	swap		d1		; 4(4:0)
	eor.w		d3,d1		; 2(2:0)
	eor.w		d1,d3		; 2(2:0)
	eor.w		d3,d1		; 2(2:0)
	swap		d3		; 4(4:0)
	swap		d2		; 4(4:0)
	eor.w		d4,d2		; 2(2:0)
	eor.w		d2,d4		; 2(2:0)
	eor.w		d4,d2		; 2(2:0)
	swap		d4		; 4(4:0)
*-------------------------------------------------------*
	move.l		d1,(a1)+
 	move.l		d2,(a1)+
	move.l		d3,(a1)+
	move.l		d4,(a1)+
*-------------------------------------------------------*
	dbra		d5,.xlp
	adda.l		a3,a0
	adda.l		a3,a2
	adda.l		a4,a1
	move.w		a5,d5
	dbra		d6,.xlp
*-------------------------------------------------------*
.none:
*-------------------------------------------------------*
	popall
*-------------------------------------------------------*
	rts
*-------------------------------------------------------*
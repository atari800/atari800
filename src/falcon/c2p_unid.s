*-------------------------------------------------------*

	OPT		P=68040,L1,O+,W-
	output		c2p_unid.o
*-------------------------------------------------------*

*-------------------------------------------------------*
*	Initialisation functions			*
*-------------------------------------------------------*

	xref		_screenw,_screenh
	xref		_vramw,_vramh
	xref		_odkud,_kam
	xref		_oldscreen

*-------------------------------------------------------*
*	General functions				*
*-------------------------------------------------------*

	xdef		_rplanes_delta

*-------------------------------------------------------*
	include		c2pmac.s
*-------------------------------------------------------*

push	macro
	move.\0		\1,-(sp)
	endm

pop	macro
	move.\0		(sp)+,\1
	endm

pushall	macro
	movem.l		d0-a6,-(sp)
	endm

popall	macro
	movem.l		(sp)+,d0-a6
	endm

*-------------------------------------------------------*
*	Initialise rendering display			*
*-------------------------------------------------------*
_rplanes_delta:
*-------------------------------------------------------*
	pushall
*-------------------------------------------------------*
			rsreset
*-------------------------------------------------------*
.local_regs		rs.l	15
*-------------------------------------------------------*
.local_rts		rs.l	1
*-------------------------------------------------------*
	move.l		_odkud,a0
	move.l		_kam,a1
	move.l		_oldscreen,a2

; centering of view at screen 
	move.w		#384,d0		; width of Atari800 emulated screen
	sub.w		_screenw,d0	; width of displayed screen
	move.w		d0,src_line_offset
	lsr.w		#1,d0		; centering
	lea		(a0,d0),a0	; offset 24 or 32 pixels
	lea		(a2,d0),a2	; offset 24 or 32 pixels

; centering of screen in videoram in horizontal axis
	move.w		_vramw,d0
	sub.w		_screenw,d0
	move.w		d0,dst_line_offset
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
	move.w		d0,line_long_width

*-------------------------------------------------------*
	movem.l		(a0)+,d1-d4
	movem.l		(a2)+,a3-a6
*-------------------------------------------------------*
	cmp.l		d1,a3
	bne.s		.doit
	cmp.l		d2,a4
	bne.s		.doit
	cmp.l		d3,a5
	bne.s		.doit
	cmp.l		d4,a6
	bne.s		.doit
	lea		16(a1),a1
	bra		.xyl
.doit:
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
*	32-bit destination				*
*-------------------------------------------------------*
	swap		d4		; 4(4:0)
	eor.w		d2,d4		; 2(2:0)
	eor.w		d4,d2		; 2(2:0)
	eor.w		d2,d4		; 2(2:0)
	swap		d2		; 4(4:0)
	swap		d3		; 4(4:0)
	eor.w		d1,d3		; 2(2:0)
	eor.w		d3,d1		; 2(2:0)
	eor.w		d1,d3		; 2(2:0)
	swap		d1		; 4(4:0)
*-------------------------------------------------------*
	move.l		d4,(a1)+
	move.l		d3,(a1)+
	move.l		d2,(a1)+
	move.l		d1,(a1)+
*-------------------------------------------------------*
.xyl:	move.w		_screenh,d6
	subq.w		#1,d6
*-------------------------------------------------------*
.ylp:	move.w		line_long_width,d5
	move.w		dst_line_offset,d0
	lea		(a1,d0),a1
*-------------------------------------------------------*
.xlp:	tst.w		d5
	bne.s		.nono
	move.w		src_line_offset,d0
	lea		(a0,d0),a0	; offset D0 pixels to beginning of next line
	lea		(a2,d0),a2	; offset D0 pixels to beginning of next line
.nono	movem.l		(a0)+,d1-d4
	movem.l		(a2)+,a3-a6
*-------------------------------------------------------*
	cmp.l		d1,a3
	bne.s		.doit2
	cmp.l		d2,a4
	bne.s		.doit2
	cmp.l		d3,a5
	bne.s		.doit2
	cmp.l		d4,a6
	bne.s		.doit2
	lea		16(a1),a1
	dbra		d5,.xlp
	dbra		d6,.ylp
	bra		.none
.doit2:
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
*	32-bit destination				*
*-------------------------------------------------------*
	swap		d4		; 4(4:0)
	eor.w		d2,d4		; 2(2:0)
	eor.w		d4,d2		; 2(2:0)
	eor.w		d2,d4		; 2(2:0)
	swap		d2		; 4(4:0)
	swap		d3		; 4(4:0)
	eor.w		d1,d3		; 2(2:0)
	eor.w		d3,d1		; 2(2:0)
	eor.w		d1,d3		; 2(2:0)
	swap		d1		; 4(4:0)
*-------------------------------------------------------*
	move.l		d4,(a1)+
	move.l		d3,(a1)+
	move.l		d2,(a1)+
	move.l		d1,(a1)+
*-------------------------------------------------------*
	dbra		d5,.xlp
;	tst.w		d6
;	beq.s		.none
	dbra		d6,.ylp
*-------------------------------------------------------*
.none:
;.none:	move.l		a2,(a1)+
;	move.l		a3,(a1)+
;	move.l		a4,(a1)+
;	move.l		a5,(a1)+
*-------------------------------------------------------*
	popall
*-------------------------------------------------------*
	rts

*-------------------------------------------------------*
			bss
*-------------------------------------------------------*
src_line_offset		ds.w	1
dst_line_offset		ds.w	1
line_long_width		ds.w	1
*-------------------------------------------------------*

	MC68020

	XDEF _ScreenData28bit

; a0 - UBYTE *screen
; a1 - UBYTE *destscreendata
; a2 - UBYTE *colortable8
; d0 - UWORD width
; d1 - UWORD height

_ScreenData28bit:
			movem.l	d2/d7,-(a7)
			lsr.w	#2,d0
			subq.l	#1,d1
			subq.l	#1,d0

			moveq	#0,d2
;			moveq	#0,d3
;			moveq	#0,d4
;			moveq	#0,d5

.heightloop:	move.l	d0,d7

.widthloop:
;			move.b	(a0)+,d2
;			move.b	(a0)+,d3
;			move.b	(a0)+,d4
;			move.b	(a0)+,d5

;			move.b	(a2,d2),(a1)+
;			move.b	(a2,d3),(a1)+
;			move.b	(a2,d4),(a1)+
;			move.b	(a2,d5),(a1)+

			move.b	(a0)+,d2
			move.b	(a2,d2),(a1)+

			move.b	(a0)+,d2
			move.b	(a2,d2),(a1)+

			move.b	(a0)+,d2
			move.b	(a2,d2),(a1)+

			move.b	(a0)+,d2
			move.b	(a2,d2),(a1)+

;			moveq	#0,d3
;			moveq	#0,d5

;			move.b	(a0)+,d2
;			move.b	(a2,d2),d3

;			move.b	(a0)+,d2
;			lsl.w	#8,d3
;			move.b	(a2,d2),d4

;			move.b	(a0)+,d2
;			or.w		d4,d3
;			move.b	(a2,d2),d5
;			swap		d3

;			move.b	(a0)+,d2
;			lsl.w	#8,d5
;			move.b	(a2,d2),d3

;			or.w		d5,d3
;			move.l	d3,(a1)+

			dbra		d7,.widthloop
			dbra		d1,.heightloop

			movem.l	(a7)+,d2/d7
			rts


			XDEF _ScreenData215bit

; a0 - UBYTE *screen
; a1 - UWORD *destscreendata
; a2 - UWORD *colortable15
; d0 - UWORD width
; d1 - UWORD height


_ScreenData215bit:
			movem.l	d2/d3/d7,-(a7)
			lsr.l	#1,d0
			subq.l	#1,d1
			subq.l	#1,d0

			moveq	#0,d2

.heightloop:	move.l	d0,d7

.widthloop:
			move.b	(a0)+,d2
			move.w	(a2,d2*2),d3

			move.b	(a0)+,d2
			swap		d3
			move.w	(a2,d2*2),d3

			move.l	d3,(a1)+

			dbra		d7,.widthloop
			dbra		d1,.heightloop

			movem.l	(a7)+,d2/d3/d7
			rts

			end

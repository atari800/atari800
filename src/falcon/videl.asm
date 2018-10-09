;  videl.asm - Atari Falcon specific port code
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

	xdef		_load_r,_save_r,_p_str_p
	xdef		_store_palette,_restore_palette,_set_falcon_palette
*-------------------------------------------------------*
	section		text
*-------------------------------------------------------*
none			=	-1

*-------------------------------------------------------*
			rsreset
*-------------------------------------------------------*
plane_bits		rs.b	2	; 0
true_bit		rs.b	1	; 2
hires_bit		rs.b	1	; 3
vga_bit			rs.b	1	; 4
pal_bit			rs.b	1	; 5
os_bit			rs.b	1	; 6
compat_bit		rs.b	1	; 7
lace_bit		rs.b	1	; 8

*-------------------------------------------------------*
			rsreset
*-------------------------------------------------------*
bpl1			rs.b	1
bpl2			rs.b	1
bpl4			rs.b	1
bpl8			rs.b	1

*-------------------------------------------------------*

true			=	1<<true_bit
hires			=	1<<hires_bit
vga			=	1<<vga_bit
pal			=	1<<pal_bit
os			=	1<<os_bit
compat			=	1<<compat_bit
lace			=	1<<lace_bit

*-------------------------------------------------------*
*	Videl registers					*
*-------------------------------------------------------*

RShift			=	$FFFF8260
RSpShift		=	$FFFF8266
ROffset			=	$FFFF820E
RWrap			=	$FFFF8210
RSync			=	$FFFF820A
RCO			=	$FFFF82C0
RMode			=	$FFFF82C2
RHHT			=	$FFFF8282
RHBB			=	$FFFF8284
RHBE			=	$FFFF8286
RHDB			=	$FFFF8288
RHDE			=	$FFFF828A
RHSS			=	$FFFF828C
RHFS			=	$FFFF828E
RHEE			=	$FFFF8290
RVFT			=	$FFFF82A2
RVBB			=	$FFFF82A4
RVBE			=	$FFFF82A6
RVDB			=	$FFFF82A8
RVDE			=	$FFFF82AA
RVSS			=	$FFFF82AC

*-------------------------------------------------------*
*	Videl register file				*
*-------------------------------------------------------*
			rsreset
*-------------------------------------------------------*
patch_code		rs.w	1			; fake modecode (describes register file)
*-------------------------------------------------------*
patch_size		rs.l	1			; total display memory
patch_width		rs.w	1			; horizontal res
patch_height		rs.w	1			; vertical res
patch_depth		rs.w	1			; colour depth (bits per pixel)
*-------------------------------------------------------*
patch_RShift		rs.b	1			; register file
patch_RSync		rs.b	1
patch_RSpShift		rs.w	1
patch_ROffset		rs.w	1
patch_RWrap		rs.w	1
patch_RCO		rs.w	1
patch_RMode		rs.w	1
patch_RHHT		rs.w	1
patch_RHBB		rs.w	1
patch_RHBE		rs.w	1
patch_RHDB		rs.w	1
patch_RHDE		rs.w	1
patch_RHSS		rs.w	1
patch_RHFS		rs.w	1
patch_RHEE		rs.w	1
patch_RVFT		rs.w	1
patch_RVBB		rs.w	1
patch_RVBE		rs.w	1
patch_RVDB		rs.w	1
patch_RVDE		rs.w	1
patch_RVSS		rs.w	1
*-------------------------------------------------------*
patch_slen		rs.b	0
*-------------------------------------------------------*

hz200			=	$4ba
vbcount			=	$462

*-------------------------------------------------------*
*	Load Videl registers				*
*-------------------------------------------------------*
_load_r:
*-------------------------------------------------------*
*	Register file pointer				*
*-------------------------------------------------------*
	move.l		_p_str_p,a0
*-------------------------------------------------------*
*	Allow previous VBlank changes to settle		*
*-------------------------------------------------------*
	moveq		#5,d0
	add.l		hz200.w,d0
.wait:	nop
	cmp.l		hz200.w,d0
	bne.s		.wait
*-------------------------------------------------------*
*	Reset Videl for new register file		*
*-------------------------------------------------------*
	clr.w		RSpShift.w
*-------------------------------------------------------*
*	Lock exceptions					*
*-------------------------------------------------------*
	move.w		sr,-(sp)
	or.w		#$700,sr
*-------------------------------------------------------*
*	Load shift mode					*
*-------------------------------------------------------*
	cmp.w		#2,patch_depth(a0)
	bne.s		.n2p
	move.b		patch_RShift(a0),RShift.w
	bra.s		.d2p
.n2p:	move.w		patch_RSpShift(a0),RSpShift.w
*-------------------------------------------------------*
*	Load line offset+wrap				*
*-------------------------------------------------------*
.d2p:	move.w		patch_ROffset(a0),ROffset.w
	move.w		patch_RWrap(a0),RWrap.w
*-------------------------------------------------------*
*	Load sync					*
*-------------------------------------------------------*
	move.b		patch_RSync(a0),RSync.w
*-------------------------------------------------------*
*	Load clock					*
*-------------------------------------------------------*
	move.w		patch_RCO(a0),RCO.w
*-------------------------------------------------------*
*	Load mode					*
*-------------------------------------------------------*
	move.w		patch_RMode(a0),RMode.w
*-------------------------------------------------------*
*	Horizontal register set				*
*-------------------------------------------------------*
	move.w		patch_RHHT(a0),RHHT.w
	move.w		patch_RHBB(a0),RHBB.w
	move.w		patch_RHBE(a0),RHBE.w
	move.w		patch_RHDB(a0),RHDB.w
	move.w		patch_RHDE(a0),RHDE.w
	move.w		patch_RHSS(a0),RHSS.w
	move.w		patch_RHFS(a0),RHFS.w
	move.w		patch_RHEE(a0),RHEE.w
*-------------------------------------------------------*
*	Vertical register set				*
*-------------------------------------------------------*
	move.w		patch_RVFT(a0),RVFT.w
	move.w		patch_RVBB(a0),RVBB.w
	move.w		patch_RVBE(a0),RVBE.w
	move.w		patch_RVDB(a0),RVDB.w
	move.w		patch_RVDE(a0),RVDE.w
	move.w		patch_RVSS(a0),RVSS.w
*-------------------------------------------------------*
*	Restore exceptions				*
*-------------------------------------------------------*
	move.w		(sp)+,sr
*-------------------------------------------------------*
*	Re-synchronize display for new settings		*
*-------------------------------------------------------*
	move.w		patch_code(a0),d1
	bsr		videl_re_sync
*-------------------------------------------------------*
	rts

*-------------------------------------------------------*
*	Save Videl registers				*
*-------------------------------------------------------*
_save_r:
	movem.l		d2/a2,-(sp)
*-------------------------------------------------------*
*	Get Modecode					*
*-------------------------------------------------------*
	move.w		#-1,-(sp)
	move.w		#88,-(sp)
	trap		#14
	addq.l		#4,sp
*-------------------------------------------------------*
*	Register file pointer				*
*-------------------------------------------------------*
	move.l		_p_str_p,a0
*-------------------------------------------------------*
*	Save Modecode					*
*-------------------------------------------------------*
	move.w		d0,patch_code(a0)
	and.w		#%0001111,d0
	move.w		d0,patch_depth(a0)
*-------------------------------------------------------*
*	Lock exceptions					*
*-------------------------------------------------------*
	move.w		sr,-(sp)
	or.w		#$700,sr
*-------------------------------------------------------*
*	Save shift mode					*
*-------------------------------------------------------*
	move.b		RShift.w,patch_RShift(a0)
	move.w		RSpShift.w,patch_RSpShift(a0)
*-------------------------------------------------------*
*	Save line offset+wrap				*
*-------------------------------------------------------*
	move.w		ROffset.w,patch_ROffset(a0)
	move.w		RWrap.w,patch_RWrap(a0)
*-------------------------------------------------------*
*	Save sync					*
*-------------------------------------------------------*
	move.b		RSync.w,patch_RSync(a0)
*-------------------------------------------------------*
*	Save clock					*
*-------------------------------------------------------*
	move.w		RCO.w,patch_RCO(a0)
*-------------------------------------------------------*
*	Save mode					*
*-------------------------------------------------------*
	move.w		RMode.w,patch_RMode(a0)
*-------------------------------------------------------*
*	Horizontal register set				*
*-------------------------------------------------------*
	move.w		RHHT.w,patch_RHHT(a0)
	move.w		RHBB.w,patch_RHBB(a0)
	move.w		RHBE.w,patch_RHBE(a0)
	move.w		RHDB.w,patch_RHDB(a0)
	move.w		RHDE.w,patch_RHDE(a0)
	move.w		RHSS.w,patch_RHSS(a0)
	move.w		RHFS.w,patch_RHFS(a0)
	move.w		RHEE.w,patch_RHEE(a0)
*-------------------------------------------------------*
*	Vertical register set				*
*-------------------------------------------------------*
	move.w		RVFT.w,patch_RVFT(a0)
	move.w		RVBB.w,patch_RVBB(a0)
	move.w		RVBE.w,patch_RVBE(a0)
	move.w		RVDB.w,patch_RVDB(a0)
	move.w		RVDE.w,patch_RVDE(a0)
	move.w		RVSS.w,patch_RVSS(a0)
*-------------------------------------------------------*
*	Restore exceptions				*
*-------------------------------------------------------*
	move.w		(sp)+,sr
	movem.l		(sp)+,d2/a2
	rts

*-------------------------------------------------------*
videl_re_sync:
	move.l		d2,-(sp)
*-------------------------------------------------------*
*	Decode new modecode				*
*-------------------------------------------------------*
	btst		#compat_bit,d1
	bne.s		.nsync
	cmp.w		#none,d1
	beq.s		.nsync
	and.w		#%111,d1
	cmp.w		#bpl2,d1
	beq.s		.nsync
*-------------------------------------------------------*
*	Reset Videl for re-sync				*
*-------------------------------------------------------*
.sync:	move.w		patch_RSpShift(a0),d1
	clr.w		RSpShift.w
*-------------------------------------------------------*
*	Wait for at least 1 VBlank period		*
*-------------------------------------------------------*
	moveq		#2,d0
	add.l		vbcount.w,d0
	moveq		#9,d2
	add.l		hz200.w,d2
.lp:	nop
	cmp.l		vbcount.w,d0
	beq.s		.stop
	cmp.l		hz200.w,d2
	bne.s		.lp
*-------------------------------------------------------*
*	Restore Videl mode				*
*-------------------------------------------------------*
.stop:	move.w		d1,RSpShift.w
*-------------------------------------------------------*
.nsync:	move.l		(sp)+,d2
	rts

_store_palette:
	movem.l		d2-d7/a2-a6,-(sp)

	lea		$ffff9800.w,a0			; save falcon palette
	lea		save_pal,a1			;
	move.w		#256-1,d7			;
.colloop:
	move.l		(a0)+,(a1)+			;
	dbra		d7,.colloop			;

	movem.l		$ffff8240.w,d0-d7		; save st palette
	movem.l		d0-d7,(a1)			;

	movem.l		(sp)+,d2-d7/a2-a6
	rts

_restore_palette
	movem.l		d2-d7/a2-a6,-(sp)

	lea		$ffff9800.w,a0			; restore falcon palette
	lea		save_pal,a1			;
	move.w		#256-1,d7			;
.loop:	move.l		(a1)+,(a0)+			;
	dbra		d7,.loop			;

	movem.l		(a1),d0-d7			; restore st palette
	movem.l		d0-d7,$ffff8240.w		;

	movem.l		(sp)+,d2-d7/a2-a6
	rts

_set_falcon_palette:
	lea		$ffff9800.w,a0			; set falcon palette
	move.l		_p_str_p,a1			;
	move.w		#256-1,d0			;
.loop:	move.l		(a1)+,(a0)+			;
	dbra		d0,.loop			;
	rts

*-------------------------------------------------------*
	section 	bss
*-------------------------------------------------------*

_p_str_p:
	ds.l	1
save_pal:
	ds.l	256+8					; old colours (falcon+st/e)

*-------------------------------------------------------*
	section		text
*-------------------------------------------------------*

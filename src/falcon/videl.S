|  videl.asm - Atari Falcon specific port code
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

	.globl		_load_r,_save_r,_p_str_p
*-------------------------------------------------------*
	.text
*-------------------------------------------------------*
	.set	none,-1

*-------------------------------------------------------*
			
*-------------------------------------------------------*
	.set	plane_bits,0+0	| 0
	.set	true_bit,0+2	| 2
	.set	hires_bit,0+3	| 3
	.set	vga_bit,0+4	| 4
	.set	pal_bit,0+5	| 5
	.set	os_bit,0+6	| 6
	.set	compat_bit,0+7	| 7
	.set	lace_bit,0+8	| 8

*-------------------------------------------------------*
			
*-------------------------------------------------------*
	.set	bpl1,0+0
	.set	bpl2,0+1
	.set	bpl4,0+2
	.set	bpl8,0+3

*-------------------------------------------------------*

	.set	true,1<<true_bit
	.set	hires,1<<hires_bit
	.set	vga,1<<vga_bit
	.set	pal,1<<pal_bit
	.set	os,1<<os_bit
	.set	compat,1<<compat_bit
	.set	lace,1<<lace_bit

*-------------------------------------------------------*
*	Videl registers					*
*-------------------------------------------------------*

	.set	RShift,0xFFFF8260
	.set	RSpShift,0xFFFF8266
	.set	RWrap,0xFFFF8210
	.set	RSync,0xFFFF820A
	.set	RCO,0xFFFF82C0
	.set	RMode,0xFFFF82C2
	.set	RHHT,0xFFFF8282
	.set	RHBB,0xFFFF8284
	.set	RHBE,0xFFFF8286
	.set	RHDB,0xFFFF8288
	.set	RHDE,0xFFFF828A
	.set	RHSS,0xFFFF828C
	.set	RHFS,0xFFFF828E
	.set	RHEE,0xFFFF8290
	.set	RVFT,0xFFFF82A2
	.set	RVBB,0xFFFF82A4
	.set	RVBE,0xFFFF82A6
	.set	RVDB,0xFFFF82A8
	.set	RVDE,0xFFFF82AA
	.set	RVSS,0xFFFF82AC

*-------------------------------------------------------*
*	Videl register file				*
*-------------------------------------------------------*
			
*-------------------------------------------------------*
	.set	patch_code,0+0			| fake modecode (describes register file)
*-------------------------------------------------------*
	.set	patch_size,0+2			| total display memory
	.set	patch_width,0+6			| horizontal res
	.set	patch_height,0+8			| vertical res
	.set	patch_depth,0+10			| colour depth (bits per pixel)
*-------------------------------------------------------*
	.set	patch_RShift,0+12			| register file
	.set	patch_RSync,0+13
	.set	patch_RSpShift,0+14
	.set	patch_RWrap,0+16
	.set	patch_RCO,0+18
	.set	patch_RMode,0+20
	.set	patch_RHHT,0+22
	.set	patch_RHBB,0+24
	.set	patch_RHBE,0+26
	.set	patch_RHDB,0+28
	.set	patch_RHDE,0+30
	.set	patch_RHSS,0+32
	.set	patch_RHFS,0+34
	.set	patch_RHEE,0+36
	.set	patch_RVFT,0+38
	.set	patch_RVBB,0+40
	.set	patch_RVBE,0+42
	.set	patch_RVDB,0+44
	.set	patch_RVDE,0+46
	.set	patch_RVSS,0+48
*-------------------------------------------------------*
	.set	patch_slen,0+50
*-------------------------------------------------------*

	.set	hz200,0x4ba
	.set	vbcount,0x462

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
load_r_wait:	nop
	cmp.l		hz200.w,d0
	bne.s		load_r_wait
*-------------------------------------------------------*
*	Reset Videl for new register file		*
*-------------------------------------------------------*
	clr.w		RSpShift.w
*-------------------------------------------------------*
*	Lock exceptions					*
*-------------------------------------------------------*
	move.w		sr,-(sp)
	or.w		#0x700,sr
*-------------------------------------------------------*
*	Load shift mode					*
*-------------------------------------------------------*
	cmp.w		#2,patch_depth(a0)
	bne.s		load_r_n2p
	move.b		patch_RShift(a0),RShift.w
	bra.s		load_r_d2p
load_r_n2p:	move.w		patch_RSpShift(a0),RSpShift.w
*-------------------------------------------------------*
*	Load line wrap					*
*-------------------------------------------------------*
load_r_d2p:	move.w		patch_RWrap(a0),RWrap.w
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
*-------------------------------------------------------*
*	Get Modecode					*
*-------------------------------------------------------*
	move		#-1,-(sp)
	move		#87,-(sp)
	trap		#14
	addq		#4,sp
*-------------------------------------------------------*
*	Register file pointer				*
*-------------------------------------------------------*
	move.l		_p_str_p,a0
*-------------------------------------------------------*
*	Save Modecode					*
*-------------------------------------------------------*
	move.w		d0,patch_code(a0)
	and.w		#0b0001111,d0
	move.w		d0,patch_depth(a0)
*-------------------------------------------------------*
*	Lock exceptions					*
*-------------------------------------------------------*
	move.w		sr,-(sp)
	or.w		#0x700,sr
*-------------------------------------------------------*
*	Save shift mode					*
*-------------------------------------------------------*
	move.b		RShift.w,patch_RShift(a0)
	move.w		RSpShift.w,patch_RSpShift(a0)
*-------------------------------------------------------*
*	Save line wrap					*
*-------------------------------------------------------*
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
	rts

*-------------------------------------------------------*
videl_re_sync:
*-------------------------------------------------------*
*	Decode new modecode				*
*-------------------------------------------------------*
	btst		#compat_bit,d1
	bne.s		idel_re_sync_nsync
	cmp.w		#none,d1
	beq.s		idel_re_sync_nsync
	and.w		#0b111,d1
	cmp.w		#bpl2,d1
	beq.s		idel_re_sync_nsync
*-------------------------------------------------------*
*	Reset Videl for re-sync				*
*-------------------------------------------------------*
idel_re_sync_sync:	move.w		RSpShift.w,d1
	clr.w		RSpShift.w
*-------------------------------------------------------*
*	Wait for at least 1 VBlank period		*
*-------------------------------------------------------*
	moveq		#2,d0
	add.l		vbcount.w,d0
	moveq		#9,d2
	add.l		hz200.w,d2
idel_re_sync_lp:	nop
	cmp.l		vbcount.w,d0
	beq.s		idel_re_sync_stop
	cmp.l		hz200.w,d2
	bne.s		idel_re_sync_lp
*-------------------------------------------------------*
*	Restore Videl mode				*
*-------------------------------------------------------*
idel_re_sync_stop:	move.w		d1,RSpShift.w
*-------------------------------------------------------*
idel_re_sync_nsync:	rts


*-------------------------------------------------------*
		.bss
*-------------------------------------------------------*

_p_str_p:	ds.l	1

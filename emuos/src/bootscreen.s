;	Altirra - Atari 800/800XL/5200 emulator
;	Modular Kernel ROM - Boot Screen
;	Copyright (C) 2008-2016 Avery Lee
;
;	Copying and distribution of this file, with or without modification,
;	are permitted in any medium without royalty provided the copyright
;	notice and this notice are preserved.  This file is offered as-is,
;	without any warranty.

		opt		o-f-l-

		org		$80

canboot	dta		0

chardt1	dta		0
chardt2	dta		0
charpos	dta		0
charadr	dta		a(0)
charmsk	dta		0
charodd	dta		0
char_x	dta		0
char_y	dta		0
scrnad1	dta		a(0)
scrnad2	dta		a(0)
msgadr	dta		a(0)

spracl	dta		0
sprach	dta		0
spricl	dta		0
sprich	dta		0

rtcsav	dta		0,0,0
rtctmp	dta		0,0,0

phase	dta		0
		opt		l+

		org		$5000,$d000
		opt		o+f+
	
.proc BootScreen
		clc
		dta		{bit $00}
boot_entry:
		sec
		php

		jsr		BootClearRam

		plp
		ror		canboot
		
		;relocate data down to RAM
		mwa		#[.adr BootRamDataStart]+$5000-$D000 scrnad1
		mwa		#BootRamDataStart scrnad2

		.if		>[BootRamDataEnd-BootRamDataStart]
		ldx		#>[BootRamDataEnd-BootRamDataStart]
		ldy		#0
page_copy:
		mva:rne	(scrnad1),y (scrnad2),y+
		inc		scrnad1+1
		inc		scrnad2+1
		dex
		bne		page_copy
		.endif

		.if <[BootRamDataEnd-BootRamDataStart]
byte_copy:
		mva		(scrnad1),y (scrnad2),y+
		cpy		#<[BootRamDataEnd-BootRamDataStart]
		bne		byte_copy
		.endif

		;copy down bitmap graphics
		mwa		#playfield3+19 scrnad2
		mwa		#BootDriveImage scrnad1
		ldx		#40
blit_loop:
		ldy		#12
		mva:rpl	(scrnad1),y (scrnad2),y-
		clc
		lda		scrnad2
		adc		#$20
		sta		scrnad2
		scc:inc	scrnad2+1
		lda		scrnad1
		clc
		adc		#13
		sta		scrnad1
		scc:inc	scrnad1+1
		dex
		bne		blit_loop

		;set up display and interrupts
		sei
		jsr		BootSwapVectors
		mva		#0 nmien
		mva		#$c0 nmien
		mva		#>player0 pmbase

		ldx		#$0c
		mva:rpl	gtia_data,x hposp0,x-

		mva		#$03 gractl
		cli
		
		;init sprites 0,1
		ldx		#0
		ldy		#32
sprinit:
		mva		spr0dat,x player0,y
		mva		spr1dat,x player1,y+
		tya
		and		#$0f
		bne		sprinit
		inx
		cpx		#8
		bne		sprinit
		
		lda		#0
		sta		char_x
		sta		char_y

		sta		msgadr
imloop:
		ldy		msgadr
		inc		msgadr
		lda		BootMessage,y
		cmp		#$ff
		beq		imdone
		jsr		PlotChar
		jmp		imloop
imdone:
		
		lda		#0
		sta		phase
loop1:
		ldx		#0
		txa
sprclr:
		sta		player2,x
		sta		player3,x
		dex
		bne		sprclr
		
		;save off the current RTCLOK
		lda:rne	vcount
		ldx		#2
		mva:rpl	rtclok,x rtcsav,x-

		;clear the break key in case it was pressed to interrupt the last read
		stx		brkkey
		
		;issue status request to D1:
		lda		#1
		sta		dunit
		mva		#$53 dcomnd
		jsr		dskinv
		bmi		loop2

		;try to read sector #1 on D1:
		mva		#$52 dcomnd
		mwa		#$0400 dbuflo
		mwa		#1 daux1
		jsr		dskinv
		bmi		loop2
		jmp		BootScreenExit
loop2:
		lda		rtclok+2
		cmp:req	rtclok+2
		lda		rtclok+2
		cmp:req	rtclok+2
		
		ldx		phase
		mwa		phasetab+1,x spricl
		
		ldy		phasetab,x
		lda		#0
		sta		spracl
		sta		sprach
clloop:
		sta		missiles,y
		sta		player2,y
		sta		player3,y
		dey
		bmi		clloop
		
		ldy		phasetab,x
		lda		#$ff
blitloop:
		ldx		sprach
		lda		sprdat2,x
		sta		player2,y
		lda		sprdat3,x
		sta		player3,y
		
		iny
		adw		spracl spricl spracl
		lda		sprach
		cmp		#$20
		bcc		blitloop
		lda		#0
blitloop2:
		sta		missiles,y
		sta		player2,y
		sta		player3,y
		iny
		cpy		#$40
		bne		blitloop2

		;if this is the first animation frame, wait a bit after it
		lda		phase
		bne		no_wait
		
		ldx		#25
waitloop:
		lda		rtclok+2
		cmp:req	rtclok+2
		dex
		bne		waitloop
		txa
		
no_wait:
		clc
		adc		#3
		sta		phase
		cmp		#phasetab_end-phasetab
		scs:jmp	loop2
		
		lda		#0
		sta		phase		
		jmp		loop1

phasetab:
		:48 dta	$7a+[48-#]/3, a([$200+#*#*3]/3)
phasetab_end:

gtia_data:
		dta		$40,$60,$a4,$98,$68,$60,$00,$00
		dta		$03,$03,$00,$03,$0f
.endp

;==========================================================================	
.proc BootScreenExit
		sei
		lda		#0
		sta		nmien

		;wait for vertical blank
		ldx		#248/2
		cpx:rne	vcount

		;shut off display
		sta		dmactl
		sta		sdmctl

		;reset GTIA (but not VBXE)
		sta:rpl	$d000,x+

		;check if we can continue the initial boot process
		bit		canboot
		bmi		continue_boot
		
		;invoke cold start
		jmp		coldsv

continue_boot:
		;swap interrupt handler registers back
		jsr		BootSwapVectors

		;re-wipe RAM areas we used
		jsr		BootClearRam

		;re-enable IRQs (but VBI is still disabled)
		cli

		;print a clear screen character to clear any latent BOOT ERRORs
		lda		#$7d
		ldx		#0
		stx		icbll
		stx		icblh
		ldy		#CIOCmdPutChars
		sty		iccmd
		jsr		ciov

		;re-enable VBI and wait for VBI stage 2 to run once
		mva		#$40 nmien

		lda		#$ff
		sta		ptrig7
		bit:rmi	ptrig7

		;all done, return to boot code with sector 1 loaded at $0400
		rts
.endp

;==========================================================================
.proc BootClearRam
		;clear $2000-$37FF
		lda		#0
		tay
		sta		charadr
		ldx		#$20
		stx		charadr+1
		ldx		#$18
clear_loop:
		sta:rne	(charadr),y+
		inc		charadr+1
		dex
		bne		clear_loop

		;clear user page zero
		sta:rpl	$80,x+

		;all done
		rts
.endp

;==========================================================================
.proc BootScreenVbi
		lda		pfadr+1
		eor		#$05
		sta		pfadr+1
		lda		color1
		sta		colpf1
		lda		color2
		sta		colpf2
		jmp		(BootVecVbi)
.endp

;==========================================================================
.proc BootScreenIrq
		bit		nmist
		bpl		do_irq
.def :BootScreenDli
		pha
		txa
		pha
		lda		vcount
		cmp		#72
		bcs		split
		lsr
		lsr
		tax
		lda		coltab-5,x
		sta		wsync
		sta		colpm0
		sta		colpm1
xit:
		sta		nmires
		pla
		tax
		pla
		rti
do_irq:
		jmp		(BootVecIrq)
split:
		lda		#$50
		ldx		#$5c
		sta		wsync
		sta		colpf2
		stx		colpf1

		jmp		xit
.endp

;==========================================================================			
.local BootMessage
		;		  0123456789012345678901234567890123456789012345678901234567890123
		dta		d"AltirraOS"
		_KERNELSTR_BIOS_NAME_INTERNAL
		dta		d" "
		_KERNELSTR_VERSION_INTERNAL
		
		dta		$9b

		.ifdef _KERNEL_816
		dta		d"Copyright (C) 2018-2022 Avery Lee",$9b
		.else
		dta		d"Copyright (C) 2012-2022 Avery Lee",$9b
		.endif

		dta		d"All Rights Reserved",$9b
		dta		d"This is a substitute for the standard OS ROM. See the help file",$9b
		dta		d"for how to use real Atari ROM images for higher compatibility."
		dta		$ff
.endl

;==========================================================================	
plotchar:
		cmp		#$9b
		bne		not_eol
		mva		#0 char_x
		inc		char_y
		rts
not_eol:
		sta		charadr

		lda		#0
		sta		scrnad1
		sta		scrnad2
		lda		char_y
		ora		#>playfield
		sta		scrnad1+1
		clc
		adc		#$05
		sta		scrnad2+1

		lda		char_x
		lsr
		sta		charpos
		ror
		sta		charodd

		lda		#$e0/8
		ldx		#3
shlloop:
		asl		charadr
		rol
		dex
		bne		shlloop
		sta		charadr+1
		
		clc
rowloop:
		ldx		#4
		ldy		#0
		lda		(charadr),y
		lsr
bitloop:
		lsr
		ror		chardt1
		lsr
		ror		chardt2
		dex
		bne		bitloop

		ldy		charpos
		bit		charodd
		bmi		odd
		lda		chardt2
		eor		(scrnad2),y
		and		#$0f
		eor		chardt2
		sta		(scrnad2),y

		lda		chardt1
		eor		(scrnad1),y
		and		#$0f
		
		bpl		even
odd:
		lda		chardt1
		lsr
		lsr
		lsr
		lsr
		sta		chardt1
		lda		chardt2
		lsr
		lsr
		lsr
		lsr
		sta		chardt2
		eor		(scrnad2),y
		and		#$f0
		eor		chardt2
		sta		(scrnad2),y
		lda		chardt1
		eor		(scrnad1),y
		and		#$f0
even:
		eor		chardt1
		sta		(scrnad1),y
		inc		charadr
		tya
		clc
		adc		#$20
		sta		charpos
		bcc		rowloop
		
		;update position
		inc		char_x
		ldx		char_x
		cpx		#64
		bcc		done
		mva		#0 char_x
		inc		char_y
done:
		rts

;==========================================================================	
spr0dat:
		dta		%00000000
		dta		%00000011
		dta		%00001111
		dta		%00111100
		dta		%00111100
		dta		%00111111
		dta		%00111100
		dta		%00000000

spr1dat:
		dta		%00000000
		dta		%11000000
		dta		%11110000
		dta		%00111100
		dta		%00111100
		dta		%11111100
		dta		%00111100
		dta		%00000000

sprdat2:
		dta		%00000000
		dta		%00000000
		dta		%00011000
		dta		%00111100
		dta		%00111100
		dta		%00111100
		dta		%00111100
		dta		%00111100
		dta		%00111100
		dta		%00011000
		dta		%00000000
		dta		%00000000
		dta		%11100111
		dta		%10000001
		dta		%10000001
		dta		%00000000
		dta		%00000000
		dta		%10000001
		dta		%10000001
		dta		%11100111
		dta		%00000000
		dta		%00000000
		dta		%00000000
		dta		%00000000
		dta		%00000000
		dta		%00000000
		dta		%00000000
		dta		%00000000
		dta		%00000000
		dta		%00000000
		dta		%00000000
		dta		%00000000
		
sprdat3:
		dta		%11111111
		dta		%11111111
		dta		%11111111
		dta		%11111111
		dta		%11111111
		dta		%11111111
		dta		%11111111
		dta		%11111111
		dta		%11111111
		dta		%11111111
		dta		%11111111
		dta		%11111111
		dta		%11100111
		dta		%11100111
		dta		%11100111
		dta		%11100111
		dta		%11100111
		dta		%11100111
		dta		%11100111
		dta		%11100111
		dta		%11111111
		dta		%11111111
		dta		%11111111
		dta		%11111111
		dta		%11111111
		dta		%11111111
		dta		%01111111
		dta		%11111111
		dta		%11111111
		dta		%11111111
		dta		%11111111
		dta		%11111111

;==========================================================================	
		
;==========================================================================	
coltab:
		dta		$1a
		dta		$fa
		dta		$ea
		dta		$da
		dta		$ca
		dta		$ba
		dta		$aa
		dta		$9a
		dta		$8a
		dta		$7a
		dta		$6a
		dta		$5a
		
		dta		$0E,$50

;==========================================================================	
.proc BootSwapVectors
		ldx		#vec_offsets_end-vec_offsets-1
loop:
		ldy		vec_offsets,x
		lda		$0200,y
		pha
		lda		BootVectors,x
		sta		$0200,y
		pla
		sta		BootVectors,x
		dex
		bpl		loop
		rts

vec_offsets:
		dta		vvblki-$0200
		dta		vvblki-$0200+1
		dta		vimirq-$0200
		dta		vimirq-$0200+1
		dta		vdslst-$0200
		dta		vdslst-$0200+1
		dta		sdlstl-$0200
		dta		sdlsth-$0200
		dta		pcolr0-$0200
		dta		pcolr1-$0200
		dta		pcolr2-$0200
		dta		pcolr3-$0200
		dta		color0-$0200
		dta		color1-$0200
		dta		color2-$0200
		dta		color3-$0200
		dta		color4-$0200
		dta		sdmctl-$0200
		dta		gprior-$0200
		dta		coldst-$0200
vec_offsets_end:
.endp

;==========================================================================	
.proc BootDriveImage
		icl		'driveimage.inc'
.endp

;==========================================================================	
; Data relocated to RAM
;
		org		$3000,*-$5000+$d000

BootRamDataStart = *

BootVectors:
BootVecVbi		dta		a(BootScreenVbi)
BootVecIrq		dta		a(BootScreenIrq)
BootVecDli		dta		a(BootScreenDli)
BootVecDlist	dta		a(BootDlist)
color_table:
		dta		$00
		dta		$00
		dta		$02
		dta		$00
		dta		$08
		dta		$0C
		dta		$02
		dta		$14
		dta		$50
		dta		$3d		;sdmctl
		dta		$01		;gprior
		dta		$ff		;coldst, to force cold reboot if RESET pressed

BootDlist:
		:4 dta	$70
		:8 dta	$F0

		;drive image (40 lines x 32 bytes = $500 bytes
		dta		$4E,a(playfield3)
		:6 dta	$0E
		dta		$8E
		:4 dta	$0E,$0E,$0E,$0E,$0E,$0E,$0E,$8E
		
		dta		$F0
		dta		$70
		
		;80-col text screen (2x 40 rows)
		dta		$4F,a(playfield)
pfadr = *-2
		:7 dta	$0F
		:8 dta	$0F
		:8 dta	$0F
		dta		$70
		:8 dta	$0F
		:8 dta	$0F
		dta		$41,a(BootDlist)

BootRamDataEnd = *

;==========================================================================	
; Version block for emulator
;
		org		$d7f8
		_KERNELSTR_VERSION
		dta		0


		org		$d7ff
		dta		0

		opt		l-f-
		
		org		$2000
playfield:
		org		$2500
playfield2:

		org		$2b00
missiles:
		org		$2c00
player0:
		org		$2d00
player1:
		org		$2e00
player2:
		org		$2f00
player3:
		org		$3200
playfield3:

		opt		l+
		org		$d800

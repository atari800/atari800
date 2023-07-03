;	Altirra - Atari 800/800XL emulator
;	Kernel ROM replacement - Initialization
;	Copyright (C) 2008-2019 Avery Lee
;
;	Copying and distribution of this file, with or without modification,
;	are permitted in any medium without royalty provided the copyright
;	notice and this notice are preserved.  This file is offered as-is,
;	without any warranty.

.if _KERNEL_XLXE
.proc InitBootSignature
	dta		$5C,$93,$25
.endp
.endif

.proc InitHandlerTable
	dta		c'P',a(printv)
	dta		c'C',a(casetv)
	dta		c'E',a(editrv)
	dta		c'S',a(screnv)
	dta		c'K',a(keybdv)
.endp

.nowarn .proc _InitReset
run_diag:
	; start diagnostic cartridge
	jmp		($bffe)

.def :InitReset
	;mask interrupts and initialize CPU
	sei
	cld
	ldx		#$ff
	txs
	
.if _KERNEL_XLXE
	;wait for everything to stabilize (0.1s) (XL/XE only)
	ldy		#140
stabilize_loop:
	dex:rne
	dey
	bne		stabilize_loop
	
	;check for warmstart signature (XL/XE)
	ldx		#2
warm_check:
	lda		pupbt1,x
	cmp		InitBootSignature,x
	bne		cold_boot
	dex
	bpl		warm_check
	
	jmp		InitWarmStart
.endif

.def :InitColdStart
cold_boot:
	; 1. initialize CPU
	sei
	cld
	ldx		#$ff
	txs
	
	; 2. clear warmstart flag
	inx
	stx		warmst
	
	; 3. test for diagnostic cartridge
	lda		$bffc
	bne		not_diag
	ldx		#$ff
	cpx		$bfff			;prevent diagnostic cart from activating if addr is $FFxx
	beq		not_diag
	stx		$bffc
	cmp		$bffc
	sta		$bffc
	bne		not_diag
	
	; is it enabled?
	bit		$bffd
	bmi		run_diag
		
not_diag:
	
	jsr		InitHardwareReset

	.if _KERNEL_XLXE	
	;check for OPTION and enable BASIC (note that we do NOT set BASICF just yet)
	lda		#4
	bit		consol
	beq		no_basic

.ifdef _KERNEL_SOFTKICK
	ldx		#$fc
.else
	ldx		#$fd
.endif
	
	;check for keyboard present + SELECT or no keyboard + no SELECT and enable game if so
	lda		trig2		;check keyboard present (1 = present)
	asl
	eor		consol		;XOR against select (0 = pressed)
	and		#$02
	
.ifdef _KERNEL_SOFTKICK
	seq:ldx	#$be
.else
	seq:ldx	#$bf
.endif

	stx		portb		;enable GAME or BASIC

no_basic:
	.endif

	; 4. measure memory -> tramsz
	jsr		InitMemory
	
	; 6. clear memory from $0008 up to [tramsz,0]
.ifdef _KERNEL_SOFTKICK
	ldx		#$50
.else
	ldx		tramsz
.endif
	ldy		#8
	mva		#0 a1
	sta		a1+1
clearloop:
	sta:rne	(a1),y+
	inc		a1+1
	dex
	bne		clearloop
	
.if _KERNEL_XLXE
	;Blip the self test ROM for a second -- this is one way that Altirra detects
	;that Option has been used by the OS. The XL/XE OS does this as part of its
	;ROM checksum routine (which we don't bother with).
	ldx		portb
	txa
	and		#$7f
	sta		portb
	stx		portb
.endif

	; 7. set dosvec to blackboard routine
.if _KERNEL_USE_BOOT_SCREEN
	mwa		#SelfTestEntry dosvec
.else
	mwa		#Blackboard dosvec
.endif
	
	; 8. set coldstart flag
	dec		coldst		;!! coldst=0 from clear loop above, now $ff
	
.if _KERNEL_XLXE
	; set BASIC flag
	lda		portb
	and		#$02
	sta		basicf
.endif

	; 9. set screen margins
	; 10. initialize RAM vectors
	; 11. set misc database values
	; 12. enable IRQ interrupts
	; 13. initialize device table
	; 14. initialize cartridges
	; 15. use IOCB #0 to open screen editor (E)
	; 16. wait for VBLANK so screen is initialized
	; 17. do cassette boot, if it was requested
	; 18. do disk boot
	; 19. reset coldstart flag
	; 20. run cartridges or blackboard
	jmp		InitEnvironment
.endp

;==============================================================================
.proc InitWarmStart
	; A. initialize CPU
	;
	; Undocumented: Check if cold start completed (COLDST=0); if not, force
	; a cold start. ACTris 2.1 relies on this since its boot doesn't reset
	; COLDST. This is also required to work on Atari800WinPLus, which doesn't
	; clear memory on a cold start.
	;
	sei								;!! FIRST TWO BYTES CHECKED BY ARCHON
	lda		coldst
	bne		InitColdStart
	cld
	ldx		#$ff
	txs

	; B. set warmstart flag
	stx		warmst
	
	; reinitialize hardware without doing a full clear
	jsr		InitHardwareReset
	
	.if _KERNEL_XLXE
	; reinitialize BASIC
	lda		basicf
	sne:mva #$fd portb
	.endif
	
	; C. check for diag, measure memory, clear hw registers
	jsr		InitMemory
	
	; D. zero 0010-007F and 0200-03EC (must not clear BASICF).
	ldx		#$60
	lda		#0
	sta:rne	$0f,x-
	
dbclear:
	sta		$0200,x
	sta		$02ed,x
	inx
	bne		dbclear
	
	; E. steps 9-16 above
	; F. if cassette boot was successful on cold boot, execute cassette init
	; G. if disk boot was successful on cold boot, execute disk init
	; H. same as steps 19 and 20
	jmp		InitEnvironment
.endp

;==============================================================================
.proc InitHardwareReset
	; clear all hardware registers
	ldy		#0
	tya
hwclear:
	sta		$d000,y
	sta		$d200,y
	sta		$d400,y
	iny
	bne		hwclear

	;initialize PIA
	lda		#$3c
	ldx		#$38

.ifdef _KERNEL_SOFTKICK
	stx		pactl		;switch to DDRA
	sty		porta		;portA -> input
	sta		pactl		;switch to IORA
	sty		porta		;portA -> $00
	sta		pbctl		;switch to IORB
	dey
	dey
	sty		portb		;portB -> $FE
	stx		pbctl		;switch to DDRB
	iny
	sty		portb		;portB -> all output
	sta		pbctl		;switch to IORB
.elseif _KERNEL_XLXE
	stx		pactl		;switch to DDRA
	sty		porta		;portA -> input
	sta		pactl		;switch to IORA
	sty		porta		;portA -> $00
	sta		pbctl		;switch to IORB
	dey
	sty		portb		;portB -> $FF
	stx		pbctl		;switch to DDRB
	sty		portb		;portB -> all output
	sta		pbctl		;switch to IORB
.else
	stx		pactl		;switch to DDRA
	stx		pbctl		;switch to DDRB
	sty		porta		;portA -> input
	sty		portb		;portB -> input
	sta		pactl		;switch to IORA
	sta		pbctl		;switch to IORB
	sty		porta		;portA -> $00
	sty		portb		;portB -> $00
.endif
	rts
.endp

;==============================================================================
.proc InitMemory	
	; 4. measure memory -> tramsz
	ldy		#$00
	sty		adress
	ldx		#$02
pageloop:
	stx		adress+1
	lda		(adress),y
	eor		#$ff
	sta		(adress),y
	cmp		(adress),y
	bne		notRAM
	eor		#$ff
	sta		(adress),y
	inx

.if _KERNEL_XLXE
	cpx		#$c0
.else
	cpx		#$d0
.endif

	bne		pageloop
notRAM:
	stx		tramsz
	
	rts
.endp

;==============================================================================
.proc InitVectorTable1
	dta		a(IntExitHandler_None)		;$0200 VDSLST
	dta		a(IntExitHandler_A)			;$0202 VPRCED
	dta		a(IntExitHandler_A)			;$0204 VINTER
	dta		a(IntExitHandler_A)			;$0206 VBREAK
	dta		a(KeyboardIRQ)				;$0208 VKEYBD
	dta		a(SIOInputReadyHandler)		;$020A VSERIN
	dta		a(SIOOutputReadyHandler)	;$020C VSEROR
	dta		a(SIOOutputCompleteHandler)	;$020E VSEROC
	dta		a(IntExitHandler_A)			;$0210 VTIMR1
	dta		a(IntExitHandler_A)			;$0212 VTIMR2
	dta		a(IntExitHandler_A)			;$0214 VTIMR4
	dta		a(IrqHandler)				;$0216 VIMIRQ
end:
.endp

.proc InitVectorTable2
	dta		a(VBIStage1)				;$0222 VVBLKI
	dta		a(VBIExit)					;$0224 VVBLKD
	dta		a(0)						;$0226 CDTMA1
.endp

krpdel_table:
	dta		48,40

;==============================================================================
.proc InitEnvironment
	mva		tramsz ramsiz

.if _KERNEL_XLXE	
	; set warmstart signature -- must be done before cart init, because
	; SDX doesn't return.
	ldx		#3
	mva:rne	InitBootSignature-1,x pupbt1-1,x-
.endif

	; 9. set screen margins
	mva		#2 lmargn
	mva		#39 rmargn
	
	;set PAL/NTSC flag (XL/XE only)
	.if _KERNEL_XLXE
	ldx		#0
	lda		pal
	ldy		#6
	and		#$0e
	bne		is_ntsc
	inx
	dey
is_ntsc:
	stx		palnts
	sty		keyrep
	mva		krpdel_table,x krpdel
.endif
	
	; 10. initialize RAM vectors

	;VDSLST-VIMIRQ
	ldx		#[.len InitVectorTable1]-1
	mva:rpl	InitVectorTable1,x vdslst,x-

	;VVBLKI-CDTMA1
	ldx		#[.len InitVectorTable2]-1
	mva:rpl	InitVectorTable2,x vvblki,x-
	
	mwa		#KeyboardBreakIRQ		brkky
	
	; 11. set misc database values
	ldx		#$ff
	stx		brkkey
	inx
	stx		memtop
	stx		memlo
	mva		tramsz memtop+1
	mvx		#$07 memlo+1
	
	jsr		DiskInit
	jsr		ScreenInit
	;jsr	DisplayInit
	jsr		KeyboardInit

.if _KERNEL_PRINTER_SUPPORT	
	jsr		PrinterInit
.endif

	; 13. initialize device table (HATABS has already been cleared)
	; NOTE: The R: emulation relies on this being before CIOINV is invoked.
	ldx		#14
	mva:rpl	InitHandlerTable,x hatabs,x-

	;jsr	CassetteInit
	jsr		cioinv
	jsr		SIOInit
	jsr		IntInitInterrupts
	
	; check for START key, and if so, set cassette boot flag
	lda		consol
	and		#1
	eor		#1
	sta		ckey
	
.if _KERNEL_PBI_SUPPORT
	jsr		PBIScan
.endif

	; 12. enable IRQ interrupts
	;
	; We do this later than the original OS specification because the PBI scan needs
	; to happen with IRQs disabled (a PBI device with interrupts may not have been
	; inited yet) and that PBI scan in turn needs to happen after HATABS has been
	; set up. There's no harm in initing HATABS with interrupts masked, so we do so.
	
	cli
	
	; 14. initialize cartridges
	mva		#0 tstdat

.if !_KERNEL_XLXE
	lda		$9ffc
	bne		skipCartBInit
	lda		$9ffb
	tax
	eor		#$ff
	sta		$9ffb
	cmp		$9ffb
	stx		$9ffb
	beq		skipCartBInit
	jsr		InitCartB
	mva		#1 tstdat
skipCartBInit:
.endif
	
	mva		#0 tramsz
	lda		$bffc
	bne		skipCartAInit
	lda		$bffb
	tax
	eor		#$ff
	sta		$bffb
	cmp		$bffb
	stx		$bffb
	beq		skipCartAInit
	jsr		InitCartA
	mva		#1 tramsz
skipCartAInit:

	; 15. use IOCB #0 ($0340) to open screen editor (E)
	;
	; NOTE: We _must_ leave $0C in the A register when invoking CIO. Pooyan
	; relies on $0C being left in CIOCHR after the last call to CIO before
	; disk boot!

	mva		#$03 iccmd		;OPEN
	mwa		#ScreenEditorName icbal
	mva		#$0c icax1		;read/write, no forced read
	ldx		#0
	stx		icax2			;mode 0
	jsr		ciov
	
	; 16. wait for VBLANK so screen is initialized
	lda		rtclok+2
waitvbl:
	cmp		rtclok+2
	beq		waitvbl

	;-------------------------------------------------------------------------
	; Pre-boot hook (AltirraOS-specific)
	;

	.ifdef	_KERNEL_PRE_BOOT_HOOK
	jsr		InitPreBootHook
	.endif

	;-------------------------------------------------------------------------
	; Run cartridge/cassette/disk
	;

	; 17. do cassette boot, if it was requested
	; F. if cassette boot was successful on cold boot, execute cassette init
	
	; The cold boot path must check the warm start flag and switch paths if
	; necessary. SpartaDOS X relies on being able to set the warm start
	; flag from its cart init handler.
	;
	; Disk boot must be attempted after cassette boot. Besides support for
	; using DOS from tape-based software, this is also required by the tape
	; version of Fun With Spelling (featuring Heathcliff), which depends on
	; the SIO request causing the tape start vector to be invoked shortly
	; after VBI so it can do an unsafe screen swap.
	;
	
	lda		warmst
	bne		reinitcas
	
	lda		ckey
	beq		postcasboot
	jsr		BootCassette
	jmp		postcasboot

reinitcas:
	lda		#2
	bit		boot?
	beq		postcasboot
	jsr		InitCassetteBoot
postcasboot:

	;-------------------------------------------------------------------------
	; 18. do disk boot
	; G. if disk boot was successful on cold boot, execute disk init
	;
	; For 800 mode, we must check if either cart A or cart B is present,
	; doing a disk boot if either there are no carts or one of the carts
	; requests disk boot. For the XL/XE case, cart B doesn't exist and we
	; can simplify the logic.
	;

	lda		warmst
	bne		reinitDisk
	
.if !_KERNEL_XLXE

	;check if we have cart B
	lda		tstdat
	bne		have_cart_b

	;no cart B -- if no cart A either, do disk boot
	ldx		tramsz			;cart A
	beq		boot_disk
	bne		have_cart_a

have_cart_b:
	;have cart B - grab boot disk flag (bit 0)
	lda		$9ffd

	;merge cart A's flags if it is present
	ldx		tramsz
	beq		no_cart_a

have_cart_a:
	ora		$bffd

no_cart_a:
	;skip disk boot if neither cart requested it
	lsr
	bcc		skip_disk_boot

.else

	;check for cart A requesting boot
	lda		tramsz
	beq		boot_disk

	;have cart A - boot disk if requested
	lda		$bffd
	lsr
	bcc		skip_disk_boot

.endif

boot_disk:
	jsr		BootDisk
	jmp		post_disk_boot

reinitDisk:
	lda		boot?
	lsr
	bcc		skip_disk_boot
	jsr		InitDiskBoot

post_disk_boot:
.if _KERNEL_XLXE
	; (XL/XE only) do type 3 poll or reinit handlers
	; !! - must only do this if a disk boot occurs; Pole Position audio breaks if
	; we do this and hit SKCTL before booting the cart
	lda		warmst
	bne		reinit_handlers
	jsr		PHStartupPoll
	jmp		post_reinit
reinit_handlers:
	jsr		PHReinitHandlers
post_reinit:
.endif

skip_disk_boot:

	;-------------------------------------------------------------------------
	; H. same as steps 19 and 20
	; 19. reset coldstart flag
	
	ldx		#0
	stx		coldst

	;-------------------------------------------------------------------------
	; 20. run cartridges or blackboard
	;
	; Weird quirk here: if the left cart is absent or doesn't request
	; cart start, and the right cart is present and also doesn't request
	; cart start, OS-B endlessly does disk boots instead of running (DOSVEC).
	; We don't emulate this behavior for now since in practice it just seems
	; useless, boot looping until DOS crashes. It is moot starting with the
	; 1200XL DOS due to support for cart B being dropped.
	;
	
	; try to boot cart A
	lda		#$04
	ldx		tramsz
	beq		NoBootCartA
	bit		$bffd
	seq:jmp	($bffa)
NoBootCartA:

	; try to boot cart B
	ldx		tstdat
	beq		NoBootCartB
	bit		$9ffd
	seq:jmp	($9ffa)
NoBootCartB:

	; run blackboard
	jmp		(dosvec)

InitCartA:
	jmp		($bffe)

InitCartB:
	jmp		($9ffe)
	
ScreenEditorName:
	dta		c"E"

.endp

;==============================================================================
.proc InitDiskBoot
	jmp		(dosini)
.endp

.proc InitCassetteBoot
.endp
	jmp		(casini)

;==============================================================================

	nop

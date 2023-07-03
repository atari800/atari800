;	Altirra - Atari 800/800XL/5200 emulator
;	Modular Kernel ROM - Vertical Blank Interrupt Services
;	Copyright (C) 2008-2023 Avery Lee
;
;	Copying and distribution of this file, with or without modification,
;	are permitted in any medium without royalty provided the copyright
;	notice and this notice are preserved.  This file is offered as-is,
;	without any warranty.

;==========================================================================
; VBIExit - Vertical Blank Interrupt Exit Routine
;
; This is a drop-in replacement for XITVBV.
;
VBIExit = VBIProcess.xit

;==========================================================================
; VBIProcess - Vertical Blank Processor
;
VBIStage1 = VBIProcess.stage_1
VBIStage2 = VBIProcess.stage_2
.proc VBIProcess
stage_1:
	;increment real-time clock and do attract processing
	inc		rtclok+2
	bne		clock_done
	inc		atract
	inc		rtclok+1
	bne		clock_done
	inc		rtclok
clock_done:

	;Pole Position depends on DRKMSK and COLRSH being reset from VBI as it
	;clears kernel vars after startup.
	ldx		#$fe				;default to no mask
	lda		#$00				;default to no color alteration
	ldy		atract				;check attract counter
	bpl		attract_off			;skip if attract is off
	stx		atract				;lock the attract counter
	ldx		#$f6				;set mask to dim colors
	lda		rtclok+1			;use clock to randomize colors
attract_off:	
	stx		drkmsk				;set color mask
	sta		colrsh				;set color modifier
	
	;atract color 1 only
	eor		color1
	and		drkmsk
	sta		colpf1

	;decrement timer 1 and check for underflow
	lda		cdtmv1				;check low byte
	bne		timer1_lobytezero	;if non-zero, decrement and check for fire
	lda		cdtmv1+1			;check high byte
	beq		timer1_done			;if clear, timer's not running
	dec		cdtmv1+1			;decrement high byte
timer1_lobytezero:
	dec		cdtmv1				;decrement low byte
	bne		timer1_done			;no underflow if non-zero
	lda		cdtmv1+1			;low byte is zero... check if high byte is too
	bne		timer1_done			;if it's not, we're not done yet ($xx00 > 0)
	jsr		timer1_dispatch		;jump through timer vector
timer1_done:

	;check for critical operation
	lda		critic
	beq		no_critic
xit:
	pla
	tay
	pla
	tax
exit_a:
	pla
exit_none:
	rti

timer1_dispatch:
	jmp		(cdtma1)

no_critic:
	lda		#$04			;I flag
	tsx
	and		$0104,x			;I flag set on pushed stack?
	bne		xit				;exit if so
	
	;======== stage 2 processing
	
stage_2:	
	;re-enable interrupts
	cli

	;update shadow registers
	mva		sdlsth	dlisth
	mva		sdlstl	dlistl
	mva		sdmctl	dmactl
	mva		chbas	chbase
	mva		chact	chactl
	mva		gprior	prior
	
	ldx		#8
	stx		consol				;sneak in speaker reset while we have an 8
ColorLoop
	lda		pcolr0,x
	eor		colrsh
	and		drkmsk
	sta		colpm0,x
	dex
	bpl		ColorLoop

	;decrement timer 2 and check for underflow
	ldx		#3
	jsr		VBIDecrementTimer
	sne:jsr	Timer2Dispatch

	;Decrement timers 3-5 and set flags
	;
	;[OS Manual] Appendix L, page 254 says that the OS never modifies CDTMF3-5
	;except to set them to zero on timeout at init. This is a LIE. It also sets
	;the flags to $FF when they are running. It does not write the flags when
	;the timer is idle. Spider Quake depends on this.
	;
	ldx		#9
timer_n_loop:
	clc
	jsr		VBIDecrementTimer
	bcs		timer_n_not_running
	seq:lda	#$ff
timer_n_not_expired:
	sta		cdtmf3-5,x
timer_n_not_running:
	dex
	dex
	cpx		#5
	bcs		timer_n_loop
	
	;Read POKEY keyboard register and handle auto-repeat
	lda		skstat				;get key status
	and		#$04				;check if key is down
	bne		no_repeat_key		;skip if not
	dec		srtimr				;decrement repeat timer
	bne		no_repeat			;skip if not time to repeat yet
	mva		kbcode ch			;repeat last key

.if _KERNEL_XLXE
	lda		keyrep				;reset repeat timer
	jmp		no_keydel			;skip debounce counter decrement
.else
	lda		#$06				;reset repeat timer
	bne		no_keydel			;skip debounce counter decrement
.endif

no_repeat_key:
	;decrement keyboard debounce counter (must only be done if key is not held down)
	lda		keydel
	seq:dec	keydel

	;reset repeat timer
	lda		#0
no_keydel:
	sta		srtimr
no_repeat:

	;Update controller shadows.
	;
	;The PORTA/PORTB decoding is a bit complex:
	;
	;	bits 0-3 -> STICK0/4 (and no, we cannot leave junk in the high bits)
	;	bits 4-7 -> STICK1/5
	;	bit 2    -> PTRIG0/4
	;	bit 3    -> PTRIG1/5
	;	bit 6    -> PTRIG2/6
	;	bit 7    -> PTRIG3/7
	;
	;XL/XE machines only have two joystick ports, so the results of ports 0-1
	;are mapped onto ports 2-3.
	;
	
.if _KERNEL_XLXE
	lda		porta
	tax
	and		#$0f
	sta		stick0
	sta		stick2
	txa
	lsr						;shr1
	lsr						;shr2
	tax
	lsr						;shr3
	lsr						;shr4
	sta		stick1
	sta		stick3
	lsr						;shr5
	lsr						;shr6
	tay
	and		#$01
	sta		ptrig2
	tya
	lsr
	sta		ptrig3
	txa
	and		#$01
	sta		ptrig0
	txa
	lsr
	and		#$01
	sta		ptrig1

	ldx		#3
pot_loop:
	lda		pot0,x
	sta		paddl0,x
	sta		paddl4,x
	lda		ptrig0,x
	sta		ptrig4,x
	dex
	bpl		pot_loop

	ldx		#1
port_loop:
	lda		trig0,x
	sta		strig0,x
	sta		strig2,x
	dex
	bpl		port_loop

.else
	ldx		#7
pot_loop:
	lda		pot0,x
	sta		paddl0,x
	lda		#0
	sta		ptrig0,x
	dex
	bpl		pot_loop
	
	ldx		#3
trig_loop:
	lda		trig0,x
	sta		strig0,x
	dex
	bpl		trig_loop

	lda		porta
	ldx		#0
	ldy		#0
	jsr		do_stick_ptrigs
	lda		portb
	ldx		#4
	ldy		#2
	jsr		do_stick_ptrigs
.endif
	
	;restart pots (required for SysInfo)
	sta		potgo
	
	;update light pen
	mva		penh lpenh
	mva		penv lpenv
	
	jmp		(vvblkd)	;jump through vblank deferred vector
	
Timer2Dispatch
	jmp		(cdtma2)
	
.if !_KERNEL_XLXE
do_stick_ptrigs:
	pha
	and		#$0f
	sta		stick0,y
	pla
	lsr
	lsr
	lsr
	rol		ptrig0,x
	lsr
	rol		ptrig1,x
	sta		stick1,y
	lsr
	lsr
	lsr
	rol		ptrig2,x
	lsr
	rol		ptrig3,x
	rts
.endif

.endp

;==========================================================================
; VBIDecrementTimer
;
; Entry:
;	X = timer index *2+1 (1-9)
;
; Exit:
;	C=1,      Z=0, A!0		timer not running
;	C=0/same, Z=1, A=0		timer just expired
;	C=0/same, Z=0, A=?		timer still running
;
.proc VBIDecrementTimer
	;check low byte
	lda		cdtmv1-1,x
	bne		lononzero
	
	;check high byte; set C=1/Z=1 if zero, C=0/Z=0 otherwise
	cmp		cdtmv1,x
	bne		lozero_hinonzero
	
	;both bytes are zero, so timer's not running
	txa
	rts
	
lozero_hinonzero:
	;decrement high byte
	dec		cdtmv1,x

lononzero:
	;decrement low byte
	dec		cdtmv1-1,x
	bne		still_running
	
	;return high byte to set Z appropriately
	lda		cdtmv1,x
still_running:
	rts
.endp

;==========================================================================
; VBISetVector - set vertical blank vector or counter
;
; This is a drop-in replacement for SETVBV.
;
; A = item to update
;	1-5	timer 1-5 counter value
;	6	VVBLKI
;	7	VVBLKD
; X = MSB
; Y = LSB
;
;NOTE:
;The Atari OS Manual says that DLIs will be disabled after SETVBV is called.
;This only applies to OS-A, which writes NMIEN from SETVBV. OS-B and the
;XL/XE OS avoid this, and the Bewesoft 8-players demo depends on DLIs being
;left enabled.
;
;IRQ mask state must be saved across this proc. DOSDISKA.ATR breaks if IRQs
;are unmasked.
;
;We also cannot touch NMIRES in this routine. This can result in dropped DLIs
;since there is a one cycle window where the 6502 can manage to both write
;NMIRES and still execute the NMI. This results in the NMI dispatcher seeing no
;bits set in NMIST and failing to run the correct handler.
;
.proc VBISetVector	
	;We're relying on a rather tight window here. We can't touch NMIEN, so we have
	;to wing it with DLIs enabled. Problem is, in certain conditions we can be under
	;very tight timing constraints. In order to do this safely we have to finish
	;before a DLI can execute. The worst case is a wide mode 2 line at the end of
	;a vertically scrolled region with P/M graphics enabled and an LMS on the next
	;mode line. In that case we only have 7 cycles before we hit the P/M graphics
	;and another two cycles after that until the DLI fires.
	;
	;The worst case cycle timing looks like this:
	;
	;*			inc wsync
	;ANTIC halts CPU until cycle 105
	;105		playfield DMA
	;106		refresh DMA
	;107-113	free cycles (7)
	;0			missiles
	;1			display list
	;2			player 0
	;3			player 1
	;4			player 2
	;5			player 3
	;6			display list address low
	;7			display list address high
	;8-9		free cycles (2)
	;10			earliest NMI timing
	;
	;There are several annoying cases:
	;
	; 1) The VBI fires on the next scanline.
	;
	;    In this case, neither display list nor P/M DMA can occur, so we have at
	;    least 17 cycles guaranteed before the VBI fires. This is the easiest case
	;    to handle. We should be 100% safe with only the VBI enabled and no DLIs.
	;
	; 2) A DLI fires on the next scanline.
	;
	;    SETVBV can't be used to set VDSLST, so it doesn't directly interact with
	;    DLIs and a DLI splitting the writes isn't an issue. However, the DLI can
	;    end shortly before the VBI and land us in trouble there. The stock OS has
	;    an ~8 cycle vulnerability here.
	;
	;    Using SETVBV with DLIs is already questionable due to WSYNC delaying DLIs,
	;    but to mitigate this, we check VCOUNT and delay the writes if we're too
	;    close to the VBI. There is just enough time to do this safely as long as
	;    the DLI executes before the VCOUNT check.
	;
	;    This can be violated if the multiple DLIs are close together or DMA is
	;    very heavy, but that's beyond the point where using SETVBV is reasonably
	;    safe at all. The case that fails is that the VCOUNT check passes prior to
	;    scan 247, then post-check code is split by a DLI that ends in the danger
	;    zone.
	;
	; 3) An NMI fires during the INC WSYNC.
	;
	;    We're fine if this is a VBI, less fine if it's the DLI. The DLI can execute
	;    at two points: either immediately after the INC WSYNC or one instruction
	;    after it. This depends on whether INC WSYNC ends by cycle 10 so the 6502
	;    can acknowledge and swap to the NMI sequence before RDY halts it. Otherwise,
	;    the 6502 will stop on the opcode fetch for the next insn, guaranteeing that
	;    the next insn completes before NMI handling starts.
	;
	;    This issue means that the first instruction cannot either be the VCOUNT
	;    check or the first write. It also means it's impossible to guarantee that
	;    the write can occur before the next NMI -- there aren't enough cycles in
	;    max DMA conditions to get off the two writes with needing a dummy insn in
	;    front, so we're stuck either potentially having the writes split by DLIs
	;    or after the DLI.
	;
	; 4) A DLI fires on the next scanline, and runs long across the VBI.
	;
	;    This appears impossible to handle in all cases, as there aren't enough
	;    cycles to guarantee atomic writes before the DLI could fire in max DMA
	;    conditions.
	;
	;    We mitigate this by instead delaying enough cycles to ensure that the
	;    writes always occur _after_ the DLI. Combined with the VBI check, this
	;    handles the case of a single DLI in reasonable conditions. In max DMA
	;    conditions it is still possible for the post-delay code to get pushed
	;    into the next horizontal blank for the DLI to split the writes... we can
	;    only do so much.
	
		;double the index to a vector offset, and save to rotate A -> Y
		asl
		sty		intemp
		tay
		txa

		;disable IRQs
		php
		sei

		;===== sync to horizontal blank =====
		inc		wsync

		;--- NMI may occur here ---

		ldx		#124				;105-106
	 
		;--- NMI may occur here ---

		;Delay enough cycles to ensure that any upcoming NMI executes prior to
		;the vector writes, _if_ no NMI occurred during the WSYNC. If one did, then
		;the horizontal timing is unknown here and we rely on the VCOUNT check
		;to save us.
		php:plp						;107-113
		php:plp						;0-6
	
		;Check if we are in the danger zone -- if we're on 248 then we can't guarantee
		;enough cycles to do the write in time and should delay again. If we're not
		;on 248, then we can just barely guarantee enough cycles to finish both writes
		;in time as long as there isn't a DLI in between that extends all the way to
		;248. The last cycle of the CPX instruction reads VCOUNT and that shows the
		;incremented value starting on cycle 111.
		;
		;The 'normal' timing is what is encountered away from scan 248 with no DLIs.
		;Early/late timings are when a DLI has interfered and represent the edge cases
		;for just barely passing or failing the VCOUNT check. For the early case, we
		;need to squeeze in the writes immediate; in the late case we must clear the
		;VBI before proceeding.
		;
		cpx		vcount				;normal:  7-10 | late: 108-111 | early: 107-110
		beq		in_danger			;normal: 11-12 | late: 112-0   | early: 111-112

resume:
		;Update the vector. In the worst case timing where we have just caught the
		;very last cycle before VCOUNT ticks over to $7C, there are just barely enough
		;cycles to guarantee an atomic update before the VBI. Note that the last write
		;extends past the NMI point; this relies on the 6502 having to the finish the
		;insn once it's started before it can handle the NMI.
		sta		cdtmv1-1,y			;normal: 12-16 | late: 11-15 | early: 113-4
		lda		intemp				;normal: 17-18 | late: 16-17 | early: 5-8
		sta		cdtmv1-2,y			;normal: 19-23 | late: 18-22 | early: 9+

		;restore IRQs
		plp

		;all done
		rts

in_danger:
		;Delay past the NMI point to ensure that any NMI there kicks off before we
		;do the writes.
		php:plp						;1-7
		beq		resume				;8-10
.endp

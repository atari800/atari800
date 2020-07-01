;	Altirra - Atari 800/800XL/5200 emulator
;	Modular Kernel ROM - Printer Handler
;	Copyright (C) 2008-2016 Avery Lee
;
;	Copying and distribution of this file, with or without modification,
;	are permitted in any medium without royalty provided the copyright
;	notice and this notice are preserved.  This file is offered as-is,
;	without any warranty.

;==========================================================================
.proc PrinterInit
	;set printer timeout to default
	mva		#30 ptimot
	rts
.endp

;==========================================================================
; Printer close
;
; Flushes any remaining bytes left in the printer buffer to the printer,
; if any.
;
; Compatibility quirks:
;	- OS-B fills with $20, while the XL/XE OS fills with $9B.
;	- If the buffer was exactly filled without an EOL and is thus empty at
;	  the time of close, no additional EOL is sent. This is arguably a
;	  bug as it can cause line splicing with the 1025.
;
PrinterClose = _PrinterPutByte.close_entry

;==========================================================================
; Printer put byte
;
; Writes a byte into the printer buffer. If the printer buffer is full
; or an EOL was added, the printer buffer is sent to the printer.
;
; Compatibility quirks:
;	- Must be usable without prior Open, or Monkey Wrench II breaks.
;	- On an EOL, the remainder of the buffer is padded with $20 before
;	  being sent to the printer, on both OS-B and the XL/XE OS.
;	- ICAX2Z can be changed on the fly to switch between normal and
;	  sideways orientation, as it is checked on each put byte.
;	- If the buffer contains more than 29 characters and the mode is
;	  switched dynamically from Sideways to Normal mode, OS-B and XLOSv2
;	  will miss the stop and overrun the printer buffer. On XLOSv2,
;	  this generally results in a GINTLK hang. We handle this somewhat
;	  more gracefully but still treat it as a Don't Do That(tm).
;	- ICAX2Z=0 must be interpreted as Normal, because BASIC uses it
;	  with LPRINT, and the 1025 will NAK any other orientation.
;
PrinterPutByte = _PrinterPutByte.put_entry

.proc _PrinterPutByte
put_entry:
	;Check for sideways mode and compute line size - we must do this on every
	;write as Monkey Wrench II depends on being able to call P: put byte
	;without an open (still broken program behavior, just less crashy now).
	ldx		#40
	ldy		icax2z
	cpy		#$53
	sne:ldx	#29
	stx		pbufsz

	;increment and load buffer index
	inc		pbpnt
	ldx		pbpnt

	;put char
	sta		prnbuf-1,x
	
	;if we wrote an EOL, force flush now
	cmp		#$9b
	beq		fill_spaces
			
	;check for end of line and flush if so
	cpx		pbufsz
	bcs		fill_done

	;exit success
xit:
	ldy		#1
	rts
	
close_entry:
	;check if we have anything in the buffer
	ldx		pbpnt
	
	;exit if buffer is empty
	beq		xit
	
	;If we're building for 800 target, emulate OS-B and fill with $20.
	;If we're building for XL/XE target, emulate XLOSv2 and fill with $9B.
.if _KERNEL_XLXE
	lda		#$9b
	dta		{bit $0100}
.endif

	;fill remainder of buffer with spaces
fill_spaces:
	lda		#$20
fill_loop:
	sta		prnbuf,x
	inx
	cpx		pbufsz
	bcc		fill_loop
fill_done:
	
	;send line to printer
	ldy		#10
	mva:rne	iocbdat-1,y ddevic-1,y-

	;empty buffer
	sty		pbpnt

	;set line length
	stx		dbytlo

	;Compute AUX1 byte from length.
	;
	;Note that the OS manual is wrong -- this byte needs to go into AUX1 and
	;not AUX2 as the manual says.
	;
	;	normal   (40): 00101000 -> 01001110 ($4E 'N')
	;	sideways (29): 00011101 -> 01010011 ($53 'S')
	;	               010_1I1_
	txa
	and		#%00011101
	eor		#%01001110
	sta		daux1			;set AUX1 to indicate width to device
	
	;send to printer and exit
do_io:
	mva		ptimot dtimlo
	jmp		siov

iocbdat:
	dta		$40			;device
	dta		$01			;unit
	dta		$57			;command 'W'
	dta		$80			;input/output mode (write)
	dta		a(prnbuf)	;buffer address
	dta		a(0)		;timeout
	dta		a(0)		;buffer length
.endp

;==============================================================================
;==========================================================================
.proc PrinterOpen
	lda		#0
	sta		pbpnt

	;fall through to status to update printer timeout

.def :PrinterGetStatus
	;setup parameter block
	ldx		#9
	mva:rpl	iocbdat,x ddevic,x-

	;issue status call
	jsr		_PrinterPutByte.do_io
	bmi		error
	
	;update timeout
	mva		dvstat+2 ptimot
	
error:
	rts

iocbdat:
	dta		$40			;device
	dta		$01			;unit
	dta		$53			;command 'S'
	dta		$40			;input/output mode (read)
	dta		a(dvstat)	;buffer address
	dta		a(0)		;timeout
	dta		a(4)		;buffer length
.endp

;==============================================================================
PrinterGetByte = CIOExitNotSupported
PrinterSpecial = CIOExitNotSupported

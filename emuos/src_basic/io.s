; Altirra BASIC - I/O module
; Copyright (C) 2014 Avery Lee, All Rights Reserved.
;
; Copying and distribution of this file, with or without modification,
; are permitted in any medium without royalty provided the copyright
; notice and this notice are preserved.  This file is offered as-is,
; without any warranty.

;==========================================================================
; Print a message from the message database.
;
; Entry:
;	X = LSB of message pointer
;
.proc IoPrintMessageIOCB0
		lda		#0
		sta		iocbidx
.def :IoPrintMessage
		stx		inbuff
loop:
		ldx		inbuff
		lda		msg_base,x
		beq		printStringINBUFF.xit
		jsr		IoPutCharAndInc
		bpl		loop			;!! - unconditional
.endp

;==========================================================================
IoPrintInt:
		jsr		ifp
IoPrintNumber:
		jsr		fasc
.proc printStringINBUFF
loop:
		ldy		#0
		lda		(inbuff),y
		pha
		and		#$7f
		jsr		IoPutCharAndInc
		pla
		bpl		loop
xit:
		rts
.endp

;==========================================================================
.proc IoConvNumToHex
		php
		jsr		ldbufa
		jsr		fpi
		ldy		#0
		plp
		lda		fr0+1
		bcc		force_16bit
		beq		print_8bit
force_16bit:
		jsr		print_digit_pair
print_8bit:
		lda		fr0
print_digit_pair:
		pha
		lsr
		lsr
		lsr
		lsr
		jsr		print_digit
		pla
		and		#$0f
print_digit:
		cmp		#10
		scc:adc	#6
		adc		#$30
		sta		(inbuff),y
		iny
		rts
.endp

;==========================================================================
.proc IoPutCharAndInc
		inc		inbuff
		sne:inc	inbuff+1
		bne		putchar			;! - unconditional
.endp

;==========================================================================
IoPutCharDirect = putchar.direct

IoPutNewline:
		lda		#$9b
		dta		{bit $0100}
IoPutSpace:
		lda		#' '
.proc putchar
		dec		ioPrintCol
		bne		not_tabstop
		mvx		ptabw ioPrintCol
not_tabstop:
direct:
		ldx		iocbidx
		jsr		dispatch
		tya

.def :ioCheck = *				;requires iocbidx
		bpl		done

.def :IoCloseIOCB7AndThrowError
		sty		errno

		;Check if we were using IOCB#7 and close it if so. It's intentional
		;that we do this for any I/O error on #7 even if it's from explicit
		;program usage, for compatibility.
		lda		iocbidx
		eor		#$70
		bne		not_iocb7
		sta		iocbexec		;clear ENTER
		jsr		IoClose
not_iocb7:
		jmp		errorDispatch
		
dispatch:
		sta		ciochr
		lda		icax1,x
		sta		icax1z
		lda		icpth,x
		pha
		lda		icptl,x
		pha
		lda		ciochr
done:
		rts
.endp

;==========================================================================
.proc IoReadLineX
		jsr		IoSetupReadLineLDBUFA_SetIOCBX
		jsr		ciov
		bpl		putchar.done
		cpy		#$88
		bne		IoCloseIOCB7AndThrowError
		rts
.endp

;==========================================================================
; Issue I/O call with a filename.
;
; Entry:
;	A = command to run
;	fr0 = Pointer to string info (ptr/len)
;	iocbidx = IOCB to use
;
; ICBAL/ICBAH is automatically filled in by this fn. Because BASIC strings
; are not terminated, this routine temporarily overwrites the end of the
; string with an EOL, issues the CIO call, and then restores that byte.
; The string is limited to 255 characters.
;
; I/O errors are checked after calling CIO and the error handler is issued
; if one occurs.
;
IoDoOpenReadWithFilename:
		lda		#4
IoDoOpenWithFilename:
		ldx		iocbidx
		sta		icax1,x
		lda		#CIOCmdOpen
.proc IoDoWithFilename
		;stash command
		ldx		iocbidx
		pha
						
		;call CIO
		jsr		IoTerminateString
		jsr		IoSetupBufferAddress
		pla
		jsr		IoTryCmdX
		jsr		IoUnterminateString
		
		;now we can check for errors and exit
		jmp		ioCheck
.endp

;==========================================================================
IoSetupIOCB7AndEval:
		jsr		IoSetupIOCB7
		jmp		evaluate

;==========================================================================
IoSetupIOCB7:
		ldx		#$70
		stx		iocbidx
IoCloseX = IoClose.with_IOCB_X
.proc IoClose
		ldx		iocbidx
with_IOCB_X:
		lda		#CIOCmdClose
.def :IoTryCmdX = *
		sta		iccmd,x
		jmp		ciov
.endp

;==========================================================================
; Open the cassette (C:) device or any other stock device.
;
; Entry (IoOpenCassette):
;	None
;
; Entry (IoOpenStockDeviceIOCB7):
;	A = AUX1 mode
;	Y = Low byte of device name address in constant page
;
; Entry (IoOpenStockDeviceX):
;	A = AUX1 mode
;	X = IOCB #
;	Y = Low byte of device name address in constant page
;
IoOpenCassette:
		sec
		ror		icax2+$70		
		ldy		#<devname_c
IoOpenStockDeviceIOCB7:
		ldx		#$70
IoOpenStockDeviceX:
		stx		iocbidx
		sty		stScratch4
		pha
		jsr		IoCloseX
		pla
		sta		icax1,x
		lda		stScratch4
		ldy		#>devname_c
		jsr		IoSetupBufferAddress
		lda		#CIOCmdOpen
IoDoCmd:
		ldx		iocbidx
		sta		iccmd,x
ioChecked:						;iocbidx clean
		jsr		ciov
		jmp		ioCheck

;==========================================================================
; Replace the byte after a string with an EOL terminator.
;
; Entry:
;	FR0 = string pointer
;	FR0+2 = string length (16-bit)

; Registers:
;	A, Y modified; X preserved
;
; Exit:
;	INBUFF = string pointer
;
; This is needed anywhere where a substring needs to be passed to a module
; that expects a terminated string, such as the math pack or CIO. This
; will temporarily munge the byte _after_ the string, which can be a
; following program token, the first byte of another string or array, or
; even the runtime stack. Therefore, the following byte MUST be restored
; ASAP.
;
; The length of the string is limited to 255 characters.
;
.proc IoTerminateString
		;compute termination offset		
		ldy		fr0+2
		lda		fr0+3
		seq:ldy	#$ff
		sty		ioTermOff
		
		;save existing byte
		lda		(fr0),y
		sta		ioTermSave
		
		inc		ioTermFlag		;!! - must be first in case reset happens in between

		;stomp it with an EOL
		lda		#$9b
		sta		(fr0),y

		;copy term address
.def :IoSetInbuffFR0 = *
		lda		fr0
		ldy		fr0+1
.def :IoSetInbuffYA = *
		sta		inbuff
		sty		inbuff+1
		rts
.endp

;==========================================================================
; Entry:
;	INBUFF = string pointer
;
; Registers:
;	Y, P.C preserved
;	P.NZ set by Y
;
.proc IoUnterminateString
		tya
		pha
		ldy		ioTermOff
		lda		ioTermSave
		sta		(inbuff),y
		dec		ioTermFlag
		pla
		tay
		rts
.endp

;==========================================================================
IoSetupReadLineLDBUFA_SetIOCBX:
		stx		iocbidx
IoSetupReadLineLDBUFA_X:
		jsr		ldbufa
.proc IoSetupReadLine
		;we are using some pretty bad hacks here:
		;- GET RECORD and >LBUFF are $05
		;- <LBUFF is $80
		ldy		#$05
		lda		#$80
		jsr		IoSetupBufferAddress
		sta		iccmd,x
		ldy		#$ff
.def :IoSetupBufferLengthY
		lda		#$00
.def :IoSetupBufferLengthAY
		sta		icblh,x
		tya
		sta		icbll,x
		rts
.endp

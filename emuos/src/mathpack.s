;	Altirra - Atari 800/800XL/5200 emulator
;	Modular Kernel ROM - Decimal Floating-Point Math Pack
;	Copyright (C) 2008-2016 Avery Lee
;
;	Copying and distribution of this file, with or without modification,
;	are permitted in any medium without royalty provided the copyright
;	notice and this notice are preserved.  This file is offered as-is,
;	without any warranty.

;==========================================================================
; Math pack design and references
;
; The current version of the AltirraOS math pack is designed with the
; following goals:
;
; - Match the behavior, results, and memory usage of the standard math pack
;   as much as possible within reasonable constraints. This includes the
;   truncation behavior of the original math pack.
;
; - When there is a difference between the OS-B and XL math packs, prefer
;   to match the XL math pack.
;
; - Try to support undocumented code and data usage of popular programs,
;   including: Atari BASIC, Basic XL/XE, Turbo Basic XL, and MAC/65.
;
; Turbo Basic XL is the most troublesome to support. Atari BASIC mainly
; just uses the ATN() and a couple of additional constants, but TBXL
; reimplements LOG/LOG10/EXP/EXP10 internally while borrowing the
; associated constants and polynomials. This means that we must use a
; compatible algorithm with the same data for TBXL's LOG(), CLOG(), EXP(),
; ^, and SQR() functions to work.
;
; To support the above, some formulas and constants have been pulled from
; the same upstream reference used by the original math pack programmers:
;
; Ruckdeschel, F.R. (1981). BASIC Scientific Subroutines, Vol. I, BYTE/McGraw-Hill
;
; In particular, the minimax polynomials for the transcendental function
; approximations come from this book. Note that in a couple of cases the
; original math pack has mistakes where the polynomials are not evaluated
; over their proper interval; where noticed, this has been replicated here.
;
;==========================================================================
; Math pack memory usage constraints
;                                                                            INBUFF          DEGFLG
;                                                                  NSIGN  DIGRT          ZTEMP3
;                                                                EEXP  FCHRFLG       ZTEMP4        FPTR2       FPSCR
;                                                              FRX   ESIGN CIX   ZTEMP1        FLPTR     PLYARG      FPSCR1
;              $D4     $D8     $DC     $E0     $E4     $E8     $EC     $F0     $F4     $F8     $FC     | $05E0 $05E6 $05EC
;              . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . | ..................
;              [-- FR0 --] [-- FRE --] [-- FR1 --] [-- FR2 --]               [-] [-] [-] [-]   [-] [-] |
; FADD/FSUB    M M M M M M M . . . !!! M M M M M M . . . . . . . . . ! ! . . . . . . M M M . ! . . . . |
; FMUL         M M M M M M M M M M M M M r r r r r M M M M M M M M M ! ! . . . . M M M . . . ! . . . . |
; FDIV         M M M M M M M M M M M M M r r r r r M M M M M M M M M ! ! . . . . M . M . . . ! . . . . |
; PLYEVL       M M M M M M M M M M M M M M M M M M M M M M M M M M M M ! . . . . M M M M M . ! M M M M | MMMMMM............
; EXP/EXP10    M M M M M M M M M M M M M M M M M M M M M M M M M M M M M M . . . M M M M M . ! M M M M | MMMMMM............
; LOG/LOG10    M M M M M M M M M M M M M M M M M M M M M M M M M M M M M M . . . M M M M M . ! M M M M | MMMMMMMMMMMM......
; REDRNG       M M M M M M M M M M M M M M M M M M M M M M M M M M M . . . . . . M . M . . . ! M M M M | MMMMMMMMMMMM......
;
; AFP          M M M M M M M . . . . . . . . . . . . . . . . . M M M M M M M . . . . . . . . ! . . . . |
; FASC         r r r r r r . . . . . . . . . . . . . . . . . . . M . . . . M M M . . M . . . ! . . . . |
; IFP          M M M M M M . . . . !!! . . . . . . . . . . . . . . . . . . . . . . . . M M . ! . . . . |
; FPI          M M M M M M . . . . . . !!!!!!!!!!! . . . . . . M . . . . . . . . M . M M M M ! . . . . |
;
; M = modified by original math pack
; r = read by original math pack
; ! = program known to rely on not being modified
;
; Notes:
; [1] BASIC XE relies on $DE/DF not being touched by FADD, or FOR/NEXT
;     breaks.
; [2] MAC/65 relies on $DE/DF not being touched by IFP.
; [3] DARG relies on FPI not touching FR1.
; [4] ACTris 1.2 relies on FASC not touching lower parts of FR2.
; [5] Atari BASIC SQR() uses ESIGN ($EF).
; [6] Atari BASIC SIN() uses FCHRFLG ($F0).
;

;==========================================================================
_fpcocnt = $00ec			;FP: temporary storage - polynomial coefficient counter
_fptemp0 = $00f8			;FP: temporary storage - transcendental temporary (officially ZTEMP4+1)
_fptemp1 = $00f9			;FP: temporary storage - transcendental temporary (officially ZTEMP3)

;==========================================================================
.macro	ckaddr
.if * <> %%1
		.error	'Incorrect address: ',*,' != ',%%1
.endif
.endm

.macro	fixadr
.if * < %%1
		.print (%%1-*),' bytes free before ',%%1
		org		%%1
.elif * > %%1
		.error 'Out of space: ',*,' > ',%%1,' (',*-%%1,' bytes over)'
		.endif
.endm

;==========================================================================
; AFP [D800]	Convert ATASCII string at INBUFF[CIX] to FR0
;
; Parses a floating point number in ATASCII starting at INBUFF[CIX] to
; FR0.
;
; Returns:
;	C = 0 and number in FR0 if successful, C = 1 if failure
;	CIX = next position after parsed number, if any
;
; A valid floating point number is of the following form:
;
;	- Zero or more spaces
;	- A single optional + or - sign
;	- A period followed by one or more digits, or one or more digits
;	  followed by a period and then zero or more digits
;	- An optional exponent consisting of a E, an optional +/- sign,
;	  and one or two digits giving a non-zero number.
;
; Additional behaviors:
;	- No spaces after leading spaces are allowed.
;	- A single period with no digits around it is not valid.
;	- Numbers of magnitude 1E+98 or larger return an error.
;	- The smallest magnitude possible is 1E-98; 1E-99 underflows to 0.
;	- Only two exponent digits at most are parsed. For instance, 1E012
;	  is parsed as 1E01 with CIX pointing to the 2.
;	- Exponents of zero are not allowed. 1E+001 is parsed as just 1 with
;	  CIX pointing to the E.
;	- Only up to 9 significant digits are parsed even though 10 can sometimes
;	  fit. However, leading and trailing zeroes don't count for this.
;	  000000123456789100 is parsed as 123456789000.
;	- Denormalized scientific notation is allowed, e.g. 0.1E+04 and 100E6.
;	  AFP can accept either scientific or non-scientific notation for any
;	  valid number, and does not require usage according to FASC's rules.
;
; Bugs:
;	- The original math pack returns an invalid signed zero when parsing
;	  -0, which is not handled by other functions in the math pack. In
;	  particular, FASC will produce garbage on a signed zero. This is a bug
;	  since the specification in the Atari OS Manual specifically says that
;	  signed zero is never generated and the exponent byte can be tested for
;	  zero to detect zero. Callers have to detect and restore a standard zero
;	  in this case.
;
	org		$d800
_afp = afp
.proc afp
dotflag = fr2
xinvert = fr2+1
cix0 = fr2+2
sign = fr2+3
digit2 = fr2+4

	;skip initial spaces
	jsr		skpspc

	;init FR0 and one extra mantissa byte
	lda		#$7f
	sta		fr0
	sta		digit2
	
	ldx		#fr0+1
	jsr		zf1

	;clear decimal flag
	sta		dotflag
	sta		sign
	
	;check for sign
	ldy		cix
	lda		(inbuff),y
	cmp		#'+'
	beq		isplus
	cmp		#'-'
	bne		postsign
	ror		sign
isplus:
	iny
postsign:	
	sty		cix0

	;skip leading zeroes
	lda		#'0'
	jsr		fp_skipchar
	
	;check if next char is a dot, indicating mantissa <1
	lda		(inbuff),y
	cmp		#'.'
	bne		not_tiny
	iny
	
	;set dot flag
	ror		dotflag

	;increment anchor so we don't count the dot as a digit for purposes
	;of seeing if we got any digits
	inc		cix0
	
	;skip zeroes and adjust exponent
	lda		#'0'
tiny_denorm_loop:
	cmp		(inbuff),y
	bne		tiny_denorm_loop_exit
	dec		fr0
	iny
	bne		tiny_denorm_loop
tiny_denorm_loop_exit:
	
not_tiny:

	;grab digits left of decimal point
	ldx		#1
nextdigit:
	lda		(inbuff),y
	cmp		#'E'
	beq		isexp
	iny
	cmp		#'.'
	beq		isdot
	eor		#'0'
	cmp		#10
	bcs		termcheck
	
	;write digit if we haven't exceeded digit count
	cpx		#6
	bcs		afterwrite
	
	bit		digit2
	bpl		writehi

	;clear second digit flag
	dec		digit2
	
	;merge in low digit
	ora		fr0,x
	sta		fr0,x
	
	;advance to next byte
	inx
	bne		afterwrite
	
writehi:
	;set second digit flag
	inc		digit2
	
	;shift digit to high nibble and write
	asl
	asl
	asl
	asl
	sta		fr0,x

afterwrite:
	;adjust digit exponent if we haven't seen a dot yet
	bit		dotflag
	smi:inc	fr0
	
	;go back for more
	jmp		nextdigit
	
isdot:
	lda		dotflag
	bne		termcheck
	
	;set the dot flag and loop back for more
	ror		dotflag
	bne		nextdigit

termcheck:
	dey
	cpy		cix0
	beq		err_carryset
term:
	;stash offset
	sty		cix

term_rollback_exp:
	;divide digit exponent by two and merge in sign
	rol		sign
	ror		fr0
	
	;check if we need a one digit shift
	bcs		nodigitshift

	;shift right one digit
	ldx		#4
digitshift:
	lsr		fr0+1
	ror		fr0+2
	ror		fr0+3
	ror		fr0+4
	ror		fr0+5
	dex
	bne		digitshift

nodigitshift:
	jmp		fp_normalize

err_carryset:
	rts

isexp:
	cpy		cix0
	beq		err_carryset
	
	;save off this point as a fallback in case we don't actually have
	;exponential notation
	sty		cix

	;check for sign
	ldx		#0
	iny
	lda		(inbuff),y
	cmp		#'+'
	beq		isexpplus
	cmp		#'-'
	bne		postexpsign
	dex						;x=$ff
isexpplus:
	iny
postexpsign:
	stx		xinvert

	;pull up to two exponent digits -- check first digit
	jsr		fp_isdigit_y
	iny
	bcs		term_rollback_exp
	
	;stash first digit
	tax
	
	;check for another digit
	jsr		fp_isdigit_y
	bcs		notexpzero2
	iny

	adc		fp_mul10,x
	tax
notexpzero2:
	txa
	
	;zero is not a valid exponent
	beq		term_rollback_exp
	
	;apply sign to exponent
	eor		xinvert
	rol		xinvert

	;bias digit exponent
	adc		fr0
	sta		fr0
expterm:
	jmp		term

.endp

;==========================================================================
.proc fp_tab_lo_100				;$0A bytes
		dta		$00, $64, $C8, $2C, $90, $F4, $58, $BC, $20, $84
.endp

;==========================================================================
; BCD to binary conversion helper.
;
; Entry:
;	A = BCD value (fp_dectobin)
;
; Exit:
;	A = unmodified or loaded BCD byte
;	Y = MSB nibble (digit)
;
; Preserved:
;	X
;
fp_ldfr0p1_dectobin:		;$0B bytes
		lda		fr0+1,x
fp_dectobin:
		pha
		lsr
		lsr
		lsr
		lsr
		tay
		pla
		clc
		rts

;==========================================================================
; FASC [D8E6]		Floating Point to ASCII conversion
;
; Converts an FP number in FR0 to an ASCII representation in LBUFF, setting
; INBUFF to the start of the string and with bit 7 set on the last
; character.
;
; Negative values are preceded by '-', followed by at least one decimal
; digit -- so values smaller than 1.0 in magnitude start with "0.".
; Exponential notation is used for values <0.01 or >=10^10, with the
; form m.mmmmmmmmE+nn; trailing zeroes on the mantissa are trimmed, along
; with the . if there are no non-zero digits after it.
;
; Quirks:
;	- INBUFF does not always point to LBUFF. It can be up to two bytes prior
;	  to LBUFF and one byte after LBUFF:
;		* +1 if the leading digit of the mantissa is trimmed off
;		* -1 if 0. is prefixed for magnitude in [0.01, 1), or odd exponents
;		  in scientific notation
;		* -1 if - prefixed for negative number
;
;	- One decimal digit is always printed for odd exponents, but not for
;	  even exponents, e.g. 1E+10 and 1.0E+11.
;
; Compatibility issues:
;	- Darg ignores INBUFF and requires integers to be validly parsable starting
;	  at LBUFF. This means that LBUFF[0] must contain a zero if INBUFF is
;	  beyond it.
;
		fixadr	$d8e6

.proc	fasc
dotcntr	= ztemp4
expval	= ztemp4+1
trimbase = ztemp4+2

	jsr		ldbufa

	;init starting output index
	ldy		#0

	;start with no exponent to display
	sty		expval
	sty		trimbase

	;write a zero to the start of the buffer
	lda		#$30
	sta		(inbuff),y

	;read exponent and check if number is zero
	lda		fr0
	beq		is_zero
	
	;set up for 5 mantissa bytes
	ldx		#-5

	;compute digit offset to place dot
	;  0.001 (10.0E-04) = 3E 10 00 00 00 00 -> -1
	;   0.01 ( 1.0E-02) = 3F 01 00 00 00 00 -> 1
	;    0.1 (10.0E-02) = 3F 10 00 00 00 00 -> 1
	;    1.0 ( 1.0E+00) = 40 01 00 00 00 00 -> 3
	;   10.0 (10.0E+00) = 40 10 00 00 00 00 -> 3
	;  100.0 ( 1.0E+02) = 40 01 00 00 00 00 -> 5
	; 1000.0 (10.0E+02) = 40 10 00 00 00 00 -> 5

	asl
	sec
	sbc		#125

	;check if we should go to exponential form (exp >= 10 or <=-3)
	cmp		#12
	bcc		noexp

	;yes - compute and stash explicit exponent
	sbc		#2				;!! - carry set from BCC fail
	sta		expval			;$0A <= expval < $FE

	;reset dot counter
	lda		#2

	;exclude first two digits from zero trim
	sta		trimbase

noexp:		
	sta		dotcntr			;$02 <= dotcntr < $0C

	;check if number is less than 1.0
	cmp		#2
	bcs		not_tiny
	
	;At this point A = 1 and expval = 0, so the value is
	;at least 0.01 and less than 1.0 and we are guaranteed to
	;need 0. inserted in front.

	;shift buffer ahead one
	dec		inbuff

	dex
	dec		trimbase
	lsr						;A=0
	beq		writelowz		;!! - unconditional

	;write out mantissa digits
digitloop:
	dec		dotcntr
	bne		no_hidot
	lda		#'.'
	sta		(inbuff),y
	iny
no_hidot:

	;write out high digit
	lda		fr0+6,x
	lsr
	lsr
	lsr
	lsr
	ora		#$30
	sta		(inbuff),y
	iny
	
writelow:
	;write out low digit
	dec		dotcntr
	bne		no_lodot
	lda		#'.'
	sta		(inbuff),y
	iny
no_lodot:
	
	lda		fr0+6,x
	and		#$0f
writelowz:
	ora		#$30
	sta		(inbuff),y
	iny

	;next digit
	inx
	bne		digitloop

	;skip trim if dot hasn't been written
	lda		dotcntr
	bpl		skip_zero_trim	
	
	;trim off trailing zeroes
	lda		#'0'
lzloop:
	cpy		trimbase
	beq		stop_zero_trim
	dey
	cmp		(inbuff),y
	beq		lzloop

	;trim off dot
stop_zero_trim:
	lda		(inbuff),y
	cmp		#'.'
	bne		no_trailing_dot

skip_zero_trim:
	dey
is_zero:
	lda		(inbuff),y
no_trailing_dot:

	;check if we have an exponent to deal with
	ldx		expval
	beq		noexp2
	
	;print an 'E'
	lda		#'E'
	iny
	sta		(inbuff),y
	
	;check for a negative exponent
	txa
	bpl		exppos
	eor		#$ff
	tax
	inx
	lda		#'-'
	dta		{bit $0100}
exppos:
	lda		#'+'
expneg:
	iny
	sta		(inbuff),y
	
	;print tens digit, if any
	txa
	sec
	ldx		#$2f
tensloop:
	inx
	sbc		#10
	bcs		tensloop
	pha
	txa
	iny
	sta		(inbuff),y
	pla
	adc		#$3a
	iny
noexp2:
	;set high bit on last char
	ora		#$80
	sta		(inbuff),y

	;prepend minus sign for negative number (must decrement INBUFF)
	lda		fr0
	bmi		negative
	rts

not_tiny:
	;check if number begins with a leading zero
	lda		fr0+6,x
	cmp		#$10
	bcs		digitloop

	dec		trimbase

	;yes - skip the high digit
	inc		inbuff
	lsr		expval
	asl		expval
	bne		writelow
	dec		dotcntr
	bcc		writelow		;!! - unconditional

negative:
	lda		#'-'
	tax
	dec		inbuff
	sta		(inbuff-'-',x)
	rts
.endp

;==========================================================================
; IFP [D9AA]	Convert 16-bit integer at FR0 to FP
;
; Converts the 16-bit unsigned binary number with LSB in FR0 and MSB in
; FR0+1 to a floating point number in FR0. This always succeeds since all
; 65536 values are representable.
;
; !NOTE! Cannot use FR2/FR3 -- MAC/65 requires that $DE-DF be preserved.
;
	fixadr	$d9aa
.proc ifp
		;clear remainder of mantissa
		lda		#0
		sta		fr0+2
		sta		fr0+3
		sta		fr0+4
		sta		fr0+5
	
		;prepare to shift up to 16 bits
		ldy		#16

		;swap around bytes so that MSB is in FR0 -- this allows FR0+1 to
		;take carries out of the BCD shift for the 5th digit
		ldx		fr0
		lda		fr0+1
		bne		big
		ldy		#8
		txa
		ldx		#0
big:
		stx		fr0+1

		;shift until we get the first set bit
find_loop:
		asl		fr0+1
		rol
		bcs		found_leading_bit
		dey
		bne		find_loop
		rts

found_leading_bit:
		sed
		sta		fr0
		jmp		ifp_cont
.endp

;==========================================================================
; FPI [D9D2]	Convert FR0 to 16-bit integer at FR0 with rounding
;
; This cannot overwrite FR1. Darg relies on being able to stash a value
; there across a call to FPI in its startup.
;
; Errors:
;	- Error if input negative or >= 65536.
;
; Quirks:
;	- Negative numbers in [-0.5, 0) cause an error instead of being
;	  rounded up to 0.
;	- Numbers in [65535, 65536) are rounded up to 65536 and then
;	  truncated to 0 without error.
;
	fixadr	$d9d2
.nowarn .proc fpi
_acc0 = fr2
_acc1 = fr2+1
	
	;error out if it's guaranteed to be too big or negative (>999999)
	lda		fr0
	cmp		#$43
	bcs		err

	;zero number if it's guaranteed to be too small (<0.01)
	sbc		#$3f-1			;!!- carry is clear
	bcc		zfr0

	tax
	
	;clear temp accum and set up rounding
	lda		#0
	ldy		fr0+1,x
	cpy		#$50
	rol						;!! - clears carry too
	sta		fr0
	lda		#0

	;check for [0.01, 1)
	dex
	bmi		done

	;convert ones/tens digit pair to binary (one result byte: 0-100)
	jsr		fp_ldfr0p1_dectobin
	adc		fr0
	sbc		fp_dectobin_tab,y
	clc
	sta		fr0
	lda		#0

	;check if we're done
	dex
	bmi		done

	;convert hundreds/thousands digit pair to binary (two result bytes: 0-10000)
	jsr		fp_ldfr0p1_dectobin
	lda		fr0
	adc		fp_tab_lo_1000,y
	sta		fr0
	lda		fp_tab_hi_1000,y
	adc		#0
	pha
	lda		fr0+1,x
	and		#$0f
	tay
	lda		fr0
	adc		fp_tab_lo_100,y
	sta		fr0
	pla
	adc		fp_tab_hi_100,y

	;check if we're done
	dex
	bmi		done

	;convert ten thousands digit pair to binary (two result bytes: 0-100000, overflow possible)
	;LSB = digit * 16
	;MSB = digit * 39.0625
	tax
	ldy		fr0+1
	cpy		#$07
	bcs		err
	tya
	asl
	asl
	asl
	asl
	adc		fr0
	sta		fr0
	txa
	adc		fp_tab_hi_10000-1,y

	;Bug compat -- if we had an overflow error, clear it if the value was
	;exactly rounded up to 0. We can only enter this path if the value was
	;between [10000, 70000), so if the high byte is 0, the only possible
	;way is a source value of [65535.5, 65791.5).
	bne		done
	ldx		fr0
	bne		done
	ldx		#$4f
	cpx		fr0+4

done:
	;move result back to FR0, with rounding
	sta		fr0+1
err:
	rts
.endp

;==========================================================================
		fixadr	$da43
fp_clc_zfr0:
		clc

;==========================================================================
; ZFR0 [DA44]	Zero FR0
; ZF1 [DA46]	Zero float at (X)
; ZFL [DA48]	Zero float at (X) with length Y (UNDOCUMENTED)
;
	fixadr	$da44
zfr0:
	ldx		#fr0
	ckaddr	$da46
zf1:
	ldy		#6
	ckaddr	$da48
zfl:
	lda		#0
zflloop:
	sta		0,x
	inx
	dey
	bne		zflloop
	rts

;==========================================================================
; LDBUFA [DA51]	Set LBUFF to #INBUFF (UNDOCUMENTED)
;
		fixadr	$da51
ldbufa:
	mwa		#lbuff inbuff
	rts

;==========================================================================
; FPILL_SHL16 [DA5A] Shift left 16-bit word at $F7:F8 (UNDOCUMENTED)
;
; Illegal entry point used by MAC/65 when doing hex conversion.
;
; Yes, even the byte ordering is wrong.
;
		fixadr	$da5a
	
.nowarn .proc fpill_shl16
		asl		$f8
		rol		$f7
		rts
.endp

;** 1 byte free**

;==========================================================================
; FSUB [DA60]	Subtract FR1 from FR0; FR1 is altered
; FADD [DA66]	Add FR1 to FR0; FR1 is altered
;
; Add or subtract two numbers. When denormalizing the smaller number, any
; precision shifted out is dropped by truncation instead of rounding.
; For instance, 1.23456788 + 0.000000019 = 1.23456789 and not 1.23456790.
;
; Quirks:
;	- FADD must be able to handle negative zeroes for either FR0 or FR1.
;	  FR1 falls out of the address of FSUB, since the most obvious
;	  implementation is to negate the FR1 sign. FR0 is possible via
;	  VAL("-0") in BASIC.
;
		fixadr	$da60
.proc fsub
		;toggle sign on FR1
		lda		fr1
		eor		#$80
		sta		fr1
	
		;fall through to FADD
		ckaddr	$da66
.def :fadd
		;if fr1 is zero or negative zero, we're done. Call the normalization
		;routine in case FR0 is negative zero.
		lda		fr1+1
		beq		norm_fr0
	
		;if fr0 is zero, swap; we know fr1 can't be a signed zero
		lda		fr0+1
		beq		return_fr1

fr0_nonzero:
		;compute difference in exponents, ignoring sign
		lda		fr1			;load fr1 sign
		eor		fr0			;compute fr0 ^ fr1 signs
		and		#$80		;mask to just sign
		tax					;store add/subtract flag
		eor		fr1			;flip fr1 sign to match fr0
		clc
		sbc		fr0			;compute difference in exponents
		bcc		noswap		;jump if FR1 exponent <= FR0 exponent
	
swap:
		jmp		fp_reverse_addsub

norm_fr0:
		jmp		fp_normalize

return_fr1:
		jmp		fp_rmove

noswap:	
		;A = FR1 - FR0 - 1
		;X = add/sub flag

		;compute positions for add/subtract	
		adc		#5			;A = (FR1) - (FR0) + 4   !! carry is clear coming in
		tay
	
		;check if FR1 is too small in magnitude to matter
		bmi		sum_xit
	
		;jump to decimal mode and prepare for add/sub loops
		sed

		;check if we are doing a sum or a difference
		cpx		#$80
		ldx		#5
		bcs		do_subtract
			
		;add mantissas
add_loop:
		lda		fr1+1,y
		adc		fr0,x
		sta		fr0,x
		dex
		dey
		bpl		add_loop
		bmi		sum_carryloop_start

sum_carryloop:
		lda		fr0+1,x
		adc		#0
		sta		fr0+1,x
sum_carryloop_start:
		bcc		sum_xit
		dex
		bpl		sum_carryloop

sum_finalize:
		;adjust exponent
		inc		fr0

		;shift down FR0
		ldx		#4
sum_shiftloop:
		lda		fr0,x
		sta		fr0+1,x
		dex
		bne		sum_shiftloop
	
		;add a $01 at the top
		inx
		stx		fr0+1

		;turn off decimal mode

		;signal overflow if we hit 1E+98 or higher in magnitude
		lda		fr0
		asl
		cmp		#(64+49)*2
sum_xit:
		cld
		rts
	
do_subtract:
		;subtract FR0 and FR1 mantissas (!! carry is set coming in)
sub_loop:
		lda		fr0,x
		sbc		fr1+1,y
		sta		fr0,x
		dex
		dey
		bpl		sub_loop
		jmp		fp_fsub_cont.entry
.endp

;==========================================================================
fp_mul10:
	dta		0,10,20,30,40,50,60,70,80,90

;==========================================================================
; FMUL [DADB]:	Multiply FR0 * FR1 -> FR0; FR1 modified
;
; Multiply two floating point numbers.
;
; FMUL generates and accumulates full partial products, so intermediate
; results produce the full 19 significant digits, but the result is then
; truncated without rounding back to 10 significant digits.
;
	fixadr	$dad8
fp_fld1r_fmul:
	jsr		fld1r
	ckaddr	$dadb
.proc fmul

		;We use FR0:FRE as a double-precision accumulator, and copy the
		;original multiplicand value in FR0 to FR1. The multiplier in
		;FR1 is converted to binary digit pairs into FR2.
	
_offset = _fr3+5
_offset2 = fr2

		;if FR0 is zero, we're done
		clc
		lda		fr0
		beq		fp_exit
	
		;if FR1 is zero, zero FR0 and exit
		lda		fr1
		beq		fp_exit_zero
		
		;compute new exponent and stash (note: C=0 from above)
		lda		fr1
		jsr		fp_adjust_exponent.fmul_entry
	
		inc		fr0

		;move fr0 to fr2, converting each digit pair to binary and inverting
		jsr		fp_fmul_fr0_to_binfr2

		;clear accumulator through to exponent byte of fr1
		ldy		#12
clear_loop:
		stx		fr0,y		;!! - X=0 from previous call
		dey
		bne		clear_loop

		;enter decimal mode for main accumulation
		sed

		;set up for 7 bits per digit pair (0-99 in 0-127)
		ldy		#7

		;jump into the middle of the bit loop, so we can avoid an unnecessary
		;double after the last bit
		jmp		fp_fmul_innerloop.offloop_entry
.endp

;--------------------------------------------------------------------------
.proc fp_adjust_exponent
underflow_overflow:
	pla
	pla
.def :fp_exit_zero
	jmp		zfr0

fdiv_entry:
	sec
fmul_entry:
	;stash modified exp1
	tax
	
	;compute new sign
	eor		fr0
	and		#$80
	sta		fr1
	
	;merge exponents
	txa
	adc		fr0
	tax
	eor		fr1
	
	;check for underflow/overflow
	cmp		#128-49
	bcc		underflow_overflow
	
	cmp		#128+49
	bcs		underflow_overflow
	
	;rebias exponent
	txa
	sbc		#$40-1		;!! - C=0 from bcs fail
	sta		fr0
.def :fp_exit
	rts
.endp

;==========================================================================
.proc fp_tab_hi_10000
		dta		$27, $4E, $75, $9C, $C3, $EA
.endp

;==========================================================================
; FDIV [DB28]	Divide FR0 / FR1 -> FR0
;
; Working space:
;	- FR0,6:	Working dividend
;	- FRE:		Mantissa byte counter
;	- FRE,6:	Divisor x1
;	- FR1+1,5:	Divisor x10
;	- FR2,6:	Working quotient
;	- FRX:		Adjusted exponent
;
		fixadr	$db23
fp_fdiv_fld1r_fpconst:
		ldy		#>fpconst_log10_e
fp_fdiv_fld1r:
		jsr		fld1r

		ckaddr	$db28
.proc fdiv
		;check if divisor is zero -- must be first as 0/0 is an error
		lda		fr1
		beq		err

		;check if dividend is zero
		ldx		fr0
		beq		ok
	
		;compute new exponent
		eor		#$7f
		jsr		fp_adjust_exponent.fdiv_entry

		jsr		fp_fdiv_init
		bne		digitloop_entry		;!! - unconditional

digitloop:
		;shift dividend
		lda		fr0+1
		sta		frx
		ldx		#$100-4
bitloop:
		lda		fr0+6,x
		sta		fr0+5,x
		inx
		bne		bitloop
		stx		fr0+5

digitloop_entry:
		;start with 0 for the tens digit
		ldx		#0

		;subtract 10x divisor and increment quotient tens digit until underflow
		sec
		jsr		fp_fdiv_decloop.entry

		tax

		;add 1x divisor and decrement quotient ones digit until overflow
		;note - C=0 from shift in decloop

incloop:
		;decrement quotient mantissa byte
		dex
	
		;add 1x divisor
		.rept 5
			lda		fr0+(5-#)
			adc		fr1+(5-#)
			sta		fr0+(5-#)
		.endr
		lda		frx
		adc		#0
		sta		frx
	
		;keep going until we overflow
		bcc		incloop	

		;write quotient digit
		stx		fr2+5-$80,y
		
		;next quo byte
		iny
		bpl		digitloop

		;if we have a zero leading quotient byte, loop back for one more iteration
		lda		fr2
		bne		quotient_ok
		sty		fr2
		dec		fr0
		lda		fr0
		asl
		cmp		#(64-49)*2
		bcc		underflow
		bcs		digitloop
	
quotient_ok:
		;copy last 5 mantissa bytes back to FR0
		ldx		#5
finalcopy_loop:
		dey
		lda		fr2+5-$80,y
		sta		fr0,x

		dex
		bne		finalcopy_loop

		cld
ok:
		clc
		rts
err:
		sec
		rts

underflow:
		cld
		jmp		fp_clc_zfr0
.endp

;==========================================================================
; SKPSPC [DBA1]	Increment CIX while INBUFF[CIX] is a space
		fixadr	$dba1
skpspc:
	lda		#' '
	ldy		cix
fp_skipchar:
skpspc_loop:
	cmp		(inbuff),y
	bne		skpspc_xit
	iny
	bne		skpspc_loop
skpspc_xit:
	sty		cix
	rts

;==========================================================================
; ISDIGT [DBAF]	Check if INBUFF[CIX] is a digit (UNDOCUMENTED)
		fixadr	$dbaf
isdigt = _isdigt
.proc _isdigt
	ldy		cix
.def :fp_isdigit_y = *
	lda		(inbuff),y
	sec
	sbc		#'0'
	cmp		#10
	rts
.endp

;==========================================================================
.proc fp_fdiv_decloop
decloop:
		;increment quotient tens digit
		inx

entry:
		;subtract 10x divisor
		.rept 5
			lda		fr0+(5-#)
			sbc		fre+(5-#)
			sta		fr0+(5-#)
		.endr
		lda		frx
		sbc		fre
		sta		frx

		;keep going until we underflow
		bcs		decloop

		;shift tens digit (con't after)
		txa
		asl
		asl
		asl
		asl

		;add +10 so we don't have to deal with BCD half carry; note that we
		;couldn't pre-add this previously due to the binary 4-bit shift
		ora		#10

		;exit C=0 (because tens digit < 10)
		rts
.endp

;==========================================================================
		fixadr	$dc00-24

.proc ifp_cont
bitloop:
		;shift in BCD bit, shift out BCD carry
		lda		fr0+3
		adc		fr0+3
		sta		fr0+3
		lda		fr0+2
		adc		fr0+2
		sta		fr0+2
	
		;shift in BCD carry, shift out binary bit
		rol		fr0+1
		rol		fr0

		dey
		bne		bitloop

		lda		#$42
		sta		fr0
.endp

;==========================================================================
; NORMALIZE [DC00]	Normalize FR0 (UNDOCUMENTED)
		ckaddr	$dc00-1
fp_normalize_cld:
	cld
	ckaddr	$dc00
fp_normalize:
.nowarn .proc normalize

	ldy		#5
normloop:
	lda		fr0
	asl
	cmp		#(64-49)*2
	bcc		underflow
	
	ldx		fr0+1
	beq		need_norm

	;Okay, we're done normalizing... check if the exponent is in bounds.
	;It needs to be within +/-48 to be valid. If the exponent is <-49,
	;we set it to zero; otherwise, we mark overflow.
	
	cmp		#(64+49)*2
	rts
	
need_norm:
	dec		fr0

	mva		fr0+2 fr0+1
	mva		fr0+3 fr0+2
	mva		fr0+4 fr0+3
	mva		fr0+5 fr0+4
	stx		fr0+5

	dey
	bne		normloop
	
	;Hmm, we shifted out everything... must be zero; reset exponent. This
	;is critical since Atari Basic depends on the exponent being zero for
	;a zero result.
underflow:
	jmp		fp_clc_zfr0
	
.endp

;==========================================================================
; HELPER ROUTINES
;==========================================================================

;--------------------------------------------------------------------------
.proc fp_fsub_cont					;$1F bytes
	;propagate borrow up
borrow_loop:
	lda		fr0+1,x
	sbc		#0
	sta		fr0+1,x
entry:
	;exit if no more borrow
	bcs		fp_normalize_cld
	dex
	bpl		borrow_loop

reverse_sub_entry:
	;we borrowed out the top... invert sign and then subtract from 0
	lda		#$80
	eor		fr0
	sta		fr0

	ldx		#5
	sec
underflow_loop:
	lda		#0
	sbc		fr0,x
	sta		fr0,x
	dex
	bne		underflow_loop
	beq		fp_normalize_cld
.endp

;--------------------------------------------------------------------------
.proc fp_rmove
		ldx		#5
loop:
		lda		fr1,x
		sta		fr0,x

		dex
		bpl		loop
		clc
		rts
.endp

;--------------------------------------------------------------------------

.proc fp_reverse_addsub
noswap:	
		;A = FR1 - FR0 - 1 on entry
		;X = add/sub flag

		;complement (~x = -x - 1)
		eor		#$ff

		;A = FR0 - FR1

		;compute positions for add/subtract	
		adc		#3			;A = (FR0) - (FR1) + 4   !! carry is set coming in

		;check if FR1 is too small in magnitude to matter
		bmi		fp_rmove

		;jump to decimal mode and prepare for add/sub loops
		sed
		tay

		;copy down exponent from larger number in FR1
		lda		fr1
		sta		fr0

		;check if we are doing a sum or a difference
		cpx		#$80
		ldx		#5
		bcs		do_subtract
			
		;add mantissas
add_loop:
		lda		fr0+1,y
		adc		fr1,x
		sta		fr0,x
		dex
		dey
		bpl		add_loop

		;apply any carries from the common mantissa bytes; we are guaranteed
		;at least one additional non-common byte, since this path is only
		;executed for non-equal exponents
sum_carryloop:
		lda		fr1,x
		adc		#0
		sta		fr0,x
		dex
		bne		sum_carryloop

		;if we still carried out, jump out to bump the exponent and shift
		;a $01 at the top of the mantissa
		bcs		sum_finalize

		;exit -- no renormalization required
		cld
		rts

sum_finalize:
		jmp		fsub.sum_finalize

do_subtract:
		;subtract FR0 and FR1 mantissas (!! carry is set coming in)
sub_loop:
		lda		fr1,x
		sbc		fr0+1,y
		sta		fr0,x
		dex
		dey
		bpl		sub_loop

		;propagate borrow up -- always at least one iteration as this
		;call path is executed with unequal exponents
borrow_loop:
		lda		fr1,x
		sbc		#0
		sta		fr0,x
		dex
		bpl		borrow_loop

		;if borrowed out the top, invert sign and then subtract from 0
		bcc		fp_fsub_cont.reverse_sub_entry

		;no more borrow, normalize and exit
		jmp		fp_normalize_cld
.endp

;--------------------------------------------------------------------------
.proc fp_fmul_innerloop
_offset = _fr3+5
_offset2 = fr2

		;begin outer loop -- this is where we process one _bit_ out of each
		;multiplier byte in FR2's mantissa (note that this is inverted in that
		;it is bytes-in-bits instead of bits-in-bytes)
offloop:
		;double multiplicand in fr1
		clc
		lda		fr1+5
		adc		fr1+5
		sta		fr1+5
		lda		fr1+4
		adc		fr1+4
		sta		fr1+4
		lda		fr1+3
		adc		fr1+3
		sta		fr1+3
		lda		fr1+2
		adc		fr1+2
		sta		fr1+2
		lda		fr1+1
		adc		fr1+1
		sta		fr1+1
		lda		fr1+0
		adc		fr1+0
		sta		fr1+0

offloop_entry:
		;begin inner loop -- here we process the same bit in each multiplier
		;byte, going from byte 5 down to byte 1
		ldx		#5
offloop2:
		;shift an inverted bit out of multiplier byte
		lsr		fr2,x
		bcs		post_addsub
			
		;add fr1 to fr0 at offset	
		.rept 6
			lda		fr0+(5-#),x
			adc		fr1+(5-#)
			sta		fr0+(5-#),x
		.endr
	
		;check if we have a carry out to the upper bytes
		bcc		no_carry
		stx		_offset2

carry_loop:
		dex
		lda		#0
		adc		fr0,x
		sta		fr0,x
		bcs		carry_loop

		ldx		_offset2
no_carry:

post_addsub:
		;go back for next byte
		dex
		bne		offloop2

		;decrement bit counter
		dey
		bne		offloop

bits_done:
		;check if we need to shift up -- note that unlike the stock
		;normalize routine, we must shift in the 6th mantissa byte
		lda		fr0+1
		bne		no_shift

		ldx		#<-5
shift_loop:
		lda		fr0+7,x
		sta		fr0+6,x
		inx
		bne		shift_loop

		dec		fr0

no_shift:
		;all done
		jmp		fp_normalize_cld
.endp

;==========================================================================
.proc fp_tab_hi_100				;$0A bytes
		dta		$00, $00, $00, $01, $01, $01, $02, $02, $03, $03
.endp

;==========================================================================
; PLYEVL [DD40]	Eval polynomial at (X:Y) with A coefficients using FR0
;
		fixadr	$dd3e
fp_plyevl_10:
	lda		#10
.nowarn .proc plyevl
	;stash arguments
	stx		fptr2
	sty		fptr2+1
	sta		_fpcocnt
	
	;copy FR0 -> PLYARG
	ldx		#<plyarg
	ldy		#>plyarg
	jsr		fst0r
	
	jsr		zfr0
	
loop:
	;load next coefficient and increment coptr
	lda		fptr2
	tax
	clc
	adc		#6
	sta		fptr2
	ldy		fptr2+1
	scc:inc	fptr2+1
	jsr		fld1r

	;add coefficient to acc
	jsr		fadd
	bcs		xit

	dec		_fpcocnt
	beq		xit
	
	;copy PLYARG -> FR1
	;multiply accumulator by Z and continue
	ldx		#<plyarg
	ldy		#>plyarg	
	jsr		fp_fld1r_fmul
	bcc		loop
xit:
	rts
.endp

;==========================================================================
; FMUL multiplier conversion helper
;
; This helper is used to convert multiplier digit pair bytes to binary
; 0-99 numbers in preparation for generating partial factors from
; multiplier bit slices.
;
; Each byte is also inverted so that a 0 bit means to add. This saves a
; CLC per add. This inversion is done for free by a tricky rearrangement
; of the math to convert the BCD tens digit.
;
.proc fp_fmul_fr0_to_binfr2		;$15 bytes
		ldx		#5
loop:
		;extract tens digit
		lda		fr0,x
		lsr
		lsr
		lsr
		lsr
		tay

		;z = ~(x - (x/16)*10)         (BCD to binary, inverted)
		;  = -(x - (x/16)*10) - 1
		;  = ((x/16)*10 - x) - 1
		sec
		lda		fp_dectobin_tab,y
		sbc		fr0,x
		sta		fr2,x

		dex
		bne		loop

		;exit X=0 for calling code
		rts
.endp

;==========================================================================
; FLD0R [DD89]	Load FR0 from (X:Y)
; FLD0P [DD8D]	Load FR0 from (FLPTR)
;
	fixadr	$dd89
fld0r:
	stx		flptr
fld0r_y:
	sty		flptr+1
	ckaddr	$dd8d
fld0p:
	ldy		#5
fld0ploop:
	lda		(flptr),y
	sta		fr0,y
	dey
	bpl		fld0ploop
	rts

;==========================================================================
; FLD1R [DD98]	Load FR1 from (X:Y)
; FLD1P [DD9C]	Load FR1 from (FLPTR)
;
	fixadr	$dd98
fld1r:
	stx		flptr
	sty		flptr+1
	ckaddr	$dd9c
fld1p:
	ldy		#5
fld1ploop:
	lda		(flptr),y
	sta		fr1,y
	dey
	bpl		fld1ploop
	rts

;==========================================================================
; FST0R [DDA7]	Store FR0 to (X:Y)
; FST0P [DDAB]	Store FR0 to (FLPTR)
;
	fixadr	$dda7
fst0r:
	stx		flptr
	sty		flptr+1
	ckaddr	$ddab
fst0p:
	ldy		#5
fst0ploop:
	lda		fr0,y
	sta		(flptr),y
	dey
	bpl		fst0ploop
	rts

;==========================================================================
; FMOVE [DDB6]	Move FR0 to FR1
;
	fixadr	$ddb6
fmove:
	ldx		#5
fmoveloop:
	lda		fr0,x
	sta		fr1,x
	dex
	bpl		fmoveloop
	rts

;==========================================================================
; EXP [DDC0]	Compute e^x
; EXP10 [DDCC]	Compute 10^x
;
; Algorithm is based on Ruckdeschel, Table VI.13, Series approximation
; for 10^X over the interval 0 <= X <= 1, using a minimax polynomial:
;
;	10^X = (1 + A1*X + A2*X^3 + A3*X^5 + A4*X^7 + A5*X^9 + A6*X^11
;		+ A7*X^13 + A8*X^15 + A9*X^17)^2
;
; The input X value is split into integer and fraction, integer/2 is added
; to the final exponent, and an additional x10 multipled if integer is odd.
;
; There are, however, adjustments to match precise behavior in the standard
; math pack, including quirks:
;
;	- The polynomial is biased down slightly from the original minimax by
;	  1 ulp.
;
;	- If the input exponent is negative, 1/10^|X| is computed. This is
;	  terrible, but needed to match.
;
;	- Smaller negative exponents underflow to 0 in the original math pack,
;	  but some larger negative exponents produce an error.
;
;	- The integer is computed by _rounding_ instead of truncation. This
;	  causes the polynomial to be evaluated over [-0.5, 0.5] instead
;	  of the [0, 1] region it was optimized over. This is also terrible,
;	  but we need to do it to match.
;
;	- A bug in the math pack causes inputs of |X| > 65535.5 to give incorrect
;	  results instead of an overflow. We don't currently replicate this
;	  bug.
;
	fixadr	$ddc0
exp10 = exp._exp10
.proc exp
		ldx		#<fpconst_log10_e
		ldy		#>fpconst_log10_e
		jsr		fld1r		;we could use fp_fld1r_fmul, but then we have a hole :(
		jsr		fmul
		bcs		err2

		ckaddr	$ddcc
_exp10:
		;check if the input is negative
		ldx		fr0
		bpl		input_positive

		;input is negative -- compute absolute, then invert
		asl		fr0
		lsr		fr0
		jsr		_exp10
		bcs		underflow

		jsr		fmove
		ldx		#<fpconst_one
		ldy		#>fpconst_one
		jsr		fld0r
		jmp		fdiv

underflow:
		jmp		fp_clc_zfr0

input_positive:
		;set up FR1 for round(X)
		lda		#0
		sta		fr1+2
		sta		fr1+3
		sta		fr1+4
		sta		fr1+5

		;check exponent to detect input < 0.01 or >= 100
		cpx		#$40

		;jump if round(x) must be zero
		bcc		tiny_input

		;check for input >= 100; this will always overflow
		bne		err2

		;load integer digit pair if in [1, 100), and round up based on the fraction
		sed
		ldx		fr0+2
		cpx		#$50
		adc		fr0+1
		cld

		;if we carried, the integer is at least 99 and we are guaranteed to overflow
		bcs		err2

tiny_input:
		;write integer digit pair to FR1
		sta		fr1+1

		;convert to binary and save it
		jsr		fp_dectobin
		sbc		fp_dectobin_tab,y

		;save integer portion to adjust exponent later
		sta		_fptemp0

		;skip subtraction if it's zero
		beq		no_integer

		;compute y = frac(x)
		ldx		#$40
		stx		fr1
		jsr		fsub

no_integer:
		;evaluate f(y)
		ldx		#<fpconst_exp10_coeff
		ldy		#>fpconst_exp10_coeff
		jsr		fp_plyevl_10

		;square result
		jsr		fmove
		jsr		fmul
	
		;halve integer portion to compute exponent adjustment
		lsr		_fptemp0
	
		;scale by 10 if necessary
		bcc		even
		ldx		#<fpconst_ten
		ldy		#>fpconst_ten
		jsr		fp_fld1r_fmul
		bcs		err2
even:

		;bias exponent
		lda		_fptemp0
		adc		fr0
		cmp		#64+49			;set error flag if overflow
		sta		fr0

err2:
xit2:
		rts
.endp	

;==========================================================================
.proc fp_tab_lo_1000
		dta		$00, $E8, $D0, $B8, $A0, $88, $70, $58, $40, $28
.endp

;==========================================================================
.proc fp_tab_hi_1000
		dta		$00, $03, $07, $0B, $0F, $13, $17, $1B, $1F, $23
.endp

;==========================================================================
; Minimax polynomial for 10^x over 0 <= x < 1 (required by Turbo Basic XL)
;
; See Ruckdeschel, Table VI.13, page 174.
;
		fixadr		$de4d
fpconst_exp10_coeff:
		dta		$3D, $17, $94, $19, $00, $00	;A9 = 0.0000179419
		dta		$3D, $57, $33, $05, $00, $00	;A8 = 0.0000573305
		dta		$3E, $05, $54, $76, $62, $00	;A7 = 0.0005547662
		dta		$3E, $32, $19, $62, $27, $00	;A6 = 0.0032196227
		dta		$3F, $01, $68, $60, $30, $36	;A5 = 0.0168603036
		dta		$3F, $07, $32, $03, $27, $41	;A4 = 0.0732032741
		dta		$3F, $25, $43, $34, $56, $75	;A3 = 0.2543345675
		dta		$3F, $66, $27, $37, $30, $50	;A2 = 0.6627373050
		dta		$40, $01, $15, $12, $92, $55	;A1 = 1.1512925485
		dta		$3F, $99, $99, $99, $99, $99	;constant 1 term (with tweak)

;==========================================================================
; log10(e) constant (required by Turbo Basic XL)
;
		fixadr		$de89
fpconst_log10_e:
		dta		$3F,$43,$42,$94,$48,$19			;0.4342944819[0325182765112891891661]

;==========================================================================	
; 10.0 factor for LOG/EXP
;
; This constant is NOT used by Turbo Basic XL.
;
fpconst_ten:
		dta		$40,$10,$00,$00,$00,$00		;10

;==========================================================================
; REDRNG [DE95]	Reduce range via y = (x-C)/(x+C) (undocumented)
;
; Used for better convergence in multiple polynomial approximations.
;
; X:Y = pointer to C argument
;
	fixadr	$de95
redrng = _redrng
.proc _redrng
	stx		fptr2
	sty		fptr2+1
	jsr		fld1r
	ldx		#<fpscr
	ldy		#>fpscr
	jsr		fst0r
	jsr		fadd
	bcs		fail
	ldx		#<plyarg
	ldy		#>plyarg
	jsr		fst0r
	ldx		#<fpscr
	ldy		#>fpscr
	jsr		fld0r
	ldx		fptr2
	ldy		fptr2+1
	jsr		fld1r
	jsr		fsub
	bcs		fail
	ldx		#<plyarg
	ldy		#>plyarg
	jmp		fp_fdiv_fld1r
	
fail = log.xit2
.endp

;==========================================================================
; LOG [DECD]	Compute ln x
; LOG10 [DED1]	Compute log10 x
;
; The algorithm is from Ruckdeschel, Table VI.11, page 173:
;
;	LOG10(X) = 0.5 + A1*Z + A2*Z^3 + ... + A10*Z^19
;		where Z = (X - c) / (X + c) where c - 3.162277660
;		for 1 <= X <= 10 [sic].
;
; The constant c is sqrt(10). The exponent of the input number is removed
; and the mantissa divided by 10 if needed to reduce range to [1, 10).
;
; LOG(x) is computed by dividing LOG10(x) by LOG10(e). It would be faster
; to multiply by the reciprocal instead, but the division needs to be done
; to match results.
;
	fixadr	$decd
.proc log
		lsr		_fptemp1
		bpl		entry
		ckaddr	$ded1
.def :log10 = *
		sec
		ror		_fptemp1
entry:
		;throw error on negative number or zero
		lda		fr0
		bmi		err
		bne		ok
err:
		sec
xit2:
		rts
	
ok:
		;stash exponentx2 - 128
		asl
		eor		#$80
		sta		_fptemp0
	
		;reset exponent so we are in 1 <= z < 100
		ldx		#$40
		stx		fr0
	
		;divide by 10 if >=10 (yes, this needs to truncate)
		lda		fr0+1
		cmp		#$10
		bcc		in_range

		ldx		#<fpconst_ten
		jsr		fp_fdiv_fld1r_fpconst				;cannot fail

		;adjust final value by +1 to reflect this
		inc		_fptemp0

in_range:
		;apply y = (z-sqrt(10))/(z+sqrt(10)) change of variable so we can use a
		;faster converging series
		ldx		#<fpconst_sqrt10
		ldy		#>fpconst_sqrt10
		jsr		redrng
	
		;stash y so we can later multiply it back in
		ldx		#<fpscr
		ldy		#>fpscr
		jsr		fst0r
	
		;square the value so we compute a series on y^2n
		jsr		fmove
		jsr		fmul
	
		;do polynomial expansion (cannot fail)
		ldx		#<fpconst_log10coeff
		ldy		#>fpconst_log10coeff
		jsr		fp_plyevl_10		;cannot fail
	
		;multiply back in so we have series on y^(2n+1)
		ldx		#<fpscr
		ldy		#>fpscr
		jsr		fp_fld1r_fmul		;cannot fail

		;add 0.5
		ldx		#<fpconst_half
		ldy		#>fpconst_half
		jsr		fld1r
		jsr		fadd
	
		;stash
		jsr		fmove
	
		;convert exponent adjustment back to float (signed)
		lda		#0
		sta		fr0+1
		ldx		_fptemp0
		bpl		expadj_positive
		sec
		sbc		_fptemp0
		tax
expadj_positive:
		stx		fr0
		jsr		ifp
	
		;merge (cannot fail)
		asl		fr0
		asl		_fptemp0
		ror		fr0
		jsr		fadd
	
		;scale if doing log
		bit		_fptemp1
		bmi		xit2

		;divide by log10(e); sadly we must do this to match results instead
		;of multiplying by ln(10)
		ldx		#<fpconst_log10_e
		jmp		fp_fdiv_fld1r_fpconst
.endp

;--------------------------------------------------------------------------
.proc fp_fdiv_init					;$22 bytes
	;save off x1 divisor
	ldx		#5
	mva:rne	fr1,x fre,x-

	;extend working dividend to 6 digit pairs
	txa
	sta		frx
	
	;compute x10 divisor
	ldy		#4
bitloop:
	asl		fre+5
	rol		fre+4
	rol		fre+3
	rol		fre+2
	rol		fre+1
	rol
	dey
	bne		bitloop

	sta		fre

	sed

	ldy		#$80-5
	rts
.endp

;==========================================================================
; sqrt(10) (required by Turbo Basic XL)
;
		fixadr	$df66
fpconst_sqrt10:
		dta		$40, $03, $16, $22, $77, $66	;3.16227766

;==========================================================================
; HALF (required by Atari BASIC and Turbo Basic XL)
;
		fixadr	$df6c
fpconst_half:
		dta		$3F, $50, $00, $00, $00, $00	;0.5
	
;==========================================================================
; log10(x) coefficients (required by Turbo Basic XL)
;
; See Ruckdeschel, Table VI.11 on page 173.
;
		fixadr	$df72
fpconst_log10coeff:		;minimax series expansion for log10((z-sqrt(10))/(z+sqrt(10)))
		dta		$3F, $49, $15, $57, $11, $08	;A10 =  0.4915571108
		dta		$BF, $51, $70, $49, $47, $08	;A9  = -0.5170494708
		dta		$3F, $39, $20, $57, $61, $95	;A8  =  0.3920576195
		dta		$BF, $04, $39, $63, $03, $55	;A7  = -0.0439630355
		dta		$3F, $10, $09, $30, $12, $64	;A6  =  0.1009301264
		dta		$3F, $09, $39, $08, $04, $60	;A5  =  0.0939080460
		dta		$3F, $12, $42, $58, $47, $42	;A4  =  0.1242584742
		dta		$3F, $17, $37, $12, $06, $08	;A3  =  0.1737120608
		dta		$3F, $28, $95, $29, $71, $17	;A2  =  0.2895297117
		dta		$3F, $86, $85, $88, $96, $44	;A1  =  0.8685889644

;==========================================================================
; Arctangent coefficients (required by Atari BASIC and Turbo Basic XL)
;
; The 11 coefficients here form a power series approximation
; f(x^2) ~= atn(x)/x. These are not used by the math pack itself and
; not officially documented, but are stored in the math pack for use
; by Atari BASIC's ATN() function.
;
; See Ruckdeschel, Table V1.10a, page 172.
;
		fixadr	$dfae
fpconst_atncoef:
		dta		$3E, $16, $05, $44, $49, $22	;A10 =  0.001605444922
		dta		$BE, $95, $68, $38, $45, $20	;A9  = -0.009568384520
		dta		$3F, $02, $68, $79, $94, $16	;A8  =  0.0268799415 61
		dta		$BF, $04, $92, $78, $90, $80	;A7  = -0.0492789080 30
		dta		$3F, $07, $03, $15, $20, $00	;A6  =  0.0703152000 33
		dta		$BF, $08, $92, $29, $12, $44	;A5  = -0.0892291243 81
		dta		$3F, $11, $08, $40, $09, $11	;A4  =  0.1108400911 04
		dta		$BF, $14, $28, $31, $56, $04	;A3  = -0.1428315603 76
		dta		$3F, $19, $99, $98, $77, $44	;A2  =  0.1999987744 21
		dta		$BF, $33, $33, $33, $31, $13	;A1  = -0.3333333112 86
fpconst_one:
		dta		$40, $01, $00, $00, $00, $00		;1.0 (also an arctan coeff)

;==========================================================================
; pi/4 (required by Atari BASIC and Turbo Basic XL ATN())
;
		fixadr	$dff0
fpconst_pi4:
		dta		$3F, $78, $53, $98, $16, $34		;0.7853981633[9744830961566084581988]
	
;==========================================================================
fp_dectobin_tab:
		:10 dta	[6*#-1]
	
;==========================================================================
; end of math pack
;
		ckaddr	$e000

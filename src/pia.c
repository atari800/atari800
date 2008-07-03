/*
 * pia.c - PIA chip emulation
 *
 * Copyright (C) 1995-1998 David Firth
 * Copyright (C) 1998-2008 Atari800 development team (see DOC/CREDITS)
 *
 * This file is part of the Atari800 emulator project which emulates
 * the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.
 *
 * Atari800 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Atari800 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Atari800; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "config.h"

#include "atari.h"
#include "cpu.h"
#include "memory.h"
#include "pia.h"
#include "sio.h"
#ifdef XEP80_EMULATION
#include "xep80.h"
#endif
#ifndef BASIC
#include "input.h"
#include "statesav.h"
#endif

UBYTE PACTL;
UBYTE PBCTL;
UBYTE PORTA;
UBYTE PORTB;
UBYTE PORT_input[2];

int xe_bank = 0;
int selftest_enabled = 0;

UBYTE atari_basic[8192];
UBYTE atari_os[16384];

UBYTE PORTA_mask;
UBYTE PORTB_mask;

void PIA_Initialise(int *argc, char *argv[])
{
	PACTL = 0x3f;
	PBCTL = 0x3f;
	PORTA = 0xff;
	PORTB = 0xff;
	PORTA_mask = 0xff;
	PORTB_mask = 0xff;
	PORT_input[0] = 0xff;
	PORT_input[1] = 0xff;
}

void PIA_Reset(void)
{
	PORTA = 0xff;
	if (machine_type == MACHINE_XLXE) {
		MEMORY_HandlePORTB(0xff, (UBYTE) (PORTB | PORTB_mask));
	}
	PORTB = 0xff;
}

UBYTE PIA_GetByte(UWORD addr)
{
	switch (addr & 0x03) {
	case _PACTL:
		return PACTL & 0x3f;
	case _PBCTL:
		return PBCTL & 0x3f;
	case _PORTA:
		if ((PACTL & 0x04) == 0) {
			/* direction register */
			return ~PORTA_mask;
		}
		else {
			/* port state */
#ifdef XEP80_EMULATION
			if (XEP80_enabled) {
				return(XEP80_GetBit() & PORT_input[0] & (PORTA | PORTA_mask));
			}
#endif /* XEP80_EMULATION */
			return PORT_input[0] & (PORTA | PORTA_mask);
		}
	case _PORTB:
		if ((PBCTL & 0x04) == 0) {
			/* direction register */
			return ~PORTB_mask;
		}
		else {
			/* port state */
			if (machine_type == MACHINE_XLXE) {
				return PORTB | PORTB_mask;
			}
			else {
				return PORT_input[1] & (PORTB | PORTB_mask);
			}
		}
	}
	/* for stupid compilers */
	return 0xff;
}

void PIA_PutByte(UWORD addr, UBYTE byte)
{
	switch (addr & 0x03) {
	case _PACTL:
                /* This code is part of the cassette emulation */
		/* The motor status has changed */
		SIO_TapeMotor(byte & 0x08 ? 0 : 1);
	
		PACTL = byte;
		break;
	case _PBCTL:
		/* This code is part of the serial I/O emulation */
		if ((PBCTL ^ byte) & 0x08) {
			/* The command line status has changed */
			SIO_SwitchCommandFrame(byte & 0x08 ? 0 : 1);
		}
		PBCTL = byte;
		break;
	case _PORTA:
		if ((PACTL & 0x04) == 0) {
			/* set direction register */
 			PORTA_mask = ~byte;
		}
		else {
			/* set output register */
#ifdef XEP80_EMULATION
			if (XEP80_enabled && (~PORTA_mask & 0x11)) {
				XEP80_PutBit(byte);
			}
#endif /* XEP80_EMULATION */
			PORTA = byte;		/* change from thor */
		}
#ifndef BASIC
		INPUT_SelectMultiJoy((PORTA | PORTA_mask) >> 4);
#endif
		break;
	case _PORTB:
		if (machine_type == MACHINE_XLXE) {
			if ((PBCTL & 0x04) == 0) {
				/* direction register */
				MEMORY_HandlePORTB((UBYTE) (PORTB | ~byte), (UBYTE) (PORTB | PORTB_mask));
				PORTB_mask = ~byte;
			}
			else {
				/* output register */
				MEMORY_HandlePORTB((UBYTE) (byte | PORTB_mask), (UBYTE) (PORTB | PORTB_mask));
				PORTB = byte;
			}
		}
		else {
			if ((PBCTL & 0x04) == 0) {
				/* direction register */
				PORTB_mask = ~byte;
			}
			else {
				/* output register */
				PORTB = byte;
			}
		}
		break;
	}
}

#ifndef BASIC

void PIAStateSave(void)
{
	int Ram256 = 0;
	if (ram_size == RAM_320_RAMBO)
		Ram256 = 1;
	else if (ram_size == RAM_320_COMPY_SHOP)
		Ram256 = 2;

	StateSav_SaveUBYTE( &PACTL, 1 );
	StateSav_SaveUBYTE( &PBCTL, 1 );
	StateSav_SaveUBYTE( &PORTA, 1 );
	StateSav_SaveUBYTE( &PORTB, 1 );

	StateSav_SaveINT( &xe_bank, 1 );
	StateSav_SaveINT( &selftest_enabled, 1 );
	StateSav_SaveINT( &Ram256, 1 );

	StateSav_SaveINT( &cartA0BF_enabled, 1 );

	StateSav_SaveUBYTE( &PORTA_mask, 1 );
	StateSav_SaveUBYTE( &PORTB_mask, 1 );
}

void PIAStateRead(void)
{
	int Ram256 = 0;

	StateSav_ReadUBYTE( &PACTL, 1 );
	StateSav_ReadUBYTE( &PBCTL, 1 );
	StateSav_ReadUBYTE( &PORTA, 1 );
	StateSav_ReadUBYTE( &PORTB, 1 );

	StateSav_ReadINT( &xe_bank, 1 );
	StateSav_ReadINT( &selftest_enabled, 1 );
	StateSav_ReadINT( &Ram256, 1 );

	if (Ram256 == 1 && machine_type == MACHINE_XLXE && ram_size == RAM_320_COMPY_SHOP)
		ram_size = RAM_320_RAMBO;

	StateSav_ReadINT( &cartA0BF_enabled, 1 );

	StateSav_ReadUBYTE( &PORTA_mask, 1 );
	StateSav_ReadUBYTE( &PORTB_mask, 1 );
}

#endif /* BASIC */

/* $Id$ */
#include <stdio.h>
#include <stdlib.h>

#include "atari.h"
#include "config.h"
#include "cpu.h"
#include "memory.h"
#include "pia.h"
#include "sio.h"
#include "log.h"
#include "statesav.h"

UBYTE PACTL;
UBYTE PBCTL;
UBYTE PORTA;
UBYTE PORTB;
UBYTE PORT_input[2] = {0xff, 0xff};

int xe_bank = 0;
int selftest_enabled = 0;

UBYTE atari_basic[8192];
UBYTE atari_os[16384];

static UBYTE PORTA_mask = 0xff;
static UBYTE PORTB_mask = 0xff;

void PIA_Initialise(int *argc, char *argv[])
{
	PORTA = 0x00;
	PORTB = 0xff;
}

UBYTE PIA_GetByte(UWORD addr)
{
	UBYTE byte = 0xff;

	addr &= 0x03;		/* HW registers are mirrored */
	switch (addr) {
	case _PACTL:
		byte = PACTL & 0x3f;
		break;
	case _PBCTL:
		byte = PBCTL & 0x3f;
#ifdef DEBUG1
		printf("RD: PBCTL = %x, PC = %x\n", PBCTL, PC);
#endif
		break;
	case _PORTA:
		if (!(PACTL & 0x04))
 			byte = ~PORTA_mask;
		else
			byte = PORT_input[0] & (PORTA | PORTA_mask);
		break;
	case _PORTB:
		if (machine_type == MACHINE_XLXE)
			byte = (PORTB & (~PORTB_mask)) | PORTB_mask;
		else
			byte = PORT_input[1] & (PORTB | PORTB_mask);
		break;
	}

	return byte;
}

void PIA_PutByte(UWORD addr, UBYTE byte)
{
	addr &= 0x03;		/* HW registers are mirrored */
	switch (addr) {
	case _PACTL:
		PACTL = byte;
		break;
	case _PBCTL:
		/* This code is part of the serial I/O emulation */
		if ((PBCTL ^ byte) & 0x08) {	/* The command line status has changed */
			SwitchCommandFrame((byte & 0x08) ? (0) : (1));
		}
		PBCTL = byte;
#ifdef DEBUG1
		printf("WR: PBCTL = %x, PC = %x\n", PBCTL, PC);
#endif
		break;
	case _PORTA:
		if (!(PACTL & 0x04))
 			PORTA_mask = ~byte;
		else
			PORTA = byte;		/* change from thor */

		break;
	case _PORTB:
		if (!(PBCTL & 0x04)) {	/* change from thor */
			PORTB_mask = ~byte;
			byte = PORTB;
			break;
		}

		if (machine_type == MACHINE_XLXE) {
#if 0
/* We don't want any hacks. This one blocked usage of a memory bank */
/* with OS ROM disabled in 1088 XE. If a game doesn't work in XL/XE */
/* because it doesn't in original, just switch to 400/800. */
			if ((byte | PORTB_mask) == 0)
				break;				/* special hack for old Atari800 games like is Tapper, for example */
#endif
			PORTB_handler(byte);
		}
		else {
		/*
			if (!(PBCTL & 0x04))
				PORTB_mask = ~byte;
		*/
			PORTB = byte;
		}
		break;
	}
}

void PIAStateSave(void)
{
	int Ram256 = 0;
	if (ram_size == RAM_320_RAMBO)
		Ram256 = 1;
	else if (ram_size == RAM_320_COMPY_SHOP)
		Ram256 = 2;

	SaveUBYTE( &PACTL, 1 );
	SaveUBYTE( &PBCTL, 1 );
	SaveUBYTE( &PORTA, 1 );
	SaveUBYTE( &PORTB, 1 );

	SaveINT( &xe_bank, 1 );
	SaveINT( &selftest_enabled, 1 );
	SaveINT( &Ram256, 1 );

	SaveINT( &cartA0BF_enabled, 1 );

	SaveUBYTE( &PORTA_mask, 1 );
	SaveUBYTE( &PORTB_mask, 1 );
}

void PIAStateRead(void)
{
	int Ram256 = 0;

	ReadUBYTE( &PACTL, 1 );
	ReadUBYTE( &PBCTL, 1 );
	ReadUBYTE( &PORTA, 1 );
	ReadUBYTE( &PORTB, 1 );

	ReadINT( &xe_bank, 1 );
	ReadINT( &selftest_enabled, 1 );
	ReadINT( &Ram256, 1 );

	if (Ram256 == 1 && machine_type == MACHINE_XLXE && ram_size == RAM_320_COMPY_SHOP)
		ram_size = RAM_320_RAMBO;

	ReadINT( &cartA0BF_enabled, 1 );

	ReadUBYTE( &PORTA_mask, 1 );
	ReadUBYTE( &PORTB_mask, 1 );
}

/*
$Log$
Revision 1.8  2002/07/14 13:25:57  pfusik
removed a hack that affected 1088 XE

Revision 1.7  2001/09/27 09:30:39  fox
Atari_PORT -> PORT_input

Revision 1.6  2001/09/17 18:12:33  fox
machine, mach_xlxe, Ram256, os, default_system -> machine_type, ram_size

Revision 1.5  2001/07/20 20:08:26  fox
Ram256 moved to atari.c,
cartA0BF_enabled in memory-d is used instead of rom_inserted

Revision 1.2  2001/03/18 06:34:58  knik
WIN32 conditionals removed

*/

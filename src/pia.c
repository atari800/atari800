#include <stdio.h>
#include <stdlib.h>

#include "atari.h"
#include "config.h"
#include "cpu.h"
#include "memory.h"
#include "pia.h"
#include "platform.h"
#include "sio.h"
#include "log.h"
#include "statesav.h"

UBYTE PACTL;
UBYTE PBCTL;
UBYTE PORTA;
UBYTE PORTB;

int xe_bank = 0;
int selftest_enabled = 0;
int Ram256 = 0;

extern int mach_xlxe;

int rom_inserted;
UBYTE atari_basic[8192];
UBYTE atarixl_os[16384];

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
		byte = PACTL;
		break;
	case _PBCTL:
		byte = PBCTL;
#ifdef DEBUG1
		printf("RD: PBCTL = %x, PC = %x\n", PBCTL, PC);
#endif
		break;
	case _PORTA:
		if (!(PACTL & 0x04))
 			byte = ~PORTA_mask;
		else
			byte = Atari_PORT(0) & (PORTA_mask|PORTA);
		break;
	case _PORTB:
		switch (machine) {
		case Atari:
			byte = Atari_PORT(1);
			byte &= PORTB_mask|PORTB;
			break;
		case AtariXL:
		case AtariXE:
		case Atari320XE:
			byte = (PORTB & (~PORTB_mask)) | PORTB_mask;
			break;
		default:
			Aprint("Fatal Error in pia.c: PIA_GetByte(): Unknown machine\n");
			Atari800_Exit(FALSE);
#ifndef WIN32
			exit(1);
#endif
			break;
		}
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

		if (((byte | PORTB_mask) == 0) && mach_xlxe)
			break;				/* special hack for old Atari800 games like is Tapper, for example */

		if (machine == Atari) {
		/*
			if (!(PBCTL & 0x04))
				PORTB_mask = ~byte;
		*/
			PORTB = byte;
		} else {
			PORTB_handler(byte);
		}
	}
}

void PIAStateSave(void)
{
	SaveUBYTE( &PACTL, 1 );
	SaveUBYTE( &PBCTL, 1 );
	SaveUBYTE( &PORTA, 1 );
	SaveUBYTE( &PORTB, 1 );

	SaveINT( &xe_bank, 1 );
	SaveINT( &selftest_enabled, 1 );
	SaveINT( &Ram256, 1 );

	SaveINT( &rom_inserted, 1 );

	SaveUBYTE( &PORTA_mask, 1 );
	SaveUBYTE( &PORTB_mask, 1 );
}

void PIAStateRead(void)
{
	ReadUBYTE( &PACTL, 1 );
	ReadUBYTE( &PBCTL, 1 );
	ReadUBYTE( &PORTA, 1 );
	ReadUBYTE( &PORTB, 1 );

	ReadINT( &xe_bank, 1 );
	ReadINT( &selftest_enabled, 1 );
	ReadINT( &Ram256, 1 );

	ReadINT( &rom_inserted, 1 );

	ReadUBYTE( &PORTA_mask, 1 );
	ReadUBYTE( &PORTB_mask, 1 );
}

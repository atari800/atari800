/*
 * videl.c - Atari Falcon specific port code
 *
 * Copyright (c) 1997-1998 Petr Stehlik and Karel Rous
 * Copyright (c) 1998-2017 Atari800 development team (see DOC/CREDITS)
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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <mint/falcon.h>
#include <mint/osbind.h>
/* WTF; _hz_200 is declared without volatile keyword
#include <mint/sysvars.h>
*/
#define _hz_200 ((volatile unsigned long *) 0x4baL)
#define _vbclock ((volatile unsigned long *) 0x462L)

#include "atari.h"
#include "videl.h"

Videl_Registers *p_str_p;

/*-------------------------------------------------------*/
/*       Videl registers                                 */
/*-------------------------------------------------------*/
#define RShift			((volatile UBYTE *) 0xFFFF8260)
#define RSpShift		((volatile UWORD *) 0xFFFF8266)
#define ROffset			((volatile UWORD *) 0xFFFF820E)
#define RWrap			((volatile UWORD *) 0xFFFF8210)
#define RSync			((volatile UBYTE *) 0xFFFF820A)
#define RCO				((volatile UWORD *) 0xFFFF82C0)
#define RMode			((volatile UWORD *) 0xFFFF82C2)
#define RHHT			((volatile UWORD *) 0xFFFF8282)
#define RHBB			((volatile UWORD *) 0xFFFF8284)
#define RHBE			((volatile UWORD *) 0xFFFF8286)
#define RHDB			((volatile UWORD *) 0xFFFF8288)
#define RHDE			((volatile UWORD *) 0xFFFF828A)
#define RHSS			((volatile UWORD *) 0xFFFF828C)
#define RHFS			((volatile UWORD *) 0xFFFF828E)
#define RHEE			((volatile UWORD *) 0xFFFF8290)
#define RVFT			((volatile UWORD *) 0xFFFF82A2)
#define RVBB			((volatile UWORD *) 0xFFFF82A4)
#define RVBE			((volatile UWORD *) 0xFFFF82A6)
#define RVDB			((volatile UWORD *) 0xFFFF82A8)
#define RVDE			((volatile UWORD *) 0xFFFF82AA)
#define RVSS			((volatile UWORD *) 0xFFFF82AC)


static __inline UWORD cli(void)
{
	register UWORD save_sr;
	
	__asm__ volatile(
		" move.w %%sr,%[save_sr]\n"
		" ori.w #0x700,%%sr\n"
	: [save_sr]"=d"(save_sr)
	:
	: "cc", "memory");
	return save_sr;
}


static __inline void sti(UWORD save_sr)
{
	__asm__ volatile(
		" move.w %[save_sr],%%sr\n"
	:
	: [save_sr]"d"(save_sr)
	: "cc", "memory");
}


static void videl_re_sync(Videl_Registers *regs)
{
	UWORD patch_code = regs->patch_code;
	unsigned long end;
	unsigned long end_vbl;
	
	if (patch_code & STMODES)
		return;
	if (patch_code == (UWORD)-1)
		return;
	if ((patch_code & 0x07) == BPS2)
		return;
	
	/* Reset Videl for re-sync */
	*RSpShift = 0;
	
	/* Wait for at least 1 VBlank period */
	end_vbl = *_vbclock + 2;
	end = *_hz_200 + 9;
	do {
		__asm__ volatile("nop");
	} while (*_vbclock != end_vbl && *_hz_200 != end);
	
	/* Restore Videl mode */
	*RSpShift = regs->patch_RSpShift;
}


/*-------------------------------------------------------*/
/*      Load Videl registers                             */
/*-------------------------------------------------------*/
void load_r(void)
{
	Videl_Registers *regs;
	unsigned long end;
	UWORD sr;
	
	regs = p_str_p;
	
	/* Allow previous VBlank changes to settle */
	end = *_hz_200 + 5;
	while (*_hz_200 != end)
	{
		__asm__ volatile("nop");
	}
	
	/* Reset Videl for new register file */
	*RSpShift = 0;
	
	/* Lock exceptions */
	sr = cli();
	
	/* Load shift mode */
	if (regs->patch_depth == BPS4)
		*RShift = regs->patch_RShift;
	else
		*RSpShift = regs->patch_RSpShift;
	
	/* Load line offset+wrap */
	*ROffset = regs->patch_ROffset;
	*RWrap = regs->patch_RWrap;
	
	/* Load sync */
	*RSync = regs->patch_RSync;
	
	/* Load clock */
	*RCO = regs->patch_RCO;
	
	/* Load mode */
	*RMode = regs->patch_RMode;
	
	/* Horizontal register set */
	*RHHT = regs->patch_RHHT;
	*RHBB = regs->patch_RHBB;
	*RHBE = regs->patch_RHBE;
	*RHDB = regs->patch_RHDB;
	*RHDE = regs->patch_RHDE;
	*RHSS = regs->patch_RHSS;
	*RHFS = regs->patch_RHFS;
	*RHEE = regs->patch_RHEE;
	
	/* Vertical register set */
	*RVFT = regs->patch_RVFT;
	*RVBB = regs->patch_RVBB;
	*RVBE = regs->patch_RVBE;
	*RVDB = regs->patch_RVDB;
	*RVDE = regs->patch_RVDE;
	*RVSS = regs->patch_RVSS;
	
	/* Restore exceptions */
	sti(sr);
	
	/* Re-synchronize display for new settings */
	videl_re_sync(regs);
}

/*-------------------------------------------------------*/
/*      Save Videl registers                             */
/*-------------------------------------------------------*/
void save_r(void)
{
	Videl_Registers *regs;
	UWORD modecode;
	UWORD sr;
	
	regs = p_str_p;

	/* Get Modecode */
	modecode = VsetMode(-1);
	regs->patch_code = modecode;
	regs->patch_depth = modecode & 0x07;
	
	/* Lock exceptions */
	sr = cli();
	
	/* Save shift mode */
	regs->patch_RShift = *RShift;
	regs->patch_RSpShift = *RSpShift;

	/* Save line offset+wrap */
	regs->patch_ROffset = *ROffset;
	regs->patch_RWrap = *RWrap;
	
	/* Save sync */
	regs->patch_RSync = *RSync;
	
	/* Save clock */
	regs->patch_RCO = *RCO;
	
	/* Save mode */
	regs->patch_RMode = *RMode;
	
	/* Horizontal register set */
	regs->patch_RHHT = *RHHT;
	regs->patch_RHBB = *RHBB;
	regs->patch_RHBE = *RHBE;
	regs->patch_RHDB = *RHDB;
	regs->patch_RHDE = *RHDE;
	regs->patch_RHSS = *RHSS;
	regs->patch_RHFS = *RHFS;
	regs->patch_RHEE = *RHEE;
	
	/* Vertical register set */
	regs->patch_RVFT = *RVFT;
	regs->patch_RVBB = *RVBB;
	regs->patch_RVBE = *RVBE;
	regs->patch_RVDB = *RVDB;
	regs->patch_RVDE = *RVDE;
	regs->patch_RVSS = *RVSS;
	
	/* Restore exceptions */
	sti(sr);
}

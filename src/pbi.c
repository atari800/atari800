/*
 * pbi.c - Parallel bus emulation
 *
 * Copyright (C) 2002 Jason Duerstock <jason@cluephone.com>
 * Copyright (C) 2002-2003 Atari800 development team (see DOC/CREDITS)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "atari.h"

void PBI_Initialise(int *argc, char *argv[])
{
}

UBYTE PBI_GetByte(UWORD addr)
{
	return 0;
}

void PBI_PutByte(UWORD addr, UBYTE byte)
{
}

UBYTE PBIM1_GetByte(UWORD addr)
{
	return 0;
}

void PBIM1_PutByte(UWORD addr, UBYTE byte)
{
}

UBYTE PBIM2_GetByte(UWORD addr)
{
	return 0;
}

void PBIM2_PutByte(UWORD addr, UBYTE byte)
{
}

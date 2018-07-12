/*
 * videl.h - Atari Falcon specific port code
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

/*-------------------------------------------------------*/
/*      Videl register file                              */
/*-------------------------------------------------------*/

typedef struct {
	UWORD patch_code;			/* fake modecode (describes register file) */

	ULONG patch_size;			/* total display memory */
	UWORD patch_width;			/* horizontal res */
	UWORD patch_height;			/* vertical res */
	UWORD patch_depth;			/* colour depth (bits per pixel) */

	UBYTE patch_RShift;			/* register file */
	UBYTE patch_RSync;
	UWORD patch_RSpShift;
	UWORD patch_ROffset;
	UWORD patch_RWrap;
	UWORD patch_RCO;
	UWORD patch_RMode;
	UWORD patch_RHHT;
	UWORD patch_RHBB;
	UWORD patch_RHBE;
	UWORD patch_RHDB;
	UWORD patch_RHDE;
	UWORD patch_RHSS;
	UWORD patch_RHFS;
	UWORD patch_RHEE;
	UWORD patch_RVFT;
	UWORD patch_RVBB;
	UWORD patch_RVBE;
	UWORD patch_RVDB;
	UWORD patch_RVDE;
	UWORD patch_RVSS;
} Videl_Registers;

extern Videl_Registers *p_str_p;

void load_r(void);
void save_r(void);

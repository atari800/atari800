/* Emulate Parallel Bus Interface
   Copyright 2002 Jason Duerstock <jason@cluephone.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef __PBI__
#define __PBI__

#include "atari.h"

void PBI_Initialise(int *argc, char *argv[]);
UBYTE PBI_GetByte(UWORD addr);
void PBI_PutByte(UWORD addr, UBYTE byte);
UBYTE PBIM1_GetByte(UWORD addr);
void PBIM1_PutByte(UWORD addr, UBYTE byte);
UBYTE PBIM2_GetByte(UWORD addr);
void PBIM2_PutByte(UWORD addr, UBYTE byte);

#endif

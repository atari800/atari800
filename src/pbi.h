#ifndef _PBI_H_
#define _PBI_H_

#include "atari.h"

void PBI_Initialise(int *argc, char *argv[]);
UBYTE PBI_GetByte(UWORD addr);
void PBI_PutByte(UWORD addr, UBYTE byte);
UBYTE PBIM1_GetByte(UWORD addr);
void PBIM1_PutByte(UWORD addr, UBYTE byte);
UBYTE PBIM2_GetByte(UWORD addr);
void PBIM2_PutByte(UWORD addr, UBYTE byte);

#endif /* _PBI_H_ */

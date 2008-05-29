#ifndef _PBI_PROTO80_H_
#define _PBI_PROTO80_H_

#include "atari.h"
void PBI_PROTO80_Initialise(int *argc, char *argv[]);
int PBI_PROTO80_ReadConfig(char *string, char *ptr);
void PBI_PROTO80_WriteConfig(FILE *fp);
int PBI_PROTO80_D1_GetByte(UWORD addr);
void PBI_PROTO80_D1_PutByte(UWORD addr, UBYTE byte);
int PBI_PROTO80_D1FF_PutByte(UBYTE byte);
UBYTE PBI_PROTO80_Pixels(int scanline, int column);
extern int PBI_PROTO80_enabled;

#endif /* _PBI_PROTO80_H_ */

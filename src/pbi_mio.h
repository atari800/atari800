#ifndef _PBI_MIO_H_
#define _PBI_MIO_H_

#include "atari.h"
#include <stdio.h>

extern int PBI_MIO_enabled;

void PBI_MIO_Initialise(int *argc, char *argv[]);
UBYTE PBI_MIO_D1_GetByte(UWORD addr);
void PBI_MIO_D1_PutByte(UWORD addr, UBYTE byte);
UBYTE PBI_MIO_D6_GetByte(UWORD addr);
void PBI_MIO_D6_PutByte(UWORD addr, UBYTE byte);
int PBI_MIO_ReadConfig(char *string, char *ptr);
void PBI_MIO_WriteConfig(FILE *fp);

#endif /* _PBI_MIO_H_ */

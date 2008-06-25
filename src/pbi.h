#ifndef _PBI_H_
#define _PBI_H_

#include "atari.h"
#include <stdio.h>

void PBI_Initialise(int *argc, char *argv[]);
int PBI_ReadConfig(char *string, char *ptr);
void PBI_WriteConfig(FILE *fp);
void PBI_Reset(void);
UBYTE PBI_D1_GetByte(UWORD addr);
void PBI_D1_PutByte(UWORD addr, UBYTE byte);
UBYTE PBI_D6_GetByte(UWORD addr);
void PBI_D6_PutByte(UWORD addr, UBYTE byte);
UBYTE PBI_D7_GetByte(UWORD addr);
void PBI_D7_PutByte(UWORD addr, UBYTE byte);
extern int PBI_IRQ;
extern int PBI_D6D7ram;
void PBIStateSave(void);
void PBIStateRead(void);
#define PBI_NOT_HANDLED -1
/* #define PBI_DEBUG */
#endif /* _PBI_H_ */

#ifndef _PBI_BB_H_
#define _PBI_BB_H_

#include "atari.h"
#include <stdio.h>

extern int PBI_BB_enabled;
void PBI_BB_Menu(void);
void PBI_BB_Frame(void);
void PBI_BB_Initialise(int *argc, char *argv[]);
UBYTE PBI_BB_D1_GetByte(UWORD addr);
void PBI_BB_D1_PutByte(UWORD addr, UBYTE byte);
UBYTE PBI_BB_D6_GetByte(UWORD addr);
void PBI_BB_D6_PutByte(UWORD addr, UBYTE byte);
int PBI_BB_ReadConfig(char *string, char *ptr);
void PBI_BB_WriteConfig(FILE *fp);
void PBI_BBStateSave(void);
void PBI_BBStateRead(void);

#endif /* _PBI_BB_H_ */

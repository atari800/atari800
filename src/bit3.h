#ifndef BIT3_H_
#define BIT3_H_

#include "atari.h"
#include <stdio.h>

extern int BIT3_palette[2];
int BIT3_Initialise(int *argc, char *argv[]);
void BIT3_Exit(void);
void BIT3_InsertRightCartridge(void);
int BIT3_ReadConfig(char *string, char *ptr);
void BIT3_WriteConfig(FILE *fp);
int BIT3_D5GetByte(UWORD addr, int no_side_effects);
void BIT3_D5PutByte(UWORD addr, UBYTE byte);
int BIT3_D6GetByte(UWORD addr, int no_side_effects);
void BIT3_D6PutByte(UWORD addr, UBYTE byte);
UBYTE BIT3_GetPixels(int scanline, int column, int *colour, int blink);
extern int BIT3_enabled;
void BIT3_Reset(void);

#endif /* BIT3_H_ */

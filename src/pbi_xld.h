#ifndef _PBI_XLD_H_
#define _PBI_XLD_H_

#include "atari.h"
void PBI_XLD_Initialise(int *argc, char *argv[]);
int PBI_XLD_ReadConfig(char *string, char *ptr);
void PBI_XLD_WriteConfig(FILE *fp);
void PBI_XLD_Reset(void);
int PBI_XLD_D1_GetByte(UWORD addr);
UBYTE PBI_XLD_D1FF_GetByte(void);
void PBI_XLD_D1_PutByte(UWORD addr, UBYTE byte);
int PBI_XLD_D1FF_PutByte(UBYTE byte);
extern int PBI_XLD_enabled;
void PBI_XLD_V_Init(int playback_freq, int num_pokeys, int bit16);
void PBI_XLD_V_Frame(void);
void PBI_XLD_V_Process(void *sndbuffer, int sndn);
void PBI_XLDStateSave(void);
void PBI_XLDStateRead(void);

#endif /* _PBI_XLD_H_ */

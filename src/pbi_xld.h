#ifndef PBI_XLD_H_
#define PBI_XLD_H_

#include "atari.h"
void PBI_XLD_Initialise(int *argc, char *argv[]);
int PBI_XLD_ReadConfig(char *string, char *ptr);
void PBI_XLD_WriteConfig(FILE *fp);
void PBI_XLD_Reset(void);
int PBI_XLD_D1GetByte(UWORD addr);
UBYTE PBI_XLD_D1ffGetByte(void);
void PBI_XLD_D1PutByte(UWORD addr, UBYTE byte);
int PBI_XLD_D1ffPutByte(UBYTE byte);
extern int PBI_XLD_enabled;
void PBI_XLD_VInit(int playback_freq, int num_pokeys, int bit16);
void PBI_XLD_VFrame(void);
void PBI_XLD_VProcess(void *sndbuffer, int sndn);
void PBI_XLD_StateSave(void);
void PBI_XLD_StateRead(void);

#endif /* PBI_XLD_H_ */

#include "atari.h"

extern int rtime_enabled;

void RTIME_Initialise(int *argc, char *argv[]);
UBYTE RTIME_GetByte(void);
void RTIME_PutByte(UBYTE byte);

#ifndef __PIA__
#define __PIA__

#include "atari.h"

#define _PORTA 0x00
#define _PORTB 0x01
#define _PACTL 0x02
#define _PBCTL 0x03

extern UBYTE PACTL;
extern UBYTE PBCTL;
extern UBYTE PORTA;
extern UBYTE PORTB;

extern UBYTE PORT_input[2];

extern int xe_bank;
extern int selftest_enabled;

extern UBYTE atari_basic[8192];
extern UBYTE atari_os[16384];

void PIA_Initialise(int *argc, char *argv[]);
UBYTE PIA_GetByte(UWORD addr);
void PIA_PutByte(UWORD addr, UBYTE byte);

#endif

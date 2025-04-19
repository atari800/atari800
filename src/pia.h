#ifndef PIA_H_
#define PIA_H_

#include "atari.h"

#define PIA_OFFSET_PORTA 0x00
#define PIA_OFFSET_PORTB 0x01
#define PIA_OFFSET_PACTL 0x02
#define PIA_OFFSET_PBCTL 0x03

extern UBYTE PIA_PACTL;
extern UBYTE PIA_PBCTL;
extern UBYTE PIA_PORTA;
extern UBYTE PIA_PORTB;
extern UBYTE PIA_PORTA_mask;
extern UBYTE PIA_PORTB_mask;
extern UBYTE PIA_PORT_input[2];
/* PROCEED/INTERRUPT pin support (CA1/CB1) */
extern int PIA_CA1;
extern int PIA_CB1;
extern int PIA_CA2;
extern int PIA_CB2;
extern int PIA_IRQ;

int PIA_Initialise(int *argc, char *argv[]);
void PIA_Reset(void);
UBYTE PIA_GetByte(UWORD addr, int no_side_effects);
void PIA_PutByte(UWORD addr, UBYTE byte);
void PIA_StateSave(void);
void PIA_StateRead(UBYTE version);
/* Set PROCEED (CA1) and INTERRUPT (CB1) pin values */
void PIA_SetCA1(int value);
void PIA_SetCB1(int value);
static void update_PIA_IRQ(void);

#endif /* PIA_H_ */

#ifndef __POKEY__
#define __POKEY__

#include "atari.h"

#define _AUDF1 0x00
#define _AUDC1 0x01
#define _AUDF2 0x02
#define _AUDC2 0x03
#define _AUDF3 0x04
#define _AUDC3 0x05
#define _AUDF4 0x06
#define _AUDC4 0x07
#define _AUDCTL 0x08
#define _STIMER 0x09
#define _SKRES 0x0a
#define _POTGO 0x0b
#define _SEROUT 0x0d
#define _IRQEN 0x0e
#define _SKCTLS 0x0f

#define _POT0 0x00
#define _POT1 0x01
#define _POT2 0x02
#define _POT3 0x03
#define _POT4 0x04
#define _POT5 0x05
#define _POT6 0x06
#define _POT7 0x07
#define _ALLPOT 0x08
#define _KBCODE 0x09
#define _RANDOM 0x0a
#define _SERIN 0x0d
#define _IRQST 0x0e
#define _SKSTAT 0x0f

extern UBYTE KBCODE;
extern UBYTE IRQST;
extern UBYTE IRQEN;
extern int DELAYED_SERIN_IRQ;
extern int DELAYED_SEROUT_IRQ;
extern int DELAYED_XMTDONE_IRQ;

UBYTE POKEY_GetByte(UWORD addr);
void POKEY_PutByte(UWORD addr, UBYTE byte);
void POKEY_Initialise(int *argc, char *argv[]);
void POKEY_Scanline(void);

/* borrowed from Thomas' version */
/* channel definitions */
#define CHAN1       0
#define CHAN2       1
#define CHAN3       2
#define CHAN4       3
#define SAMPLE      4

extern UBYTE AUDF[8];    /* AUDFx (D200, D202, D204, D206) */
extern UBYTE AUDC[8];    /* AUDCx (D201, D203, D205, D207) */
extern UBYTE AUDCTL;     /* AUDCTL (D208) */      
extern int DivNIRQ[4],DivNMax[4];

#define DIV_64      28			/* divisor for 1.79MHz clock to 64 kHz */
#define DIV_15      114			/* divisor for 1.79MHz clock to 15 kHz */
#define POLY9       0x80		/* selects POLY9 or POLY17 */
#define CH1_179     0x40		/* selects 1.78979 MHz for Ch 1 */
#define CH3_179     0x20		/* selects 1.78979 MHz for Ch 3 */
#define CH1_CH2     0x10		/* clocks channel 1 w/channel 2 */
#define CH3_CH4     0x08		/* clocks channel 3 w/channel 4 */
#define CH1_FILTER  0x04		/* selects channel 1 high pass filter */
#define CH2_FILTER  0x02		/* selects channel 2 high pass filter */
#define CLOCK_15    0x01		/* selects 15.6999kHz or 63.9210kHz */

#endif

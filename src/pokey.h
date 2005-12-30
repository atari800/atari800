#ifndef _POKEY_H_
#define _POKEY_H_

#ifdef ASAP /* external project, see http://asap.sf.net */
#include "asap_internal.h"
#else
#include "atari.h"
#endif

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

#define _POKEY2 0x10			/* offset to second pokey chip (STEREO expansion) */

#ifndef ASAP

extern UBYTE KBCODE;
extern UBYTE IRQST;
extern UBYTE IRQEN;
extern UBYTE SKSTAT;
extern int DELAYED_SERIN_IRQ;
extern int DELAYED_SEROUT_IRQ;
extern int DELAYED_XMTDONE_IRQ;

extern UBYTE POT_input[8];

ULONG POKEY_GetRandomCounter(void);
void POKEY_SetRandomCounter(ULONG value);
UBYTE POKEY_GetByte(UWORD addr);
void POKEY_PutByte(UWORD addr, UBYTE byte);
void POKEY_Initialise(int *argc, char *argv[]);
void POKEY_Frame(void);
void POKEY_Scanline(void);

#endif

/* CONSTANT DEFINITIONS */

/* definitions for AUDCx (D201, D203, D205, D207) */
#define NOTPOLY5    0x80		/* selects POLY5 or direct CLOCK */
#define POLY4       0x40		/* selects POLY4 or POLY17 */
#define PURETONE    0x20		/* selects POLY4/17 or PURE tone */
#define VOL_ONLY    0x10		/* selects VOLUME OUTPUT ONLY */
#define VOLUME_MASK 0x0f		/* volume mask */

/* definitions for AUDCTL (D208) */
#define POLY9       0x80		/* selects POLY9 or POLY17 */
#define CH1_179     0x40		/* selects 1.78979 MHz for Ch 1 */
#define CH3_179     0x20		/* selects 1.78979 MHz for Ch 3 */
#define CH1_CH2     0x10		/* clocks channel 1 w/channel 2 */
#define CH3_CH4     0x08		/* clocks channel 3 w/channel 4 */
#define CH1_FILTER  0x04		/* selects channel 1 high pass filter */
#define CH2_FILTER  0x02		/* selects channel 2 high pass filter */
#define CLOCK_15    0x01		/* selects 15.6999kHz or 63.9210kHz */

/* for accuracy, the 64kHz and 15kHz clocks are exact divisions of
   the 1.79MHz clock */
#define DIV_64      28			/* divisor for 1.79MHz clock to 64 kHz */
#define DIV_15      114			/* divisor for 1.79MHz clock to 15 kHz */

/* the size (in entries) of the 4 polynomial tables */
#define POLY4_SIZE  0x000f
#define POLY5_SIZE  0x001f
#define POLY9_SIZE  0x01ff
#define POLY17_SIZE 0x0001ffff

#define MAXPOKEYS         2		/* max number of emulated chips */

/* channel/chip definitions */
#define CHAN1       0
#define CHAN2       1
#define CHAN3       2
#define CHAN4       3
#define CHIP1       0
#define CHIP2       4
#define CHIP3       8
#define CHIP4      12
#define SAMPLE    127

/* structures to hold the 9 pokey control bytes */
extern UBYTE AUDF[4 * MAXPOKEYS];	/* AUDFx (D200, D202, D204, D206) */
extern UBYTE AUDC[4 * MAXPOKEYS];	/* AUDCx (D201, D203, D205, D207) */
extern UBYTE AUDCTL[MAXPOKEYS];		/* AUDCTL (D208) */

extern int DivNIRQ[4], DivNMax[4];
extern int Base_mult[MAXPOKEYS];	/* selects either 64Khz or 15Khz clock mult */

extern UBYTE poly9_lookup[POLY9_SIZE];
extern UBYTE poly17_lookup[16385];

#endif

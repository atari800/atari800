#ifndef _GTIA_H_
#define _GTIA_H_

#include "atari.h"

#define _HPOSP0 0x00
#define _M0PF 0x00
#define _HPOSP1 0x01
#define _M1PF 0x01
#define _HPOSP2 0x02
#define _M2PF 0x02
#define _HPOSP3 0x03
#define _M3PF 0x03
#define _HPOSM0 0x04
#define _P0PF 0x04
#define _HPOSM1 0x05
#define _P1PF 0x05
#define _HPOSM2 0x06
#define _P2PF 0x06
#define _HPOSM3 0x07
#define _P3PF 0x07
#define _SIZEP0 0x08
#define _M0PL 0x08
#define _SIZEP1 0x09
#define _M1PL 0x09
#define _SIZEP2 0x0a
#define _M2PL 0x0a
#define _SIZEP3 0x0b
#define _M3PL 0x0b
#define _SIZEM 0x0c
#define _P0PL 0x0c
#define _GRAFP0 0x0d
#define _P1PL 0x0d
#define _GRAFP1 0x0e
#define _P2PL 0x0e
#define _GRAFP2 0x0f
#define _P3PL 0x0f
#define _GRAFP3 0x10
#define _TRIG0 0x10
#define _GRAFM 0x11
#define _TRIG1 0x11
#define _COLPM0 0x12
#define _TRIG2 0x12
#define _COLPM1 0x13
#define _TRIG3 0x13
#define _COLPM2 0x14
#define _PAL 0x14
#define _COLPM3 0x15
#define _COLPF0 0x16
#define _COLPF1 0x17
#define _COLPF2 0x18
#define _COLPF3 0x19
#define _COLBK 0x1a
#define _PRIOR 0x1b
#define _VDELAY 0x1c
#define _GRACTL 0x1d
#define _HITCLR 0x1e
#define _CONSOL 0x1f

extern UBYTE GRAFM;
extern UBYTE GRAFP0;
extern UBYTE GRAFP1;
extern UBYTE GRAFP2;
extern UBYTE GRAFP3;
extern UBYTE HPOSP0;
extern UBYTE HPOSP1;
extern UBYTE HPOSP2;
extern UBYTE HPOSP3;
extern UBYTE HPOSM0;
extern UBYTE HPOSM1;
extern UBYTE HPOSM2;
extern UBYTE HPOSM3;
extern UBYTE SIZEP0;
extern UBYTE SIZEP1;
extern UBYTE SIZEP2;
extern UBYTE SIZEP3;
extern UBYTE SIZEM;
extern UBYTE COLPM0;
extern UBYTE COLPM1;
extern UBYTE COLPM2;
extern UBYTE COLPM3;
extern UBYTE COLPF0;
extern UBYTE COLPF1;
extern UBYTE COLPF2;
extern UBYTE COLPF3;
extern UBYTE COLBK;
extern UBYTE GRACTL;
extern UBYTE M0PL;
extern UBYTE M1PL;
extern UBYTE M2PL;
extern UBYTE M3PL;
extern UBYTE P0PL;
extern UBYTE P1PL;
extern UBYTE P2PL;
extern UBYTE P3PL;
extern UBYTE PRIOR;
extern UBYTE VDELAY;

#ifdef USE_COLOUR_TRANSLATION_TABLE

extern UWORD colour_translation_table[256];
#define COLOUR_BLACK colour_translation_table[0]
#define COLOUR_TO_WORD(dest,src) dest = colour_translation_table[src];

#else

#define COLOUR_BLACK 0
#define COLOUR_TO_WORD(dest,src) dest = (((UWORD) (src)) << 8) | (src);

#endif /* USE_COLOUR_TRANSLATION_TABLE */

extern UBYTE collisions_mask_missile_playfield;
extern UBYTE collisions_mask_player_playfield;
extern UBYTE collisions_mask_missile_player;
extern UBYTE collisions_mask_player_player;

extern UBYTE TRIG[4];
extern UBYTE TRIG_latch[4];

extern int consol_index;
extern UBYTE consol_table[3];

void GTIA_Initialise(int *argc, char *argv[]);
void GTIA_Frame(void);
void new_pm_scanline(void);
UBYTE GTIA_GetByte(UWORD addr);
void GTIA_PutByte(UWORD addr, UBYTE byte);

#ifdef NEW_CYCLE_EXACT
void update_pmpl_colls(void);
#endif
#endif /* _GTIA_H_ */

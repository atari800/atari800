/* GTIA emulation --------------------------------- */
/* Original Author:                                 */
/*              David Firth                         */
/* Clean ups and optimizations:                     */
/*              Piotr Fusik <pfusik@elka.pw.edu.pl> */
/* Last changes: 27th July 2000                     */
/* ------------------------------------------------ */

#include <string.h>

#include "antic.h"
#include "config.h"
#include "gtia.h"
#include "platform.h"
#include "statesav.h"

#ifndef NO_CONSOL_SOUND
void Update_consol_sound(int set);
#endif

/* GTIA Registers ---------------------------------------------------------- */

UBYTE M0PL;
UBYTE M1PL;
UBYTE M2PL;
UBYTE M3PL;
UBYTE P0PL;
UBYTE P1PL;
UBYTE P2PL;
UBYTE P3PL;
UBYTE HPOSP0;
UBYTE HPOSP1;
UBYTE HPOSP2;
UBYTE HPOSP3;
UBYTE HPOSM0;
UBYTE HPOSM1;
UBYTE HPOSM2;
UBYTE HPOSM3;
UBYTE SIZEP0;
UBYTE SIZEP1;
UBYTE SIZEP2;
UBYTE SIZEP3;
UBYTE SIZEM;
UBYTE GRAFP0;
UBYTE GRAFP1;
UBYTE GRAFP2;
UBYTE GRAFP3;
UBYTE GRAFM;
UBYTE COLPM0;
UBYTE COLPM1;
UBYTE COLPM2;
UBYTE COLPM3;
UBYTE COLPF0;
UBYTE COLPF1;
UBYTE COLPF2;
UBYTE COLPF3;
UBYTE COLBK;
UBYTE PRIOR;
UBYTE VDELAY;
UBYTE GRACTL;

/* Internal GTIA state ----------------------------------------------------- */

int atari_speaker;
int next_console_value = 7;			/* for 'hold OPTION during reboot' */
UBYTE consol_mask;
static UBYTE TRIG[4];
UBYTE TRIG_latch[4];
int TRIG_auto[4];		/* autofire */
extern int nframes;		/* base autofire speed on frame count */
extern int rom_inserted;
extern int mach_xlxe;
void set_prior(UBYTE byte);			/* in antic.c */

/* Player/Missile stuff ---------------------------------------------------- */

extern UBYTE player_dma_enabled;
extern UBYTE missile_dma_enabled;
extern UBYTE player_gra_enabled;
extern UBYTE missile_gra_enabled;
extern UBYTE player_flickering;
extern UBYTE missile_flickering;

static UBYTE *hposp_ptr[4];
static UBYTE *hposm_ptr[4];
static UBYTE hposp_skip[4];

static ULONG grafp_lookup[4][256];
static ULONG *grafp_ptr[4];
static int global_sizem[4];

static UBYTE PM_Width[4] = {1, 2, 1, 4};

/* Meaning of bits in pm_scanline:
bit 0 - Player 0
bit 1 - Player 1
bit 2 - Player 2
bit 3 - Player 3
bit 4 - Missile 0
bit 5 - Missile 1
bit 6 - Missile 2
bit 7 - Missile 3
*/

UBYTE pm_scanline[ATARI_WIDTH / 2 + 8];	/* there's a byte for every *pair* of pixels */
UBYTE pm_dirty = TRUE;

#define C_PM0	0x01
#define C_PM1	0x02
#define C_PM01	0x03
#define C_PM2	0x04
#define C_PM3	0x05
#define C_PM23	0x06
#define C_PM023	0x07
#define C_PM123	0x08
#define C_PM0123 0x09
#define C_PM25	0x0a
#define C_PM35	0x0b
#define C_PM235	0x0c
#define C_COLLS	0x0d
#define C_BAK	0x00
#define C_HI2	0x20
#define C_HI3	0x30
#define C_PF0	0x40
#define C_PF1	0x50
#define C_PF2	0x60
#define C_PF3	0x70

#define PF0PM (*(UBYTE *) &cl_lookup[C_PF0 | C_COLLS])
#define PF1PM (*(UBYTE *) &cl_lookup[C_PF1 | C_COLLS])
#define PF2PM (*(UBYTE *) &cl_lookup[C_PF2 | C_COLLS])
#define PF3PM (*(UBYTE *) &cl_lookup[C_PF3 | C_COLLS])

/* Colours ----------------------------------------------------------------- */

#ifdef USE_COLOUR_TRANSLATION_TABLE

UWORD colour_translation_table[256];

#else

extern UWORD hires_lookup_l[128];

#endif /* USE_COLOUR_TRANSLATION_TABLE */

extern ULONG lookup_gtia9[16];
extern ULONG lookup_gtia11[16];
extern UWORD cl_lookup[128];

void setup_gtia9_11(void) {
	int i;
#ifdef USE_COLOUR_TRANSLATION_TABLE
	UWORD temp;
	temp = colour_translation_table[COLBK & 0xf0];
	lookup_gtia11[0] = ((ULONG)temp << 16) | temp;
	for (i = 1; i < 16; i++) {
		temp = colour_translation_table[COLBK | i];
		lookup_gtia9[i] = ((ULONG)temp << 16) | temp;
		temp = colour_translation_table[COLBK | (i << 4)];
		lookup_gtia11[i] = ((ULONG)temp << 16) | temp;
	}
#else
	ULONG count9 = 0;
	ULONG count11 = 0;
	lookup_gtia11[0] = lookup_gtia9[0] & 0xf0f0f0f0;
	for (i = 1; i < 16; i++) {
		lookup_gtia9[i] = lookup_gtia9[0] | (count9 += 0x01010101);
		lookup_gtia11[i] = lookup_gtia9[0] | (count11 += 0x10101010);
	}
#endif
}

/* Initialization ---------------------------------------------------------- */

void GTIA_Initialise(int *argc, char *argv[])
{
	int i;
	for (i = 0; i < 256; i++) {
		int tmp = i + 0x100;
		ULONG grafp1 = 0;
		ULONG grafp2 = 0;
		ULONG grafp4 = 0;
		do {
			grafp1 <<= 1;
			grafp2 <<= 2;
			grafp4 <<= 4;
			if (tmp & 1) {
				grafp1++;
				grafp2 += 3;
				grafp4 += 15;
			}
			tmp >>= 1;
		} while (tmp != 1);
		grafp_lookup[2][i] = grafp_lookup[0][i] = grafp1;
		grafp_lookup[1][i] = grafp2;
		grafp_lookup[3][i] = grafp4;
	}
	memset(cl_lookup, COLOUR_BLACK, sizeof(cl_lookup));
	for (i = 0; i < 32; i++)
		GTIA_PutByte((UWORD) i, 0);
}

/* Prepare PMG scanline ---------------------------------------------------- */

void new_pm_scanline(void)
{
/* Clear if necessary */
	if (pm_dirty) {
		memset(pm_scanline, 0, ATARI_WIDTH / 2);
		pm_dirty = FALSE;
	}

/* Draw Players */

#define DO_PLAYER(n)	if (GRAFP##n) {						\
	ULONG grafp = grafp_ptr[n][GRAFP##n] >> hposp_skip[n];	\
	if (grafp) {											\
		UBYTE *ptr = hposp_ptr[n];							\
		if (ptr < pm_scanline + ATARI_WIDTH / 2 - 2) {		\
			pm_dirty = TRUE;								\
			do {											\
				if (grafp & 1)								\
					P##n##PL |= *ptr |= 1 << n;				\
				ptr++;										\
				grafp >>= 1;								\
			} while (grafp && ptr < pm_scanline + ATARI_WIDTH / 2 - 2);	\
		}													\
	}														\
}

	DO_PLAYER(0)
	DO_PLAYER(1)
	DO_PLAYER(2)
	DO_PLAYER(3)

/* Draw Missiles */

#define DO_MISSILE(n,p,m,r,l)	if (GRAFM & m) {	\
	int j = global_sizem[n];						\
	UBYTE *ptr = hposm_ptr[n];						\
	if (GRAFM & r) {								\
		if (GRAFM & l)								\
			j <<= 1;								\
	}												\
	else											\
		ptr += j;									\
	if (ptr < pm_scanline + 2) {					\
		j += ptr - pm_scanline - 2;					\
		ptr = pm_scanline + 2;						\
	}												\
	else if (ptr + j > pm_scanline + ATARI_WIDTH / 2 - 2)	\
		j = pm_scanline + ATARI_WIDTH / 2 - 2 - ptr;		\
	if (j > 0)										\
		do											\
			M##n##PL |= *ptr++ |= p;				\
		while (--j);								\
}

	if (GRAFM) {
		pm_dirty = TRUE;
		DO_MISSILE(3,0x80,0xc0,0x80,0x40)
		DO_MISSILE(2,0x40,0x30,0x20,0x10)
		DO_MISSILE(1,0x20,0x0c,0x08,0x04)
		DO_MISSILE(0,0x10,0x03,0x02,0x01)
	}
}

/* GTIA registers ---------------------------------------------------------- */

void GTIA_Triggers(void)
{
	int i;

	for(i = 0; i < 4; i++)
	{
		TRIG[i] = Atari_TRIG(i);
		if ((TRIG_auto[i] && !TRIG[i]) || TRIG_auto[i] == 2)
			TRIG[i] = (nframes & 2) ? 1 : 0;
	}
	TRIG[2] = mach_xlxe ? 1 : TRIG[2];
	TRIG[3] = mach_xlxe ? rom_inserted : TRIG[3];
	if (GRACTL & 4) {
		TRIG_latch[0] &= TRIG[0];
		TRIG_latch[1] &= TRIG[1];
		TRIG_latch[2] &= TRIG[2];
		TRIG_latch[3] &= TRIG[3];
	}
}

UBYTE GTIA_GetByte(UWORD addr)
{
	UBYTE byte = 0x0f;	/* write-only registers return 0x0f */

	switch (addr & 0x1f) {
	case _M0PF:
		byte = (PF0PM & 0x10) >> 4;
		byte |= (PF1PM & 0x10) >> 3;
		byte |= (PF2PM & 0x10) >> 2;
		byte |= (PF3PM & 0x10) >> 1;
		break;
	case _M1PF:
		byte = (PF0PM & 0x20) >> 5;
		byte |= (PF1PM & 0x20) >> 4;
		byte |= (PF2PM & 0x20) >> 3;
		byte |= (PF3PM & 0x20) >> 2;
		break;
	case _M2PF:
		byte = (PF0PM & 0x40) >> 6;
		byte |= (PF1PM & 0x40) >> 5;
		byte |= (PF2PM & 0x40) >> 4;
		byte |= (PF3PM & 0x40) >> 3;
		break;
	case _M3PF:
		byte = (PF0PM & 0x80) >> 7;
		byte |= (PF1PM & 0x80) >> 6;
		byte |= (PF2PM & 0x80) >> 5;
		byte |= (PF3PM & 0x80) >> 4;
		break;
	case _P0PF:
		byte = (PF0PM & 0x01);
		byte |= (PF1PM & 0x01) << 1;
		byte |= (PF2PM & 0x01) << 2;
		byte |= (PF3PM & 0x01) << 3;
		break;
	case _P1PF:
		byte = (PF0PM & 0x02) >> 1;
		byte |= (PF1PM & 0x02);
		byte |= (PF2PM & 0x02) << 1;
		byte |= (PF3PM & 0x02) << 2;
		break;
	case _P2PF:
		byte = (PF0PM & 0x04) >> 2;
		byte |= (PF1PM & 0x04) >> 1;
		byte |= (PF2PM & 0x04);
		byte |= (PF3PM & 0x04) << 1;
		break;
	case _P3PF:
		byte = (PF0PM & 0x08) >> 3;
		byte |= (PF1PM & 0x08) >> 2;
		byte |= (PF2PM & 0x08) >> 1;
		byte |= (PF3PM & 0x08);
		break;
	case _M0PL:
		byte = M0PL & 0x0f;		/* AAA fix for galaxian. easier to do it here. */
		break;
	case _M1PL:
		byte = M1PL & 0x0f;
		break;
	case _M2PL:
		byte = M2PL & 0x0f;
		break;
	case _M3PL:
		byte = M3PL & 0x0f;
		break;
	case _P0PL:
		byte = (P1PL & 0x01) << 1;	/* mask in player 1 */
		byte |= (P2PL & 0x01) << 2;	/* mask in player 2 */
		byte |= (P3PL & 0x01) << 3;	/* mask in player 3 */
		break;
	case _P1PL:
		byte = (P1PL & 0x01);		/* mask in player 0 */
		byte |= (P2PL & 0x02) << 1;	/* mask in player 2 */
		byte |= (P3PL & 0x02) << 2;	/* mask in player 3 */
		break;
	case _P2PL:
		byte = (P2PL & 0x03);		/* mask in player 0 and 1 */
		byte |= (P3PL & 0x04) << 1;	/* mask in player 3 */
		break;
	case _P3PL:
		byte = P3PL & 0x07;			/* mask in player 0,1, and 2 */
		break;
	case _TRIG0:
		byte = TRIG[0] & TRIG_latch[0];
		break;
	case _TRIG1:
		byte = TRIG[1] & TRIG_latch[1];
		break;
	case _TRIG2:
		byte = TRIG[2] & TRIG_latch[2];
		break;
	case _TRIG3:
		byte = TRIG[3] & TRIG_latch[3];
		break;
	case _PAL:
		if (tv_mode == TV_PAL)
			byte = 0x01;
		/* 0x0f is default */
		/* else
			byte = 0x0f; */
		break;
	case _CONSOL:
		if (next_console_value != 7) {
			byte = (next_console_value | 0x08) & consol_mask;
			next_console_value = 0x07;
		}
		else {
			/* 0x08 is because 'speaker is always 'on' '
			consol_mask is set by CONSOL (write) !PM! */
			byte = (Atari_CONSOL() | 0x08) & consol_mask;
		}
		break;
	}

	return byte;
}

void GTIA_PutByte(UWORD addr, UBYTE byte)
{
	UWORD cword;
	UWORD cword2;

	switch (addr & 0x1f) {
#ifdef USE_COLOUR_TRANSLATION_TABLE
	case _COLBK:
		COLBK = byte &= 0xfe;
		cl_lookup[C_BAK] = cword = colour_translation_table[byte];
		if (cword != (UWORD) (lookup_gtia9[0]) ) {
			lookup_gtia9[0] = cword | (cword << 16);
			if (PRIOR & 0x40)
				setup_gtia9_11();
		}
		break;
	case _COLPF0:
		COLPF0 = byte &= 0xfe;
		cl_lookup[C_PF0] = cword = colour_translation_table[byte];
		if ((PRIOR & 1) == 0) {
			cl_lookup[C_PF0 | C_PM23] = cl_lookup[C_PF0 | C_PM3] = cl_lookup[C_PF0 | C_PM2] = cword;
			if ((PRIOR & 3) == 0) {
				if (PRIOR & 0xf) {
					cl_lookup[C_PF0 | C_PM01] = cl_lookup[C_PF0 | C_PM1] = cl_lookup[C_PF0 | C_PM0] = cword;
					if ((PRIOR & 0xf) == 0xc)
						cl_lookup[C_PF0 | C_PM0123] = cl_lookup[C_PF0 | C_PM123] = cl_lookup[C_PF0 | C_PM023] = cword;
				}
				else {
					cl_lookup[C_PF0 | C_PM0] = colour_translation_table[byte | COLPM0];
					cl_lookup[C_PF0 | C_PM1] = colour_translation_table[byte | COLPM1];
					cl_lookup[C_PF0 | C_PM01] = colour_translation_table[byte | COLPM0 | COLPM1];
				}
			}
			if ((PRIOR & 0xf) >= 0xa)
				cl_lookup[C_PF0 | C_PM25] = cword;
		}
		break;
	case _COLPF1:
		COLPF1 = byte &= 0xfe;
		cl_lookup[C_PF1] = cword = colour_translation_table[byte];
		if ((PRIOR & 1) == 0) {
			cl_lookup[C_PF1 | C_PM23] = cl_lookup[C_PF1 | C_PM3] = cl_lookup[C_PF1 | C_PM2] = cword;
			if ((PRIOR & 3) == 0) {
				if (PRIOR & 0xf) {
					cl_lookup[C_PF1 | C_PM01] = cl_lookup[C_PF1 | C_PM1] = cl_lookup[C_PF1 | C_PM0] = cword;
					if ((PRIOR & 0xf) == 0xc)
						cl_lookup[C_PF1 | C_PM0123] = cl_lookup[C_PF1 | C_PM123] = cl_lookup[C_PF1 | C_PM023] = cword;
				}
				else {
					cl_lookup[C_PF1 | C_PM0] = colour_translation_table[byte | COLPM0];
					cl_lookup[C_PF1 | C_PM1] = colour_translation_table[byte | COLPM1];
					cl_lookup[C_PF1 | C_PM01] = colour_translation_table[byte | COLPM0 | COLPM1];
				}
			}
		}
		{
			UBYTE byte2 = (COLPF2 & 0xf0) | (byte & 0xf);
			cl_lookup[C_HI2] = cword = colour_translation_table[byte2];
			cl_lookup[C_HI3] = colour_translation_table[(COLPF3 & 0xf0) | (byte & 0xf)];
			if (PRIOR & 4)
				cl_lookup[C_HI2 | C_PM01] = cl_lookup[C_HI2 | C_PM1] = cl_lookup[C_HI2 | C_PM0] = cword;
			if ((PRIOR & 9) == 0) {
				if (PRIOR & 0xf)
					cl_lookup[C_HI2 | C_PM23] = cl_lookup[C_HI2 | C_PM3] = cl_lookup[C_HI2 | C_PM2] = cword;
				else {
					cl_lookup[C_HI2 | C_PM2] = colour_translation_table[byte2 | (COLPM2 & 0xf0)];
					cl_lookup[C_HI2 | C_PM3] = colour_translation_table[byte2 | (COLPM3 & 0xf0)];
					cl_lookup[C_HI2 | C_PM23] = colour_translation_table[byte2 | ((COLPM2 | COLPM3) & 0xf0)];
				}
			}
		}
		break;
	case _COLPF2:
		COLPF2 = byte &= 0xfe;
		cl_lookup[C_PF2] = cword = colour_translation_table[byte];
		{
			UBYTE byte2 = (byte & 0xf0) | (COLPF1 & 0xf);
			cl_lookup[C_HI2] = cword2 = colour_translation_table[byte2];
			if (PRIOR & 4) {
				cl_lookup[C_PF2 | C_PM01] = cl_lookup[C_PF2 | C_PM1] = cl_lookup[C_PF2 | C_PM0] = cword;
				cl_lookup[C_HI2 | C_PM01] = cl_lookup[C_HI2 | C_PM1] = cl_lookup[C_HI2 | C_PM0] = cword2;
			}
			if ((PRIOR & 9) == 0) {
				if (PRIOR & 0xf) {
					cl_lookup[C_PF2 | C_PM23] = cl_lookup[C_PF2 | C_PM3] = cl_lookup[C_PF2 | C_PM2] = cword;
					cl_lookup[C_HI2 | C_PM23] = cl_lookup[C_HI2 | C_PM3] = cl_lookup[C_HI2 | C_PM2] = cword2;
				}
				else {
					cl_lookup[C_PF2 | C_PM2] = colour_translation_table[byte | COLPM2];
					cl_lookup[C_PF2 | C_PM3] = colour_translation_table[byte | COLPM3];
					cl_lookup[C_PF2 | C_PM23] = colour_translation_table[byte | COLPM2 | COLPM3];
					cl_lookup[C_HI2 | C_PM2] = colour_translation_table[byte2 | (COLPM2 & 0xf0)];
					cl_lookup[C_HI2 | C_PM3] = colour_translation_table[byte2 | (COLPM3 & 0xf0)];
					cl_lookup[C_HI2 | C_PM23] = colour_translation_table[byte2 | ((COLPM2 | COLPM3) & 0xf0)];
				}
			}
		}
		break;
	case _COLPF3:
		COLPF3 = byte &= 0xfe;
		cl_lookup[C_PF3] = cword = colour_translation_table[byte];
		cl_lookup[C_HI3] = cword2 = colour_translation_table[(byte & 0xf0) | (COLPF1 & 0xf)];
		if (PRIOR & 4)
			cl_lookup[C_PF3 | C_PM01] = cl_lookup[C_PF3 | C_PM1] = cl_lookup[C_PF3 | C_PM0] = cword;
		if ((PRIOR & 9) == 0) {
			if (PRIOR & 0xf)
				cl_lookup[C_PF3 | C_PM23] = cl_lookup[C_PF3 | C_PM3] = cl_lookup[C_PF3 | C_PM2] = cword;
			else {
				cl_lookup[C_PF3 | C_PM25] = cl_lookup[C_PF2 | C_PM25] = cl_lookup[C_PM25] = cl_lookup[C_PF3 | C_PM2] = colour_translation_table[byte | COLPM2];
				cl_lookup[C_PF3 | C_PM35] = cl_lookup[C_PF2 | C_PM35] = cl_lookup[C_PM35] = cl_lookup[C_PF3 | C_PM3] = colour_translation_table[byte | COLPM3];
				cl_lookup[C_PF3 | C_PM235] = cl_lookup[C_PF2 | C_PM235] = cl_lookup[C_PM235] = cl_lookup[C_PF3 | C_PM23] = colour_translation_table[byte | COLPM2 | COLPM3];
				cl_lookup[C_PF0 | C_PM235] = cl_lookup[C_PF0 | C_PM35] = cl_lookup[C_PF0 | C_PM25] =
				cl_lookup[C_PF1 | C_PM235] = cl_lookup[C_PF1 | C_PM35] = cl_lookup[C_PF1 | C_PM25] = cword;
			}
		}
		break;
	case _COLPM0:
		COLPM0 = byte &= 0xfe;
		cl_lookup[C_PM023] = cl_lookup[C_PM0] = cword = colour_translation_table[byte];
		{
			UBYTE byte2 = byte | COLPM1;
			cl_lookup[C_PM0123] = cl_lookup[C_PM01] = cword2 = colour_translation_table[byte2];
			if ((PRIOR & 4) == 0) {
				cl_lookup[C_PF2 | C_PM0] = cl_lookup[C_PF3 | C_PM0] = cword;
				cl_lookup[C_PF2 | C_PM01] = cl_lookup[C_PF3 | C_PM01] = cword2;
				cl_lookup[C_HI2 | C_PM0] = colour_translation_table[(byte & 0xf0) | (COLPF1 & 0xf)];
				cl_lookup[C_HI2 | C_PM01] = colour_translation_table[(byte2 & 0xf0) | (COLPF1 & 0xf)];
				if ((PRIOR & 0xc) == 0) {
					if (PRIOR & 3) {
						cl_lookup[C_PF0 | C_PM0] = cl_lookup[C_PF1 | C_PM0] = cword;
						cl_lookup[C_PF0 | C_PM01] = cl_lookup[C_PF1 | C_PM01] = cword2;
					}
					else {
						cl_lookup[C_PF0 | C_PM0] = colour_translation_table[byte | COLPF0];
						cl_lookup[C_PF1 | C_PM0] = colour_translation_table[byte | COLPF1];
						cl_lookup[C_PF0 | C_PM01] = colour_translation_table[byte2 | COLPF0];
						cl_lookup[C_PF1 | C_PM01] = colour_translation_table[byte2 | COLPF1];
					}
				}
			}
		}
		break;
	case _COLPM1:
		COLPM1 = byte &= 0xfe;
		cl_lookup[C_PM123] = cl_lookup[C_PM1] = cword = colour_translation_table[byte];
		{
			UBYTE byte2 = byte | COLPM0;
			cl_lookup[C_PM0123] = cl_lookup[C_PM01] = cword2 = colour_translation_table[byte2];
			if ((PRIOR & 4) == 0) {
				cl_lookup[C_PF2 | C_PM1] = cl_lookup[C_PF3 | C_PM1] = cword;
				cl_lookup[C_PF2 | C_PM01] = cl_lookup[C_PF3 | C_PM01] = cword2;
				cl_lookup[C_HI2 | C_PM1] = colour_translation_table[(byte & 0xf0) | (COLPF1 & 0xf)];
				cl_lookup[C_HI2 | C_PM01] = colour_translation_table[(byte2 & 0xf0) | (COLPF1 & 0xf)];
				if ((PRIOR & 0xc) == 0) {
					if (PRIOR & 3) {
						cl_lookup[C_PF0 | C_PM1] = cl_lookup[C_PF1 | C_PM1] = cword;
						cl_lookup[C_PF0 | C_PM01] = cl_lookup[C_PF1 | C_PM01] = cword2;
					}
					else {
						cl_lookup[C_PF0 | C_PM1] = colour_translation_table[byte | COLPF0];
						cl_lookup[C_PF1 | C_PM1] = colour_translation_table[byte | COLPF1];
						cl_lookup[C_PF0 | C_PM01] = colour_translation_table[byte2 | COLPF0];
						cl_lookup[C_PF1 | C_PM01] = colour_translation_table[byte2 | COLPF1];
					}
				}
			}
		}
		break;
	case _COLPM2:
		COLPM2 = byte &= 0xfe;
		cl_lookup[C_PM2] = cword = colour_translation_table[byte];
		{
			UBYTE byte2 = byte | COLPM3;
			cl_lookup[C_PM23] = cword2 = colour_translation_table[byte2];
			if (PRIOR & 1) {
				cl_lookup[C_PF0 | C_PM2] = cl_lookup[C_PF1 | C_PM2] = cword;
				cl_lookup[C_PF0 | C_PM23] = cl_lookup[C_PF1 | C_PM23] = cword2;
			}
			if ((PRIOR & 6) == 0) {
				if (PRIOR & 9) {
					cl_lookup[C_PF2 | C_PM2] = cl_lookup[C_PF3 | C_PM2] = cword;
					cl_lookup[C_PF2 | C_PM23] = cl_lookup[C_PF3 | C_PM23] = cword2;
					cl_lookup[C_HI2 | C_PM2] = colour_translation_table[(byte & 0xf0) | (COLPF1 & 0xf)];
					cl_lookup[C_HI2 | C_PM23] = colour_translation_table[(byte2 & 0xf0) | (COLPF1 & 0xf)];
				}
				else {
					cl_lookup[C_PF2 | C_PM2] = colour_translation_table[byte | COLPF2];
					cl_lookup[C_PF3 | C_PM25] = cl_lookup[C_PF2 | C_PM25] = cl_lookup[C_PM25] = cl_lookup[C_PF3 | C_PM2] = colour_translation_table[byte | COLPF3];
					cl_lookup[C_PF2 | C_PM23] = colour_translation_table[byte2 | COLPF2];
					cl_lookup[C_PF3 | C_PM235] = cl_lookup[C_PF2 | C_PM235] = cl_lookup[C_PM235] = cl_lookup[C_PF3 | C_PM23] = colour_translation_table[byte2 | COLPF3];
					cl_lookup[C_HI2 | C_PM2] = colour_translation_table[((byte | COLPF2) & 0xf0) | (COLPF1 & 0xf)];
					cl_lookup[C_HI2 | C_PM25] = colour_translation_table[((byte | COLPF3) & 0xf0) | (COLPF1 & 0xf)];
					cl_lookup[C_HI2 | C_PM23] = colour_translation_table[((byte2 | COLPF2) & 0xf0) | (COLPF1 & 0xf)];
					cl_lookup[C_HI2 | C_PM235] = colour_translation_table[((byte2 | COLPF3) & 0xf0) | (COLPF1 & 0xf)];
				}
			}
		}
		break;
	case _COLPM3:
		COLPM3 = byte &= 0xfe;
		cl_lookup[C_PM3] = cword = colour_translation_table[byte];
		{
			UBYTE byte2 = byte | COLPM2;
			cl_lookup[C_PM23] = cword2 = colour_translation_table[byte2];
			if (PRIOR & 1) {
				cl_lookup[C_PF0 | C_PM3] = cl_lookup[C_PF1 | C_PM3] = cword;
				cl_lookup[C_PF0 | C_PM23] = cl_lookup[C_PF1 | C_PM23] = cword2;
			}
			if ((PRIOR & 6) == 0) {
				if (PRIOR & 9) {
					cl_lookup[C_PF2 | C_PM3] = cl_lookup[C_PF3 | C_PM3] = cword;
					cl_lookup[C_PF2 | C_PM23] = cl_lookup[C_PF3 | C_PM23] = cword2;
				}
				else {
					cl_lookup[C_PF2 | C_PM3] = colour_translation_table[byte | COLPF2];
					cl_lookup[C_PF3 | C_PM35] = cl_lookup[C_PF2 | C_PM35] = cl_lookup[C_PM35] = cl_lookup[C_PF3 | C_PM3] = colour_translation_table[byte | COLPF3];
					cl_lookup[C_PF2 | C_PM23] = colour_translation_table[byte2 | COLPF2];
					cl_lookup[C_PF3 | C_PM235] = cl_lookup[C_PF2 | C_PM235] = cl_lookup[C_PM235] = cl_lookup[C_PF3 | C_PM23] = colour_translation_table[byte2 | COLPF3];
					cl_lookup[C_HI2 | C_PM3] = colour_translation_table[((byte | COLPF2) & 0xf0) | (COLPF1 & 0xf)];
					cl_lookup[C_HI2 | C_PM23] = colour_translation_table[((byte2 | COLPF2) & 0xf0) | (COLPF1 & 0xf)];
				}
			}
		}
		break;
#else /* USE_COLOUR_TRANSLATION_TABLE */
	case _COLBK:
		COLBK = byte &= 0xfe;
		COLOUR_TO_WORD(cword,byte);
		cl_lookup[C_BAK] = cword;
		if (cword != (UWORD) (lookup_gtia9[0]) ) {
			lookup_gtia9[0] = cword | (cword << 16);
			if (PRIOR & 0x40)
				setup_gtia9_11();
		}
		break;
	case _COLPF0:
		COLPF0 = byte &= 0xfe;
		COLOUR_TO_WORD(cword,byte);
		cl_lookup[C_PF0] = cword;
		if ((PRIOR & 1) == 0) {
			cl_lookup[C_PF0 | C_PM23] = cl_lookup[C_PF0 | C_PM3] = cl_lookup[C_PF0 | C_PM2] = cword;
			if ((PRIOR & 3) == 0) {
				if (PRIOR & 0xf) {
					cl_lookup[C_PF0 | C_PM01] = cl_lookup[C_PF0 | C_PM1] = cl_lookup[C_PF0 | C_PM0] = cword;
					if ((PRIOR & 0xf) == 0xc)
						cl_lookup[C_PF0 | C_PM0123] = cl_lookup[C_PF0 | C_PM123] = cl_lookup[C_PF0 | C_PM023] = cword;
				}
				else
					cl_lookup[C_PF0 | C_PM01] = (cl_lookup[C_PF0 | C_PM0] = cword | cl_lookup[C_PM0]) | (cl_lookup[C_PF0 | C_PM1] = cword | cl_lookup[C_PM1]);
			}
			if ((PRIOR & 0xf) >= 0xa)
				cl_lookup[C_PF0 | C_PM25] = cword;
		}
		break;
	case _COLPF1:
		COLPF1 = byte &= 0xfe;
		COLOUR_TO_WORD(cword,byte);
		cl_lookup[C_PF1] = cword;
		if ((PRIOR & 1) == 0) {
			cl_lookup[C_PF1 | C_PM23] = cl_lookup[C_PF1 | C_PM3] = cl_lookup[C_PF1 | C_PM2] = cword;
			if ((PRIOR & 3) == 0) {
				if (PRIOR & 0xf) {
					cl_lookup[C_PF1 | C_PM01] = cl_lookup[C_PF1 | C_PM1] = cl_lookup[C_PF1 | C_PM0] = cword;
					if ((PRIOR & 0xf) == 0xc)
						cl_lookup[C_PF1 | C_PM0123] = cl_lookup[C_PF1 | C_PM123] = cl_lookup[C_PF1 | C_PM023] = cword;
				}
				else
					cl_lookup[C_PF1 | C_PM01] = (cl_lookup[C_PF1 | C_PM0] = cword | cl_lookup[C_PM0]) | (cl_lookup[C_PF1 | C_PM1] = cword | cl_lookup[C_PM1]);
			}
		}
		((UBYTE *)hires_lookup_l)[0x80] = ((UBYTE *)hires_lookup_l)[0x41] = (UBYTE)
			(hires_lookup_l[0x60] = cword & 0xf0f);
		break;
	case _COLPF2:
		COLPF2 = byte &= 0xfe;
		COLOUR_TO_WORD(cword,byte);
		cl_lookup[C_PF2] = cword;
		if (PRIOR & 4)
			cl_lookup[C_PF2 | C_PM01] = cl_lookup[C_PF2 | C_PM1] = cl_lookup[C_PF2 | C_PM0] = cword;
		if ((PRIOR & 9) == 0) {
			if (PRIOR & 0xf)
				cl_lookup[C_PF2 | C_PM23] = cl_lookup[C_PF2 | C_PM3] = cl_lookup[C_PF2 | C_PM2] = cword;
			else
				cl_lookup[C_PF2 | C_PM23] = (cl_lookup[C_PF2 | C_PM2] = cword | cl_lookup[C_PM2]) | (cl_lookup[C_PF2 | C_PM3] = cword | cl_lookup[C_PM3]);
		}
		break;
	case _COLPF3:
		COLPF3 = byte &= 0xfe;
		COLOUR_TO_WORD(cword,byte);
		cl_lookup[C_PF3] = cword;
		if (PRIOR & 4)
			cl_lookup[C_PF3 | C_PM01] = cl_lookup[C_PF3 | C_PM1] = cl_lookup[C_PF3 | C_PM0] = cword;
		if ((PRIOR & 9) == 0) {
			if (PRIOR & 0xf)
				cl_lookup[C_PF3 | C_PM23] = cl_lookup[C_PF3 | C_PM3] = cl_lookup[C_PF3 | C_PM2] = cword;
			else {
				cl_lookup[C_PF3 | C_PM25] = cl_lookup[C_PF2 | C_PM25] = cl_lookup[C_PM25] = cl_lookup[C_PF3 | C_PM2] = cword | cl_lookup[C_PM2];
				cl_lookup[C_PF3 | C_PM35] = cl_lookup[C_PF2 | C_PM35] = cl_lookup[C_PM35] = cl_lookup[C_PF3 | C_PM3] = cword | cl_lookup[C_PM3];
				cl_lookup[C_PF3 | C_PM235] = cl_lookup[C_PF2 | C_PM235] = cl_lookup[C_PM235] = cl_lookup[C_PF3 | C_PM23] = cl_lookup[C_PF3 | C_PM2] | cl_lookup[C_PF3 | C_PM3];
				cl_lookup[C_PF0 | C_PM235] = cl_lookup[C_PF0 | C_PM35] = cl_lookup[C_PF0 | C_PM25] =
				cl_lookup[C_PF1 | C_PM235] = cl_lookup[C_PF1 | C_PM35] = cl_lookup[C_PF1 | C_PM25] = cword;
			}
		}
		break;
	case _COLPM0:
		COLPM0 = byte &= 0xfe;
		COLOUR_TO_WORD(cword,byte);
		cl_lookup[C_PM023] = cl_lookup[C_PM0] = cword;
		cl_lookup[C_PM0123] = cl_lookup[C_PM01] = cword2 = cword | cl_lookup[C_PM1];
		if ((PRIOR & 4) == 0) {
			cl_lookup[C_PF2 | C_PM0] = cl_lookup[C_PF3 | C_PM0] = cword;
			cl_lookup[C_PF2 | C_PM01] = cl_lookup[C_PF3 | C_PM01] = cword2;
			if ((PRIOR & 0xc) == 0) {
				if (PRIOR & 3) {
					cl_lookup[C_PF0 | C_PM0] = cl_lookup[C_PF1 | C_PM0] = cword;
					cl_lookup[C_PF0 | C_PM01] = cl_lookup[C_PF1 | C_PM01] = cword2;
				}
				else {
					cl_lookup[C_PF0 | C_PM0] = cword | cl_lookup[C_PF0];
					cl_lookup[C_PF1 | C_PM0] = cword | cl_lookup[C_PF1];
					cl_lookup[C_PF0 | C_PM01] = cword2 | cl_lookup[C_PF0];
					cl_lookup[C_PF1 | C_PM01] = cword2 | cl_lookup[C_PF1];
				}
			}
		}
		break;
	case _COLPM1:
		COLPM1 = byte &= 0xfe;
		COLOUR_TO_WORD(cword,byte);
		cl_lookup[C_PM123] = cl_lookup[C_PM1] = cword;
		cl_lookup[C_PM0123] = cl_lookup[C_PM01] = cword2 = cword | cl_lookup[C_PM0];
		if ((PRIOR & 4) == 0) {
			cl_lookup[C_PF2 | C_PM1] = cl_lookup[C_PF3 | C_PM1] = cword;
			cl_lookup[C_PF2 | C_PM01] = cl_lookup[C_PF3 | C_PM01] = cword2;
			if ((PRIOR & 0xc) == 0) {
				if (PRIOR & 3) {
					cl_lookup[C_PF0 | C_PM1] = cl_lookup[C_PF1 | C_PM1] = cword;
					cl_lookup[C_PF0 | C_PM01] = cl_lookup[C_PF1 | C_PM01] = cword2;
				}
				else {
					cl_lookup[C_PF0 | C_PM1] = cword | cl_lookup[C_PF0];
					cl_lookup[C_PF1 | C_PM1] = cword | cl_lookup[C_PF1];
					cl_lookup[C_PF0 | C_PM01] = cword2 | cl_lookup[C_PF0];
					cl_lookup[C_PF1 | C_PM01] = cword2 | cl_lookup[C_PF1];
				}
			}
		}
		break;
	case _COLPM2:
		COLPM2 = byte &= 0xfe;
		COLOUR_TO_WORD(cword,byte);
		cl_lookup[C_PM2] = cword;
		cl_lookup[C_PM23] = cword2 = cword | cl_lookup[C_PM3];
		if (PRIOR & 1) {
			cl_lookup[C_PF0 | C_PM2] = cl_lookup[C_PF1 | C_PM2] = cword;
			cl_lookup[C_PF0 | C_PM23] = cl_lookup[C_PF1 | C_PM23] = cword2;
		}
		if ((PRIOR & 6) == 0) {
			if (PRIOR & 9) {
				cl_lookup[C_PF2 | C_PM2] = cl_lookup[C_PF3 | C_PM2] = cword;
				cl_lookup[C_PF2 | C_PM23] = cl_lookup[C_PF3 | C_PM23] = cword2;
			}
			else {
				cl_lookup[C_PF2 | C_PM2] = cword | cl_lookup[C_PF2];
				cl_lookup[C_PF3 | C_PM25] = cl_lookup[C_PF2 | C_PM25] = cl_lookup[C_PM25] = cl_lookup[C_PF3 | C_PM2] = cword | cl_lookup[C_PF3];
				cl_lookup[C_PF2 | C_PM23] = cword2 | cl_lookup[C_PF2];
				cl_lookup[C_PF3 | C_PM235] = cl_lookup[C_PF2 | C_PM235] = cl_lookup[C_PM235] = cl_lookup[C_PF3 | C_PM23] = cword2 | cl_lookup[C_PF3];
			}
		}
		break;
	case _COLPM3:
		COLPM3 = byte &= 0xfe;
		COLOUR_TO_WORD(cword,byte);
		cl_lookup[C_PM3] = cword;
		cl_lookup[C_PM23] = cword2 = cword | cl_lookup[C_PM2];
		if (PRIOR & 1) {
			cl_lookup[C_PF0 | C_PM3] = cl_lookup[C_PF1 | C_PM3] = cword;
			cl_lookup[C_PF0 | C_PM23] = cl_lookup[C_PF1 | C_PM23] = cword2;
		}
		if ((PRIOR & 6) == 0) {
			if (PRIOR & 9) {
				cl_lookup[C_PF2 | C_PM3] = cl_lookup[C_PF3 | C_PM3] = cword;
				cl_lookup[C_PF2 | C_PM23] = cl_lookup[C_PF3 | C_PM23] = cword2;
			}
			else {
				cl_lookup[C_PF2 | C_PM3] = cword | cl_lookup[C_PF2];
				cl_lookup[C_PF3 | C_PM35] = cl_lookup[C_PF2 | C_PM35] = cl_lookup[C_PM35] = cl_lookup[C_PF3 | C_PM3] = cword | cl_lookup[C_PF3];
				cl_lookup[C_PF2 | C_PM23] = cword2 | cl_lookup[C_PF2];
				cl_lookup[C_PF3 | C_PM235] = cl_lookup[C_PF2 | C_PM235] = cl_lookup[C_PM235] = cl_lookup[C_PF3 | C_PM23] = cword2 | cl_lookup[C_PF3];
			}
		}
		break;
#endif /* USE_COLOUR_TRANSLATION_TABLE */
	case _CONSOL:
		atari_speaker = !(byte & 0x08);
#ifndef NO_CONSOL_SOUND
		Update_consol_sound(1);
#endif
		consol_mask = (~byte) & 0x0f;
		break;
	case _GRAFM:
		GRAFM = byte;
		break;
	case _GRAFP0:
		GRAFP0 = byte;
		break;
	case _GRAFP1:
		GRAFP1 = byte;
		break;
	case _GRAFP2:
		GRAFP2 = byte;
		break;
	case _GRAFP3:
		GRAFP3 = byte;
		break;
	case _HITCLR:
		M0PL = M1PL = M2PL = M3PL = 0;
		P0PL = P1PL = P2PL = P3PL = 0;
		PF0PM = PF1PM = PF2PM = PF3PM = 0;
		break;
	case _HPOSM0:
		HPOSM0 = byte;
		hposm_ptr[0] = pm_scanline + byte - 0x20;
		break;
	case _HPOSM1:
		HPOSM1 = byte;
		hposm_ptr[1] = pm_scanline + byte - 0x20;
		break;
	case _HPOSM2:
		HPOSM2 = byte;
		hposm_ptr[2] = pm_scanline + byte - 0x20;
		break;
	case _HPOSM3:
		HPOSM3 = byte;
		hposm_ptr[3] = pm_scanline + byte - 0x20;
		break;

#define DO_HPOSP(n)	case _HPOSP##n:							\
	HPOSP##n = byte;										\
	if (byte >= 0x22) {										\
		hposp_ptr[n] = pm_scanline + byte - 0x20;			\
		hposp_skip[n] = 0;									\
	}														\
	else if (byte > 2) {									\
		hposp_ptr[n] = pm_scanline + 2;						\
		hposp_skip[n] = 0x22 - byte;						\
	}														\
	else													\
		hposp_ptr[n] = pm_scanline + ATARI_WIDTH / 2 - 2;	\
	break;

	DO_HPOSP(0)
	DO_HPOSP(1)
	DO_HPOSP(2)
	DO_HPOSP(3)

	case _SIZEM:
		SIZEM = byte;
		global_sizem[0] = PM_Width[byte & 0x03];
		global_sizem[1] = PM_Width[(byte & 0x0c) >> 2];
		global_sizem[2] = PM_Width[(byte & 0x30) >> 4];
		global_sizem[3] = PM_Width[(byte & 0xc0) >> 6];
		break;
	case _SIZEP0:
		SIZEP0 = byte;
		grafp_ptr[0] = grafp_lookup[byte & 3];
		break;
	case _SIZEP1:
		SIZEP1 = byte;
		grafp_ptr[1] = grafp_lookup[byte & 3];
		break;
	case _SIZEP2:
		SIZEP2 = byte;
		grafp_ptr[2] = grafp_lookup[byte & 3];
		break;
	case _SIZEP3:
		SIZEP3 = byte;
		grafp_ptr[3] = grafp_lookup[byte & 3];
		break;
	case _PRIOR:
		set_prior(byte);
		PRIOR = byte;
		if (byte & 0x40)
			setup_gtia9_11();
		break;
	case _VDELAY:
		VDELAY = byte;
		break;
	case _GRACTL:
		GRACTL = byte;
		missile_gra_enabled = (byte & 0x01);
		player_gra_enabled = (byte & 0x02);
		player_flickering = ((player_dma_enabled | player_gra_enabled) == 0x02);
		missile_flickering = ((missile_dma_enabled | missile_gra_enabled) == 0x01);
		if ((byte & 4) == 0)
			TRIG_latch[0] = TRIG_latch[1] = TRIG_latch[2] = TRIG_latch[3] = 1;
		break;
	}
}

/* State ------------------------------------------------------------------- */

void GTIAStateSave( void )
{
	SaveUBYTE( &HPOSP0, 1 );
	SaveUBYTE( &HPOSP1, 1 );
	SaveUBYTE( &HPOSP2, 1 );
	SaveUBYTE( &HPOSP3, 1 );
	SaveUBYTE( &HPOSM0, 1 );
	SaveUBYTE( &HPOSM1, 1 );
	SaveUBYTE( &HPOSM2, 1 );
	SaveUBYTE( &HPOSM3, 1 );
	SaveUBYTE( &PF0PM, 1 );
	SaveUBYTE( &PF1PM, 1 );
	SaveUBYTE( &PF2PM, 1 );
	SaveUBYTE( &PF3PM, 1 );
	SaveUBYTE( &M0PL, 1 );
	SaveUBYTE( &M1PL, 1 );
	SaveUBYTE( &M2PL, 1 );
	SaveUBYTE( &M3PL, 1 );
	SaveUBYTE( &P0PL, 1 );
	SaveUBYTE( &P1PL, 1 );
	SaveUBYTE( &P2PL, 1 );
	SaveUBYTE( &P3PL, 1 );
	SaveUBYTE( &SIZEP0, 1 );
	SaveUBYTE( &SIZEP1, 1 );
	SaveUBYTE( &SIZEP2, 1 );
	SaveUBYTE( &SIZEP3, 1 );
	SaveUBYTE( &SIZEM, 1 );
	SaveUBYTE( &GRAFP0, 1 );
	SaveUBYTE( &GRAFP1, 1 );
	SaveUBYTE( &GRAFP2, 1 );
	SaveUBYTE( &GRAFP3, 1 );
	SaveUBYTE( &GRAFM, 1 );
	SaveUBYTE( &COLPM0, 1 );
	SaveUBYTE( &COLPM1, 1 );
	SaveUBYTE( &COLPM2, 1 );
	SaveUBYTE( &COLPM3, 1 );
	SaveUBYTE( &COLPF0, 1 );
	SaveUBYTE( &COLPF1, 1 );
	SaveUBYTE( &COLPF2, 1 );
	SaveUBYTE( &COLPF3, 1 );
	SaveUBYTE( &COLBK, 1 );
	SaveUBYTE( &PRIOR, 1 );
	SaveUBYTE( &VDELAY, 1 );
	SaveUBYTE( &GRACTL, 1 );

	SaveUBYTE( &consol_mask, 1 );

	SaveINT( &atari_speaker, 1 );
	SaveINT( &next_console_value, 1 );
}

void GTIAStateRead( void )
{
	ReadUBYTE( &HPOSP0, 1 );
	ReadUBYTE( &HPOSP1, 1 );
	ReadUBYTE( &HPOSP2, 1 );
	ReadUBYTE( &HPOSP3, 1 );
	ReadUBYTE( &HPOSM0, 1 );
	ReadUBYTE( &HPOSM1, 1 );
	ReadUBYTE( &HPOSM2, 1 );
	ReadUBYTE( &HPOSM3, 1 );
	ReadUBYTE( &PF0PM, 1 );
	ReadUBYTE( &PF1PM, 1 );
	ReadUBYTE( &PF2PM, 1 );
	ReadUBYTE( &PF3PM, 1 );
	ReadUBYTE( &M0PL, 1 );
	ReadUBYTE( &M1PL, 1 );
	ReadUBYTE( &M2PL, 1 );
	ReadUBYTE( &M3PL, 1 );
	ReadUBYTE( &P0PL, 1 );
	ReadUBYTE( &P1PL, 1 );
	ReadUBYTE( &P2PL, 1 );
	ReadUBYTE( &P3PL, 1 );
	ReadUBYTE( &SIZEP0, 1 );
	ReadUBYTE( &SIZEP1, 1 );
	ReadUBYTE( &SIZEP2, 1 );
	ReadUBYTE( &SIZEP3, 1 );
	ReadUBYTE( &SIZEM, 1 );
	ReadUBYTE( &GRAFP0, 1 );
	ReadUBYTE( &GRAFP1, 1 );
	ReadUBYTE( &GRAFP2, 1 );
	ReadUBYTE( &GRAFP3, 1 );
	ReadUBYTE( &GRAFM, 1 );
	ReadUBYTE( &COLPM0, 1 );
	ReadUBYTE( &COLPM1, 1 );
	ReadUBYTE( &COLPM2, 1 );
	ReadUBYTE( &COLPM3, 1 );
	ReadUBYTE( &COLPF0, 1 );
	ReadUBYTE( &COLPF1, 1 );
	ReadUBYTE( &COLPF2, 1 );
	ReadUBYTE( &COLPF3, 1 );
	ReadUBYTE( &COLBK, 1 );
	ReadUBYTE( &PRIOR, 1 );
	ReadUBYTE( &VDELAY, 1 );
	ReadUBYTE( &GRACTL, 1 );

	ReadUBYTE( &consol_mask, 1 );

	ReadINT( &atari_speaker, 1 );
	ReadINT( &next_console_value, 1 );

	GTIA_PutByte(_HPOSP0, HPOSP0);
	GTIA_PutByte(_HPOSP1, HPOSP1);
	GTIA_PutByte(_HPOSP2, HPOSP2);
	GTIA_PutByte(_HPOSP3, HPOSP3);
	GTIA_PutByte(_HPOSM0, HPOSM0);
	GTIA_PutByte(_HPOSM1, HPOSM1);
	GTIA_PutByte(_HPOSM2, HPOSM2);
	GTIA_PutByte(_HPOSM3, HPOSM3);
	GTIA_PutByte(_SIZEP0, SIZEP0);
	GTIA_PutByte(_SIZEP1, SIZEP1);
	GTIA_PutByte(_SIZEP2, SIZEP2);
	GTIA_PutByte(_SIZEP3, SIZEP3);
	GTIA_PutByte(_SIZEM, SIZEM);
	GTIA_PutByte(_GRAFP0, GRAFP0);
	GTIA_PutByte(_GRAFP1, GRAFP1);
	GTIA_PutByte(_GRAFP2, GRAFP2);
	GTIA_PutByte(_GRAFP3, GRAFP3);
	GTIA_PutByte(_GRAFM, GRAFM);
	GTIA_PutByte(_COLPM0, COLPM0);
	GTIA_PutByte(_COLPM1, COLPM1);
	GTIA_PutByte(_COLPM2, COLPM2);
	GTIA_PutByte(_COLPM3, COLPM3);
	GTIA_PutByte(_COLPF0, COLPF0);
	GTIA_PutByte(_COLPF1, COLPF1);
	GTIA_PutByte(_COLPF2, COLPF2);
	GTIA_PutByte(_COLPF3, COLPF3);
	GTIA_PutByte(_COLBK, COLBK);
	GTIA_PutByte(_PRIOR, PRIOR);
	GTIA_PutByte(_GRACTL, GRACTL);
}

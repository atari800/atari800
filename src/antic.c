/* ANTIC emulation -------------------------------- */
/* Original Author:                                 */
/*              David Firth                         */
/* Correct timing, internal memory and other fixes: */
/*              Piotr Fusik <pfusik@elka.pw.edu.pl> */
/* Last changes: 27th September 2001                */
/* ------------------------------------------------ */

#include <string.h>

#define LCHOP 3			/* do not build lefmost 0..3 characters in wide mode */
#define RCHOP 3			/* do not build rightmost 0..3 characters in wide mode */

#include "antic.h"
#include "atari.h"
#include "config.h"
#include "cpu.h"
#include "log.h"
#include "gtia.h"
#include "memory.h"
#include "platform.h"
#include "pokey.h"
#include "rt-config.h"
#include "statesav.h"

/* Memory access helpers----------------------------------------------------- */
/* Some optimizations result in unaligned 32-bit accesses. These macros have
   been introduced for machines that don't allow unaligned memory accesses. */

#ifdef UNALIGNED_LONG_OK
#define IS_ZERO_ULONG(x) (! *(ULONG *)(x))
#define DO_GTIA_BYTE(p,l,x) {\
		((ULONG *)(p))[0] = (l)[(x) >> 4];\
		((ULONG *)(p))[1] = (l)[(x) & 0xf];\
	}
#else
#define IS_ZERO_ULONG(x) (!((UBYTE *)(x))[0] && !((UBYTE *)(x))[1] && !((UBYTE *)(x))[2] && !((UBYTE *)(x))[3])
#define DO_GTIA_BYTE(p,l,x) {\
		((UWORD *)(p))[1] = ((UWORD *)(p))[0] = (UWORD) ((l)[(x) >> 4]);\
		((UWORD *)(p))[3] = ((UWORD *)(p))[2] = (UWORD) ((l)[(x) & 0xf]);\
	}
#endif

/* ANTIC Registers --------------------------------------------------------- */

UBYTE DMACTL;
UBYTE CHACTL;
UWORD dlist;
UBYTE HSCROL;
UBYTE VSCROL;
UBYTE PMBASE;
UBYTE CHBASE;
UBYTE NMIEN;
UBYTE NMIST;

/* ANTIC Memory ------------------------------------------------------------ */

UBYTE ANTIC_memory[52];
#define ANTIC_margin 4
/* It's number of bytes in ANTIC_memory, which are never loaded, but may be
   read in wide playfield mode. These bytes are uninitialized, because on
   real computer there's some kind of 'garbage'. Possibly 1 is enough, but
   4 bytes surely won't cause negative indexes. :) */

/* Screen -----------------------------------------------------------------
   Define screen as ULONG to ensure that it is Longword aligned.
   This allows special optimisations under certain conditions.
   ------------------------------------------------------------------------ */

UWORD *scrn_ptr;

/* ANTIC Timing --------------------------------------------------------------

I've introduced global variable xpos, which contains current number of cycle
in a line. This simplifies ANTIC/CPU timing much. The GO() function which
emulates CPU is now void and is called with xpos limit, below which CPU can go.

All strange variables holding 'unused cycles', 'DMA cycles', 'allocated cycles'
etc. are removed. Simply whenever ANTIC fetches a byte, it takes single cycle,
which can be done now with xpos++. There's only one exception: in text modes
2-5 ANTIC takes more bytes than cycles, because it does less than DMAR refresh
cycles.

Now emulation is really screenline-oriented. We do ypos++ after a line,
not inside it.

This simplified diagram shows when what is done in a line:

MDPPPPDD..............(------R/S/F------)..........
^  ^     ^      ^     ^                     ^    ^ ^        ---> time/xpos
0  |  NMIST_C NMI_C SCR_C                 WSYNC_C|LINE_C
VSCON_C                                        VSCOF_C

M - fetch Missiles
D - fetch DL
P - fetch Players
S - fetch Screen
F - fetch Font (in text modes)
R - refresh Memory (DMAR cycles)

Only Memory Refresh happens in every line, other tasks are optional.

Below are exact diagrams for some non-scrolled modes:
                                                                                                    11111111111111
          11111111112222222222333333333344444444445555555555666666666677777777778888888888999999999900000000001111
012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123
                            /--------------------------narrow------------------------------\
                    /----------------------------------normal--------------------------------------\
            /-------------------------------------------wide--------------------------------------------\

blank line:
MDPPPPDD.................R...R...R...R...R...R...R...R...R........................................................

mode 8,9:
MDPPPPDD....S.......S....R..SR...R..SR...R..SR...R..SR...R..S.......S.......S.......S.......S.......S.............

mode a,b,c:
MDPPPPDD....S...S...S...SR..SR..SR..SR..SR..SR..SR..SR..SR..S...S...S...S...S...S...S...S...S...S...S...S.........

mode d,e,f:
MDPPPPDD....S.S.S.S.S.S.SRS.SRS.SRS.SRS.SRS.SRS.SRS.SRS.SRS.S.S.S.S.S.S.S.S.S.S.S.S.S.S.S.S.S.S.S.S.S.S.S.........

Notes:
* At the beginning of a line fetched are:
  - a byte of Missiles
  - a byte of DL (instruction)
  - four bytes of Players
  - two bytes of DL argument (jump or screen address)
  The emulator, however, fetches them all continuously.

* Refresh cycles and Screen/Font fetches have been tested for some modes (see above).
  This is for making the emulator more accurate, able to change colour registers,
  sprite positions or GTIA modes during scanline. These modes are the most commonly used
  with those effects.
  Currently this isn't implemented, and all R/S/F cycles are fetched continuously in *all* modes
  (however, right number of cycles is taken in every mode, basing on screen width and HSCROL).

There are a few constants representing following events:

* VSCON_C - in first VSC line dctr is loaded with VSCROL

* NMIST_C - NMIST is updated (set to 0x9f on DLI, set to 0x5f on VBLKI)

* NMI_C - If NMIEN permits, NMI interrupt is generated

* SCR_C - We draw whole line of screen. On a real computer you can change
  ANTIC/GTIA registers while displaying screen, however this emulator
  isn't that accurate.

* WSYNC_C - ANTIC holds CPU until this moment, when WSYNC is written

* VSCOF_C - in last VSC line dctr is compared with VSCROL

* LINE_C - simply end of line (this used to be called CPUL)

All constants are determined by tests on real Atari computer. It is assumed,
that ANTIC registers are read with LDA, LDX, LDY and written with STA, STX,
STY, all in absolute addressing mode. All these instructions last 4 cycles
and perform read/write operation in last cycle. The CPU emulation should
correctly emulate WSYNC and add cycles for current instruction BEFORE
executing it. That's why VSCOF_C > LINE_C is correct.

How WSYNC is now implemented:

* On writing WSYNC:
  - if xpos <= WSYNC_C && xpos_limit >= WSYNC_C,
    we only change xpos to WSYNC_C - that's all
  - otherwise we set wsync_halt and change xpos to xpos_limit causing GO()
    to return

* At the beginning of GO() (CPU emulation), when wsync_halt is set:
  - if xpos_limit < WSYNC_C we return
  - else we set xpos to WSYNC_C, reset wsync_halt and emulate some cycles

We don't emulate NMIST_C, NMI_C and SCR_C if it is unnecessary.
These are all cases:

* Common overscreen line
  Nothing happens except that ANTIC gets DMAR cycles:
  xpos += DMAR; GOEOL;

* First overscreen line - start of vertical blank
  - CPU goes until NMIST_C
  - ANTIC sets NMIST to 0x5f
  if (NMIEN & 0x40) {
	  - CPU goes until NMI_C
	  - ANTIC forces NMI
  }
  - ANTIC gets DMAR cycles
  - CPU goes until LINE_C

* Screen line without DLI
  - ANTIC fetches DL and P/MG
  - CPU goes until SCR_C
  - ANTIC draws whole line fetching Screen/Font and refreshing memory
  - CPU goes until LINE_C

* Screen line with DLI
  - ANTIC fetches DL and P/MG
  - CPU goes until NMIST_C
  - ANTIC sets NMIST to 0x9f
  if (NMIEN & 0x80) {
	  - CPU goes until NMI_C
	  - ANTIC forces NMI
  }
  - CPU goes until SCR_C
  - ANTIC draws line with DMAR
  - CPU goes until LINE_C

  -------------------------------------------------------------------------- */

#define VSCON_C	5
#define NMIST_C	10
#define	NMI_C	16
#define	SCR_C	32
#define WSYNC_C	110	/* defined also in atari.h */
#define LINE_C	114	/* defined also in atari.h */
#define VSCOF_C	116

#define DMAR	9	/* defined also in atari.h */

unsigned int screenline_cpu_clock = 0;
#define GOEOL	GO(LINE_C); xpos -= LINE_C; screenline_cpu_clock += LINE_C; ypos++
#define OVERSCREEN_LINE	xpos += DMAR; GOEOL

int xpos = 0;
int xpos_limit;
UBYTE wsync_halt = FALSE;

int ypos;						/* Line number - lines 8..247 are on screen */

/* Timing in first line of modes 2-5
In these modes ANTIC takes more bytes than cycles. Despite this, it would be
possible that SCR_C + cycles_taken > WSYNC_C. To avoid this we must take some
cycles before SCR_C. before_cycles contains number of them, while extra_cycles
contains difference between bytes taken and cycles taken plus before_cycles. */

#define BEFORE_CYCLES (SCR_C - 32)
/* It's number of cycles taken before SCR_C for not scrolled, narrow playfield.
   It wasn't tested, but should be ok. ;) */

/* Internal ANTIC registers ------------------------------------------------ */

static UWORD screenaddr;		/* Screen Pointer */
static UBYTE IR;				/* Instruction Register */
static UBYTE anticmode;			/* Antic mode */
static UBYTE dctr;				/* Delta Counter */
static UBYTE lastline;			/* dctr limit */
static UBYTE need_dl;			/* boolean: fetch DL next line */
static UBYTE vscrol_off;		/* boolean: displaying line ending VSC */

/* Light pen support ------------------------------------------------------- */

int light_pen_enabled = 0;		/* set to non-zero at any time to enable light pen */
static UBYTE PEN_X;
static UBYTE PEN_Y;
static UBYTE light_pen_x;
static UBYTE light_pen_y;
extern UBYTE TRIG_latch[4];	/* in gtia.c */
void GTIA_Triggers(void);		/* in gtia.c */

/* Pre-computed values for improved performance ---------------------------- */

#define NORMAL0 0				/* modes 2,3,4,5,0xd,0xe,0xf */
#define NORMAL1 1				/* modes 6,7,0xa,0xb,0xc */
#define NORMAL2 2				/* modes 8,9 */
#define SCROLL0 3				/* modes 2,3,4,5,0xd,0xe,0xf with HSC */
#define SCROLL1 4				/* modes 6,7,0xa,0xb,0xc with HSC */
#define SCROLL2 5				/* modes 8,9 with HSC */
static int md;					/* current mode NORMAL0..SCROLL2 */
/* tables for modes NORMAL0..SCROLL2 */
static int chars_read[6];
static int chars_displayed[6];
static int x_min[6];
static int ch_offset[6];
static int load_cycles[6];
static int font_cycles[6];
static int before_cycles[6];
static int extra_cycles[6];

/* border parameters for current display width */
static int left_border_chars;
static int right_border_start;

/* set with CHBASE *and* CHACTL - bits 0..2 set if flip on */
static UWORD chbase_20;			/* CHBASE for 20 character mode */

/* set with CHACTL */
static UBYTE invert_mask;
static int blank_mask;

/* lookup tables */
static UBYTE blank_lookup[256];
static UWORD lookup2[256];
ULONG lookup_gtia9[16];
ULONG lookup_gtia11[16];
static UBYTE playfield_lookup[257];

/* Colour lookup table
   This single table replaces 4 previously used: cl_word, cur_prior,
   prior_table and pf_colls. It should be treated as two-dimensional table,
   with playfield colours in rows and PMG colours in columns:
       no_PMG PM0 PM1 PM01 PM2 PM3 PM23 PM023 PM123 PM0123 PM25 PM35 PM235 colls ... ...
   BAK
   ...
   HI2
   HI3
   PF0
   PF1
   PF2
   PF3
   The table contains word value (lsb = msb) of colour to be drawn.
   The table is being updated taking current PRIOR setting into consideration.
   '...' represent two unused columns and single unused row.
   HI2 and HI3 are used only if colour_translation_table is being used.
   They're colours of hi-res pixels on PF2 and PF3 respectively (PF2 is
   default background for hi-res, PF3 is PM5).
   Columns PM023, PM123 and PM0123 are used when PRIOR & 0xf equals any
   of 5,7,0xc,0xd,0xe,0xf. The columns represent PM0, PM1 and PM01 respectively
   covered by PM2 and/or PM3. This is to handle black colour on PF2 and PF3.
   Columns PM25, PM35 and PM235 are used when PRIOR & 0x1f equals any
   of 0x10,0x1a,0x1c,0x1e. The columns represent PM2, PM3 and PM23
   respectively covered by PM5. This to handle colour on PF0 and PF1:
   PF3 if (PRIOR & 0x1f) == 0x10, PF0 or PF1 otherwise.
   Additional column 'colls' holds collisions of playfields with PMG. */

UWORD cl_lookup[128];

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
#define C_BLACK	(C_PF3 | C_PM25)

/* these are byte-offsets in the table, so left shift for indexing word table
   has been avoided */
#define COLOUR(x) (*(UWORD *) ((UBYTE *) cl_lookup + (x) ))
#define L_PM0	(2 * C_PM0)
#define L_PM1	(2 * C_PM1)
#define L_PM01	(2 * C_PM01)
#define L_PM2	(2 * C_PM2)
#define L_PM3	(2 * C_PM3)
#define L_PM23	(2 * C_PM23)
#define L_PM023	(2 * C_PM023)
#define L_PM123	(2 * C_PM123)
#define L_PM0123 (2 * C_PM0123)
#define L_PM25	(2 * C_PM25)
#define L_PM35	(2 * C_PM35)
#define L_PM235	(2 * C_PM235)
#define L_COLLS	(2 * C_COLLS)
#define L_BAK	(2 * C_BAK)
#define L_HI2	(2 * C_HI2)
#define L_HI3	(2 * C_HI3)
#define L_PF0	(2 * C_PF0)
#define L_PF1	(2 * C_PF1)
#define L_PF2	(2 * C_PF2)
#define L_PF3	(2 * C_PF3)
#define L_BLACK	(2 * C_BLACK)

/* Blank areas optimizations
   Routines for most graphics modes take advantage of fact, that often
   large areas of screen are background colour. If it is possible, 8 pixels
   of background are drawn at once - with two longs or four words, if
   the platform doesn't allow unaligned long access.
   Artifacting also uses unaligned long access if it's supported. */

#ifdef UNALIGNED_LONG_OK

#define INIT_BACKGROUND_6 ULONG background = cl_lookup[C_PF2] | (((ULONG) cl_lookup[C_PF2]) << 16);
#define INIT_BACKGROUND_8 ULONG background = lookup_gtia9[0];
#define DRAW_BACKGROUND(colreg) {\
		ULONG *l_ptr = (ULONG *) ptr;\
		*l_ptr++ = background;\
		*l_ptr++ = background;\
		ptr = (UWORD *) l_ptr;\
	}
#define DRAW_ARTIF {\
		*(ULONG *) ptr = art_curtable[(UBYTE) (screendata_tally >> 10)];\
		((ULONG *) ptr)[1] = art_curtable[(UBYTE) (screendata_tally >> 6)];\
		ptr += 4;\
	}

#else

#define INIT_BACKGROUND_6
#define INIT_BACKGROUND_8
#define DRAW_BACKGROUND(colreg) {\
		ptr[3] = ptr[2] = ptr[1] = ptr[0] = cl_lookup[colreg];\
		ptr += 4;\
	}
#define DRAW_ARTIF {\
		*ptr++ = ((UWORD *) art_curtable)[(screendata_tally & 0x03fc00) >> 9];\
		*ptr++ = ((UWORD *) art_curtable)[((screendata_tally & 0x03fc00) >> 9) + 1];\
		*ptr++ = ((UWORD *) art_curtable)[(screendata_tally & 0x003fc0) >> 5];\
		*ptr++ = ((UWORD *) art_curtable)[((screendata_tally & 0x003fc0) >> 5) + 1];\
	}

#endif /* UNALIGNED_LONG_OK */

/* Hi-res modes optimizations
   Now hi-res modes are drawn with words, not bytes. Endianess defaults
   to little-endian. WORDS_BIGENDIAN should be defined when compiling on
   big-endian machine. */

#ifdef WORDS_BIGENDIAN
#define BYTE0_MASK		0xff00
#define BYTE1_MASK		0x00ff
#define HIRES_MASK_01	0xfff0
#define HIRES_MASK_10	0xf0ff
#define HIRES_LUM_01	0x000f
#define HIRES_LUM_10	0x0f00
#else
#define BYTE0_MASK		0x00ff
#define BYTE1_MASK		0xff00
#define HIRES_MASK_01	0xf0ff
#define HIRES_MASK_10	0xfff0
#define HIRES_LUM_01	0x0f00
#define HIRES_LUM_10	0x000f
#endif

static UWORD hires_lookup_n[128];
static UWORD hires_lookup_m[128];
#define hires_norm(x)	hires_lookup_n[(x) >> 1]
#define hires_mask(x)	hires_lookup_m[(x) >> 1]

#ifndef USE_COLOUR_TRANSLATION_TABLE
UWORD hires_lookup_l[128];	/* accessed in gtia.c */
#define hires_lum(x)	hires_lookup_l[(x) >> 1]
#endif

/* Player/Missile Graphics ------------------------------------------------- */

#define PF0PM (*(UBYTE *) &cl_lookup[C_PF0 | C_COLLS])
#define PF1PM (*(UBYTE *) &cl_lookup[C_PF1 | C_COLLS])
#define PF2PM (*(UBYTE *) &cl_lookup[C_PF2 | C_COLLS])
#define PF3PM (*(UBYTE *) &cl_lookup[C_PF3 | C_COLLS])
#define PF_COLLS(x) (((UBYTE *) &cl_lookup)[(x) + L_COLLS])

static UBYTE singleline;
UBYTE player_dma_enabled;
UBYTE player_gra_enabled;
UBYTE missile_dma_enabled;
UBYTE missile_gra_enabled;
UBYTE player_flickering;
UBYTE missile_flickering;

static UWORD pmbase_s;
static UWORD pmbase_d;

extern UBYTE pm_scanline[ATARI_WIDTH / 2];
extern UBYTE pm_dirty;

/* PMG lookup tables */
UBYTE pm_lookup_table[20][256];
/* current PMG lookup table */
UBYTE *pm_lookup_ptr;

#define PL_00	0	/* 0x00,0x01,0x02,0x03,0x04,0x06,0x08,0x09,0x0a,0x0b */
#define PL_05	1	/* 0x05,0x07,0x0c,0x0d,0x0e,0x0f */
#define PL_10	2	/* 0x10,0x1a */
#define PL_11	3	/* 0x11,0x18,0x19 */
#define PL_12	4	/* 0x12 */
#define PL_13	5	/* 0x13,0x1b */
#define PL_14	6	/* 0x14,0x16 */
#define PL_15	7	/* 0x15,0x17,0x1d,0x1f */
#define PL_1c	8	/* 0x1c */
#define PL_1e	9	/* 0x1e */
#define PL_20	10	/* 0x20,0x21,0x22,0x23,0x24,0x26,0x28,0x29,0x2a,0x2b */
#define PL_25	11	/* 0x25,0x27,0x2c,0x2d,0x2e,0x2f */
#define PL_30	12	/* 0x30,0x3a */
#define PL_31	13	/* 0x31,0x38,0x39 */
#define PL_32	14	/* 0x32 */
#define PL_33	15	/* 0x33,0x3b */
#define PL_34	16	/* 0x34,0x36 */
#define PL_35	17	/* 0x35,0x37,0x3d,0x3f */
#define PL_3c	18	/* 0x3c */
#define PL_3e	19	/* 0x3e */

static UBYTE prior_to_pm_lookup[64] = {
	PL_00, PL_00, PL_00, PL_00, PL_00, PL_05, PL_00, PL_05,
	PL_00, PL_00, PL_00, PL_00, PL_05, PL_05, PL_05, PL_05,
	PL_10, PL_11, PL_12, PL_13, PL_14, PL_15, PL_14, PL_15,
	PL_11, PL_11, PL_10, PL_13, PL_1c, PL_15, PL_1e, PL_15,
	PL_20, PL_20, PL_20, PL_20, PL_20, PL_25, PL_20, PL_25,
	PL_20, PL_20, PL_20, PL_20, PL_25, PL_25, PL_25, PL_25,
	PL_30, PL_31, PL_32, PL_33, PL_34, PL_35, PL_34, PL_35,
	PL_31, PL_31, PL_30, PL_33, PL_3c, PL_35, PL_3e, PL_35
};

void init_pm_lookup(void)
{
	static UBYTE pm_lookup_template[10][16] = {
		/* PL_20 */
		{ L_BAK, L_PM0, L_PM1, L_PM01, L_PM2, L_PM0, L_PM1, L_PM01,
		L_PM3, L_PM0, L_PM1, L_PM01, L_PM23, L_PM0, L_PM1, L_PM01 },
		/* PL_25 */
		{ L_BAK, L_PM0, L_PM1, L_PM01, L_PM2, L_PM023, L_PM123, L_PM0123,
		L_PM3, L_PM023, L_PM123, L_PM0123, L_PM23, L_PM023, L_PM123, L_PM0123 },
		/* PL_30 */
		{ L_PF3, L_PM0, L_PM1, L_PM01, L_PM25, L_PM0, L_PM1, L_PM01,
		L_PM35, L_PM0, L_PM1, L_PM01, L_PM235, L_PM0, L_PM1, L_PM01 },
		/* PL_31 */
		{ L_PF3, L_PM0, L_PM1, L_PM01, L_PM2, L_PM0, L_PM1, L_PM01,
		L_PM3, L_PM0, L_PM1, L_PM01, L_PM23, L_PM0, L_PM1, L_PM01 },
		/* PL_32 */
		{ L_PF3, L_PM0, L_PM1, L_PM01, L_PF3, L_PM0, L_PM1, L_PM01,
		L_PF3, L_PM0, L_PM1, L_PM01, L_PF3, L_PM0, L_PM1, L_PM01 },
		/* PL_33 */
		{ L_PF3, L_PM0, L_PM1, L_PM01, L_BLACK, L_PM0, L_PM1, L_PM01,
		L_BLACK, L_PM0, L_PM1, L_PM01, L_BLACK, L_PM0, L_PM1, L_PM01 },
		/* PL_34 */
		{ L_PF3, L_PF3, L_PF3, L_PF3, L_PF3, L_PF3, L_PF3, L_PF3,
		L_PF3, L_PF3, L_PF3, L_PF3, L_PF3, L_PF3, L_PF3, L_PF3 },
		/* PL_35 */
		{ L_PF3, L_PF3, L_PF3, L_PF3, L_BLACK, L_BLACK, L_BLACK, L_BLACK,
		L_BLACK, L_BLACK, L_BLACK, L_BLACK, L_BLACK, L_BLACK, L_BLACK, L_BLACK },
		/* PL_3c */
		{ L_PF3, L_PF3, L_PF3, L_PF3, L_PM25, L_PM25, L_PM25, L_PM25,
		L_PM25, L_PM25, L_PM25, L_PM25, L_PM25, L_PM25, L_PM25, L_PM25 },
		/* PL_3e */
		{ L_PF3, L_PF3, L_PF3, L_PF3, L_PM25, L_BLACK, L_BLACK, L_BLACK,
		L_PM25, L_BLACK, L_BLACK, L_BLACK, L_PM25, L_BLACK, L_BLACK, L_BLACK }
	};

	static UBYTE multi_to_normal[] = {
		L_BAK,
		L_PM0, L_PM1, L_PM0,
		L_PM2, L_PM3, L_PM2,
		L_PM023, L_PM123, L_PM023,
		L_PM25, L_PM35, L_PM25
	};

	int i;
	int j;
	UBYTE temp;

	for (i = 0; i <= 1; i++)
		for (j = 0; j <= 255; j++) {
			pm_lookup_table[i + 10][j] = temp = pm_lookup_template[i][(j & 0xf) | (j >> 4)];
			pm_lookup_table[i][j] = temp <= L_PM235 ? multi_to_normal[temp >> 1] : temp;
		}
	for (; i <= 9; i++) {
		for (j = 0; j <= 15; j++) {
			pm_lookup_table[i + 10][j] = temp = pm_lookup_template[i < 7 ? 0 : 1][j];
			pm_lookup_table[i][j] = temp <= L_PM235 ? multi_to_normal[temp >> 1] : temp;
		}
		for (; j <= 255; j++) {
			pm_lookup_table[i + 10][j] = temp = pm_lookup_template[i][j & 0xf];
			pm_lookup_table[i][j] = temp <= L_PM235 ? multi_to_normal[temp >> 1] : temp;
		}
	}
}

static UBYTE hold_missiles_tab[16] = {
	0x00,0x03,0x0c,0x0f,0x30,0x33,0x3c,0x3f,
	0xc0,0xc3,0xcc,0xcf,0xf0,0xf3,0xfc,0xff};

void pmg_dma(void)
{
	/* VDELAY bit set == GTIA ignores PMG DMA in even lines */
	if (player_dma_enabled) {
		if (player_gra_enabled) {
			if (singleline) {
				if (ypos & 1) {
					GRAFP0 = dGetByte(pmbase_s + ypos + 0x400);
					GRAFP1 = dGetByte(pmbase_s + ypos + 0x500);
					GRAFP2 = dGetByte(pmbase_s + ypos + 0x600);
					GRAFP3 = dGetByte(pmbase_s + ypos + 0x700);
				}
				else {
					if ((VDELAY & 0x10) == 0)
						GRAFP0 = dGetByte(pmbase_s + ypos + 0x400);
					if ((VDELAY & 0x20) == 0)
						GRAFP1 = dGetByte(pmbase_s + ypos + 0x500);
					if ((VDELAY & 0x40) == 0)
						GRAFP2 = dGetByte(pmbase_s + ypos + 0x600);
					if ((VDELAY & 0x80) == 0)
						GRAFP3 = dGetByte(pmbase_s + ypos + 0x700);
				}
			}
			else {
				if (ypos & 1) {
					GRAFP0 = dGetByte(pmbase_d + (ypos >> 1) + 0x200);
					GRAFP1 = dGetByte(pmbase_d + (ypos >> 1) + 0x280);
					GRAFP2 = dGetByte(pmbase_d + (ypos >> 1) + 0x300);
					GRAFP3 = dGetByte(pmbase_d + (ypos >> 1) + 0x380);
				}
				else {
					if ((VDELAY & 0x10) == 0)
						GRAFP0 = dGetByte(pmbase_d + (ypos >> 1) + 0x200);
					if ((VDELAY & 0x20) == 0)
						GRAFP1 = dGetByte(pmbase_d + (ypos >> 1) + 0x280);
					if ((VDELAY & 0x40) == 0)
						GRAFP2 = dGetByte(pmbase_d + (ypos >> 1) + 0x300);
					if ((VDELAY & 0x80) == 0)
						GRAFP3 = dGetByte(pmbase_d + (ypos >> 1) + 0x380);
				}
			}
		}
		xpos += 4;
	}
	if (missile_dma_enabled) {
		if (missile_gra_enabled) {
			UBYTE data = dGetByte(singleline ? pmbase_s + ypos + 0x300 : pmbase_d + (ypos >> 1) + 0x180);
			/* in odd lines load all missiles, in even only those, for which VDELAY bit is zero */
			GRAFM = ypos & 1 ? data : ((GRAFM ^ data) & hold_missiles_tab[VDELAY & 0xf]) ^ data;
		}
		xpos++;
	}
}

/* Artifacting ------------------------------------------------------------ */

int global_artif_mode;

static ULONG art_lookup_normal[256];
static ULONG art_lookup_reverse[256];
static ULONG art_bkmask_normal[256];
static ULONG art_lummask_normal[256];
static ULONG art_bkmask_reverse[256];
static ULONG art_lummask_reverse[256];

static ULONG *art_curtable = art_lookup_normal;
static ULONG *art_curbkmask = art_bkmask_normal;
static ULONG *art_curlummask = art_lummask_normal;

static UWORD art_normal_colpf1_save;
static UWORD art_normal_colpf2_save;
static UWORD art_reverse_colpf1_save;
static UWORD art_reverse_colpf2_save;

void setup_art_colours(void)
{
	static UWORD *art_colpf1_save = &art_normal_colpf1_save;
	static UWORD *art_colpf2_save = &art_normal_colpf2_save;
	UWORD curlum = cl_lookup[C_PF1] & 0x0f0f;

	if (curlum != *art_colpf1_save || cl_lookup[C_PF2] != *art_colpf2_save) {
		if (curlum < (cl_lookup[C_PF2] & 0x0f0f)) {
			art_colpf1_save = &art_reverse_colpf1_save;
			art_colpf2_save = &art_reverse_colpf2_save;
			art_curtable = art_lookup_reverse;
			art_curlummask = art_lummask_reverse;
			art_curbkmask = art_bkmask_reverse;
		}
		else {
			art_colpf1_save = &art_normal_colpf1_save;
			art_colpf2_save = &art_normal_colpf2_save;
			art_curtable = art_lookup_normal;
			art_curlummask = art_lummask_normal;
			art_curbkmask = art_bkmask_normal;
		}
		if (curlum ^ *art_colpf1_save) {
			int i;
			ULONG new_colour = curlum ^ *art_colpf1_save;
			new_colour |= new_colour << 16;
			*art_colpf1_save = curlum;
			for (i = 0; i <= 255; i++)
				art_curtable[i] ^= art_curlummask[i] & new_colour;
		}
		if (cl_lookup[C_PF2] ^ *art_colpf2_save) {
			int i;
			ULONG new_colour = cl_lookup[C_PF2] ^ *art_colpf2_save;
			new_colour |= new_colour << 16;
			*art_colpf2_save = cl_lookup[C_PF2];
			for (i = 0; i <= 255; i++)
				art_curtable[i] ^= art_curbkmask[i] & new_colour;
		}

	}
}

/* Initialization ---------------------------------------------------------- */

void ANTIC_Initialise(int *argc, char *argv[])
{
	int i, j;

	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-artif") == 0) {
			global_artif_mode = argv[++i][0] - '0';
			if (global_artif_mode < 0 || global_artif_mode > 4) {
				Aprint("Invalid artifacting mode, using default.");
				global_artif_mode = 0;
			}
		}
		else
			argv[j++] = argv[i];
	}
	*argc = j;

	artif_init();

	playfield_lookup[0x00] = L_BAK;
	playfield_lookup[0x40] = L_PF0;
	playfield_lookup[0x80] = L_PF1;
	playfield_lookup[0xc0] = L_PF2;
	playfield_lookup[0x100] = L_PF3;
	blank_lookup[0x80] = blank_lookup[0xa0] = blank_lookup[0xc0] = blank_lookup[0xe0] = 0x00;
	hires_mask(0x00) = 0xffff;
#ifdef USE_COLOUR_TRANSLATION_TABLE
	hires_mask(0x40) = BYTE0_MASK;
	hires_mask(0x80) = BYTE1_MASK;
	hires_mask(0xc0) = 0;
#else
	hires_mask(0x40) = HIRES_MASK_01;
	hires_mask(0x80) = HIRES_MASK_10;
	hires_mask(0xc0) = 0xf0f0;
	hires_lum(0x00) = hires_lum(0x40) = hires_lum(0x80) = hires_lum(0xc0) = 0;
#endif
	init_pm_lookup();
}

void ANTIC_Reset(void) {
	NMIEN = 0x00;
	NMIST = 0x3f;	/* Probably bit 5 is Reset flag */
	ANTIC_PutByte(_DMACTL, 0);
}

/* Border ------------------------------------------------------------------ */

#define DO_BORDER_1 {\
	if (!(*(ULONG *) pm_scanline_ptr)) {\
		ULONG *l_ptr = (ULONG *) ptr;\
		*l_ptr++ = background;\
		*l_ptr++ = background;\
		ptr = (UWORD *) l_ptr;\
		pm_scanline_ptr += 4;\
	} else {\
		int k = 4;\
		do

#define DO_BORDER DO_BORDER_1\
			*ptr++ = COLOUR(pm_lookup_ptr[*pm_scanline_ptr++]);\
		while (--k);\
	}\
}

#define DO_GTIA10_BORDER DO_BORDER_1\
			*ptr++ = COLOUR(pm_lookup_ptr[*pm_scanline_ptr++ | 1]);\
		while (--k);\
	}\
}
				
void do_border(void)
{
	int kk;
	UWORD *ptr = &scrn_ptr[LCHOP * 4];
	UBYTE *pm_scanline_ptr = &pm_scanline[LCHOP * 4];
	ULONG background = lookup_gtia9[0];
	/* left border */
	for (kk = left_border_chars; kk; kk--)
			DO_BORDER
	/* right border */
	ptr = &scrn_ptr[right_border_start];
	pm_scanline_ptr = &pm_scanline[right_border_start];
	while (pm_scanline_ptr < &pm_scanline[(48 - RCHOP) * 4])
			DO_BORDER
}

void do_border_gtia10(void)
{
	int kk;
	UWORD *ptr = &scrn_ptr[LCHOP * 4];
	UBYTE *pm_scanline_ptr = &pm_scanline[LCHOP * 4];
	ULONG background = cl_lookup[C_PM0] | (cl_lookup[C_PM0] << 16);
	/* left border */
	for (kk = left_border_chars; kk; kk--)
		DO_GTIA10_BORDER
	*ptr = COLOUR(pm_lookup_ptr[*pm_scanline_ptr | 1]);
	/* right border */
	pm_scanline_ptr = &pm_scanline[right_border_start + 1];
	if (pm_scanline_ptr < &pm_scanline[(48 - RCHOP) * 4]) {
		ptr = &scrn_ptr[right_border_start + 1];
		*ptr++ = COLOUR(pm_lookup_ptr[pm_scanline_ptr[1] | 1]);
		*ptr++ = COLOUR(pm_lookup_ptr[pm_scanline_ptr[2] | 1]);
		*ptr++ = COLOUR(pm_lookup_ptr[pm_scanline_ptr[3] | 1]);
		pm_scanline_ptr += 4;
		while (pm_scanline_ptr < &pm_scanline[(48 - RCHOP) * 4])
			DO_GTIA10_BORDER
	}
}

void do_border_gtia11(void)
{
	int kk;
	UWORD *ptr = &scrn_ptr[LCHOP * 4];
	UBYTE *pm_scanline_ptr = &pm_scanline[LCHOP * 4];
	ULONG background = lookup_gtia11[0];
#ifdef USE_COLOUR_TRANSLATION_TABLE
	cl_lookup[C_PF3] = colour_translation_table[COLPF3 & 0xf0];
#else
	cl_lookup[C_PF3] &= 0xf0f0;
#endif
	cl_lookup[C_BAK] = (UWORD) background;
	/* left border */
	for (kk = left_border_chars; kk; kk--)
		DO_BORDER
	/* right border */
	ptr = &scrn_ptr[right_border_start];
	pm_scanline_ptr = &pm_scanline[right_border_start];
	while (pm_scanline_ptr < &pm_scanline[(48 - RCHOP) * 4])
		DO_BORDER
	COLOUR_TO_WORD(cl_lookup[C_PF3],COLPF3)
	COLOUR_TO_WORD(cl_lookup[C_BAK],COLBK)
}

void draw_antic_0(void)
{
	UWORD *ptr = scrn_ptr + 4 * LCHOP;
	if (pm_dirty) {
		UBYTE *pm_scanline_ptr = &pm_scanline[LCHOP * 4];
		ULONG background = lookup_gtia9[0];
		do
			DO_BORDER
		while (pm_scanline_ptr < &pm_scanline[(48 - RCHOP) * 4]);
	}
	else
		memset(ptr, cl_lookup[C_BAK], (48 - LCHOP - RCHOP) * 8);
}

void draw_antic_0_gtia10(void)
{
	UWORD *ptr = scrn_ptr + 4 * LCHOP;
	if (pm_dirty) {
		UBYTE *pm_scanline_ptr = &pm_scanline[LCHOP * 4];
		ULONG background = cl_lookup[C_PM0] | (cl_lookup[C_PM0] << 16);
		do
			DO_GTIA10_BORDER
		while (pm_scanline_ptr < &pm_scanline[(48 - RCHOP) * 4]);
	}
	else
		memset(ptr, cl_lookup[C_PM0], (48 - LCHOP - RCHOP) * 8);
}

void draw_antic_0_gtia11(void)
{
	UWORD *ptr = scrn_ptr + 4 * LCHOP;
	if (pm_dirty) {
		UBYTE *pm_scanline_ptr = &pm_scanline[LCHOP * 4];
		ULONG background = lookup_gtia11[0];
#ifdef USE_COLOUR_TRANSLATION_TABLE
		cl_lookup[C_PF3] = colour_translation_table[COLPF3 & 0xf0];
#else
		cl_lookup[C_PF3] &= 0xf0f0;
#endif
		cl_lookup[C_BAK] = (UWORD) background;
		do
			DO_BORDER
		while (pm_scanline_ptr < &pm_scanline[(48 - RCHOP) * 4]);
		COLOUR_TO_WORD(cl_lookup[C_PF3],COLPF3)
		COLOUR_TO_WORD(cl_lookup[C_BAK],COLBK)
	}
	else
		memset(ptr, lookup_gtia11[0], (48 - LCHOP - RCHOP) * 8);
}

/* ANTIC modes ------------------------------------------------------------- */

static UBYTE gtia_10_lookup[] =
{L_BAK, L_BAK, L_BAK, L_BAK, L_PF0, L_PF1, L_PF2, L_PF3,
 L_BAK, L_BAK, L_BAK, L_BAK, L_PF0, L_PF1, L_PF2, L_PF3};
static UBYTE gtia_10_pm[] =
{1, 2, 4, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

#define CHAR_LOOP_BEGIN do {
#define CHAR_LOOP_END } while (--nchars);

#define DO_PMG_LORES PF_COLLS(colreg) |= pm_pixel = *c_pm_scanline_ptr++;\
	*ptr++ = COLOUR(pm_lookup_ptr[pm_pixel] | colreg);

#ifdef ALTERNATE_LOOP_COUNTERS 	/* speeds-up pmg in hires a bit or not? try it :) */
#define FOUR_LOOP_BEGIN(data) data |= 0x800000; do {	/* data becomes negative after four data <<= 2 */
#define FOUR_LOOP_END(data) } while (data >= 0);
#else
#define FOUR_LOOP_BEGIN(data) int k = 4; do {
#define FOUR_LOOP_END(data) } while (--k);
#endif

#ifdef USE_COLOUR_TRANSLATION_TABLE

#define INIT_HIRES hires_norm(0x00) = cl_lookup[C_PF2];\
	hires_norm(0x40) = hires_norm(0x10) = hires_norm(0x04) = (cl_lookup[C_PF2] & BYTE0_MASK) | (cl_lookup[C_HI2] & BYTE1_MASK);\
	hires_norm(0x80) = hires_norm(0x20) = hires_norm(0x08) = (cl_lookup[C_HI2] & BYTE0_MASK) | (cl_lookup[C_PF2] & BYTE1_MASK);\
	hires_norm(0xc0) = hires_norm(0x30) = hires_norm(0x0c) = cl_lookup[C_HI2];

#define DO_PMG_HIRES(data) {\
	UBYTE *c_pm_scanline_ptr = (UBYTE *) t_pm_scanline_ptr;\
	int pm_pixel;\
	int mask;\
	FOUR_LOOP_BEGIN(data)\
		pm_pixel = *c_pm_scanline_ptr++;\
		if (data & 0xc0)\
			PF2PM |= pm_pixel;\
		mask = hires_mask(data & 0xc0);\
		pm_pixel = pm_lookup_ptr[pm_pixel] | L_PF2;\
		*ptr++ = (COLOUR(pm_pixel) & mask) | (COLOUR(pm_pixel + (L_HI2 - L_PF2)) & ~mask);\
		data <<= 2;\
	FOUR_LOOP_END(data)\
}

#else /* USE_COLOUR_TRANSLATION_TABLE */

#define INIT_HIRES hires_norm(0x00) = cl_lookup[C_PF2];\
	hires_norm(0x40) = hires_norm(0x10) = hires_norm(0x04) = (cl_lookup[C_PF2] & HIRES_MASK_01) | hires_lum(0x40);\
	hires_norm(0x80) = hires_norm(0x20) = hires_norm(0x08) = (cl_lookup[C_PF2] & HIRES_MASK_10) | hires_lum(0x80);\
	hires_norm(0xc0) = hires_norm(0x30) = hires_norm(0x0c) = (cl_lookup[C_PF2] & 0xf0f0) | hires_lum(0xc0);

#define DO_PMG_HIRES(data) {\
	UBYTE *c_pm_scanline_ptr = (UBYTE *) t_pm_scanline_ptr;\
	int pm_pixel;\
	FOUR_LOOP_BEGIN(data)\
		pm_pixel = *c_pm_scanline_ptr++;\
		if (data & 0xc0)\
			PF2PM |= pm_pixel;\
		*ptr++ = (COLOUR(pm_lookup_ptr[pm_pixel] | L_PF2) & hires_mask(data & 0xc0)) | hires_lum(data & 0xc0);\
		data <<= 2;\
	FOUR_LOOP_END(data)\
}

#endif /* USE_COLOUR_TRANSLATION_TABLE */

#define INIT_ANTIC_2	int t_chbase = (dctr ^ chbase_20) & 0xfc07;\
	xpos += font_cycles[md];\
	blank_lookup[0x60] = (anticmode == 2 || dctr & 0xe) ? 0xff : 0;\
	blank_lookup[0x00] = blank_lookup[0x20] = blank_lookup[0x40] = (dctr & 0xe) == 8 ? 0 : 0xff;

void draw_antic_2(int nchars, UBYTE *ANTIC_memptr, UWORD *ptr, ULONG *t_pm_scanline_ptr)
{
	INIT_BACKGROUND_6
	INIT_ANTIC_2
	INIT_HIRES

	CHAR_LOOP_BEGIN
		UBYTE screendata = *ANTIC_memptr++;
		int chdata;

		chdata = (screendata & invert_mask) ? 0xff : 0;
		if (blank_lookup[screendata & blank_mask])
			chdata ^= dGetByte(t_chbase + ((UWORD) (screendata & 0x7f) << 3));
		if (IS_ZERO_ULONG(t_pm_scanline_ptr)) {
			if (chdata) {
				*ptr++ = hires_norm(chdata & 0xc0);
				*ptr++ = hires_norm(chdata & 0x30);
				*ptr++ = hires_norm(chdata & 0x0c);
				*ptr++ = hires_norm((chdata & 0x03) << 2);
			}
			else
				DRAW_BACKGROUND(C_PF2)
		}
		else
			DO_PMG_HIRES(chdata)
		t_pm_scanline_ptr++;
	CHAR_LOOP_END
	do_border();
}

void draw_antic_2_artif(int nchars, UBYTE *ANTIC_memptr, UWORD *ptr, ULONG *t_pm_scanline_ptr)
{
	ULONG screendata_tally;
	UBYTE screendata = *ANTIC_memptr++;
	UBYTE chdata;
	INIT_ANTIC_2
	chdata = (screendata & invert_mask) ? 0xff : 0;
	if (blank_lookup[screendata & blank_mask])
		chdata ^= dGetByte(t_chbase + ((UWORD) (screendata & 0x7f) << 3));
	screendata_tally = chdata;
	setup_art_colours();

	CHAR_LOOP_BEGIN
		UBYTE screendata = *ANTIC_memptr++;
		ULONG chdata;

		chdata = (screendata & invert_mask) ? 0xff : 0;
		if (blank_lookup[screendata & blank_mask])
			chdata ^= dGetByte(t_chbase + ((UWORD) (screendata & 0x7f) << 3));
		screendata_tally <<= 8;
		screendata_tally |= chdata;
		if (IS_ZERO_ULONG(t_pm_scanline_ptr))
			DRAW_ARTIF
		else {
			chdata = screendata_tally >> 8;
			DO_PMG_HIRES(chdata)
		}
		t_pm_scanline_ptr++;
	CHAR_LOOP_END
	do_border();
}

void draw_antic_2_gtia9(int nchars, UBYTE *ANTIC_memptr, UWORD *ptr, ULONG *t_pm_scanline_ptr)
{
	INIT_ANTIC_2

	CHAR_LOOP_BEGIN
		UBYTE screendata = *ANTIC_memptr++;
		int chdata;

		chdata = (screendata & invert_mask) ? 0xff : 0;
		if (blank_lookup[screendata & blank_mask])
			chdata ^= dGetByte(t_chbase + ((UWORD) (screendata & 0x7f) << 3));
		DO_GTIA_BYTE(ptr, lookup_gtia9, chdata)
		if (IS_ZERO_ULONG(t_pm_scanline_ptr))
			ptr += 4;
		else {
			UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
			int k = 4;
			UBYTE pm_reg;
			do {
				pm_reg = pm_lookup_ptr[*c_pm_scanline_ptr++];
				if (pm_reg) {
					if (pm_reg == L_PF3) {
						UBYTE tmp = k > 2 ? chdata >> 4 : chdata & 0xf;
#ifdef USE_COLOUR_TRANSLATION_TABLE
						*ptr = colour_translation_table[tmp | COLPF3];
#else
						*ptr = tmp | ((UWORD)tmp << 8) | cl_lookup[C_PF3];
#endif
					}
					else
						*ptr = COLOUR(pm_reg);
				}
				ptr++;
			} while (--k);
		}
		t_pm_scanline_ptr++;
	CHAR_LOOP_END
	do_border();
}

void draw_antic_2_gtia10(int nchars, UBYTE *ANTIC_memptr, UWORD *ptr, ULONG *t_pm_scanline_ptr)
{
#ifdef UNALIGNED_LONG_OK
	ULONG lookup_gtia10[16];
#else
	UWORD lookup_gtia10[16];
#endif
	INIT_ANTIC_2

#ifdef UNALIGNED_LONG_OK
	lookup_gtia10[0] = cl_lookup[C_PM0] | (cl_lookup[C_PM0] << 16);
	lookup_gtia10[1] = cl_lookup[C_PM1] | (cl_lookup[C_PM1] << 16);
	lookup_gtia10[2] = cl_lookup[C_PM2] | (cl_lookup[C_PM2] << 16);
	lookup_gtia10[3] = cl_lookup[C_PM3] | (cl_lookup[C_PM3] << 16);
	lookup_gtia10[12] = lookup_gtia10[4] = cl_lookup[C_PF0] | (cl_lookup[C_PF0] << 16);
	lookup_gtia10[13] = lookup_gtia10[5] = cl_lookup[C_PF1] | (cl_lookup[C_PF1] << 16);
	lookup_gtia10[14] = lookup_gtia10[6] = cl_lookup[C_PF2] | (cl_lookup[C_PF2] << 16);
	lookup_gtia10[15] = lookup_gtia10[7] = cl_lookup[C_PF3] | (cl_lookup[C_PF3] << 16);
	lookup_gtia10[8] = lookup_gtia10[9] = lookup_gtia10[10] = lookup_gtia10[11] = lookup_gtia9[0];
#else
	lookup_gtia10[0] = cl_lookup[C_PM0];
	lookup_gtia10[1] = cl_lookup[C_PM1];
	lookup_gtia10[2] = cl_lookup[C_PM2];
	lookup_gtia10[3] = cl_lookup[C_PM3];
	lookup_gtia10[12] = lookup_gtia10[4] = cl_lookup[C_PF0];
	lookup_gtia10[13] = lookup_gtia10[5] = cl_lookup[C_PF1];
	lookup_gtia10[14] = lookup_gtia10[6] = cl_lookup[C_PF2];
	lookup_gtia10[15] = lookup_gtia10[7] = cl_lookup[C_PF3];
	lookup_gtia10[8] = lookup_gtia10[9] = lookup_gtia10[10] = lookup_gtia10[11] = cl_lookup[C_BAK];
#endif
	t_pm_scanline_ptr = (ULONG *) (((UBYTE *) t_pm_scanline_ptr) + 1);
	CHAR_LOOP_BEGIN
		UBYTE screendata = *ANTIC_memptr++;
		int chdata;

		chdata = (screendata & invert_mask) ? 0xff : 0;
		if (blank_lookup[screendata & blank_mask])
			chdata ^= dGetByte(t_chbase + ((UWORD) (screendata & 0x7f) << 3));
		if (IS_ZERO_ULONG(t_pm_scanline_ptr)) {
			DO_GTIA_BYTE(ptr, lookup_gtia10, chdata)
			ptr += 4;
		}
		else {
			UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
			int pm_pixel;
			int colreg;
			int k = 4;
			UBYTE t_screendata = chdata >> 4;
			do {
				colreg = gtia_10_lookup[t_screendata];
				PF_COLLS(colreg) |= pm_pixel = *c_pm_scanline_ptr++;
				pm_pixel |= gtia_10_pm[t_screendata];
				*ptr++ = COLOUR(pm_lookup_ptr[pm_pixel] | colreg);
				if (k == 3)
					t_screendata = chdata & 0x0f;
			} while (--k);
		}
		t_pm_scanline_ptr++;
	CHAR_LOOP_END
	do_border_gtia10();
}

void draw_antic_2_gtia11(int nchars, UBYTE *ANTIC_memptr, UWORD *ptr, ULONG *t_pm_scanline_ptr)
{
	INIT_ANTIC_2

	CHAR_LOOP_BEGIN
		UBYTE screendata = *ANTIC_memptr++;
		int chdata;

		chdata = (screendata & invert_mask) ? 0xff : 0;
		if (blank_lookup[screendata & blank_mask])
			chdata ^= dGetByte(t_chbase + ((UWORD) (screendata & 0x7f) << 3));
		DO_GTIA_BYTE(ptr, lookup_gtia11, chdata)
		if (IS_ZERO_ULONG(t_pm_scanline_ptr))
			ptr += 4;
		else {
			UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
			int k = 4;
			UBYTE pm_reg;
			do {
				pm_reg = pm_lookup_ptr[*c_pm_scanline_ptr++];
				if (pm_reg) {
					if (pm_reg == L_PF3) {
						UBYTE tmp = k > 2 ? chdata & 0xf0 : chdata << 4;
#ifdef USE_COLOUR_TRANSLATION_TABLE
						*ptr = colour_translation_table[tmp ? tmp | COLPF3 : COLPF3 & 0xf0];
#else
						*ptr = tmp ? tmp | ((UWORD)tmp << 8) | cl_lookup[C_PF3] : cl_lookup[C_PF3] & 0xf0f0;
#endif
					}
					else
						*ptr = COLOUR(pm_reg);
				}
				ptr++;
			} while (--k);
		}
		t_pm_scanline_ptr++;
	CHAR_LOOP_END
	do_border_gtia11();
}

void draw_antic_4(int nchars, UBYTE *ANTIC_memptr, UWORD *ptr, ULONG *t_pm_scanline_ptr)
{
	INIT_BACKGROUND_8
	UWORD t_chbase = ((anticmode == 4 ? dctr : dctr >> 1) ^ chbase_20) & 0xfc07;

	xpos += font_cycles[md];
	lookup2[0x0f] = lookup2[0x00] = cl_lookup[C_BAK];
	lookup2[0x4f] = lookup2[0x1f] = lookup2[0x13] =
	lookup2[0x40] = lookup2[0x10] = lookup2[0x04] = lookup2[0x01] = cl_lookup[C_PF0];
	lookup2[0x8f] = lookup2[0x2f] = lookup2[0x17] = lookup2[0x11] =
	lookup2[0x80] = lookup2[0x20] = lookup2[0x08] = lookup2[0x02] = cl_lookup[C_PF1];
	lookup2[0xc0] = lookup2[0x30] = lookup2[0x0c] = lookup2[0x03] = cl_lookup[C_PF2];
	lookup2[0xcf] = lookup2[0x3f] = lookup2[0x1b] = lookup2[0x12] = cl_lookup[C_PF3];

	CHAR_LOOP_BEGIN
		UBYTE screendata = *ANTIC_memptr++;
		UWORD *lookup;
		UBYTE chdata;
		if (screendata & 0x80)
			lookup = lookup2 + 0xf;
		else
			lookup = lookup2;
		chdata = dGetByte(t_chbase + ((UWORD) (screendata & 0x7f) << 3));
		if (IS_ZERO_ULONG(t_pm_scanline_ptr)) {
			if (chdata) {
				*ptr++ = lookup[chdata & 0xc0];
				*ptr++ = lookup[chdata & 0x30];
				*ptr++ = lookup[chdata & 0x0c];
				*ptr++ = lookup[chdata & 0x03];
			}
			else
				DRAW_BACKGROUND(C_BAK)
		}
		else {
			UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
			int pm_pixel;
			int colreg;
			int k = 4;
			playfield_lookup[0xc0] = screendata & 0x80 ? L_PF3 : L_PF2;
			do {
				colreg = playfield_lookup[chdata & 0xc0];
				DO_PMG_LORES
				chdata <<= 2;
			} while (--k);
		}
		t_pm_scanline_ptr++;
	CHAR_LOOP_END
	playfield_lookup[0xc0] = L_PF2;
	do_border();
}

void draw_antic_6(int nchars, UBYTE *ANTIC_memptr, UWORD *ptr, ULONG *t_pm_scanline_ptr)
{
	UWORD t_chbase = (anticmode == 6 ? dctr & 7 : dctr >> 1) ^ chbase_20;

	xpos += font_cycles[md];
	CHAR_LOOP_BEGIN
		UBYTE screendata = *ANTIC_memptr++;
		UBYTE chdata;
		UWORD colour;
		int kk = 2;
		colour = COLOUR((playfield_lookup + 0x40)[screendata & 0xc0]);
		chdata = dGetByte(t_chbase + ((UWORD) (screendata & 0x3f) << 3));
		do {
			if (IS_ZERO_ULONG(t_pm_scanline_ptr)) {
				if (chdata & 0xf0) {
					if (chdata & 0x80)
						*ptr++ = colour;
					else
						*ptr++ = cl_lookup[C_BAK];
					if (chdata & 0x40)
						*ptr++ = colour;
					else
						*ptr++ = cl_lookup[C_BAK];
					if (chdata & 0x20)
						*ptr++ = colour;
					else
						*ptr++ = cl_lookup[C_BAK];
					if (chdata & 0x10)
						*ptr++ = colour;
					else
						*ptr++ = cl_lookup[C_BAK];
				}
				else {
					ptr[3] = ptr[2] = ptr[1] = ptr[0] = cl_lookup[C_BAK];
					ptr += 4;
				}
				chdata <<= 4;
			}
			else {
				UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
				int pm_pixel;
				UBYTE setcol = (playfield_lookup + 0x40)[screendata & 0xc0];
				int colreg;
				int k = 4;
				do {
					colreg = chdata & 0x80 ? setcol : L_BAK;
					DO_PMG_LORES
					chdata <<= 1;
				} while (--k);

			}
			t_pm_scanline_ptr++;
		} while (--kk);
	CHAR_LOOP_END
	do_border();
}

void draw_antic_8(int nchars, UBYTE *ANTIC_memptr, UWORD *ptr, ULONG *t_pm_scanline_ptr)
{
	lookup2[0x00] = cl_lookup[C_BAK];
	lookup2[0x40] = cl_lookup[C_PF0];
	lookup2[0x80] = cl_lookup[C_PF1];
	lookup2[0xc0] = cl_lookup[C_PF2];
	CHAR_LOOP_BEGIN
		UBYTE screendata = *ANTIC_memptr++;
		int kk = 4;
		do {
			if ((UBYTE *) t_pm_scanline_ptr >= pm_scanline + 4 * (48 - RCHOP))
				break;
			if (IS_ZERO_ULONG(t_pm_scanline_ptr)) {
				ptr[3] = ptr[2] = ptr[1] = ptr[0] = lookup2[screendata & 0xc0];
				ptr += 4;
			}
			else {
				UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
				int pm_pixel;
				int colreg = playfield_lookup[screendata & 0xc0];
				int k = 4;
				do {
					DO_PMG_LORES
				} while (--k);
			}
			screendata <<= 2;
			t_pm_scanline_ptr++;
		} while (--kk);
	CHAR_LOOP_END
	do_border();
}

void draw_antic_9(int nchars, UBYTE *ANTIC_memptr, UWORD *ptr, ULONG *t_pm_scanline_ptr)
{
	lookup2[0x00] = cl_lookup[C_BAK];
	lookup2[0x80] = lookup2[0x40] = cl_lookup[C_PF0];
	CHAR_LOOP_BEGIN
		UBYTE screendata = *ANTIC_memptr++;
		int kk = 4;
		do {
			if ((UBYTE *) t_pm_scanline_ptr >= pm_scanline + 4 * (48 - RCHOP))
				break;
			if (IS_ZERO_ULONG(t_pm_scanline_ptr)) {
				ptr[1] = ptr[0] = lookup2[screendata & 0x80];
				ptr[3] = ptr[2] = lookup2[screendata & 0x40];
				ptr += 4;
				screendata <<= 2;
			}
			else {
				UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
				int pm_pixel;
				int colreg;
				int k = 4;
				do {
					colreg = (screendata & 0x80) ? L_PF0 : L_BAK;
					DO_PMG_LORES
					if (k & 0x01)
						screendata <<= 1;
				} while (--k);
			}
			t_pm_scanline_ptr++;
		} while (--kk);
	CHAR_LOOP_END
	do_border();
}

/* ANTIC modes 9, b and c use BAK and PF0 colours only so they're not visible in GTIA modes */
/* Direct use of draw_antic_0* routines may cause some problems, I think */
void draw_antic_9_gtia9(int nchars, UBYTE *ANTIC_memptr, UWORD *ptr, ULONG *t_pm_scanline_ptr)
{
	draw_antic_0();
}

void draw_antic_9_gtia10(int nchars, UBYTE *ANTIC_memptr, UWORD *ptr, ULONG *t_pm_scanline_ptr)
{
	draw_antic_0_gtia10();
}

void draw_antic_9_gtia11(int nchars, UBYTE *ANTIC_memptr, UWORD *ptr, ULONG *t_pm_scanline_ptr)
{
	draw_antic_0_gtia11();
}

void draw_antic_a(int nchars, UBYTE *ANTIC_memptr, UWORD *ptr, ULONG *t_pm_scanline_ptr)
{
	lookup2[0x00] = cl_lookup[C_BAK];
	lookup2[0x40] = lookup2[0x10] = cl_lookup[C_PF0];
	lookup2[0x80] = lookup2[0x20] = cl_lookup[C_PF1];
	lookup2[0xc0] = lookup2[0x30] = cl_lookup[C_PF2];
	CHAR_LOOP_BEGIN
		UBYTE screendata = *ANTIC_memptr++;
		int kk = 2;
		do {
			if (IS_ZERO_ULONG(t_pm_scanline_ptr)) {
				ptr[1] = ptr[0] = lookup2[screendata & 0xc0];
				ptr[3] = ptr[2] = lookup2[screendata & 0x30];
				ptr += 4;
				screendata <<= 4;
			}
			else {
				UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
				int pm_pixel;
				int colreg;
				int k = 4;
				do {
					colreg = playfield_lookup[screendata & 0xc0];
					DO_PMG_LORES
					if (k & 0x01)
						screendata <<= 2;
				} while (--k);
			}
			t_pm_scanline_ptr++;
		} while (--kk);
	CHAR_LOOP_END
	do_border();
}

void draw_antic_c(int nchars, UBYTE *ANTIC_memptr, UWORD *ptr, ULONG *t_pm_scanline_ptr)
{
	lookup2[0x00] = cl_lookup[C_BAK];
	lookup2[0x80] = lookup2[0x40] = lookup2[0x20] = lookup2[0x10] = cl_lookup[C_PF0];
	CHAR_LOOP_BEGIN
		UBYTE screendata = *ANTIC_memptr++;
		int kk = 2;
		do {
			if (IS_ZERO_ULONG(t_pm_scanline_ptr)) {
				*ptr++ = lookup2[screendata & 0x80];
				*ptr++ = lookup2[screendata & 0x40];
				*ptr++ = lookup2[screendata & 0x20];
				*ptr++ = lookup2[screendata & 0x10];
				screendata <<= 4;
			}
			else {
				UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
				int pm_pixel;
				int colreg;
				int k = 4;
				do {
					colreg = (screendata & 0x80) ? L_PF0 : L_BAK;
					DO_PMG_LORES
					screendata <<= 1;
				} while (--k);
			}
			t_pm_scanline_ptr++;
		} while (--kk);
	CHAR_LOOP_END
	do_border();
}

void draw_antic_e(int nchars, UBYTE *ANTIC_memptr, UWORD *ptr, ULONG *t_pm_scanline_ptr)
{
	INIT_BACKGROUND_8
	lookup2[0x00] = cl_lookup[C_BAK];
	lookup2[0x40] = lookup2[0x10] = lookup2[0x04] = lookup2[0x01] = cl_lookup[C_PF0];
	lookup2[0x80] = lookup2[0x20] = lookup2[0x08] = lookup2[0x02] = cl_lookup[C_PF1];
	lookup2[0xc0] = lookup2[0x30] = lookup2[0x0c] = lookup2[0x03] = cl_lookup[C_PF2];

	CHAR_LOOP_BEGIN
		UBYTE screendata = *ANTIC_memptr++;
		if (IS_ZERO_ULONG(t_pm_scanline_ptr)) {
			if (screendata) {
				*ptr++ = lookup2[screendata & 0xc0];
				*ptr++ = lookup2[screendata & 0x30];
				*ptr++ = lookup2[screendata & 0x0c];
				*ptr++ = lookup2[screendata & 0x03];
			}
			else
				DRAW_BACKGROUND(C_BAK)
		}
		else {
			UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
			int pm_pixel;
			int colreg;
			int k = 4;
			do {
				colreg = playfield_lookup[screendata & 0xc0];
				DO_PMG_LORES
				screendata <<= 2;
			} while (--k);

		}
		t_pm_scanline_ptr++;
	CHAR_LOOP_END
	do_border();
}

void draw_antic_f(int nchars, UBYTE *ANTIC_memptr, UWORD *ptr, ULONG *t_pm_scanline_ptr)
{
	INIT_BACKGROUND_6
	INIT_HIRES

	CHAR_LOOP_BEGIN
		int screendata = *ANTIC_memptr++;
		if (IS_ZERO_ULONG(t_pm_scanline_ptr)) {
			if (screendata) {
				*ptr++ = hires_norm(screendata & 0xc0);
				*ptr++ = hires_norm(screendata & 0x30);
				*ptr++ = hires_norm(screendata & 0x0c);
				*ptr++ = hires_norm((screendata & 0x03) << 2);
			}
			else
				DRAW_BACKGROUND(C_PF2)
		}
		else
			DO_PMG_HIRES(screendata)
		t_pm_scanline_ptr++;
	CHAR_LOOP_END
	do_border();
}

void draw_antic_f_artif(int nchars, UBYTE *ANTIC_memptr, UWORD *ptr, ULONG *t_pm_scanline_ptr)
{
	ULONG screendata_tally = *ANTIC_memptr++;

	setup_art_colours();
	CHAR_LOOP_BEGIN
		int screendata = *ANTIC_memptr++;
		screendata_tally <<= 8;
		screendata_tally |= screendata;
		if (IS_ZERO_ULONG(t_pm_scanline_ptr))
			DRAW_ARTIF
		else {
			screendata = ANTIC_memptr[-2];
			DO_PMG_HIRES(screendata)
		}
		t_pm_scanline_ptr++;
	CHAR_LOOP_END
	do_border();
}

void draw_antic_f_gtia9(int nchars, UBYTE *ANTIC_memptr, UWORD *ptr, ULONG *t_pm_scanline_ptr)
{
	CHAR_LOOP_BEGIN
		UBYTE screendata = *ANTIC_memptr++;
		DO_GTIA_BYTE(ptr, lookup_gtia9, screendata)
		if (IS_ZERO_ULONG(t_pm_scanline_ptr))
			ptr += 4;
		else {
			UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
			int k = 4;
			UBYTE pm_reg;
			do {
				pm_reg = pm_lookup_ptr[*c_pm_scanline_ptr++];
				if (pm_reg) {
					if (pm_reg == L_PF3) {
						UBYTE tmp = k > 2 ? screendata >> 4 : screendata & 0xf;
#ifdef USE_COLOUR_TRANSLATION_TABLE
						*ptr = colour_translation_table[tmp | COLPF3];
#else
						*ptr = tmp | ((UWORD)tmp << 8) | cl_lookup[C_PF3];
#endif
					}
					else
						*ptr = COLOUR(pm_reg);
				}
				ptr++;
			} while (--k);
		}
		t_pm_scanline_ptr++;
	CHAR_LOOP_END
	do_border();
}

void draw_antic_f_gtia10(int nchars, UBYTE *ANTIC_memptr, UWORD *ptr, ULONG *t_pm_scanline_ptr)
{
#ifdef UNALIGNED_LONG_OK
	ULONG lookup_gtia10[16];
	lookup_gtia10[0] = cl_lookup[C_PM0] | (cl_lookup[C_PM0] << 16);
	lookup_gtia10[1] = cl_lookup[C_PM1] | (cl_lookup[C_PM1] << 16);
	lookup_gtia10[2] = cl_lookup[C_PM2] | (cl_lookup[C_PM2] << 16);
	lookup_gtia10[3] = cl_lookup[C_PM3] | (cl_lookup[C_PM3] << 16);
	lookup_gtia10[12] = lookup_gtia10[4] = cl_lookup[C_PF0] | (cl_lookup[C_PF0] << 16);
	lookup_gtia10[13] = lookup_gtia10[5] = cl_lookup[C_PF1] | (cl_lookup[C_PF1] << 16);
	lookup_gtia10[14] = lookup_gtia10[6] = cl_lookup[C_PF2] | (cl_lookup[C_PF2] << 16);
	lookup_gtia10[15] = lookup_gtia10[7] = cl_lookup[C_PF3] | (cl_lookup[C_PF3] << 16);
	lookup_gtia10[8] = lookup_gtia10[9] = lookup_gtia10[10] = lookup_gtia10[11] = lookup_gtia9[0];
#else
	UWORD lookup_gtia10[16];
	lookup_gtia10[0] = cl_lookup[C_PM0];
	lookup_gtia10[1] = cl_lookup[C_PM1];
	lookup_gtia10[2] = cl_lookup[C_PM2];
	lookup_gtia10[3] = cl_lookup[C_PM3];
	lookup_gtia10[12] = lookup_gtia10[4] = cl_lookup[C_PF0];
	lookup_gtia10[13] = lookup_gtia10[5] = cl_lookup[C_PF1];
	lookup_gtia10[14] = lookup_gtia10[6] = cl_lookup[C_PF2];
	lookup_gtia10[15] = lookup_gtia10[7] = cl_lookup[C_PF3];
	lookup_gtia10[8] = lookup_gtia10[9] = lookup_gtia10[10] = lookup_gtia10[11] = cl_lookup[C_BAK];
#endif
	ptr++;
	t_pm_scanline_ptr = (ULONG *) (((UBYTE *) t_pm_scanline_ptr) + 1);
	CHAR_LOOP_BEGIN
		UBYTE screendata = *ANTIC_memptr++;
		if (IS_ZERO_ULONG(t_pm_scanline_ptr)) {
			DO_GTIA_BYTE(ptr, lookup_gtia10, screendata)
			ptr += 4;
		}
		else {
			UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
			int pm_pixel;
			int colreg;
			int k = 4;
			UBYTE t_screendata = screendata >> 4;
			do {
				colreg = gtia_10_lookup[t_screendata];
				PF_COLLS(colreg) |= pm_pixel = *c_pm_scanline_ptr++;
				pm_pixel |= gtia_10_pm[t_screendata];
				*ptr++ = COLOUR(pm_lookup_ptr[pm_pixel] | colreg);
				if (k == 3)
					t_screendata = screendata & 0x0f;
			} while (--k);
		}
		t_pm_scanline_ptr++;
	CHAR_LOOP_END
	do_border_gtia10();
}

void draw_antic_f_gtia11(int nchars, UBYTE *ANTIC_memptr, UWORD *ptr, ULONG *t_pm_scanline_ptr)
{
	CHAR_LOOP_BEGIN
		UBYTE screendata = *ANTIC_memptr++;
		DO_GTIA_BYTE(ptr, lookup_gtia11, screendata)
		if (IS_ZERO_ULONG(t_pm_scanline_ptr))
			ptr += 4;
		else {
			UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
			int k = 4;
			UBYTE pm_reg;
			do {
				pm_reg = pm_lookup_ptr[*c_pm_scanline_ptr++];
				if (pm_reg) {
					if (pm_reg == L_PF3) {
						UBYTE tmp = k > 2 ? screendata & 0xf0 : screendata << 4;
#ifdef USE_COLOUR_TRANSLATION_TABLE
						*ptr = colour_translation_table[tmp ? tmp | COLPF3 : COLPF3 & 0xf0];
#else
						*ptr = tmp ? tmp | ((UWORD)tmp << 8) | cl_lookup[C_PF3] : cl_lookup[C_PF3] & 0xf0f0;
#endif
					}
					else
						*ptr = COLOUR(pm_reg);
				}
				ptr++;
			} while (--k);
		}
		t_pm_scanline_ptr++;
	CHAR_LOOP_END
	do_border_gtia11();
}

/* GTIA-switch-to-mode-00 bug
If while drawing line in hi-res mode PRIOR is changed from 0x40..0xff to
0x00..0x3f, GTIA doesn't back to hi-res, but starts generating mode similar
to ANTIC's 0xe, but with colours PF0, PF1, PF2, PF3. */

void draw_antic_f_gtia_bug(int nchars, UBYTE *ANTIC_memptr, UWORD *ptr, ULONG *t_pm_scanline_ptr)
{
	lookup2[0x00] = cl_lookup[C_PF0];
	lookup2[0x40] = lookup2[0x10] = lookup2[0x04] = lookup2[0x01] = cl_lookup[C_PF1];
	lookup2[0x80] = lookup2[0x20] = lookup2[0x08] = lookup2[0x02] = cl_lookup[C_PF2];
	lookup2[0xc0] = lookup2[0x30] = lookup2[0x0c] = lookup2[0x03] = cl_lookup[C_PF3];

	CHAR_LOOP_BEGIN
		UBYTE screendata = *ANTIC_memptr++;
		if (IS_ZERO_ULONG(t_pm_scanline_ptr)) {
			*ptr++ = lookup2[screendata & 0xc0];
			*ptr++ = lookup2[screendata & 0x30];
			*ptr++ = lookup2[screendata & 0x0c];
			*ptr++ = lookup2[screendata & 0x03];
		}
		else {
			UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
			int pm_pixel;
			int colreg;
			int k = 4;
			do {
				colreg = (playfield_lookup + 0x40)[screendata & 0xc0];
				DO_PMG_LORES
				screendata <<= 2;
			} while (--k);
		}
		t_pm_scanline_ptr++;
	CHAR_LOOP_END
	do_border();
}

/* pointer to a function drawing single line of graphics */
typedef void (*draw_antic_function)(int nchars, UBYTE *ANTIC_memptr, UWORD *ptr, ULONG *t_pm_scanline_ptr);

/* tables for all GTIA and ANTIC modes */
draw_antic_function draw_antic_table[4][16] = {
/* normal */
		{ NULL,			NULL,			draw_antic_2,	draw_antic_2,
		draw_antic_4,	draw_antic_4,	draw_antic_6,	draw_antic_6,
		draw_antic_8,	draw_antic_9,	draw_antic_a,	draw_antic_c,
		draw_antic_c,	draw_antic_e,	draw_antic_e,	draw_antic_f},
/* GTIA 9 */
		{ NULL,			NULL,			draw_antic_2_gtia9,	draw_antic_2_gtia9,
/* Only few of below are right... A lot of proper functions must be implemented */
		draw_antic_4,	draw_antic_4,	draw_antic_6,	draw_antic_6,
		draw_antic_8,	draw_antic_9_gtia9,	draw_antic_a,	draw_antic_9_gtia9,
		draw_antic_9_gtia9, draw_antic_e,	draw_antic_e,	draw_antic_f_gtia9},
/* GTIA 10 */
		{ NULL,			NULL,			draw_antic_2_gtia10,	draw_antic_2_gtia10,
		draw_antic_4,	draw_antic_4,	draw_antic_6,	draw_antic_6,
		draw_antic_8,	draw_antic_9_gtia10,	draw_antic_a,	draw_antic_9_gtia10,
		draw_antic_9_gtia10,	draw_antic_e,	draw_antic_e,	draw_antic_f_gtia10},
/* GTIA 11 */
		{ NULL,			NULL,			draw_antic_2_gtia11,	draw_antic_2_gtia11,
		draw_antic_4,	draw_antic_4,	draw_antic_6,	draw_antic_6,
		draw_antic_8,	draw_antic_9_gtia11,	draw_antic_a,	draw_antic_9_gtia11,
		draw_antic_9_gtia11,	draw_antic_e,	draw_antic_e,	draw_antic_f_gtia11}};

/* pointer to current GTIA/ANTIC mode routine */
draw_antic_function draw_antic_ptr = draw_antic_8;

/* pointer to current GTIA mode blank drawing routine */
void (*draw_antic_0_ptr)(void) = draw_antic_0;

/* Artifacting ------------------------------------------------------------ */

void artif_init(void)
{
#define ART_BROWN 0
#define ART_BLUE 1
#define ART_DARK_BROWN 2
#define ART_DARK_BLUE 3
#define ART_BRIGHT_BROWN 4
#define ART_BRIGHT_BLUE 5
#define ART_RED 6
#define ART_GREEN 7
	static UBYTE art_colour_table[4][8] = {
	{ 0x88, 0x14, 0x88, 0x14, 0x8f, 0x1f, 0xbb, 0x5f },	/* brownblue */
	{ 0x14, 0x88, 0x14, 0x88, 0x1f, 0x8f, 0x5f, 0xbb },	/* bluebrown */
	{ 0x46, 0xd6, 0x46, 0xd6, 0x4a, 0xdf, 0xac, 0x4f },	/* greenred */
	{ 0xd6, 0x46, 0xd6, 0x46, 0xdf, 0x4a, 0x4f, 0xac }	/* redgreen */
	};

	int i;
	int j;
	int c;
	UBYTE *art_colours;
	UBYTE q;
	UBYTE art_white;

	if (global_artif_mode == 0) {
		draw_antic_table[0][2] = draw_antic_table[0][3] = draw_antic_2;
		draw_antic_table[0][0xf] = draw_antic_f;
		return;
	}

	draw_antic_table[0][2] = draw_antic_table[0][3] = draw_antic_2_artif;
	draw_antic_table[0][0xf] = draw_antic_f_artif;

	art_colours = (global_artif_mode <= 4 ? art_colour_table[global_artif_mode - 1] : art_colour_table[2]);

	art_reverse_colpf1_save = art_normal_colpf1_save = cl_lookup[C_PF1] & 0x0f0f;
	art_reverse_colpf2_save = art_normal_colpf2_save = cl_lookup[C_PF2];
	art_white = (cl_lookup[C_PF2] & 0xf0) | (cl_lookup[C_PF1] & 0x0f);

	for (i = 0; i <= 255; i++) {
		art_bkmask_normal[i] = 0;
		art_lummask_normal[i] = 0;
		art_bkmask_reverse[255 - i] = 0;
		art_lummask_reverse[255 - i] = 0;

		for (j = 0; j <= 3; j++) {
			q = i << j;
			if (!(q & 0x20)) {
				if ((q & 0xf8) == 0x50)
					c = ART_BLUE;				/* 01010 */
				else if ((q & 0xf8) == 0xD8)
					c = ART_DARK_BLUE;			/* 11011 */
				else {							/* xx0xx */
					((UBYTE *)art_lookup_normal)[(i << 2) + j] = COLPF2;
					((UBYTE *)art_lookup_reverse)[((255 - i) << 2) + j] = art_white;
					((UBYTE *)art_bkmask_normal)[(i << 2) + j] = 0xff;
					((UBYTE *)art_lummask_reverse)[((255 - i) << 2) + j] = 0x0f;
					((UBYTE *)art_bkmask_reverse)[((255 - i) << 2) + j] = 0xf0;
					continue;
				}
			}
			else if (q & 0x40) {
				if (q & 0x10)
					goto colpf1_pixel;			/* x111x */
				else if (q & 0x80) {
					if (q & 0x08)
						c = ART_BRIGHT_BROWN;	/* 11101 */
					else
						goto colpf1_pixel;		/* 11100 */
				}
				else
					c = ART_GREEN;				/* 0110x */
			}
			else if (q & 0x10) {
				if (q & 0x08) {
					if (q & 0x80)
						c = ART_BRIGHT_BROWN;	/* 00111 */
					else
						goto colpf1_pixel;		/* 10111 */
				}
				else
					c = ART_RED;				/* x0110 */
			}
			else
				c = ART_BROWN;					/* x010x */


			((UBYTE *)art_lookup_reverse)[((255 - i) << 2) + j] =
			((UBYTE *)art_lookup_normal)[(i << 2) + j] = art_colours[(j & 1) ^ c];
			continue;

			colpf1_pixel:
			((UBYTE *)art_lookup_normal)[(i << 2) + j] = art_white;
			((UBYTE *)art_lookup_reverse)[((255 - i) << 2) + j] = COLPF2;
			((UBYTE *)art_bkmask_reverse)[((255 - i) << 2) + j] = 0xff;
			((UBYTE *)art_lummask_normal)[(i << 2) + j] = 0x0f;
			((UBYTE *)art_bkmask_normal)[(i << 2) + j] = 0xf0;
		}
	}
}

/* Display List ------------------------------------------------------------ */

UBYTE get_DL_byte(void)
{
	UBYTE result = dGetByte(dlist);
	dlist++;
	if( (dlist & 0x3FF) == 0 )
		dlist -= 0x400;
	xpos++;
	return result;
}

UWORD get_DL_word(void)
{
	UBYTE lsb = get_DL_byte();
	if (player_flickering && ((VDELAY & 0x80) == 0 || ypos & 1))
		GRAFP3 = lsb;
	return (get_DL_byte() << 8) | lsb;
}

/* Real ANTIC doesn't fetch beginning bytes in HSC
   nor screen+47 in wide playfield. This function does. */
void ANTIC_load(void) {
#ifdef PAGED_MEM
	UBYTE *ANTIC_memptr = ANTIC_memory + ANTIC_margin;
	UWORD new_screenaddr = screenaddr + chars_read[md];
	if ((screenaddr ^ new_screenaddr) & 0xf000) {
		do
			*ANTIC_memptr++ = dGetByte(screenaddr++);
		while (screenaddr & 0xfff);
		screenaddr -= 0x1000;
		new_screenaddr -= 0x1000;
	}
	while (screenaddr < new_screenaddr)
		*ANTIC_memptr++ = dGetByte(screenaddr++);
#else
	UWORD new_screenaddr = screenaddr + chars_read[md];
	if ((screenaddr ^ new_screenaddr) & 0xf000) {
		int bytes = (-screenaddr) & 0xfff;
		memcpy(ANTIC_memory + ANTIC_margin, memory + screenaddr, bytes);
		if (new_screenaddr & 0xfff)
			memcpy(ANTIC_memory + ANTIC_margin + bytes, memory + screenaddr + bytes - 0x1000, new_screenaddr & 0xfff);
		screenaddr = new_screenaddr - 0x1000;
	}
	else {
		memcpy(ANTIC_memory + ANTIC_margin, memory + screenaddr, chars_read[md]);
		screenaddr = new_screenaddr;
	}
#endif
}

#ifndef NO_CYCLE_EXACT
int scrn_ofs = -1;
#endif

/* This function emulates one frame drawing screen at atari_screen */
void ANTIC_RunDisplayList(void)
{
	static UBYTE mode_type[32] = {
	NORMAL0, NORMAL0, NORMAL0, NORMAL0, NORMAL0, NORMAL0, NORMAL1, NORMAL1,
	NORMAL2, NORMAL2, NORMAL1, NORMAL1, NORMAL1, NORMAL0, NORMAL0, NORMAL0,
	SCROLL0, SCROLL0, SCROLL0, SCROLL0, SCROLL0, SCROLL0, SCROLL1, SCROLL1,
	SCROLL2, SCROLL2, SCROLL1, SCROLL1, SCROLL1, SCROLL0, SCROLL0, SCROLL0};
	static UBYTE normal_lastline[16] =
	{ 0, 0, 7, 9, 7, 15, 7, 15, 7, 3, 3, 1, 0, 1, 0, 0 };
	UBYTE vscrol_flag = FALSE;
	UBYTE no_jvb = TRUE;
	UBYTE need_load;
#ifndef NO_GTIA11_DELAY
	int delayed_gtia11 = 250;
#endif

	GTIA_Triggers();
	PEN_X = Atari_PEN(0);
	PEN_Y = Atari_PEN(1);

	ypos = 0;
	do {
		POKEY_Scanline();		/* check and generate IRQ */
		OVERSCREEN_LINE;
	} while (ypos < 8);

	scrn_ptr = (UWORD *) atari_screen;
#ifndef NO_CYCLE_EXACT
	scrn_ofs = -1;
#endif
	need_dl = TRUE;
	do {
		if (light_pen_enabled && ypos >> 1 == PEN_Y) {
			light_pen_x = PEN_X;
			light_pen_y = PEN_Y;
			if (GRACTL & 4)
				TRIG_latch[0] = 0;
		}

		POKEY_Scanline();		/* check and generate IRQ */
		pmg_dma();

		need_load = FALSE;
		if (need_dl) {
			if (DMACTL & 0x20) {
				IR = get_DL_byte();
				anticmode = IR & 0xf;
				/* PMG flickering :-) */
				if (missile_flickering)
					GRAFM = ypos & 1 ? IR : ((GRAFM ^ IR) & hold_missiles_tab[VDELAY & 0xf]) ^ IR;
				if (player_flickering) {
					UBYTE hold = ypos & 1 ? 0 : VDELAY;
					if ((hold & 0x10) == 0)
						GRAFP0 = dGetByte((UWORD)(regPC - xpos + 8));
					if ((hold & 0x20) == 0)
						GRAFP1 = dGetByte((UWORD)(regPC - xpos + 9));
					if ((hold & 0x40) == 0)
						GRAFP2 = dGetByte((UWORD)(regPC - xpos + 10));
					if ((hold & 0x80) == 0)
						GRAFP3 = dGetByte((UWORD)(regPC - xpos + 11));
				}
			}
			else
				IR &= 0x7f;	/* repeat last instruction, but don't generate DLI */

			dctr = 0;
			need_dl = FALSE;
			vscrol_off = FALSE;

			switch (anticmode) {
			case 0x00:
				lastline = (IR >> 4) & 7;
				if (vscrol_flag) {
					lastline = VSCROL;
					vscrol_flag = FALSE;
					vscrol_off = TRUE;
				}
				break;
			case 0x01:
				lastline = 0;
				if (IR & 0x40 && DMACTL & 0x20) {
					dlist = get_DL_word();
					anticmode = 0;
					no_jvb = FALSE;
				}
				else
					if (vscrol_flag) {
						lastline = VSCROL;
						vscrol_flag = FALSE;
						vscrol_off = TRUE;
					}
				break;
			default:
				lastline = normal_lastline[anticmode];
				if (IR & 0x20) {
					if (!vscrol_flag) {
						GO(VSCON_C);
						dctr = VSCROL;
						vscrol_flag = TRUE;
					}
				}
				else if (vscrol_flag) {
					lastline = VSCROL;
					vscrol_flag = FALSE;
					vscrol_off = TRUE;
				}
				if (IR & 0x40 && DMACTL & 0x20)
					screenaddr = get_DL_word();
				md = mode_type[IR & 0x1f];
				need_load = TRUE;
				draw_antic_ptr = draw_antic_table[(PRIOR & 0xc0) >> 6][anticmode];
				break;
			}
		}

		if (anticmode == 1 && DMACTL & 0x20)
			dlist = get_DL_word();

		if (dctr == lastline) {
			if (no_jvb)
				need_dl = TRUE;
			if (IR & 0x80) {
				GO(NMIST_C);
				NMIST = 0x9f;
				if(NMIEN & 0x80) {
					GO(NMI_C);
					NMI();
				}
			}
		}

		if (!draw_display) {
			xpos += DMAR;
			if (anticmode < 2 || (DMACTL & 3) == 0) {
				GOEOL;
				if (no_jvb) {
					dctr++;
					dctr &= 0xf;
				}
				continue;
			}
			if (need_load) {
				xpos += load_cycles[md];
				if (anticmode <= 5)	/* extra cycles in font modes */
					xpos += before_cycles[md] - extra_cycles[md];
			}
			if (anticmode < 8)
				xpos += font_cycles[md];
			GOEOL;
			dctr++;
			dctr &= 0xf;
			continue;
		}

		if (need_load && anticmode <= 5 && DMACTL & 3)
			xpos += before_cycles[md];

#ifndef NO_CYCLE_EXACT
		if ((anticmode < 2 || (DMACTL & 3) == 0)
			 && (GRAFP0 == 0 || HPOSP0 <= 0x0c || HPOSP0 >= 0xd4)
			 && (GRAFP1 == 0 || HPOSP1 <= 0x0c || HPOSP1 >= 0xd4)
			 && (GRAFP2 == 0 || HPOSP2 <= 0x0c || HPOSP2 >= 0xd4)
			 && (GRAFP3 == 0 || HPOSP3 <= 0x0c || HPOSP3 >= 0xd4)
			 && GRAFM == 0
			 && PRIOR < 0x80) {
			xpos += DMAR;
			scrn_ofs = 8 * LCHOP;
			GOEOL;
			memset(((UBYTE *) scrn_ptr) + scrn_ofs, COLBK, (48 - RCHOP) * 8 - scrn_ofs);
			scrn_ofs = -1;
			scrn_ptr += ATARI_WIDTH / 2;
			if (no_jvb) {
				dctr++;
				dctr &= 0xf;
			}
			continue;
		}
#endif

		GO(SCR_C);
		new_pm_scanline();

		xpos += DMAR;

		if (anticmode < 2 || (DMACTL & 3) == 0) {
			draw_antic_0_ptr();
			GOEOL;
			scrn_ptr += ATARI_WIDTH / 2;
			if (no_jvb) {
				dctr++;
				dctr &= 0xf;
			}
			continue;
		}

		if (need_load) {
			ANTIC_load();
			xpos += load_cycles[md];
			if (anticmode <= 5)	/* extra cycles in font modes */
				xpos -= extra_cycles[md];
		}

		draw_antic_ptr(chars_displayed[md],
			ANTIC_memory + ANTIC_margin + ch_offset[md],
			scrn_ptr + x_min[md],
			(ULONG *) &pm_scanline[x_min[md]]);
#ifndef NO_GTIA11_DELAY
		if (PRIOR >= 0xc0)
			delayed_gtia11 = ypos + 1;
		else
			if (ypos == delayed_gtia11) {
				ULONG *ptr = (ULONG *) (scrn_ptr + 4 * LCHOP);
				int k = 2 * (48 - LCHOP - RCHOP);
				do {
					*ptr |= ptr[-ATARI_WIDTH / 4];
					ptr++;
				} while (--k);
			}
#endif
		GOEOL;
		scrn_ptr += ATARI_WIDTH / 2;
		dctr++;
		dctr &= 0xf;
	} while (ypos < (ATARI_HEIGHT + 8));

	POKEY_Scanline();		/* check and generate IRQ */
	GO(NMIST_C);
	NMIST = 0x5f;				/* Set VBLANK */
	if (NMIEN & 0x40) {
		GO(NMI_C);
		NMI();
	}
	xpos += DMAR;
	GOEOL;

	do {
		POKEY_Scanline();		/* check and generate IRQ */
		OVERSCREEN_LINE;
	} while (ypos < max_ypos);

	POKEY_Frame();				/* update random_frame_counter */
}

/* ANTIC registers --------------------------------------------------------- */

UBYTE ANTIC_GetByte(UWORD addr)
{
	switch (addr & 0xf) {
	case _VCOUNT:
		return ypos >> 1;
	case _PENH:
		return light_pen_x;
	case _PENV:
		return light_pen_y;
	case _NMIST:
		return NMIST;
	default:
		return 0xff;
	}
}

/* GTIA calls it on write to PRIOR */
void set_prior(UBYTE byte) {
	if ((byte ^ PRIOR) & 0x0f) {
#ifdef USE_COLOUR_TRANSLATION_TABLE
		UBYTE col = 0;
		UBYTE col2 = 0;
		UBYTE hi;
		UBYTE hi2;
		if ((byte & 3) == 0) {
			col = COLPF0;
			col2 = COLPF1;
		}
		if ((byte & 0xc) == 0) {
			cl_lookup[C_PF0 | C_PM0] = colour_translation_table[col | COLPM0];
			cl_lookup[C_PF0 | C_PM1] = colour_translation_table[col | COLPM1];
			cl_lookup[C_PF0 | C_PM01] = colour_translation_table[col | COLPM0 | COLPM1];
			cl_lookup[C_PF1 | C_PM0] = colour_translation_table[col2 | COLPM0];
			cl_lookup[C_PF1 | C_PM1] = colour_translation_table[col2 | COLPM1];
			cl_lookup[C_PF1 | C_PM01] = colour_translation_table[col2 | COLPM0 | COLPM1];
		}
		else {
			cl_lookup[C_PF0 | C_PM01] = cl_lookup[C_PF0 | C_PM1] = cl_lookup[C_PF0 | C_PM0] = colour_translation_table[col];
			cl_lookup[C_PF1 | C_PM01] = cl_lookup[C_PF1 | C_PM1] = cl_lookup[C_PF1 | C_PM0] = colour_translation_table[col2];
		}
		if (byte & 4) {
			cl_lookup[C_PF2 | C_PM01] = cl_lookup[C_PF2 | C_PM1] = cl_lookup[C_PF2 | C_PM0] = cl_lookup[C_PF2];
			cl_lookup[C_PF3 | C_PM01] = cl_lookup[C_PF3 | C_PM1] = cl_lookup[C_PF3 | C_PM0] = cl_lookup[C_PF3];
			cl_lookup[C_HI2 | C_PM01] = cl_lookup[C_HI2 | C_PM1] = cl_lookup[C_HI2 | C_PM0] = cl_lookup[C_HI2];
		}
		else {
			cl_lookup[C_PF3 | C_PM0] = cl_lookup[C_PF2 | C_PM0] = cl_lookup[C_PM0];
			cl_lookup[C_PF3 | C_PM1] = cl_lookup[C_PF2 | C_PM1] = cl_lookup[C_PM1];
			cl_lookup[C_PF3 | C_PM01] = cl_lookup[C_PF2 | C_PM01] = cl_lookup[C_PM01];
			cl_lookup[C_HI2 | C_PM0] = colour_translation_table[(COLPM0 & 0xf0) | (COLPF1 & 0xf)];
			cl_lookup[C_HI2 | C_PM1] = colour_translation_table[(COLPM1 & 0xf0) | (COLPF1 & 0xf)];
			cl_lookup[C_HI2 | C_PM01] = colour_translation_table[((COLPM0 | COLPM1) & 0xf0) | (COLPF1 & 0xf)];
		}
		col = col2 = 0;
		hi = hi2 = COLPF1 & 0xf;
		cl_lookup[C_BLACK - C_PF2 + C_HI2] = colour_translation_table[hi];
		if ((byte & 9) == 0) {
			col = COLPF2;
			col2 = COLPF3;
			hi |= col & 0xf0;
			hi2 |= col2 & 0xf0;
		}
		if ((byte & 6) == 0) {
			cl_lookup[C_PF2 | C_PM2] = colour_translation_table[col | COLPM2];
			cl_lookup[C_PF2 | C_PM3] = colour_translation_table[col | COLPM3];
			cl_lookup[C_PF2 | C_PM23] = colour_translation_table[col | COLPM2 | COLPM3];
			cl_lookup[C_PF3 | C_PM2] = colour_translation_table[col2 | COLPM2];
			cl_lookup[C_PF3 | C_PM3] = colour_translation_table[col2 | COLPM3];
			cl_lookup[C_PF3 | C_PM23] = colour_translation_table[col2 | COLPM2 | COLPM3];
			cl_lookup[C_HI2 | C_PM2] = colour_translation_table[hi | (COLPM2 & 0xf0)];
			cl_lookup[C_HI2 | C_PM3] = colour_translation_table[hi | (COLPM3 & 0xf0)];
			cl_lookup[C_HI2 | C_PM23] = colour_translation_table[hi | ((COLPM2 | COLPM3) & 0xf0)];
			cl_lookup[C_HI2 | C_PM25] = colour_translation_table[hi2 | (COLPM2 & 0xf0)];
			cl_lookup[C_HI2 | C_PM35] = colour_translation_table[hi2 | (COLPM3 & 0xf0)];
			cl_lookup[C_HI2 | C_PM235] = colour_translation_table[hi2 | ((COLPM2 | COLPM3) & 0xf0)];
		}
		else {
			cl_lookup[C_PF2 | C_PM23] = cl_lookup[C_PF2 | C_PM3] = cl_lookup[C_PF2 | C_PM2] = colour_translation_table[col];
			cl_lookup[C_PF3 | C_PM23] = cl_lookup[C_PF3 | C_PM3] = cl_lookup[C_PF3 | C_PM2] = colour_translation_table[col2];
			cl_lookup[C_HI2 | C_PM23] = cl_lookup[C_HI2 | C_PM3] = cl_lookup[C_HI2 | C_PM2] = colour_translation_table[hi];
		}
#else /* USE_COLOUR_TRANSLATION_TABLE */
		UWORD cword = 0;
		UWORD cword2 = 0;
		if ((byte & 3) == 0) {
			cword = cl_lookup[C_PF0];
			cword2 = cl_lookup[C_PF1];
		}
		if ((byte & 0xc) == 0) {
			cl_lookup[C_PF0 | C_PM0] = cword | cl_lookup[C_PM0];
			cl_lookup[C_PF0 | C_PM1] = cword | cl_lookup[C_PM1];
			cl_lookup[C_PF0 | C_PM01] = cword | cl_lookup[C_PM01];
			cl_lookup[C_PF1 | C_PM0] = cword2 | cl_lookup[C_PM0];
			cl_lookup[C_PF1 | C_PM1] = cword2 | cl_lookup[C_PM1];
			cl_lookup[C_PF1 | C_PM01] = cword2 | cl_lookup[C_PM01];
		}
		else {
			cl_lookup[C_PF0 | C_PM01] = cl_lookup[C_PF0 | C_PM1] = cl_lookup[C_PF0 | C_PM0] = cword;
			cl_lookup[C_PF1 | C_PM01] = cl_lookup[C_PF1 | C_PM1] = cl_lookup[C_PF1 | C_PM0] = cword2;
		}
		if (byte & 4) {
			cl_lookup[C_PF2 | C_PM01] = cl_lookup[C_PF2 | C_PM1] = cl_lookup[C_PF2 | C_PM0] = cl_lookup[C_PF2];
			cl_lookup[C_PF3 | C_PM01] = cl_lookup[C_PF3 | C_PM1] = cl_lookup[C_PF3 | C_PM0] = cl_lookup[C_PF3];
		}
		else {
			cl_lookup[C_PF3 | C_PM0] = cl_lookup[C_PF2 | C_PM0] = cl_lookup[C_PM0];
			cl_lookup[C_PF3 | C_PM1] = cl_lookup[C_PF2 | C_PM1] = cl_lookup[C_PM1];
			cl_lookup[C_PF3 | C_PM01] = cl_lookup[C_PF2 | C_PM01] = cl_lookup[C_PM01];
		}
		cword = cword2 = 0;
		if ((byte & 9) == 0) {
			cword = cl_lookup[C_PF2];
			cword2 = cl_lookup[C_PF3];
		}
		if ((byte & 6) == 0) {
			cl_lookup[C_PF2 | C_PM2] = cword | cl_lookup[C_PM2];
			cl_lookup[C_PF2 | C_PM3] = cword | cl_lookup[C_PM3];
			cl_lookup[C_PF2 | C_PM23] = cword | cl_lookup[C_PM23];
			cl_lookup[C_PF3 | C_PM2] = cword2 | cl_lookup[C_PM2];
			cl_lookup[C_PF3 | C_PM3] = cword2 | cl_lookup[C_PM3];
			cl_lookup[C_PF3 | C_PM23] = cword2 | cl_lookup[C_PM23];
		}
		else {
			cl_lookup[C_PF2 | C_PM23] = cl_lookup[C_PF2 | C_PM3] = cl_lookup[C_PF2 | C_PM2] = cword;
			cl_lookup[C_PF3 | C_PM23] = cl_lookup[C_PF3 | C_PM3] = cl_lookup[C_PF3 | C_PM2] = cword2;
		}
#endif /* USE_COLOUR_TRANSLATION_TABLE */
		if (byte & 1) {
			cl_lookup[C_PF1 | C_PM2] = cl_lookup[C_PF0 | C_PM2] = cl_lookup[C_PM2];
			cl_lookup[C_PF1 | C_PM3] = cl_lookup[C_PF0 | C_PM3] = cl_lookup[C_PM3];
			cl_lookup[C_PF1 | C_PM23] = cl_lookup[C_PF0 | C_PM23] = cl_lookup[C_PM23];
		}
		else {
			cl_lookup[C_PF0 | C_PM23] = cl_lookup[C_PF0 | C_PM3] = cl_lookup[C_PF0 | C_PM2] = cl_lookup[C_PF0];
			cl_lookup[C_PF1 | C_PM23] = cl_lookup[C_PF1 | C_PM3] = cl_lookup[C_PF1 | C_PM2] = cl_lookup[C_PF1];
		}
		if ((byte & 0xf) == 0xc) {
			cl_lookup[C_PF0 | C_PM0123] = cl_lookup[C_PF0 | C_PM123] = cl_lookup[C_PF0 | C_PM023] = cl_lookup[C_PF0];
			cl_lookup[C_PF1 | C_PM0123] = cl_lookup[C_PF1 | C_PM123] = cl_lookup[C_PF1 | C_PM023] = cl_lookup[C_PF1];
		}
		else
			cl_lookup[C_PF0 | C_PM0123] = cl_lookup[C_PF0 | C_PM123] = cl_lookup[C_PF0 | C_PM023] =
			cl_lookup[C_PF1 | C_PM0123] = cl_lookup[C_PF1 | C_PM123] = cl_lookup[C_PF1 | C_PM023] = COLOUR_BLACK;
		if (byte & 0xf) {
			cl_lookup[C_PF0 | C_PM25] = cl_lookup[C_PF0];
			cl_lookup[C_PF1 | C_PM25] = cl_lookup[C_PF1];
			cl_lookup[C_PF3 | C_PM25] = cl_lookup[C_PF2 | C_PM25] = cl_lookup[C_PM25] = COLOUR_BLACK;
		}
		else {
			cl_lookup[C_PF0 | C_PM235] = cl_lookup[C_PF0 | C_PM35] = cl_lookup[C_PF0 | C_PM25] =
			cl_lookup[C_PF1 | C_PM235] = cl_lookup[C_PF1 | C_PM35] = cl_lookup[C_PF1 | C_PM25] = cl_lookup[C_PF3];
			cl_lookup[C_PF3 | C_PM25] = cl_lookup[C_PF2 | C_PM25] = cl_lookup[C_PM25] = cl_lookup[C_PF3 | C_PM2];
			cl_lookup[C_PF3 | C_PM35] = cl_lookup[C_PF2 | C_PM35] = cl_lookup[C_PM35] = cl_lookup[C_PF3 | C_PM3];
			cl_lookup[C_PF3 | C_PM235] = cl_lookup[C_PF2 | C_PM235] = cl_lookup[C_PM235] = cl_lookup[C_PF3 | C_PM23];
		}
	}
	pm_lookup_ptr = pm_lookup_table[prior_to_pm_lookup[byte & 0x3f]];
	draw_antic_0_ptr = byte < 0x80 ? draw_antic_0 : byte < 0xc0 ? draw_antic_0_gtia10 : draw_antic_0_gtia11;
	if (byte < 0x40 && PRIOR >= 0x40 && anticmode == 0xf && xpos >= ((DMACTL & 3) == 3 ? 20 : 22))
		draw_antic_ptr = draw_antic_f_gtia_bug;
	else
		draw_antic_ptr = draw_antic_table[(byte & 0xc0) >> 6][anticmode];
}

void ANTIC_PutByte(UWORD addr, UBYTE byte)
{
	switch (addr & 0xf) {
	case _CHACTL:
		if ((CHACTL ^ byte) & 4)
			chbase_20 ^= 7;
		CHACTL = byte;
		invert_mask = byte & 2 ? 0x80 : 0;
		blank_mask = byte & 1 ? 0xe0 : 0x60;
		break;
	case _DLISTL:
		dlist = (dlist & 0xff00) | byte;
		break;
	case _DLISTH:
		dlist = (dlist & 0x00ff) | (byte << 8);
		break;
	case _DMACTL:
		DMACTL = byte;

		switch (byte & 0x03) {
		case 0x00:
			/* no ANTIC_load when screen off */
			/* chars_read[NORMAL0] = 0;
			chars_read[NORMAL1] = 0;
			chars_read[NORMAL2] = 0;
			chars_read[SCROLL0] = 0;
			chars_read[SCROLL1] = 0;
			chars_read[SCROLL2] = 0; */
			/* no draw_antic_* when screen off */
			/* chars_displayed[NORMAL0] = 0;
			chars_displayed[NORMAL1] = 0;
			chars_displayed[NORMAL2] = 0;
			chars_displayed[SCROLL0] = 0;
			chars_displayed[SCROLL1] = 0;
			chars_displayed[SCROLL2] = 0;
			x_min[NORMAL0] = 0;
			x_min[NORMAL1] = 0;
			x_min[NORMAL2] = 0;
			x_min[SCROLL0] = 0;
			x_min[SCROLL1] = 0;
			x_min[SCROLL2] = 0;
			ch_offset[NORMAL0] = 0;
			ch_offset[NORMAL1] = 0;
			ch_offset[NORMAL2] = 0;
			ch_offset[SCROLL0] = 0;
			ch_offset[SCROLL1] = 0;
			ch_offset[SCROLL2] = 0; */
			/* no borders when screen off, only background */
			/* left_border_chars = 48 - LCHOP - RCHOP;
			right_border_start = 0; */
			break;
		case 0x01:
			chars_read[NORMAL0] = 32;
			chars_read[NORMAL1] = 16;
			chars_read[NORMAL2] = 8;
			chars_read[SCROLL0] = 40;
			chars_read[SCROLL1] = 20;
			chars_read[SCROLL2] = 10;
			chars_displayed[NORMAL0] = 32;
			chars_displayed[NORMAL1] = 16;
			chars_displayed[NORMAL2] = 8;
			x_min[NORMAL0] = 32;
			x_min[NORMAL1] = 32;
			x_min[NORMAL2] = 32;
			ch_offset[NORMAL0] = 0;
			ch_offset[NORMAL1] = 0;
			ch_offset[NORMAL2] = 0;
			font_cycles[NORMAL0] = load_cycles[NORMAL0] = 32;
			font_cycles[NORMAL1] = load_cycles[NORMAL1] = 16;
			load_cycles[NORMAL2] = 8;
			before_cycles[NORMAL0] = BEFORE_CYCLES;
			before_cycles[SCROLL0] = BEFORE_CYCLES + 8;
			extra_cycles[NORMAL0] = 7 + BEFORE_CYCLES;
			extra_cycles[SCROLL0] = 8 + BEFORE_CYCLES + 8;
			left_border_chars = 8 - LCHOP;
			right_border_start = (ATARI_WIDTH - 64) / 2;
			break;
		case 0x02:
			chars_read[NORMAL0] = 40;
			chars_read[NORMAL1] = 20;
			chars_read[NORMAL2] = 10;
			chars_read[SCROLL0] = 48;
			chars_read[SCROLL1] = 24;
			chars_read[SCROLL2] = 12;
			chars_displayed[NORMAL0] = 40;
			chars_displayed[NORMAL1] = 20;
			chars_displayed[NORMAL2] = 10;
			x_min[NORMAL0] = 16;
			x_min[NORMAL1] = 16;
			x_min[NORMAL2] = 16;
			ch_offset[NORMAL0] = 0;
			ch_offset[NORMAL1] = 0;
			ch_offset[NORMAL2] = 0;
			font_cycles[NORMAL0] = load_cycles[NORMAL0] = 40;
			font_cycles[NORMAL1] = load_cycles[NORMAL1] = 20;
			load_cycles[NORMAL2] = 10;
			before_cycles[NORMAL0] = BEFORE_CYCLES + 8;
			before_cycles[SCROLL0] = BEFORE_CYCLES + 16;
			extra_cycles[NORMAL0] = 8 + BEFORE_CYCLES + 8;
			extra_cycles[SCROLL0] = 7 + BEFORE_CYCLES + 16;
			left_border_chars = 4 - LCHOP;
			right_border_start = (ATARI_WIDTH - 32) / 2;
			break;
		case 0x03:
			chars_read[NORMAL0] = 48;
			chars_read[NORMAL1] = 24;
			chars_read[NORMAL2] = 12;
			chars_read[SCROLL0] = 48;
			chars_read[SCROLL1] = 24;
			chars_read[SCROLL2] = 12;
			chars_displayed[NORMAL0] = 42;
			chars_displayed[NORMAL1] = 22;
			chars_displayed[NORMAL2] = 12;
			x_min[NORMAL0] = 12;
			x_min[NORMAL1] = 8;
			x_min[NORMAL2] = 0;
			ch_offset[NORMAL0] = 3;
			ch_offset[NORMAL1] = 1;
			ch_offset[NORMAL2] = 0;
			font_cycles[NORMAL0] = load_cycles[NORMAL0] = 47;
			font_cycles[NORMAL1] = load_cycles[NORMAL1] = 24;
			load_cycles[NORMAL2] = 12;
			before_cycles[NORMAL0] = BEFORE_CYCLES + 16;
			before_cycles[SCROLL0] = BEFORE_CYCLES + 16;
			extra_cycles[NORMAL0] = 7 + BEFORE_CYCLES + 16;
			extra_cycles[SCROLL0] = 7 + BEFORE_CYCLES + 16;
			left_border_chars = 3 - LCHOP;
			right_border_start = (ATARI_WIDTH - 8) / 2;
			break;
		}

		missile_dma_enabled = (byte & 0x0c);	/* no player dma without missile */
		player_dma_enabled = (byte & 0x08);
		singleline = (byte & 0x10);
		player_flickering = ((player_dma_enabled | player_gra_enabled) == 0x02);
		missile_flickering = ((missile_dma_enabled | missile_gra_enabled) == 0x01);

		byte = HSCROL;	/* update horizontal scroll data */

	case _HSCROL:
		HSCROL = byte &= 0x0f;
		if (DMACTL & 3) {
			chars_displayed[SCROLL0] = chars_displayed[NORMAL0];
			ch_offset[SCROLL0] = 4 - (byte >> 2);
			x_min[SCROLL0] = x_min[NORMAL0];
			if (byte & 3) {
				x_min[SCROLL0] += (byte & 3) - 4;
				chars_displayed[SCROLL0]++;
				ch_offset[SCROLL0]--;
			}
			chars_displayed[SCROLL2] = chars_displayed[NORMAL2];
			if ((DMACTL & 3) == 3) {	/* wide playfield */
				ch_offset[SCROLL0]--;
				if (byte == 4 || byte == 12)
					chars_displayed[SCROLL1] = 21;
				else
					chars_displayed[SCROLL1] = 22;
				if (byte <= 4) {
					x_min[SCROLL1] = byte + 8;
					ch_offset[SCROLL1] = 1;
				}
				else if (byte <= 12) {
					x_min[SCROLL1] = byte;
					ch_offset[SCROLL1] = 0;
				}
				else {
					x_min[SCROLL1] = byte - 8;
					ch_offset[SCROLL1] = -1;
				}
				x_min[SCROLL2] = byte;
				ch_offset[SCROLL2] = 0;
			}
			else {
				chars_displayed[SCROLL1] = chars_displayed[NORMAL1];
				ch_offset[SCROLL1] = 2 - (byte >> 3);
				x_min[SCROLL1] = x_min[NORMAL0];
				if (byte) {
					if (byte & 7) {
						x_min[SCROLL1] += (byte & 7) - 8;
						chars_displayed[SCROLL1]++;
						ch_offset[SCROLL1]--;
					}
					x_min[SCROLL2] = x_min[NORMAL2] + byte - 16;
					chars_displayed[SCROLL2]++;
					ch_offset[SCROLL2] = 0;
				}
				else {
					x_min[SCROLL2] = x_min[NORMAL2];
					ch_offset[SCROLL2] = 1;
				}
			}

			if (DMACTL & 2) {		/* normal & wide playfield */
				load_cycles[SCROLL0] = 47 - (byte >> 2);
				font_cycles[SCROLL0] = (47 * 4 + 1 - byte) >> 2;
				load_cycles[SCROLL1] = (24 * 8 + 3 - byte) >> 3;
				font_cycles[SCROLL1] = (24 * 8 + 1 - byte) >> 3;
				load_cycles[SCROLL2] = byte < 0xc ? 12 : 11;
			}
			else {					/* narrow playfield */
				font_cycles[SCROLL0] = load_cycles[SCROLL0] = 40;
				font_cycles[SCROLL1] = load_cycles[SCROLL1] = 20;
				load_cycles[SCROLL2] = 16;
			}
		}
		break;
	case _VSCROL:
		VSCROL = byte & 0x0f;
		if (vscrol_off) {
			lastline = VSCROL;
			if (xpos < VSCOF_C)
				need_dl = dctr == lastline;
		}
		break;
	case _PMBASE:
		PMBASE = byte;
		pmbase_d = (byte & 0xfc) << 8;
		pmbase_s = pmbase_d & 0xf8ff;
		break;
	case _CHBASE:
		CHBASE = byte;
		chbase_20 = (byte & 0xfe) << 8;
		if (CHACTL & 4)
			chbase_20 ^= 7;
		break;
	case _WSYNC:
		if (xpos <= WSYNC_C && xpos_limit >= WSYNC_C)
			xpos = WSYNC_C;
		else {
			wsync_halt = TRUE;
			xpos = xpos_limit;
		}
		break;
	case _NMIEN:
		NMIEN = byte;
		break;
	case _NMIRES:
		NMIST = 0x1f;
		break;
	}
}

/* State ------------------------------------------------------------------- */

void AnticStateSave( void )
{
	SaveUBYTE( &DMACTL, 1 );
	SaveUBYTE( &CHACTL, 1 );
	SaveUBYTE( &HSCROL, 1 );
	SaveUBYTE( &VSCROL, 1 );
	SaveUBYTE( &PMBASE, 1 );
	SaveUBYTE( &CHBASE, 1 );
	SaveUBYTE( &NMIEN, 1 );
	SaveUBYTE( &NMIST, 1 );
	SaveUBYTE( &IR, 1 );
	SaveUBYTE( &anticmode, 1 );
	SaveUBYTE( &dctr, 1 );
	SaveUBYTE( &lastline, 1 );
	SaveUBYTE( &need_dl, 1 );
	SaveUBYTE( &vscrol_off, 1 );

	SaveUWORD( &dlist, 1 );
	SaveUWORD( &screenaddr, 1 );
	
	SaveINT( &xpos, 1 );
	SaveINT( &xpos_limit, 1 );
	SaveINT( &ypos, 1 );
}

void AnticStateRead( void )
{
	ReadUBYTE( &DMACTL, 1 );
	ReadUBYTE( &CHACTL, 1 );
	ReadUBYTE( &HSCROL, 1 );
	ReadUBYTE( &VSCROL, 1 );
	ReadUBYTE( &PMBASE, 1 );
	ReadUBYTE( &CHBASE, 1 );
	ReadUBYTE( &NMIEN, 1 );
	ReadUBYTE( &NMIST, 1 );
	ReadUBYTE( &IR, 1 );
	ReadUBYTE( &anticmode, 1 );
	ReadUBYTE( &dctr, 1 );
	ReadUBYTE( &lastline, 1 );
	ReadUBYTE( &need_dl, 1 );
	ReadUBYTE( &vscrol_off, 1 );

	ReadUWORD( &dlist, 1 );
	ReadUWORD( &screenaddr, 1 );
	
	ReadINT( &xpos, 1 );
	ReadINT( &xpos_limit, 1 );
	ReadINT( &ypos, 1 );

	ANTIC_PutByte(_DMACTL, DMACTL);
	ANTIC_PutByte(_CHACTL, CHACTL);
	ANTIC_PutByte(_PMBASE, PMBASE);
	ANTIC_PutByte(_CHBASE, CHBASE);
}

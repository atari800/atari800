#ifndef _ATARI_H_
#define	_ATARI_H_

#include "config.h"

/*
   =================================
   Define Data Types on Local System
   =================================
 */

#define	SBYTE signed char
#define	SWORD signed short int
#if SIZEOF_LONG > 4
# define	SLONG signed int
#else
# define	SLONG signed long int
#endif

#define	UBYTE unsigned char
#define	UWORD unsigned short int
#if SIZEOF_LONG > 4
# define	ULONG unsigned int
#else
# define	ULONG unsigned long int
#endif

#ifdef SHM
/* #define USE_COLOUR_TRANSLATION_TABLE */
#endif

#define MACHINE_OSA		0
#define MACHINE_OSB		1
#define MACHINE_XLXE	2
#define MACHINE_5200	3
extern int machine_type;

#define RAM_320_RAMBO		320
#define RAM_320_COMPY_SHOP	321
extern int ram_size;

#define TV_PAL 312
#define TV_NTSC 262
extern int tv_mode;				/* now it is simply number of scanlines */

extern int nframes;
extern double deltatime;

extern int verbose;

#ifndef FALSE
#define FALSE	0
#endif
#ifndef TRUE
#define TRUE	1
#endif

#define ATARI_WIDTH  384
#define ATARI_HEIGHT 240

#define ATARI_TITLE  "Atari 800 Emulator, Version 1.3.0"

extern int xpos;
extern int xpos_limit;
#define WSYNC_C	106
#define LINE_C	114
#define DMAR	9
#define max_ypos tv_mode		/* number of scanlines */
/* if tv_mod is enum, max_ypos is (tv_mod == TV_PAL ? 312 : 262) */

extern ULONG *atari_screen;
#ifdef DIRTYRECT
#ifndef CLIENTUPDATE
extern UBYTE *screen_dirty;
#endif
#endif
void entire_screen_dirty();

#ifdef BITPL_SCR
extern ULONG *atari_screen_b;
extern ULONG *atari_screen1;
extern ULONG *atari_screen2;
#endif

extern int screen_visible_x1;
extern int screen_visible_y1;
extern int screen_visible_x2;
extern int screen_visible_y2;

extern unsigned int screenline_cpu_clock;
#define cpu_clock (screenline_cpu_clock + xpos)

/* Escape codes */
enum ESCAPE {
	ESC_SIOV,
/*
 * These are special device escape codes required by the Basic version
 */
	ESC_EHOPEN,
	ESC_EHCLOS,
	ESC_EHREAD,
	ESC_EHWRIT,
	ESC_EHSTAT,
	ESC_EHSPEC,

	ESC_KHOPEN,
	ESC_KHCLOS,
	ESC_KHREAD,
	ESC_KHWRIT,
	ESC_KHSTAT,
	ESC_KHSPEC,

	ESC_BINLOADER_CONT,

/*
 * These are Escape codes for the normal device handlers.
 * Some are never used and some are only sometimes used.
 */

	ESC_PHOPEN = 0xb0,
	ESC_PHCLOS = 0xb1,
	ESC_PHREAD = 0xb2,
	ESC_PHWRIT = 0xb3,
	ESC_PHSTAT = 0xb4,
	ESC_PHSPEC = 0xb5,
	ESC_PHINIT = 0xb6,

	ESC_HHOPEN = 0xc0,
	ESC_HHCLOS = 0xc1,
	ESC_HHREAD = 0xc2,
	ESC_HHWRIT = 0xc3,
	ESC_HHSTAT = 0xc4,
	ESC_HHSPEC = 0xc5,
	ESC_HHINIT = 0xc6
};

typedef void (*EscFunctionType)(void);

void Atari800_AddEsc(UWORD address, UBYTE esc_code, EscFunctionType function);
void Atari800_AddEscRts(UWORD address, UBYTE esc_code, EscFunctionType function);
void Atari800_AddEscRts2(UWORD address, UBYTE esc_code, EscFunctionType function);
void Atari800_RemoveEsc(UBYTE esc_code);
void Atari800_RunEsc(UBYTE esc_code);

UBYTE Atari800_GetByte(UWORD addr);
void Atari800_PutByte(UWORD addr, UBYTE byte);
void Atari800_PatchOS(void);

/* 
   =================
   ATR Info
   =================
 */
 
#define	MAGIC1	0x96
#define	MAGIC2	0x02

struct ATR_Header {
	unsigned char magic1;
	unsigned char magic2;
	unsigned char seccountlo;
	unsigned char seccounthi;
	unsigned char secsizelo;
	unsigned char secsizehi;
	unsigned char hiseccountlo;
	unsigned char hiseccounthi;
	unsigned char gash[7];
	unsigned char writeprotect;
};

/*
   ===========================
   non-standard keyboard codes
   ===========================
 */

#define AKEY_WARMSTART -2
#define AKEY_COLDSTART -3
#define AKEY_EXIT -4
#define AKEY_BREAK -5
#define AKEY_PIL -6
#define AKEY_UI -7
#define AKEY_SCREENSHOT -8
#define AKEY_SCREENSHOT_INTERLACE -9

/*
   ==============
   menu functions
   ==============
 */

#define MENU_DISK		0
#define MENU_CARTRIDGE	1		
#define MENU_RUN		2
#define MENU_SYSTEM		3
#define MENU_SOUND		4
#define MENU_SOUND_RECORDING	5
#define MENU_ARTIF		6
#define MENU_SETTINGS	7
#define MENU_SAVESTATE	8
#define MENU_LOADSTATE	9
#define MENU_PCX		10
#define MENU_PCXI		11
#define MENU_BACK		12
#define MENU_RESETW		13
#define MENU_RESETC		14
#define MENU_MONITOR	15
#define MENU_ABOUT		16
#define MENU_EXIT		17
#define MENU_CASSETTE   18

int Atari800_Initialise(int *argc, char *argv[]);

#define EMULATE_BASIC		0	/* no screen, no interrupts */
#define EMULATE_NO_SCREEN	1	/* don't draw screen */
#define EMULATE_FULL		2	/* normal mode */
void Atari800_Frame(int mode);

void EnablePILL(void);
void Coldstart(void);
void Warmstart(void);
int Atari800_InitialiseMachine(void);
int Atari800_Exit(int run_monitor);
void Atari800_UpdatePatches(void);
void atari_sync(void);

#endif /* _ATARI_H_ */

/*
$Log$
Revision 1.36  2003/02/24 09:32:37  joy
header cleanup

Revision 1.35  2003/02/10 11:22:32  joy
preparing for 1.3.0 release

Revision 1.34  2003/02/09 22:00:18  joy
updated

Revision 1.33  2003/02/09 20:31:27  joy
prepare for tag

Revision 1.32  2003/01/27 14:13:02  joy
Perry's cycle-exact ANTIC/GTIA emulation

Revision 1.31  2003/01/27 13:25:35  joy
updated

Revision 1.30  2002/12/02 12:51:30  joy
version updated

Revision 1.29  2002/08/07 07:48:08  joy
ver++

Revision 1.28  2002/07/08 20:27:14  joy
ver++

Revision 1.27  2002/07/04 22:35:07  vasyl
Added cassette support in main menu

Revision 1.26  2002/03/29 10:39:09  vasyl
Dirty rectangle scheme implementation. All accesses to video memory (except
those in UI_BASIC.C) are converted to macros. Define symbol DIRTYRECT
to enable dirty rectangle tracking. Global array screen_dirty contains dirty
flags (per 8 horizontal pixels).

Revision 1.25  2001/12/31 08:44:53  joy
updated

Revision 1.24  2001/12/14 17:18:54  joy
version++

Revision 1.23  2001/10/29 17:58:49  fox
changed implementation of WSYNC/VCOUNT timing

Revision 1.22  2001/10/10 06:58:31  joy
version++

Revision 1.21  2001/10/03 16:49:04  fox
added screen_visible_* variables

Revision 1.20  2001/10/03 16:39:54  fox
rewritten escape codes handling

Revision 1.19  2001/09/22 09:21:33  fox
declared nframes and deltatime, AKEY_SHFT etc. moved to input.h

Revision 1.18  2001/09/21 17:08:41  fox
removed draw_display, added Atari800_Initialise

Revision 1.17  2001/09/21 17:00:33  fox
joystick positions and Atari key codes moved to input.h

Revision 1.16  2001/09/21 16:54:38  fox
Atari800_Frame()

Revision 1.15  2001/09/17 18:10:37  fox
machine, mach_xlxe, Ram256, os, default_system -> machine_type, ram_size

Revision 1.14  2001/09/06 17:13:40  fox
MENU_PATCHES -> MENU_SETTINGS

Revision 1.13  2001/07/20 20:01:54  fox
added declarations of mach_xlxe and Ram256,
removed cartridge types and Insert/Remove functions for cartridges

Revision 1.9  2001/05/04 15:38:34  joy
version++

Revision 1.8  2001/04/24 10:22:36  joy
COLOUR TRANSLATION TABLE defined out for SHM under X11 - Rudolf says "it produced funny results"

Revision 1.7  2001/04/15 09:10:52  knik
atari_64_bit -> sizeof_long (autoconf compatibility)

Revision 1.6  2001/03/25 07:00:05  knik
removed o_binary define

Revision 1.5  2001/03/18 06:34:58  knik
WIN32 conditionals removed

*/

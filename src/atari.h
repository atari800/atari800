#ifndef _ATARI_H_
#define _ATARI_H_

#include "config.h"

/* Fundamental declarations ---------------------------------------------- */

#define ATARI_TITLE  "Atari 800 Emulator, Version 1.3.6"

#ifndef FALSE
#define FALSE  0
#endif
#ifndef TRUE
#define TRUE   1
#endif

/* SBYTE and UBYTE must be exactly 1 byte long. */
/* SWORD and UWORD must be exactly 2 bytes long. */
/* SLONG and ULONG must be exactly 4 bytes long. */
#define SBYTE signed char
#define SWORD signed short
#define SLONG signed int
#define UBYTE unsigned char
#define UWORD unsigned short
#define ULONG unsigned int
/* Note: in various parts of the emulator we assume that char is 1 byte
   and int is 4 bytes. */


/* Public interface ------------------------------------------------------ */

/* Machine type. */
#define MACHINE_OSA   0
#define MACHINE_OSB   1
#define MACHINE_XLXE  2
#define MACHINE_5200  3
extern int machine_type;

/* RAM size in kilobytes.
   Valid values for MACHINE_OSA and MACHINE_OSB are: 16, 48, 52.
   Valid values for MACHINE_XLXE are: 16, 64, 128, RAM_320_RAMBO,
   RAM_320_COMPY_SHOP, 576, 1088.
   The only valid value for MACHINE_5200 is 16. */
#define RAM_320_RAMBO       320
#define RAM_320_COMPY_SHOP  321
extern int ram_size;

/* Always call Atari800_InitialiseMachine() after changing machine_type
   or ram_size! */

/* Video system. */
#define TV_PAL 312
#define TV_NTSC 262
extern int tv_mode;

/* Dimensions of atari_screen.
   atari_screen is ATARI_WIDTH * ATARI_HEIGHT bytes.
   Each byte is an Atari color code - use Palette_Get[RGB] functions
   to get actual RGB codes.
   You should never display anything outside the middle 336 columns. */
#define ATARI_WIDTH  384
#define ATARI_HEIGHT 240

/* If Atari800_Frame() sets it to TRUE, then the current contents
   of atari_screen should be displayed. */
extern int display_screen;

/* Simply incremented by Atari800_Frame(). */
extern int nframes;

/* Time between Atari frames, in seconds
   (normally 1.0/50 for PAL, 1.0/60 for NTSC). */
extern double deltatime;

/* You can read it to see how fast is the emulator compared to real Atari
   (100 if running at real Atari speed). */
extern int percent_atari_speed;

/* Set to TRUE for faster emulation with refresh_rate > 1.
   Set to FALSE for accurate emulation with refresh_rate > 1. */
extern int sprite_collisions_in_skipped_frames;

/* Special key codes.
   Store in key_code. */
#define AKEY_WARMSTART             -2
#define AKEY_COLDSTART             -3
#define AKEY_EXIT                  -4
#define AKEY_BREAK                 -5
#define AKEY_UI                    -7
#define AKEY_SCREENSHOT            -8
#define AKEY_SCREENSHOT_INTERLACE  -9

/* Menu codes for Alt+letter shortcuts.
   Store in alt_function and put AKEY_UI in key_code.*/
#define MENU_DISK             0
#define MENU_CARTRIDGE        1
#define MENU_RUN              2
#define MENU_SYSTEM           3
#define MENU_SOUND            4
#define MENU_SOUND_RECORDING  5
#define MENU_ARTIF            6
#define MENU_SETTINGS         7
#define MENU_SAVESTATE        8
#define MENU_LOADSTATE        9
#define MENU_PCX             10
#define MENU_PCXI            11
#define MENU_BACK            12
#define MENU_RESETW          13
#define MENU_RESETC          14
#define MENU_MONITOR         15
#define MENU_ABOUT           16
#define MENU_EXIT            17
#define MENU_CASSETTE        18

/* Initializes Atari800 emulation core. */
int Atari800_Initialise(int *argc, char *argv[]);

/* Emulates one frame (1/50sec for PAL, 1/60sec for NTSC). */
void Atari800_Frame(void);

/* Reboots the emulated Atari. */
void Coldstart(void);

/* Presses the Reset key in the emulated Atari. */
void Warmstart(void);

/* Reinitializes after machine_type or ram_size change.
   You should call Coldstart() after it. */
int Atari800_InitialiseMachine(void);

/* Reinitializes patches after enable_*_patch change. */
void Atari800_UpdatePatches(void);

/* Shuts down Atari800 emulation core. */
int Atari800_Exit(int run_monitor);


/* Private interface ----------------------------------------------------- */
/* Don't use outside the emulation core! */

/* ATR format header */
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

/* First two bytes of an ATR file. */
#define MAGIC1  0x96
#define MAGIC2  0x02

/* Current clock cycle in a scanline.
   Normally 0 <= xpos && xpos < LINE_C, but in some cases xpos >= LINE_C,
   which means that we are already in line (ypos + 1). */
extern int xpos;

/* xpos limit for the currently running 6502 emulation. */
extern int xpos_limit;

/* Number of cycles per scanline. */
#define LINE_C   114

/* STA WSYNC resumes here. */
#define WSYNC_C  106

/* Number of memory refresh cycles per scanline.
   In the first scanline of a font mode there are actually less than DMAR
   memory refresh cycles. */
#define DMAR     9

/* Number of scanlines per frame. */
#define max_ypos tv_mode

/* Main clock value at the beginning of the current scanline. */
extern unsigned int screenline_cpu_clock;

/* Current main clock value. */
#define cpu_clock (screenline_cpu_clock + xpos)

/* Escape codes used to mark places in 6502 code that must
   be handled specially by the emulator. An escape sequence
   is an illegal 6502 opcode 0xF2 or 0xD2 followed
   by one of these escape codes: */
enum ESCAPE {

	/* SIO patch. */
	ESC_SIOV,

	/* stdio-based handlers for the BASIC version
	   and handlers for Atari Basic loader. */
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

	/* Atari executable loader. */
	ESC_BINLOADER_CONT,

	/* Cassette emulation. */
	ESC_COPENLOAD = 0xa8,
	ESC_COPENSAVE = 0xa9,

	/* Printer. */
	ESC_PHOPEN = 0xb0,
	ESC_PHCLOS = 0xb1,
	ESC_PHREAD = 0xb2,
	ESC_PHWRIT = 0xb3,
	ESC_PHSTAT = 0xb4,
	ESC_PHSPEC = 0xb5,
	ESC_PHINIT = 0xb6,

#ifdef R_IO_DEVICE
	/* R: device. */
	ESC_ROPEN = 0xd0,
	ESC_RCLOS = 0xd1,
	ESC_RREAD = 0xd2,
	ESC_RWRIT = 0xd3,
	ESC_RSTAT = 0xd4,
	ESC_RSPEC = 0xd5,
	ESC_RINIT = 0xd6,
#endif

	/* H: device. */
	ESC_HHOPEN = 0xc0,
	ESC_HHCLOS = 0xc1,
	ESC_HHREAD = 0xc2,
	ESC_HHWRIT = 0xc3,
	ESC_HHSTAT = 0xc4,
	ESC_HHSPEC = 0xc5,
	ESC_HHINIT = 0xc6
};

/* A function called to handle an escape sequence. */
typedef void (*EscFunctionType)(void);

/* Puts an escape sequence at the specified address. */
void Atari800_AddEsc(UWORD address, UBYTE esc_code, EscFunctionType function);

/* Puts an escape sequence followed by the RTS instruction. */
void Atari800_AddEscRts(UWORD address, UBYTE esc_code, EscFunctionType function);

/* Puts an escape sequence with an integrated RTS. */
void Atari800_AddEscRts2(UWORD address, UBYTE esc_code, EscFunctionType function);

/* Unregisters an escape sequence. You must cleanup the Atari memory yourself. */
void Atari800_RemoveEsc(UBYTE esc_code);

/* Handles an escape sequence. */
void Atari800_RunEsc(UBYTE esc_code);

/* Reads a byte from the specified special address (not RAM or ROM). */
UBYTE Atari800_GetByte(UWORD addr);

/* Stores a byte at the specified special address (not RAM or ROM). */
void Atari800_PutByte(UWORD addr, UBYTE byte);

/* Installs SIO patch and disables ROM checksum test. */
void Atari800_PatchOS(void);

/* Effects deltatime delay. */
void atari_sync(void);

#endif /* _ATARI_H_ */


/*
$Log$
Revision 1.50  2005/08/15 20:35:40  pfusik
completely reorganized, and now with good comments;
why define ULONG as long if we assume that int is 32-bit
in other parts of the emulator?

Revision 1.49  2005/04/30 14:07:17  joy
version increased for release

Revision 1.48  2005/03/05 12:28:24  pfusik
support for special AKEY_*, refresh rate control and atari_sync()
moved to Atari800_Frame()

Revision 1.47  2005/03/03 09:36:26  pfusik
moved screen-related variables to the new "screen" module

Revision 1.46  2004/12/30 18:43:03  joy
updated for new release

Revision 1.45  2004/12/27 13:40:56  joy
updated for 1.3.4 release

Revision 1.44  2004/08/08 08:43:37  joy
version increased

Revision 1.43  2003/12/20 20:49:28  joy
version increased

Revision 1.42  2003/11/22 23:26:19  joy
cassette support improved

Revision 1.41  2003/09/04 21:15:59  joy
ver++

Revision 1.40  2003/09/04 18:40:11  joy
ver++

Revision 1.39  2003/09/03 20:44:41  joy
version increased

Revision 1.38  2003/08/31 21:57:43  joy
rdevice module compiled in conditionally

Revision 1.37  2003/05/28 19:10:42  joy
R: device codes

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

#ifndef _ATARI_H_
#define _ATARI_H_

#include "config.h"
#include <stdio.h> /* FILENAME_MAX */
#ifdef WIN32
#include <windows.h>
#endif

/* Fundamental declarations ---------------------------------------------- */

#define ATARI_TITLE  "Atari 800 Emulator, Version 1.4.0"

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
#ifndef WIN32
/* Windows headers typedef ULONG */
#define ULONG unsigned int
#endif
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

/* TRUE to disable Atari BASIC when booting Atari (hold Option in XL/XE). */
extern int disable_basic;

/* TRUE to enable patched (fast) Serial I/O. */
extern int enable_sio_patch;

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

/* You can read it to see how fast is the emulator compared to real Atari
   (100 if running at real Atari speed). */
extern int percent_atari_speed;

/* How often the screen is updated (1 = every Atari frame). */
extern int refresh_rate;

/* Set to TRUE for faster emulation with refresh_rate > 1.
   Set to FALSE for accurate emulation with refresh_rate > 1. */
extern int sprite_collisions_in_skipped_frames;

/* Paths to ROM images. */
extern char atari_osa_filename[FILENAME_MAX];
extern char atari_osb_filename[FILENAME_MAX];
extern char atari_xlxe_filename[FILENAME_MAX];
extern char atari_5200_filename[FILENAME_MAX];
extern char atari_basic_filename[FILENAME_MAX];

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
   Store in alt_function and put AKEY_UI in key_code. */
#define MENU_DISK             0
#define MENU_CARTRIDGE        1
#define MENU_RUN              2
#define MENU_SYSTEM           3
#define MENU_SOUND            4
#define MENU_SOUND_RECORDING  5
#define MENU_DISPLAY          6
#define MENU_SETTINGS         7
#define MENU_SAVESTATE        8
#define MENU_LOADSTATE        9
#define MENU_PCX              10
#define MENU_PCXI             11
#define MENU_BACK             12
#define MENU_RESETW           13
#define MENU_RESETC           14
#define MENU_MONITOR          15
#define MENU_ABOUT            16
#define MENU_EXIT             17
#define MENU_CASSETTE         18

/* File types returned by Atari800_DetectFileType() and Atari800_OpenFile(). */
#define AFILE_ERROR      0
#define AFILE_ATR        1
#define AFILE_XFD        2
#define AFILE_ATR_GZ     3
#define AFILE_XFD_GZ     4
#define AFILE_DCM        5
#define AFILE_XEX        6
#define AFILE_BAS        7
#define AFILE_LST        8
#define AFILE_CART       9
#define AFILE_ROM        10
#define AFILE_CAS        11
#define AFILE_BOOT_TAPE  12
#define AFILE_STATE      13
#define AFILE_STATE_GZ   14

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

/* Auto-detects file type and returns one of AFILE_* values. */
int Atari800_DetectFileType(const char *filename);

/* Auto-detects file type and mounts the file in the emulator.
   reboot: Coldstart() for disks, cartridges and tapes
   diskno: drive number for disks (1-8)
   readonly: mount disks as read-only */
int Atari800_OpenFile(const char *filename, int reboot, int diskno, int readonly);

/* Checks for "popular" filenames of ROM images in the specified directory
   and sets atari_*_filename to the ones found.
   If only_if_not_set is TRUE, then atari_*_filename is modified only when
   Util_filenamenotset() is TRUE for it. */
void Atari800_FindROMImages(const char *directory, int only_if_not_set);

/* Load Atari800 text configuration file. */
int Atari800_LoadConfig(const char *alternate_config_filename);

/* Writes Atari800 text configuration file. */
int Atari800_WriteConfig(void);

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

/* Sleeps until it's time to emulate next Atari frame. */
void atari_sync(void);

#endif /* _ATARI_H_ */

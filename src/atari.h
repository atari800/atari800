#ifndef __ATARI__
#define	__ATARI__

#ifdef WIN32
#include "windows.h"
#endif

#include "config.h"

/*
   =================================
   Define Data Types on Local System
   =================================
 */

#ifdef VMS
#define SBYTE char
#define SWORD short int
#define SLONG long int
#else
#define	SBYTE signed char
#define	SWORD signed short int
#ifndef ATARI800_64_BIT
#define	SLONG signed long int
#else
#define	SLONG signed int
#endif
#endif

#define	UBYTE unsigned char
#define	UWORD unsigned short int
#ifndef WIN32
#ifndef ATARI800_64_BIT
#define	ULONG unsigned long int
#else
#define	ULONG unsigned int
#endif
#endif

#ifndef O_BINARY				/* flag for binary files on MS-DOS */
#define O_BINARY	0			/* this won't cause any trouble */
#endif

#ifdef SHM
#define USE_COLOUR_TRANSLATION_TABLE
#endif

typedef enum {
	Atari,
	AtariXL,
	AtariXE,
	Atari320XE,
	Atari5200
} Machine;

#define TV_PAL 312
#define TV_NTSC 262

extern int tv_mode;				/* now it is simply number of scanlines */
extern Machine machine;
extern int verbose;
extern int draw_display;		/* Draw actualy generated screen */

#ifndef FALSE
#define FALSE	0
#endif
#ifndef TRUE
#define TRUE	1
#endif

#define ATARI_WIDTH  384
#define ATARI_HEIGHT 240

#define ATARI_TITLE  "Atari 800 Emulator, Version 1.0.4"

extern int xpos;
extern int xpos_limit;
#define WSYNC_C	110
#define LINE_C	114
#define DMAR	9
#define max_ypos tv_mode		/* number of scanlines */
/* if tv_mod is enum, max_ypos is (tv_mod == TV_PAL ? 312 : 262) */

extern ULONG *atari_screen;
#ifdef BITPL_SCR
extern ULONG *atari_screen_b;
extern ULONG *atari_screen1;
extern ULONG *atari_screen2;
#endif

#ifndef NO_VOL_ONLY
extern unsigned int screenline_cpu_clock;
#define cpu_clock (screenline_cpu_clock + xpos)
#endif

#define NO_CART 0
#define NORMAL8_CART 1
#define NORMAL16_CART 2
#define AGS32_CART 3
#define OSS_SUPERCART 4
#define DB_SUPERCART 5
#define CARTRIDGE 6

enum ESCAPE {
	ESC_SIOV,
#ifdef MONITOR_BREAK
	ESC_BREAK,
#endif
/*
 * These are special device escape codes required by the Basic version
 */
	ESC_E_OPEN,
	ESC_E_CLOSE,
	ESC_E_READ,
	ESC_E_WRITE,
	ESC_E_STATUS,
	ESC_E_SPECIAL,

	ESC_K_OPEN,
	ESC_K_CLOSE,
	ESC_K_READ,
	ESC_K_WRITE,
	ESC_K_STATUS,
	ESC_K_SPECIAL,

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
   =================
   Joystick Position
   =================
 */

#define	STICK_LL	0x09
#define	STICK_BACK	0x0d
#define	STICK_LR	0x05
#define	STICK_LEFT	0x0b
#define	STICK_CENTRE	0x0f
#define	STICK_RIGHT	0x07
#define	STICK_UL	0x0a
#define	STICK_FORWARD	0x0e
#define	STICK_UR	0x06

/*
   ===========================
   non-standard keyboard codes
   ===========================
 */

#define AKEY_NONE -1
#define AKEY_WARMSTART -2
#define AKEY_COLDSTART -3
#define AKEY_EXIT -4
#define AKEY_BREAK -5
#define AKEY_PIL -6
#define AKEY_UI -7
#define AKEY_SCREENSHOT -8
#define AKEY_SCREENSHOT_INTERLACE -9

#define AKEY_SHFT 0x40
#define AKEY_CTRL 0x80
#define AKEY_SHFTCTRL 0xc0

/*
   ===============================
   keyboard codes for numbers 0..9
   ===============================
 */

#define AKEY_0 0x32
#define AKEY_1 0x1f
#define AKEY_2 0x1e
#define AKEY_3 0x1a
#define AKEY_4 0x18
#define AKEY_5 0x1d
#define AKEY_6 0x1b
#define AKEY_7 0x33
#define AKEY_8 0x35
#define AKEY_9 0x30

#define AKEY_CTRL_0 (AKEY_CTRL | AKEY_0)
#define AKEY_CTRL_1 (AKEY_CTRL | AKEY_1)
#define AKEY_CTRL_2 (AKEY_CTRL | AKEY_2)
#define AKEY_CTRL_3 (AKEY_CTRL | AKEY_3)
#define AKEY_CTRL_4 (AKEY_CTRL | AKEY_4)
#define AKEY_CTRL_5 (AKEY_CTRL | AKEY_5)
#define AKEY_CTRL_6 (AKEY_CTRL | AKEY_6)
#define AKEY_CTRL_7 (AKEY_CTRL | AKEY_7)
#define AKEY_CTRL_8 (AKEY_CTRL | AKEY_8)
#define AKEY_CTRL_9 (AKEY_CTRL | AKEY_9)

#define AKEY_a 0x3f
#define AKEY_b 0x15
#define AKEY_c 0x12
#define AKEY_d 0x3a
#define AKEY_e 0x2a
#define AKEY_f 0x38
#define AKEY_g 0x3d
#define AKEY_h 0x39
#define AKEY_i 0x0d
#define AKEY_j 0x01
#define AKEY_k 0x05
#define AKEY_l 0x00
#define AKEY_m 0x25
#define AKEY_n 0x23
#define AKEY_o 0x08
#define AKEY_p 0x0a
#define AKEY_q 0x2f
#define AKEY_r 0x28
#define AKEY_s 0x3e
#define AKEY_t 0x2d
#define AKEY_u 0x0b
#define AKEY_v 0x10
#define AKEY_w 0x2e
#define AKEY_x 0x16
#define AKEY_y 0x2b
#define AKEY_z 0x17

#define AKEY_A (AKEY_SHFT | AKEY_a)
#define AKEY_B (AKEY_SHFT | AKEY_b)
#define AKEY_C (AKEY_SHFT | AKEY_c)
#define AKEY_D (AKEY_SHFT | AKEY_d)
#define AKEY_E (AKEY_SHFT | AKEY_e)
#define AKEY_F (AKEY_SHFT | AKEY_f)
#define AKEY_G (AKEY_SHFT | AKEY_g)
#define AKEY_H (AKEY_SHFT | AKEY_h)
#define AKEY_I (AKEY_SHFT | AKEY_i)
#define AKEY_J (AKEY_SHFT | AKEY_j)
#define AKEY_K (AKEY_SHFT | AKEY_k)
#define AKEY_L (AKEY_SHFT | AKEY_l)
#define AKEY_M (AKEY_SHFT | AKEY_m)
#define AKEY_N (AKEY_SHFT | AKEY_n)
#define AKEY_O (AKEY_SHFT | AKEY_o)
#define AKEY_P (AKEY_SHFT | AKEY_p)
#define AKEY_Q (AKEY_SHFT | AKEY_q)
#define AKEY_R (AKEY_SHFT | AKEY_r)
#define AKEY_S (AKEY_SHFT | AKEY_s)
#define AKEY_T (AKEY_SHFT | AKEY_t)
#define AKEY_U (AKEY_SHFT | AKEY_u)
#define AKEY_V (AKEY_SHFT | AKEY_v)
#define AKEY_W (AKEY_SHFT | AKEY_w)
#define AKEY_X (AKEY_SHFT | AKEY_x)
#define AKEY_Y (AKEY_SHFT | AKEY_y)
#define AKEY_Z (AKEY_SHFT | AKEY_z)

#define AKEY_CTRL_a (AKEY_CTRL | AKEY_a)
#define AKEY_CTRL_b (AKEY_CTRL | AKEY_b)
#define AKEY_CTRL_c (AKEY_CTRL | AKEY_c)
#define AKEY_CTRL_d (AKEY_CTRL | AKEY_d)
#define AKEY_CTRL_e (AKEY_CTRL | AKEY_e)
#define AKEY_CTRL_f (AKEY_CTRL | AKEY_f)
#define AKEY_CTRL_g (AKEY_CTRL | AKEY_g)
#define AKEY_CTRL_h (AKEY_CTRL | AKEY_h)
#define AKEY_CTRL_i (AKEY_CTRL | AKEY_i)
#define AKEY_CTRL_j (AKEY_CTRL | AKEY_j)
#define AKEY_CTRL_k (AKEY_CTRL | AKEY_k)
#define AKEY_CTRL_l (AKEY_CTRL | AKEY_l)
#define AKEY_CTRL_m (AKEY_CTRL | AKEY_m)
#define AKEY_CTRL_n (AKEY_CTRL | AKEY_n)
#define AKEY_CTRL_o (AKEY_CTRL | AKEY_o)
#define AKEY_CTRL_p (AKEY_CTRL | AKEY_p)
#define AKEY_CTRL_q (AKEY_CTRL | AKEY_q)
#define AKEY_CTRL_r (AKEY_CTRL | AKEY_r)
#define AKEY_CTRL_s (AKEY_CTRL | AKEY_s)
#define AKEY_CTRL_t (AKEY_CTRL | AKEY_t)
#define AKEY_CTRL_u (AKEY_CTRL | AKEY_u)
#define AKEY_CTRL_v (AKEY_CTRL | AKEY_v)
#define AKEY_CTRL_w (AKEY_CTRL | AKEY_w)
#define AKEY_CTRL_x (AKEY_CTRL | AKEY_x)
#define AKEY_CTRL_y (AKEY_CTRL | AKEY_y)
#define AKEY_CTRL_z (AKEY_CTRL | AKEY_z)

#define AKEY_CTRL_A (AKEY_CTRL | AKEY_A)
#define AKEY_CTRL_B (AKEY_CTRL | AKEY_B)
#define AKEY_CTRL_C (AKEY_CTRL | AKEY_C)
#define AKEY_CTRL_D (AKEY_CTRL | AKEY_D)
#define AKEY_CTRL_E (AKEY_CTRL | AKEY_E)
#define AKEY_CTRL_F (AKEY_CTRL | AKEY_F)
#define AKEY_CTRL_G (AKEY_CTRL | AKEY_G)
#define AKEY_CTRL_H (AKEY_CTRL | AKEY_H)
#define AKEY_CTRL_I (AKEY_CTRL | AKEY_I)
#define AKEY_CTRL_J (AKEY_CTRL | AKEY_J)
#define AKEY_CTRL_K (AKEY_CTRL | AKEY_K)
#define AKEY_CTRL_L (AKEY_CTRL | AKEY_L)
#define AKEY_CTRL_M (AKEY_CTRL | AKEY_M)
#define AKEY_CTRL_N (AKEY_CTRL | AKEY_N)
#define AKEY_CTRL_O (AKEY_CTRL | AKEY_O)
#define AKEY_CTRL_P (AKEY_CTRL | AKEY_P)
#define AKEY_CTRL_Q (AKEY_CTRL | AKEY_Q)
#define AKEY_CTRL_R (AKEY_CTRL | AKEY_R)
#define AKEY_CTRL_S (AKEY_CTRL | AKEY_S)
#define AKEY_CTRL_T (AKEY_CTRL | AKEY_T)
#define AKEY_CTRL_U (AKEY_CTRL | AKEY_U)
#define AKEY_CTRL_V (AKEY_CTRL | AKEY_V)
#define AKEY_CTRL_W (AKEY_CTRL | AKEY_W)
#define AKEY_CTRL_X (AKEY_CTRL | AKEY_X)
#define AKEY_CTRL_Y (AKEY_CTRL | AKEY_Y)
#define AKEY_CTRL_Z (AKEY_CTRL | AKEY_Z)

#define AKEY_HELP 0x11
#define AKEY_DOWN 0x8f
#define AKEY_LEFT 0x86
#define AKEY_RIGHT 0x87
#define AKEY_UP 0x8e
#define AKEY_BACKSPACE 0x34
#define AKEY_DELETE_CHAR 0xb4
#define AKEY_DELETE_LINE 0x74
#define AKEY_INSERT_CHAR 0xb7
#define AKEY_INSERT_LINE 0x77
#define AKEY_ESCAPE 0x1c
#define AKEY_ATARI 0x27
#define AKEY_CAPSLOCK 0x7c
#define AKEY_CAPSTOGGLE 0x3c
#define AKEY_TAB 0x2c
#define AKEY_SETTAB 0x6c
#define AKEY_CLRTAB 0xac
#define AKEY_RETURN 0x0c
#define AKEY_SPACE 0x21
#define AKEY_EXCLAMATION 0x5f
#define AKEY_DBLQUOTE 0x5e
#define AKEY_HASH 0x5a
#define AKEY_DOLLAR 0x58
#define AKEY_PERCENT 0x5d
#define AKEY_AMPERSAND 0x5b
#define AKEY_QUOTE 0x73
#define AKEY_AT 0x75
#define AKEY_PARENLEFT 0x70
#define AKEY_PARENRIGHT 0x72
#define AKEY_LESS 0x36
#define AKEY_GREATER 0x37
#define AKEY_EQUAL 0x0f
#define AKEY_QUESTION 0x66
#define AKEY_MINUS 0x0e
#define AKEY_PLUS 0x06
#define AKEY_ASTERISK 0x07
#define AKEY_SLASH 0x26
#define AKEY_COLON 0x42
#define AKEY_SEMICOLON 0x02
#define AKEY_COMMA 0x20
#define AKEY_FULLSTOP 0x22
#define AKEY_UNDERSCORE 0x4e
#define AKEY_BRACKETLEFT 0x60
#define AKEY_BRACKETRIGHT 0x62
#define AKEY_CIRCUMFLEX 0x47
#define AKEY_BACKSLASH 0x46
#define AKEY_BAR 0x4f
#define AKEY_CLEAR (AKEY_SHFT | AKEY_LESS)
#define AKEY_CARET (AKEY_SHFT | AKEY_ASTERISK )
#define AKEY_F1 0x03
#define AKEY_F2 0x04
#define AKEY_F3	0x13
#define AKEY_F4 0x14

/* Following keys cannot be read with both shift and control pressed:
   J K L ; + * Z X C V B F1 F2 F3 F4 HELP	 */


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
#define MENU_SAVESTATE	7
#define MENU_LOADSTATE	8
#define MENU_PCX		9
#define MENU_PCXI		10
#define MENU_BACK		11
#define MENU_RESETW		12
#define MENU_RESETC		13
#define MENU_MONITOR	14
#define MENU_ABOUT		15
#define MENU_EXIT		16



void EnablePILL(void);
int Insert_8K_ROM(char *filename);
int Insert_16K_ROM(char *filename);
int Insert_OSS_ROM(char *filename);
int Insert_DB_ROM(char *filename);
int Insert_32K_5200ROM(char *filename);
int Remove_ROM(void);
void Coldstart(void);
void Warmstart(void);
int Initialise_AtariOSA(void);
int Initialise_AtariOSB(void);
int Initialise_AtariXL(void);
int Initialise_AtariXE(void);
int Initialise_Atari320XE(void);
int Initialise_Atari5200(void);
int Atari800_Exit(int run_monitor);
UBYTE Atari800_GetByte(UWORD addr);
void Atari800_PutByte(UWORD addr, UBYTE byte);
void AtariEscape(UBYTE esc_code);
int Initialise_EmuOS(void);
int Insert_Cartridge(char *filename);
void atari_sync(void);

#endif

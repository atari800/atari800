#ifndef _ATARI_WIN32_H_
#define _ATARI_WIN32_H_

#define AKEY32_WINDOWSIZEUP       	-100
#define AKEY32_WINDOWSIZEDOWN		-101
#define AKEY32_TOGGLESCANLINEMODE	-102
#define AKEY32_TOGGLESCREENSAVER	-103
#define AKEY32_TILTSCREEN		-104
#define AKEY32_TOGGLEFULLSCREEN   	-105

#include <dinput.h>
#include "akey.h"

#define MAXKEYNAMELENGTH 40

/* keyboard code key-value pairs      */
/* for use in joystick button mapping */
typedef struct {
  int keyvalue;
  char* keyname;
} _keyref;

/* Set of keys that may be mapped to joystick/gamepad buttons                 */
/* If you add to this list, be sure to TEST them, not all keys work the same  */
/* and may require additional code support.                                   */
static const _keyref keyref[] = {
	{AKEY_0, "0"}, {AKEY_1, "1"}, {AKEY_2, "2"}, {AKEY_3, "3"}, {AKEY_4, "4"},
	{AKEY_5, "5"}, {AKEY_6, "6"}, {AKEY_7, "7"}, {AKEY_8, "8"}, {AKEY_9, "9"},
	{AKEY_a, "a"}, {AKEY_b, "b"}, {AKEY_c, "c"}, {AKEY_d, "d"}, {AKEY_e, "e"},
	{AKEY_f, "f"}, {AKEY_g, "g"}, {AKEY_h, "h"}, {AKEY_i, "i"}, {AKEY_j, "j"},
	{AKEY_k, "k"}, {AKEY_l, "l"}, {AKEY_m, "m"}, {AKEY_n, "n"}, {AKEY_o, "o"},
	{AKEY_p, "p"}, {AKEY_q, "q"}, {AKEY_r, "r"}, {AKEY_s, "s"}, {AKEY_t, "t"},
	{AKEY_u, "u"}, {AKEY_v, "v"}, {AKEY_w, "w"}, {AKEY_x, "x"}, {AKEY_y, "y"},
	{AKEY_z, "z"}, {AKEY_A, "A"}, {AKEY_B, "B"}, {AKEY_C, "C"}, {AKEY_D, "D"},
	{AKEY_E, "E"}, {AKEY_F, "F"}, {AKEY_G, "G"}, {AKEY_H, "H"}, {AKEY_I, "I"},
	{AKEY_J, "J"}, {AKEY_K, "K"}, {AKEY_L, "L"}, {AKEY_M, "M"}, {AKEY_N, "N"},
	{AKEY_O, "O"}, {AKEY_P, "P"}, {AKEY_Q, "Q"}, {AKEY_R, "R"}, {AKEY_S, "S"},
	{AKEY_T, "T"}, {AKEY_U, "U"}, {AKEY_V, "V"}, {AKEY_W, "W"}, {AKEY_X, "X"},
	{AKEY_Y, "Y"}, {AKEY_Z, "Z"}, {AKEY_START, "START"}, {AKEY_SELECT, "SELECT"}, 
	{AKEY_OPTION, "OPTION"}, {AKEY_RETURN, "RETURN"}, {AKEY_SPACE, "SPACE"}
};

static const int MAXKEYREFS = sizeof(keyref)/sizeof(*keyref);

/* keypad mode joydefs */
static const UBYTE joydefs[] =
{
	DIK_NUMPAD0, /* fire */
	DIK_NUMPAD7, /* up/left */
	DIK_NUMPAD8, /* up */
	DIK_NUMPAD9, /* up/right */
	DIK_NUMPAD4, /* left */
	DIK_NUMPAD6, /* right */
	DIK_NUMPAD1, /* down/left */
	DIK_NUMPAD2, /* down */
	DIK_NUMPAD3, /* down/right */
};

/* keypad mode joymask */
static const UBYTE joymask[] =
{
	INPUT_STICK_CENTRE,  /* not used */
	INPUT_STICK_UL,      /* up/left */
	INPUT_STICK_FORWARD, /* up */
	INPUT_STICK_UR,      /* up/right */
	INPUT_STICK_LEFT,    /* left */
	INPUT_STICK_RIGHT,   /* right */
	INPUT_STICK_LL,      /* down/left */
	INPUT_STICK_BACK,    /* down */
	INPUT_STICK_LR,      /* down/right */
};

/* keypad plus mode joydefs */
static const UBYTE joydefs_plus[] =
{
	DIK_NUMPAD0, /* fire */
	DIK_NUMPAD7, /* up/left */
	DIK_NUMPAD8, /* up */
	DIK_NUMPAD9, /* up/right */
	DIK_NUMPAD4, /* left */
	DIK_NUMPAD6, /* right */
	DIK_NUMPAD1, /* down/left */
	DIK_NUMPAD2, /* down */
	DIK_NUMPAD5, /* duplicate down on 5 key for inverted T */
	DIK_NUMPAD3, /* down/right */
};

/* keypad plus mode joymask */
static const UBYTE joymask_plus[] =
{
	INPUT_STICK_CENTRE,  /* not used */
	INPUT_STICK_UL,      /* up/left */
	INPUT_STICK_FORWARD, /* up */
	INPUT_STICK_UR,      /* up/right */
	INPUT_STICK_LEFT,    /* left */
	INPUT_STICK_RIGHT,   /* right */
	INPUT_STICK_LL,      /* down/left */
	INPUT_STICK_BACK,    /* down */
	INPUT_STICK_BACK,    /* duplicate down on 5 key for inverted T */
	INPUT_STICK_LR,      /* down/right */
};

/* arrow mode joydefs */
static const UBYTE joydefs_arrow[] =
{
	DIK_NUMPAD0, 
	DIK_UP,
	DIK_LEFT,
	DIK_RIGHT,
	DIK_DOWN,
};

/* arrow mod joymask */
static const UBYTE joymask_arrow[] =
{
	INPUT_STICK_CENTRE,  /* not used */
	INPUT_STICK_FORWARD, /* up */
	INPUT_STICK_LEFT,    /* left */
	INPUT_STICK_RIGHT,   /* right */
	INPUT_STICK_BACK,    /* down */
};

void Atari800_Frame32(void);
int getkeyvalue(char *name);
void getkeyname(int value, char *name);
void Win32_Init();

#endif /* _ATARI_WIN32_H_ */


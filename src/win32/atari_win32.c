/*
 * atari_win32.c - Win32 port specific code
 *
 * Copyright (C) 2000 Krzysztof Nikiel
 * Copyright (C) 2000-2009 Atari800 development team (see DOC/CREDITS)
 *
 * This file is part of the Atari800 emulator project which emulates
 * the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.
 *
 * Atari800 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Atari800 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Atari800; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "config.h"
#define DIRECTINPUT_VERSION 0x0500
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <dinput.h>
#include <ctype.h>

#include "atari.h"
#include "input.h"
#include "akey.h"
#include "log.h"
#include "monitor.h"
#include "platform.h"
#include "screen.h"
#include "sound.h"
#include "ui.h"

#include "joystick.h"
#include "keyboard.h"
#include "main.h"
#include "screen_win32.h"

static int usesnd = 1;

static int kbjoy = 0;
static int win32keys = FALSE;

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
#ifdef USE_CURSORBLOCK
	DIK_UP,
	DIK_LEFT,
	DIK_RIGHT,
	DIK_DOWN,
#endif
};

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
#ifdef USE_CURSORBLOCK
	INPUT_STICK_FORWARD, /* up */
	INPUT_STICK_LEFT,    /* left */
	INPUT_STICK_RIGHT,   /* right */
	INPUT_STICK_BACK,    /* down */
#endif
};

static int trig0 = 1;
static int trig1 = 1;
static int stick0 = INPUT_STICK_CENTRE;
static int stick1 = INPUT_STICK_CENTRE;

#define KBSCAN(name) \
		case DIK_##name: \
			keycode |= (AKEY_##name & ~AKEY_SHFTCTRL); \
			break;
#define KBSCAN_5200(name) \
			case DIK_##name: \
				return AKEY_5200_##name + keycode;

/*
void gotoxy(int x, int y)
{
    COORD coord;
    coord.X = x; coord.Y = y;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
}
*/
static int Atari_Win32_keys(void)
{
	int keycode=0;
	BYTE lpKeyState[256];
	WORD buf[2];
	char Asciikey;
	UINT vk;

	switch (kbcode) {
		KBSCAN(ESCAPE)
			KBSCAN(BACKSPACE)
			KBSCAN(TAB)
			KBSCAN(RETURN)
			KBSCAN(CAPSLOCK)
			KBSCAN(SPACE)
		case DIK_UP:
			keycode |= AKEY_UP;
			break;
		case DIK_DOWN:
			keycode |= AKEY_DOWN;
			break;
		case DIK_LEFT:
			keycode |= AKEY_LEFT;
			break;
		case DIK_RIGHT:
			keycode |= AKEY_RIGHT;
			break;
		case DIK_DELETE:
			if (INPUT_key_shift)
				keycode = AKEY_DELETE_LINE;
			else
				keycode |= AKEY_DELETE_CHAR;
			break;
		case DIK_INSERT:
			if (INPUT_key_shift)
				keycode = AKEY_INSERT_LINE;
			else
				keycode |= AKEY_INSERT_CHAR;
			break;
		case DIK_HOME:
			keycode = AKEY_CLEAR;
			break;
		case DIK_F6:
		case DIK_END:
			keycode = AKEY_HELP;
			break;
		default:
		{

			/* get the win32 keyboard state */
			if(!GetKeyboardState(lpKeyState)) {
				return AKEY_NONE;
			}

			/* Debug: To test keyboard codes
			gotoxy(0,9);
			Log_printS("kbcode:%d\n   ",kbcode);

			gotoxy(0,10);
			for(i=0; i<256; i++) {
				if (i%32==0) Log_printS("\n%d:", i);
				Log_printS("%2x",lpKeyState[i]);
			}
			Log_printS("\nA:%d   \n",lpKeyState[65]);
			Log_printS("B:%d   \n",lpKeyState[66]);
			Log_printS("C:%d   \n",lpKeyState[67]);
			*/

			/* translates the scan code to a virtual key */
			vk = MapVirtualKey(kbcode,1);
			/* translates the virtual key to ascii */
			if(ToAscii(vk, kbcode, lpKeyState, buf, 0)<1) {
				return AKEY_NONE;
			}
			Asciikey = (char)buf[0];

			/* Change Upper case with lower case due to the reverse effect of the CAPS key */
			/* bit 0x80 in lpKeyState signal if the key is pressed, while bit 0x01 togles */
			/* each time the key is pressed... */
			if (lpKeyState[VK_CAPITAL] && 0x01) {
				if (isupper(Asciikey)) Asciikey=tolower(Asciikey);
				else if (islower(Asciikey)) Asciikey=toupper(Asciikey);
			}

			switch (Asciikey) {
		case '0': keycode |= AKEY_0; break;
		case '1': keycode |= AKEY_1; break;
		case '2': keycode |= AKEY_2; break;
		case '3': keycode |= AKEY_3; break;
		case '4': keycode |= AKEY_4; break;
		case '5': keycode |= AKEY_5; break;
		case '6': keycode |= AKEY_6; break;
		case '7': keycode |= AKEY_7; break;
		case '8': keycode |= AKEY_8; break;
		case '9': keycode |= AKEY_9; break;

		case 'a': keycode |= AKEY_a; break;
		case 'b': keycode |= AKEY_b; break;
		case 'c': keycode |= AKEY_c; break;
		case 'd': keycode |= AKEY_d; break;
		case 'e': keycode |= AKEY_e; break;
		case 'f': keycode |= AKEY_f; break;
		case 'g': keycode |= AKEY_g; break;
		case 'h': keycode |= AKEY_h; break;
		case 'i': keycode |= AKEY_i; break;
		case 'j': keycode |= AKEY_j; break;
		case 'k': keycode |= AKEY_k; break;
		case 'l': keycode |= AKEY_l; break;
		case 'm': keycode |= AKEY_m; break;
		case 'n': keycode |= AKEY_n; break;
		case 'o': keycode |= AKEY_o; break;
		case 'p': keycode |= AKEY_p; break;
		case 'q': keycode |= AKEY_q; break;
		case 'r': keycode |= AKEY_r; break;
		case 's': keycode |= AKEY_s; break;
		case 't': keycode |= AKEY_t; break;
		case 'u': keycode |= AKEY_u; break;
		case 'v': keycode |= AKEY_v; break;
		case 'w': keycode |= AKEY_w; break;
		case 'x': keycode |= AKEY_x; break;
		case 'y': keycode |= AKEY_y; break;
		case 'z': keycode |= AKEY_z; break;

		case 'A': keycode |= AKEY_A; break;
		case 'B': keycode |= AKEY_B; break;
		case 'C': keycode |= AKEY_C; break;
		case 'D': keycode |= AKEY_D; break;
		case 'E': keycode |= AKEY_E; break;
		case 'F': keycode |= AKEY_F; break;
		case 'G': keycode |= AKEY_G; break;
		case 'H': keycode |= AKEY_H; break;
		case 'I': keycode |= AKEY_I; break;
		case 'J': keycode |= AKEY_J; break;
		case 'K': keycode |= AKEY_K; break;
		case 'L': keycode |= AKEY_L; break;
		case 'M': keycode |= AKEY_M; break;
		case 'N': keycode |= AKEY_N; break;
		case 'O': keycode |= AKEY_O; break;
		case 'P': keycode |= AKEY_P; break;
		case 'Q': keycode |= AKEY_Q; break;
		case 'R': keycode |= AKEY_R; break;
		case 'S': keycode |= AKEY_S; break;
		case 'T': keycode |= AKEY_T; break;
		case 'U': keycode |= AKEY_U; break;
		case 'V': keycode |= AKEY_V; break;
		case 'W': keycode |= AKEY_W; break;
		case 'X': keycode |= AKEY_X; break;
		case 'Y': keycode |= AKEY_Y; break;
		case 'Z': keycode |= AKEY_Z; break;

		case '!': keycode |= AKEY_EXCLAMATION; break;
		case '"': keycode |= AKEY_DBLQUOTE; break;
		case '#': keycode |= AKEY_HASH; break;
		case '$': keycode |= AKEY_DOLLAR; break;
		case '%': keycode |= AKEY_PERCENT; break;
		case '&': keycode |= AKEY_AMPERSAND; break;
		case '\'': keycode |= AKEY_QUOTE; break;
		case '(': keycode |= AKEY_PARENLEFT; break;
		case ')': keycode |= AKEY_PARENRIGHT; break;
		case '*': keycode |= AKEY_ASTERISK; break;
		case '+': keycode |= AKEY_PLUS; break;
		case ',': keycode |= AKEY_COMMA; break;
		case '-': keycode |= AKEY_MINUS; break;
		case '.': keycode |= AKEY_FULLSTOP; break;
		case '/': keycode |= AKEY_SLASH; break;

		case ':': keycode |= AKEY_COLON; break;
		case ';': keycode |= AKEY_SEMICOLON; break;
		case '<': keycode |= AKEY_LESS; break;
		case '=': keycode |= AKEY_EQUAL; break;
		case '>': keycode |= AKEY_GREATER; break;
		case '?': keycode |= AKEY_QUESTION; break;
		case '@': keycode |= AKEY_AT; break;
		case '[': keycode |= AKEY_BRACKETLEFT; break;
		case '\\': keycode |= AKEY_SLASH; break;
		case ']': keycode |= AKEY_BRACKETRIGHT; break;
		case '^': keycode |= AKEY_CIRCUMFLEX; break;
		case '_': keycode |= AKEY_UNDERSCORE; break;
		case '`': keycode |= AKEY_NONE; break;
		case '{': keycode |= AKEY_NONE; break;
		case '|': keycode |= AKEY_BAR; break;
		case '}': keycode |= AKEY_NONE; break;
		case '~': keycode |= AKEY_NONE; break;
		default: keycode = AKEY_NONE;
			} /* switch (Asciikey) */
		} /* default */
	} /* switch (kbcode) */

 	return keycode;
 }

 int PLATFORM_Keyboard(void)
{
	int keycode;
	int i;

	prockb();

	stick0 = INPUT_STICK_CENTRE;
	stick1 = INPUT_STICK_CENTRE;
	if (kbjoy) {
		/* fire */
#ifdef USE_CURSORBLOCK
		trig0 = (kbhits[joydefs[0]] ? 0 : 1) & (kbhits[DIK_LCONTROL] ? 0 : 1);
#else
		trig0 = kbhits[joydefs[0]] ? 0 : 1;
#endif
		for (i = 1; i < sizeof(joydefs) / sizeof(joydefs[0]); i++)
			if (kbhits[joydefs[i]])
				stick0 &= joymask[i];
	}
	else {
		if (!procjoy(0)) {
			trig0 = !joystat.trig;
			stick0 &= joystat.stick;
		}
		if (!procjoy(1)) {
			trig1 = !joystat.trig;
			stick1 &= joystat.stick;
		}
	}

	INPUT_key_consol = (kbhits[DIK_F2] ? 0 : INPUT_CONSOL_OPTION)
	           + (kbhits[DIK_F3] ? 0 : INPUT_CONSOL_SELECT)
	           + (kbhits[DIK_F4] ? 0 : INPUT_CONSOL_START);

	if (pause_hit) {
		pause_hit = 0;
		return AKEY_BREAK;
	}

	INPUT_key_shift = (kbhits[DIK_LSHIFT] | kbhits[DIK_RSHIFT]) ? 1 : 0;

	if (kbhits[DIK_LMENU]) { /* left Alt key is pressed */
		switch (kbcode) {
		case DIK_R:
			UI_alt_function = UI_MENU_RUN;       /* ALT+R .. Run file */
			return AKEY_UI;
		case DIK_Y:
			UI_alt_function = UI_MENU_SYSTEM;    /* ALT+Y .. Select system */
			return AKEY_UI;
		case DIK_O:
			UI_alt_function = UI_MENU_SOUND;     /* ALT+O .. mono/stereo sound */
			return AKEY_UI;
		case DIK_A:
			UI_alt_function = UI_MENU_ABOUT;     /* ALT+A .. About */
			return AKEY_UI;
		case DIK_S:
			UI_alt_function = UI_MENU_SAVESTATE; /* ALT+S .. Save state */
			return AKEY_UI;
		case DIK_D:
			UI_alt_function = UI_MENU_DISK;      /* ALT+D .. Disk management */
			return AKEY_UI;
		case DIK_L:
			UI_alt_function = UI_MENU_LOADSTATE; /* ALT+L .. Load state */
			return AKEY_UI;
		case DIK_C:
			UI_alt_function = UI_MENU_CARTRIDGE; /* ALT+C .. Cartridge management */
			return AKEY_UI;
		case DIK_BACKSLASH:
			return AKEY_PBI_BB_MENU; /* BLACK BOX */
		default:
			break;
		}
	}

	switch (kbcode) {
	case DIK_F1:
		return AKEY_UI;
	case DIK_F5:
		return INPUT_key_shift ? AKEY_COLDSTART : AKEY_WARMSTART;
	case DIK_F7:
		return AKEY_BREAK;
	case DIK_F8:
		keycode = PLATFORM_Exit(1) ? AKEY_NONE : AKEY_EXIT;
		kbcode = 0;
		return keycode;
	case DIK_F9:
		return AKEY_EXIT;
	case DIK_SYSRQ:
	case DIK_F10:
		kbcode = 0;
		return INPUT_key_shift ? AKEY_SCREENSHOT_INTERLACE : AKEY_SCREENSHOT;
	case DIK_F11:
		for (i = 0; i < 4; i++) {
			if (++INPUT_joy_autofire[i] > 2)
				INPUT_joy_autofire[i] = 0;
		}
		kbcode = 0;
		return AKEY_NONE;
	case DIK_SCROLL: /* Scroll Lock = pause */
		while (kbhits[DIK_SCROLL])
			prockb();
		while (!kbhits[DIK_SCROLL])
			prockb();
		kbcode = 0;
		return AKEY_NONE;
	default:
		break;
	}

	if (Atari800_machine_type == Atari800_MACHINE_5200 && !UI_is_active) {
		keycode = (INPUT_key_shift ? 0x40 : 0);
		switch (kbcode) {
		case DIK_F4:
			return AKEY_5200_START + keycode;
		case DIK_P:
			return AKEY_5200_PAUSE + keycode;
		case DIK_R:
			return AKEY_5200_RESET + keycode;
		KBSCAN_5200(1)
		KBSCAN_5200(2)
		case DIK_3:
			return INPUT_key_shift ? AKEY_5200_HASH : AKEY_5200_3;
		KBSCAN_5200(4)
		KBSCAN_5200(5)
		KBSCAN_5200(6)
		KBSCAN_5200(7)
		case DIK_8:
			return INPUT_key_shift ? AKEY_5200_ASTERISK : AKEY_5200_8;
		KBSCAN_5200(9)
		KBSCAN_5200(0)
		case DIK_EQUALS:
			return AKEY_5200_HASH + keycode;
		default:
			return AKEY_NONE;
		}
	}

	/* use win32 keys and international keyboard only for non control characters */
	if (win32keys && !kbhits[DIK_LCONTROL] && !kbhits[DIK_RCONTROL])
		return Atari_Win32_keys();

	/* need to set shift mask here to avoid conflict with PC layout */
	keycode = (INPUT_key_shift ? 0x40 : 0)
	        + ((kbhits[DIK_LCONTROL] | kbhits[DIK_RCONTROL]) ? 0x80 : 0);

	switch (kbcode) {
	KBSCAN(ESCAPE)
	KBSCAN(1)
	case DIK_2:
		if (INPUT_key_shift && !(keycode & AKEY_CTRL))
			keycode = AKEY_AT;
		else
			keycode |= AKEY_2;
		break;
	KBSCAN(3)
	KBSCAN(4)
	KBSCAN(5)
	case DIK_6:
		if (INPUT_key_shift && !(keycode & AKEY_CTRL))
			keycode = AKEY_CARET;
		else
			keycode |= AKEY_6;
		break;
	case DIK_7:
		if (INPUT_key_shift && !(keycode & AKEY_CTRL))
			keycode = AKEY_AMPERSAND;
		else
			keycode |= AKEY_7;
		break;
	case DIK_8:
		if (INPUT_key_shift && !(keycode & AKEY_CTRL))
			keycode = AKEY_ASTERISK;
		else
			keycode |= AKEY_8;
		break;
	KBSCAN(9)
	KBSCAN(0)
	KBSCAN(MINUS)
	case DIK_EQUALS:
		if (INPUT_key_shift)
			keycode = AKEY_PLUS;
		else
			keycode |= AKEY_EQUAL;
		break;
	KBSCAN(BACKSPACE)
	KBSCAN(TAB)
	KBSCAN(Q)
	KBSCAN(W)
	KBSCAN(E)
	KBSCAN(R)
	KBSCAN(T)
	KBSCAN(Y)
	KBSCAN(U)
	KBSCAN(I)
	KBSCAN(O)
	KBSCAN(P)
	case DIK_LBRACKET:
		keycode |= AKEY_BRACKETLEFT;
		break;
	case DIK_RBRACKET:
		keycode |= AKEY_BRACKETRIGHT;
		break;
	KBSCAN(RETURN)
	KBSCAN(A)
	KBSCAN(S)
	KBSCAN(D)
	KBSCAN(F)
	KBSCAN(G)
	KBSCAN(H)
	KBSCAN(J)
	KBSCAN(K)
	KBSCAN(L)
	KBSCAN(SEMICOLON)
	case DIK_APOSTROPHE:
		if (INPUT_key_shift)
			keycode = AKEY_DBLQUOTE;
		else
			keycode |= AKEY_QUOTE;
		break;
	case DIK_GRAVE:
		keycode |= AKEY_ATARI;
		break;
	case DIK_BACKSLASH:
		if (keycode & AKEY_CTRL)
			keycode |= AKEY_ESCAPE;
		else if (INPUT_key_shift)
			keycode = AKEY_BAR;
		else
			keycode |= AKEY_BACKSLASH;
		break;
	KBSCAN(Z)
	KBSCAN(X)
	KBSCAN(C)
	KBSCAN(V)
	KBSCAN(B)
	KBSCAN(N)
	KBSCAN(M)
	case DIK_COMMA:
		if (INPUT_key_shift && !(keycode & AKEY_CTRL))
			keycode = AKEY_LESS;
		else
			keycode |= AKEY_COMMA;
		break;
	case DIK_PERIOD:
		if (INPUT_key_shift && !(keycode & AKEY_CTRL))
			keycode = AKEY_GREATER;
		else
			keycode |= AKEY_FULLSTOP;
		break;
	KBSCAN(SLASH)
	KBSCAN(SPACE)
	KBSCAN(CAPSLOCK)
	case DIK_UP:
		keycode ^= (INPUT_key_shift ? AKEY_MINUS : AKEY_UP);
		break;
	case DIK_DOWN:
		keycode ^= (INPUT_key_shift ? AKEY_EQUAL : AKEY_DOWN);
		break;
	case DIK_LEFT:
		keycode ^= (INPUT_key_shift ? AKEY_PLUS : AKEY_LEFT);
		break;
	case DIK_RIGHT:
		keycode ^= (INPUT_key_shift ? AKEY_ASTERISK : AKEY_RIGHT);
		break;
	case DIK_DELETE:
		if (INPUT_key_shift)
			keycode |= AKEY_DELETE_LINE;
		else
			keycode |= AKEY_DELETE_CHAR;
		break;
	case DIK_INSERT:
		if (INPUT_key_shift)
			keycode |= AKEY_INSERT_LINE;
		else
			keycode |= AKEY_INSERT_CHAR;
		break;
	case DIK_HOME:
		keycode |= ((keycode & AKEY_CTRL) ? AKEY_LESS : AKEY_CLEAR);
		break;
	case DIK_F6:
	case DIK_END:
		keycode |= AKEY_HELP;
		break;
	default:
		keycode = AKEY_NONE;
		break;
	}
	return keycode;
}

int PLATFORM_Initialise(int *argc, char *argv[])
{
	int i;
	int j;
	int help_only = FALSE;

	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-nojoystick") == 0) {
			kbjoy = 1;
			Log_print("no joystick");
		}
		else if (strcmp(argv[i], "-win32keys") == 0) {
			win32keys = TRUE;
			Log_print("win32keys keys");
		}
		else {
			if (strcmp(argv[i], "-help") == 0) {
				help_only = TRUE;
				Log_print("\t-nojoystick      Disable joystick");
				Log_print("\t-win32keys       Support for international keyboards");
			}
			argv[j++] = argv[i];
		}
	}

	*argc = j;

	if (gron(argc, argv))
		exit(1);

#ifdef SOUND
	if (usesnd)
		Sound_Initialise(argc, argv);
#endif

	if (help_only)
		return TRUE;

	if (initinput()) {
		MessageBox(hWndMain, "DirectInput Init FAILED",
		           myname, MB_ICONEXCLAMATION | MB_OK);
		exit(1);
	}
	clearkb();

	if (!kbjoy) {
		if (initjoystick()) {
			kbjoy = 1;
			Log_print("joystick not found");
		}
	}

	trig0 = 1;
	stick0 = 15;
	INPUT_key_consol = INPUT_CONSOL_NONE;

	ShowWindow(hWndMain, SW_RESTORE);

	return TRUE;
}

int PLATFORM_Exit(int run_monitor)
{
	Log_flushlog();

	if (run_monitor) {
		int i;
#ifdef SOUND
		Sound_Pause();
#endif
		ShowWindow(hWndMain, SW_MINIMIZE);
		i = MONITOR_Run();
		ShowWindow(hWndMain, SW_RESTORE);
#ifdef SOUND
		Sound_Continue();
#endif
		if (i)
			return TRUE; /* return to emulation */
	}

	return FALSE;
}

void PLATFORM_DisplayScreen(void)
{
	refreshv((UBYTE *) Screen_atari + 24);
}

int PLATFORM_PORT(int num)
{
	if (num == 0)
		return (stick1 << 4) | stick0;
	return 0xff;
}

int PLATFORM_TRIG(int num)
{
	switch (num) {
	case 0:
		return trig0;
	case 1:
		return trig1;
	default:
		return 1;
	}
}

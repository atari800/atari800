/*
 * atari_javanvm.c - Java NestedVM port-specific code
 *
 * Copyright (c) 2001-2002 Jacek Poplawski (original atari_sdl.c)
 * Copyright (c) 2007 Perry McFarlane (javanvm port)
 * Copyright (C) 2001-2007 Atari800 development team (see DOC/CREDITS)
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Atari800 includes */
#include "input.h"
#include "colours.h"
#include "monitor.h"
#include "platform.h"
#include "ui.h"
#include "screen.h"
#include "pokeysnd.h"
#include "gtia.h"
#include "antic.h"
#include "devices.h"
#include "cpu.h"
#include "memory.h"
#include "pia.h"
#include "log.h"
#include "util.h"
#include "javanvm/atari_javanvm.h"
//#include "atari_ntsc.h"

/* joystick emulation */

/* a runtime switch for the kbd_joy_X_enabled vars is in the UI */
int kbd_joy_0_enabled = TRUE;	/* enabled by default, doesn't hurt */
int kbd_joy_1_enabled = FALSE;	/* disabled, would steal normal keys */

int KBD_TRIG_0 = VK_CONTROL;
int KBD_TRIG_0_LOC = KEY_LOCATION_LEFT;
int KBD_STICK_0_LEFT = VK_NUMPAD4;
int KBD_STICK_0_LEFT_LOC = KEY_LOCATION_NUMPAD;
int KBD_STICK_0_RIGHT = VK_NUMPAD6;
int KBD_STICK_0_RIGHT_LOC = KEY_LOCATION_NUMPAD;
int KBD_STICK_0_DOWN = VK_NUMPAD2;
int KBD_STICK_0_DOWN_LOC = KEY_LOCATION_NUMPAD;
int KBD_STICK_0_UP = VK_NUMPAD8;
int KBD_STICK_0_UP_LOC = KEY_LOCATION_NUMPAD;
int KBD_STICK_0_LEFTUP = VK_NUMPAD7;
int KBD_STICK_0_LEFTUP_LOC = KEY_LOCATION_NUMPAD;
int KBD_STICK_0_RIGHTUP = VK_NUMPAD9;
int KBD_STICK_0_RIGHTUP_LOC = KEY_LOCATION_NUMPAD;
int KBD_STICK_0_LEFTDOWN = VK_NUMPAD1;
int KBD_STICK_0_LEFTDOWN_LOC = KEY_LOCATION_NUMPAD;
int KBD_STICK_0_RIGHTDOWN = VK_NUMPAD3;
int KBD_STICK_0_RIGHTDOWN_LOC = KEY_LOCATION_NUMPAD;

int KBD_TRIG_1 = VK_TAB;
int KBD_TRIG_1_LOC = KEY_LOCATION_STANDARD;
int KBD_STICK_1_LEFT = VK_A;
int KBD_STICK_1_LEFT_LOC = KEY_LOCATION_STANDARD;
int KBD_STICK_1_RIGHT = VK_D;
int KBD_STICK_1_RIGHT_LOC = KEY_LOCATION_STANDARD;
int KBD_STICK_1_DOWN = VK_X;
int KBD_STICK_1_DOWN_LOC = KEY_LOCATION_STANDARD;
int KBD_STICK_1_UP = VK_W;
int KBD_STICK_1_UP_LOC = KEY_LOCATION_STANDARD;
int KBD_STICK_1_LEFTUP = VK_Q;
int KBD_STICK_1_LEFTUP_LOC = KEY_LOCATION_STANDARD;
int KBD_STICK_1_RIGHTUP = VK_E;
int KBD_STICK_1_RIGHTUP_LOC = KEY_LOCATION_STANDARD;
int KBD_STICK_1_LEFTDOWN = VK_Z;
int KBD_STICK_1_LEFTDOWN_LOC = KEY_LOCATION_STANDARD;
int KBD_STICK_1_RIGHTDOWN = VK_C;
int KBD_STICK_1_RIGHTDOWN_LOC = KEY_LOCATION_STANDARD;
static int swap_joysticks = 0;

/* These functions call the NestedVM runtime */
extern int _call_java(int a, int b, int c, int d);

static void JAVANVM_DisplayScreen(void *as){
	_call_java(1, (int)as, 0, 0);
}
static void JAVANVM_InitPalette(void *ct){
	_call_java(2, (int)ct, 0, 0);
}
static int JAVANVM_Kbhits(int key, int loc){
	return _call_java(3, key, loc, 0);
}
static int JAVANVM_PollKeyEvent(void *event){
	return _call_java(4, (int)event, 0, 0);
}
static int JAVANVM_GetWindowClosed(void){
	return _call_java(5, 0, 0, 0);
}
static int JAVANVM_Sleep(int millis){
	return _call_java(6, millis, 0, 0);
}
static int JAVANVM_InitGraphics(void *config){
	return _call_java(7, (int)config, 0, 0);
}
/* These constants are for use with arrays passed to and from the NestedVM runtime */
#define JAVANVM_KeyEventType 0
#define JAVANVM_KeyEventKeyCode 1
#define JAVANVM_KeyEventKeyChar 2
#define JAVANVM_KeyEventKeyLocation 3
#define JAVANVM_KeyEventSIZE 4
#define JAVANVM_InitGraphicsScalew 0
#define JAVANVM_InitGraphicsScaleh 1
#define JAVANVM_InitGraphicsATARI_WIDTH 2
#define JAVANVM_InitGraphicsATARI_HEIGHT 3
#define JAVANVM_InitGraphicsATARI_VISIBLE_WIDTH 4
#define JAVANVM_InitGraphicsATARI_LEFT_MARGIN 5
#define JAVANVM_InitGraphicsSIZE 6

void Atari_PaletteUpdate(void)
{
	JAVANVM_InitPalette((void *)&colortable[0]);
}

int Atari_Keyboard(void)
{
	static int lastkey = 0, key_pressed = 0, key_control = 0;
 	static int lastuni = 0;
	static int lastloc = KEY_LOCATION_UNKNOWN;
	int shiftctrl = 0;
    int event[JAVANVM_KeyEventSIZE];

	/* Exit if the user closed the window */
	if (JAVANVM_GetWindowClosed()) {
		return AKEY_EXIT;
	}

	if (JAVANVM_PollKeyEvent(event)) {
		switch (event[JAVANVM_KeyEventType]) {
		case KEY_PRESSED:
			lastkey = event[JAVANVM_KeyEventKeyCode];
			lastuni = event[JAVANVM_KeyEventKeyChar];
			lastloc = event[JAVANVM_KeyEventKeyLocation];
			key_pressed = 1;
			break;
		case KEY_RELEASED:
			lastkey = event[JAVANVM_KeyEventKeyCode];
 			lastuni = 0; 
			key_pressed = 0;
			break;
		}

	}
	else if (!key_pressed)
		return AKEY_NONE;

	alt_function = -1;
	if (JAVANVM_Kbhits(VK_ALT,KEY_LOCATION_LEFT)) {
		if (key_pressed) {
			switch (lastkey) {
			case VK_F:
				key_pressed = 0;
				//SwitchFullscreen();
				break;
			case VK_G:
				key_pressed = 0;
				//SwitchWidth();
				break;
			case VK_B:
				key_pressed = 0;
				//SwitchBW();
				break;
			case VK_J:
				key_pressed = 0;
				//SwapJoysticks();
				break;
			case VK_R:
				alt_function = MENU_RUN;
				break;
			case VK_Y:
				alt_function = MENU_SYSTEM;
				break;
			case VK_O:
				alt_function = MENU_SOUND;
				break;
			case VK_W:
				alt_function = MENU_SOUND_RECORDING;
				break;
			case VK_A:
				alt_function = MENU_ABOUT;
				break;
			case VK_S:
				alt_function = MENU_SAVESTATE;
				break;
			case VK_D:
				alt_function = MENU_DISK;
				break;
			case VK_L:
				alt_function = MENU_LOADSTATE;
				break;
			case VK_C:
				alt_function = MENU_CARTRIDGE;
				break;
			break;
			}
		}
		if (alt_function != -1) {
			key_pressed = 0;
			return AKEY_UI;
		}
	}
	/* SHIFT STATE */
	if ((JAVANVM_Kbhits(VK_SHIFT,KEY_LOCATION_LEFT)) || (JAVANVM_Kbhits(VK_SHIFT,KEY_LOCATION_RIGHT)))
		key_shift = 1;
	else
		key_shift = 0;

    /* CONTROL STATE */
	if ((JAVANVM_Kbhits(VK_CONTROL,KEY_LOCATION_LEFT)) || (JAVANVM_Kbhits(VK_CONTROL,KEY_LOCATION_RIGHT)))
		key_control = 1;
	else
		key_control = 0;

	/* OPTION / SELECT / START keys */
	key_consol = CONSOL_NONE;
	if (JAVANVM_Kbhits(VK_F2,KEY_LOCATION_STANDARD))
		key_consol &= (~CONSOL_OPTION);
	if (JAVANVM_Kbhits(VK_F3,KEY_LOCATION_STANDARD))
		key_consol &= (~CONSOL_SELECT);
	if (JAVANVM_Kbhits(VK_F4,KEY_LOCATION_STANDARD))
		key_consol &= (~CONSOL_START);

	if (key_pressed == 0)
		return AKEY_NONE;

	/* Handle movement and special keys. */
	switch (lastkey) {
	case VK_F1:
		key_pressed = 0;
		return AKEY_UI;
	case VK_F5:
		key_pressed = 0;
		return key_shift ? AKEY_COLDSTART : AKEY_WARMSTART;
	case VK_F8:
		key_pressed = 0;
		return (Atari_Exit(1) ? AKEY_NONE : AKEY_EXIT);
	case VK_F9:
		return AKEY_EXIT;
	case VK_F10:
		key_pressed = 0;
		return key_shift ? AKEY_SCREENSHOT_INTERLACE : AKEY_SCREENSHOT_INTERLACE;
	}
	/* keyboard joysticks: don't pass the keypresses to emulation
	 * as some games pause on a keypress (River Raid, Bruce Lee)
	 */
#define LQE(x) (lastkey == KBD_##x && lastloc == KBD_##x##_LOC)
	if (!ui_is_active && kbd_joy_0_enabled) {
		if (LQE(STICK_0_LEFT) || LQE(STICK_0_RIGHT) ||
			LQE(STICK_0_UP) || LQE(STICK_0_DOWN) ||
			LQE(STICK_0_LEFTUP) || LQE(STICK_0_LEFTDOWN) ||
			LQE(STICK_0_RIGHTUP) || LQE(STICK_0_RIGHTDOWN) ||
			LQE(TRIG_0)) {
			key_pressed = 0;
			return AKEY_NONE;
		}
	}

	if (!ui_is_active && kbd_joy_1_enabled) {
		if (LQE(STICK_1_LEFT) || LQE(STICK_1_RIGHT) ||
			LQE(STICK_1_UP) || LQE(STICK_1_DOWN) ||
			LQE(STICK_1_LEFTUP) || LQE(STICK_1_LEFTDOWN) ||
			LQE(STICK_1_RIGHTUP) || LQE(STICK_1_RIGHTDOWN) ||
			LQE(TRIG_1)) {
			key_pressed = 0;
			return AKEY_NONE;
		}
	}
#undef LQE

	if (key_shift)
		shiftctrl ^= AKEY_SHFT;

	if (machine_type == MACHINE_5200 && !ui_is_active) {
		if (lastkey == VK_F4)
			return AKEY_5200_START ^ shiftctrl;
		switch (lastuni) {
		case 'p':
			return AKEY_5200_PAUSE ^ shiftctrl;
		case 'r':
			return AKEY_5200_RESET ^ shiftctrl;
		case '0':
			return AKEY_5200_0 ^ shiftctrl;
		case '1':
			return AKEY_5200_1 ^ shiftctrl;
		case '2':
			return AKEY_5200_2 ^ shiftctrl;
		case '3':
			return AKEY_5200_3 ^ shiftctrl;
		case '4':
			return AKEY_5200_4 ^ shiftctrl;
		case '5':
			return AKEY_5200_5 ^ shiftctrl;
		case '6':
			return AKEY_5200_6 ^ shiftctrl;
		case '7':
			return AKEY_5200_7 ^ shiftctrl;
		case '8':
			return AKEY_5200_8 ^ shiftctrl;
		case '9':
			return AKEY_5200_9 ^ shiftctrl;
		/* XXX: " ^ shiftctrl" harmful for '#' and '*' ? */
		case '#':
		case '=':
			return AKEY_5200_HASH;
		case '*':
			return AKEY_5200_ASTERISK;
		}
		return AKEY_NONE;
	}

	if (key_control)
		shiftctrl ^= AKEY_CTRL;

	switch (lastkey) {
	case VK_BACK_QUOTE: /* fallthrough */
		return AKEY_ATARI ^ shiftctrl;
		/* These are the "Windows" keys, but they don't work on Windows*/
		/*
	case SDLK_LSUPER:
		return AKEY_ATARI ^ shiftctrl;
	case SDLK_RSUPER:
		if (key_shift)
			return AKEY_CAPSLOCK;
		else
			return AKEY_CAPSTOGGLE;
			*/
	case VK_END:
	case VK_F6:
		return AKEY_HELP ^ shiftctrl;
	case VK_PAGE_DOWN:
		return AKEY_F2 | AKEY_SHFT;
	case VK_PAGE_UP:
		return AKEY_F1 | AKEY_SHFT;
	case VK_HOME:
		return AKEY_CLEAR;
	case VK_PAUSE:
	case VK_F7:
		return AKEY_BREAK;
	case VK_CAPS_LOCK:
		if (key_shift)
			return AKEY_CAPSLOCK;
		else
			return AKEY_CAPSTOGGLE;
	case VK_SPACE:
		return AKEY_SPACE ^ shiftctrl;
	case VK_BACK_SPACE:
		return AKEY_BACKSPACE;
	case VK_ENTER:
		return AKEY_RETURN ^ shiftctrl;
	case VK_LEFT:
		return AKEY_LEFT ^ shiftctrl;
	case VK_RIGHT:
		return AKEY_RIGHT ^ shiftctrl;
	case VK_UP:
		return AKEY_UP ^ shiftctrl;
	case VK_DOWN:
		return AKEY_DOWN ^ shiftctrl;
	case VK_ESCAPE:
		return AKEY_ESCAPE ^ shiftctrl;
	case VK_TAB:
		return AKEY_TAB ^ shiftctrl;
	case VK_DELETE:
		if (key_shift)
			return AKEY_DELETE_LINE;
		else
			return AKEY_DELETE_CHAR;
	case VK_INSERT:
		if (key_shift)
			return AKEY_INSERT_LINE;
		else
			return AKEY_INSERT_CHAR;
	}

	/* Handle CTRL-0 to CTRL-9 */
	if (key_control) {
		switch(lastuni) {
		case '.':
			return AKEY_FULLSTOP | AKEY_CTRL;
		case ',':
			return AKEY_COMMA | AKEY_CTRL;
		case ';':
			return AKEY_SEMICOLON | AKEY_CTRL;
		}
		if(lastloc == KEY_LOCATION_STANDARD) {
		switch (lastkey) {
		case VK_0:
			return AKEY_CTRL_0;
		case VK_1:
			return AKEY_CTRL_1;
		case VK_2:
			return AKEY_CTRL_2;
		case VK_3:
			return AKEY_CTRL_3;
		case VK_4:
			return AKEY_CTRL_4;
		case VK_5:
			return AKEY_CTRL_5;
		case VK_6:
			return AKEY_CTRL_6;
		case VK_7:
			return AKEY_CTRL_7;
		case VK_8:
			return AKEY_CTRL_8;
		case VK_9:
			return AKEY_CTRL_9;
		}
		}
	}

	/* Host Caps Lock will make lastuni switch case, so prevent this*/
    if(lastuni>='A' && lastuni <= 'Z' && !key_shift) lastuni += 0x20;
    if(lastuni>='a' && lastuni <= 'z' && key_shift) lastuni -= 0x20;
	/* Uses only UNICODE translation, no shift states (this was added to
	 * support non-US keyboard layouts)*/
	switch (lastuni) {
	case 1:
		return AKEY_CTRL_a;
	case 2:
		return AKEY_CTRL_b;
	case 3:
		return AKEY_CTRL_c;
	case 4:
		return AKEY_CTRL_d;
	case 5:
		return AKEY_CTRL_e;
	case 6:
		return AKEY_CTRL_f;
	case 7:
		return AKEY_CTRL_g;
	case 8:
		return AKEY_CTRL_h;
	case 9:
		return AKEY_CTRL_i;
	case 10:
		return AKEY_CTRL_j;
	case 11:
		return AKEY_CTRL_k;
	case 12:
		return AKEY_CTRL_l;
	case 13:
		return AKEY_CTRL_m;
	case 14:
		return AKEY_CTRL_n;
	case 15:
		return AKEY_CTRL_o;
	case 16:
		return AKEY_CTRL_p;
	case 17:
		return AKEY_CTRL_q;
	case 18:
		return AKEY_CTRL_r;
	case 19:
		return AKEY_CTRL_s;
	case 20:
		return AKEY_CTRL_t;
	case 21:
		return AKEY_CTRL_u;
	case 22:
		return AKEY_CTRL_v;
	case 23:
		return AKEY_CTRL_w;
	case 24:
		return AKEY_CTRL_x;
	case 25:
		return AKEY_CTRL_y;
	case 26:
		return AKEY_CTRL_z;
	case 'A':
		return AKEY_A;
	case 'B':
		return AKEY_B;
	case 'C':
		return AKEY_C;
	case 'D':
		return AKEY_D;
	case 'E':
		return AKEY_E;
	case 'F':
		return AKEY_F;
	case 'G':
		return AKEY_G;
	case 'H':
		return AKEY_H;
	case 'I':
		return AKEY_I;
	case 'J':
		return AKEY_J;
	case 'K':
		return AKEY_K;
	case 'L':
		return AKEY_L;
	case 'M':
		return AKEY_M;
	case 'N':
		return AKEY_N;
	case 'O':
		return AKEY_O;
	case 'P':
		return AKEY_P;
	case 'Q':
		return AKEY_Q;
	case 'R':
		return AKEY_R;
	case 'S':
		return AKEY_S;
	case 'T':
		return AKEY_T;
	case 'U':
		return AKEY_U;
	case 'V':
		return AKEY_V;
	case 'W':
		return AKEY_W;
	case 'X':
		return AKEY_X;
	case 'Y':
		return AKEY_Y;
	case 'Z':
		return AKEY_Z;
	case ':':
		return AKEY_COLON;
	case '!':
		return AKEY_EXCLAMATION;
	case '@':
		return AKEY_AT;
	case '#':
		return AKEY_HASH;
	case '$':
		return AKEY_DOLLAR;
	case '%':
		return AKEY_PERCENT;
	case '^':
		return AKEY_CARET;
	case '&':
		return AKEY_AMPERSAND;
	case '*':
		return AKEY_ASTERISK;
	case '(':
		return AKEY_PARENLEFT;
	case ')':
		return AKEY_PARENRIGHT;
	case '+':
		return AKEY_PLUS;
	case '_':
		return AKEY_UNDERSCORE;
	case '"':
		return AKEY_DBLQUOTE;
	case '?':
		return AKEY_QUESTION;
	case '<':
		return AKEY_LESS;
	case '>':
		return AKEY_GREATER;
	case 'a':
		return AKEY_a;
	case 'b':
		return AKEY_b;
	case 'c':
		return AKEY_c;
	case 'd':
		return AKEY_d;
	case 'e':
		return AKEY_e;
	case 'f':
		return AKEY_f;
	case 'g':
		return AKEY_g;
	case 'h':
		return AKEY_h;
	case 'i':
		return AKEY_i;
	case 'j':
		return AKEY_j;
	case 'k':
		return AKEY_k;
	case 'l':
		return AKEY_l;
	case 'm':
		return AKEY_m;
	case 'n':
		return AKEY_n;
	case 'o':
		return AKEY_o;
	case 'p':
		return AKEY_p;
	case 'q':
		return AKEY_q;
	case 'r':
		return AKEY_r;
	case 's':
		return AKEY_s;
	case 't':
		return AKEY_t;
	case 'u':
		return AKEY_u;
	case 'v':
		return AKEY_v;
	case 'w':
		return AKEY_w;
	case 'x':
		return AKEY_x;
	case 'y':
		return AKEY_y;
	case 'z':
		return AKEY_z;
	case ';':
		return AKEY_SEMICOLON;
	case '0':
		return AKEY_0;
	case '1':
		return AKEY_1;
	case '2':
		return AKEY_2;
	case '3':
		return AKEY_3;
	case '4':
		return AKEY_4;
	case '5':
		return AKEY_5;
	case '6':
		return AKEY_6;
	case '7':
		return AKEY_7;
	case '8':
		return AKEY_8;
	case '9':
		return AKEY_9;
	case ',':
		return AKEY_COMMA;
	case '.':
		return AKEY_FULLSTOP;
	case '=':
		return AKEY_EQUAL;
	case '-':
		return AKEY_MINUS;
	case '\'':
		return AKEY_QUOTE;
	case '/':
		return AKEY_SLASH;
	case '\\':
		return AKEY_BACKSLASH;
	case '[':
		return AKEY_BRACKETLEFT;
	case ']':
		return AKEY_BRACKETRIGHT;
	case '|':
		return AKEY_BAR;
	}

	return AKEY_NONE;
}

void Atari_Initialise(int *argc, char *argv[])
{
	int i, j;
	int help_only = FALSE;
	int scale = 2;
	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-scale") == 0) {
			scale = Util_sscandec(argv[++i]);
		}
		else {
			if (strcmp(argv[i], "-help") == 0) {
				help_only = TRUE;
				Aprint("\t-scale <n>       Scale width and height by <n>");
			}
			argv[j++] = argv[i];
		}
	}
	*argc = j;

	if(!help_only) {
        int config[JAVANVM_InitGraphicsSIZE];
        config[JAVANVM_InitGraphicsScalew] = scale;
        config[JAVANVM_InitGraphicsScaleh] = scale;
        config[JAVANVM_InitGraphicsATARI_WIDTH] = ATARI_WIDTH;
        config[JAVANVM_InitGraphicsATARI_HEIGHT] = ATARI_HEIGHT;
        config[JAVANVM_InitGraphicsATARI_VISIBLE_WIDTH] = 336;
        config[JAVANVM_InitGraphicsATARI_LEFT_MARGIN] = 24;
		JAVANVM_InitGraphics((void *)&config[0]);
		JAVANVM_InitPalette((void *)&colortable[0]);
	}
	return;
}

int Atari_Exit(int run_monitor){
	int restart;
	if (run_monitor) {
		restart = monitor();
	}
   	else {
		restart = FALSE;
	}
	if (restart) {
		return 1;
	}
	return restart;
}

void Atari_DisplayScreen(void){
	JAVANVM_DisplayScreen((void *)atari_screen);
	return;
}

static void do_Atari_PORT(UBYTE *s0, UBYTE *s1)
{
#define KBHITS(x) (JAVANVM_Kbhits(x,x##_LOC))
	int stick0, stick1;
	stick0 = stick1 = STICK_CENTRE;

	if (kbd_joy_0_enabled) {
		if (KBHITS(KBD_STICK_0_LEFT))
			stick0 = STICK_LEFT;
		if (KBHITS(KBD_STICK_0_RIGHT))
			stick0 = STICK_RIGHT;
		if (KBHITS(KBD_STICK_0_UP))
			stick0 = STICK_FORWARD;
		if (KBHITS(KBD_STICK_0_DOWN))
			stick0 = STICK_BACK;
		if ((KBHITS(KBD_STICK_0_LEFTUP))
			|| ((KBHITS(KBD_STICK_0_LEFT)) && (KBHITS(KBD_STICK_0_UP))))
			stick0 = STICK_UL;
		if ((KBHITS(KBD_STICK_0_LEFTDOWN))
			|| ((KBHITS(KBD_STICK_0_LEFT)) && (KBHITS(KBD_STICK_0_DOWN))))
			stick0 = STICK_LL;
		if ((KBHITS(KBD_STICK_0_RIGHTUP))
			|| ((KBHITS(KBD_STICK_0_RIGHT)) && (KBHITS(KBD_STICK_0_UP))))
			stick0 = STICK_UR;
		if ((KBHITS(KBD_STICK_0_RIGHTDOWN))
			|| ((KBHITS(KBD_STICK_0_RIGHT)) && (KBHITS(KBD_STICK_0_DOWN))))
			stick0 = STICK_LR;
	}
	if (kbd_joy_1_enabled) {
		if (KBHITS(KBD_STICK_1_LEFT))
			stick1 = STICK_LEFT;
		if (KBHITS(KBD_STICK_1_RIGHT))
			stick1 = STICK_RIGHT;
		if (KBHITS(KBD_STICK_1_UP))
			stick1 = STICK_FORWARD;
		if (KBHITS(KBD_STICK_1_DOWN))
			stick1 = STICK_BACK;
		if ((KBHITS(KBD_STICK_1_LEFTUP))
			|| ((KBHITS(KBD_STICK_1_LEFT)) && (KBHITS(KBD_STICK_1_UP))))
			stick1 = STICK_UL;
		if ((KBHITS(KBD_STICK_1_LEFTDOWN))
			|| ((KBHITS(KBD_STICK_1_LEFT)) && (KBHITS(KBD_STICK_1_DOWN))))
			stick1 = STICK_LL;
		if ((KBHITS(KBD_STICK_1_RIGHTUP))
			|| ((KBHITS(KBD_STICK_1_RIGHT)) && (KBHITS(KBD_STICK_1_UP))))
			stick1 = STICK_UR;
		if ((KBHITS(KBD_STICK_1_RIGHTDOWN))
			|| ((KBHITS(KBD_STICK_1_RIGHT)) && (KBHITS(KBD_STICK_1_DOWN))))
			stick1 = STICK_LR;
	}

	if (swap_joysticks) {
		*s1 = stick0;
		*s0 = stick1;
	}
	else {
		*s0 = stick0;
		*s1 = stick1;
	}
}

static void do_Atari_TRIG(UBYTE *t0, UBYTE *t1)
{
	int trig0, trig1, i;
	trig0 = trig1 = 1;

	if (kbd_joy_0_enabled) {
		trig0 = KBHITS(KBD_TRIG_0) ? 0 : 1;
	}

	if (kbd_joy_1_enabled) {
		trig1 = KBHITS(KBD_TRIG_1) ? 0 : 1;
	}

	if (swap_joysticks) {
		*t1 = trig0;
		*t0 = trig1;
	}
	else {
		*t0 = trig0;
		*t1 = trig1;
	}
}
#undef KBHITS

int Atari_PORT(int num)
{
#ifndef DONT_DISPLAY
	if (num == 0) {
		UBYTE a, b;
		do_Atari_PORT(&a, &b);
		return (b << 4) | (a & 0x0f);
	}
#endif
	return 0xff;
}

int Atari_TRIG(int num)
{
#ifndef DONT_DISPLAY
	UBYTE a, b;
	do_Atari_TRIG(&a, &b);
	switch (num) {
	case 0:
		return a;
	case 1:
		return b;
	default:
		break;
	}
#endif
	return 1;
}

void Atari_sleep(double s){
	JAVANVM_Sleep((int)(s*1e3));
}

int main(int argc, char **argv)
{
	/* initialise Atari800 core */
	if (!Atari800_Initialise(&argc, argv))
		return 3;

	/* main loop */
	for (;;) {
		key_code = Atari_Keyboard();
		Atari800_Frame();
		if (display_screen)
			Atari_DisplayScreen();
	}
}

/*
vim:ts=4:sw=4:
*/

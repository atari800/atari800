/*
 * atari_javanvm.c - Java NestedVM port-specific code
 *
 * Copyright (c) 2001-2002 Jacek Poplawski (original atari_sdl.c)
 * Copyright (c) 2007-2008 Perry McFarlane (javanvm port)
 * Copyright (C) 2001-2008 Atari800 development team (see DOC/CREDITS)
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
#include "akey.h"
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
#include "sound.h"
#include "javanvm/atari_javanvm.h"

/* joystick emulation */

/* a runtime switch for the kbd_joy_X_enabled vars is in the UI */
int kbd_joy_0_enabled = TRUE;	/* enabled by default, doesn't hurt */
int kbd_joy_1_enabled = FALSE;	/* disabled, would steal normal keys */

static int KBD_TRIG_0 = VK_CONTROL;
static int KBD_TRIG_0_LOC = KEY_LOCATION_LEFT;
static int KBD_STICK_0_LEFT = VK_NUMPAD4;
static int KBD_STICK_0_LEFT_LOC = KEY_LOCATION_NUMPAD;
static int KBD_STICK_0_RIGHT = VK_NUMPAD6;
static int KBD_STICK_0_RIGHT_LOC = KEY_LOCATION_NUMPAD;
static int KBD_STICK_0_DOWN = VK_NUMPAD2;
static int KBD_STICK_0_DOWN_LOC = KEY_LOCATION_NUMPAD;
static int KBD_STICK_0_UP = VK_NUMPAD8;
static int KBD_STICK_0_UP_LOC = KEY_LOCATION_NUMPAD;
static int KBD_STICK_0_LEFTUP = VK_NUMPAD7;
static int KBD_STICK_0_LEFTUP_LOC = KEY_LOCATION_NUMPAD;
static int KBD_STICK_0_RIGHTUP = VK_NUMPAD9;
static int KBD_STICK_0_RIGHTUP_LOC = KEY_LOCATION_NUMPAD;
static int KBD_STICK_0_LEFTDOWN = VK_NUMPAD1;
static int KBD_STICK_0_LEFTDOWN_LOC = KEY_LOCATION_NUMPAD;
static int KBD_STICK_0_RIGHTDOWN = VK_NUMPAD3;
static int KBD_STICK_0_RIGHTDOWN_LOC = KEY_LOCATION_NUMPAD;

static int KBD_TRIG_1 = VK_TAB;
static int KBD_TRIG_1_LOC = KEY_LOCATION_STANDARD;
static int KBD_STICK_1_LEFT = VK_A;
static int KBD_STICK_1_LEFT_LOC = KEY_LOCATION_STANDARD;
static int KBD_STICK_1_RIGHT = VK_D;
static int KBD_STICK_1_RIGHT_LOC = KEY_LOCATION_STANDARD;
static int KBD_STICK_1_DOWN = VK_X;
static int KBD_STICK_1_DOWN_LOC = KEY_LOCATION_STANDARD;
static int KBD_STICK_1_UP = VK_W;
static int KBD_STICK_1_UP_LOC = KEY_LOCATION_STANDARD;
static int KBD_STICK_1_LEFTUP = VK_Q;
static int KBD_STICK_1_LEFTUP_LOC = KEY_LOCATION_STANDARD;
static int KBD_STICK_1_RIGHTUP = VK_E;
static int KBD_STICK_1_RIGHTUP_LOC = KEY_LOCATION_STANDARD;
static int KBD_STICK_1_LEFTDOWN = VK_Z;
static int KBD_STICK_1_LEFTDOWN_LOC = KEY_LOCATION_STANDARD;
static int KBD_STICK_1_RIGHTDOWN = VK_C;
static int KBD_STICK_1_RIGHTDOWN_LOC = KEY_LOCATION_STANDARD;
static int swap_joysticks = 0;

/* Sound */
#ifdef SOUND
static UBYTE *dsp_buffer = NULL;
static int line_buffer_size;
static int dsp_buffer_size;
#endif /* SOUND */

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
static int JAVANVM_InitSound(void *config){
	return _call_java(8, (int)config, 0, 0);
}
static int JAVANVM_SoundAvailable(void){
	return _call_java(9, 0, 0, 0);
}
static int JAVANVM_SoundWrite(void *buffer,int len){
	return _call_java(10, (int)buffer, len, 0);
}
static int JAVANVM_SoundPause(void){
	return _call_java(11, 0, 0, 0);
}
static int JAVANVM_SoundContinue(void){
	return _call_java(12, 0, 0, 0);
}
static int JAVANVM_CheckThreadStatus(void){
	return _call_java(13, 0, 0, 0);
}

/* These constants are for use with arrays passed to and from the NestedVM runtime */
#define JAVANVM_KeyEventType 0
#define JAVANVM_KeyEventKeyCode 1
#define JAVANVM_KeyEventKeyChar 2
#define JAVANVM_KeyEventKeyLocation 3
#define JAVANVM_KeyEventSIZE 4
#define JAVANVM_InitGraphicsScalew 0
#define JAVANVM_InitGraphicsScaleh 1
#define JAVANVM_InitGraphicsScreen_WIDTH 2
#define JAVANVM_InitGraphicsScreen_HEIGHT 3
#define JAVANVM_InitGraphicsATARI_VISIBLE_WIDTH 4
#define JAVANVM_InitGraphicsATARI_LEFT_MARGIN 5
#define JAVANVM_InitGraphicsSIZE 6
#define JAVANVM_InitSoundSampleRate 0
#define JAVANVM_InitSoundBitsPerSample 1
#define JAVANVM_InitSoundChannels 2
#define JAVANVM_InitSoundSigned 3
#define JAVANVM_InitSoundBigEndian 4
#define JAVANVM_InitSoundSIZE 5

#ifdef SOUND

void Sound_Pause(void)
{
	/* stop audio output */
	JAVANVM_SoundPause();
}

void Sound_Continue(void)
{
	/* start audio output */
	JAVANVM_SoundContinue();
}

void Sound_Update(void)
{
	int avail = JAVANVM_SoundAvailable();
	avail -= (line_buffer_size-dsp_buffer_size);
	if (avail<0) avail = 0;
	POKEYSND_Process(dsp_buffer, avail/2);
	JAVANVM_SoundWrite((void *)dsp_buffer, avail);
}

#endif /* SOUND */

void PLATFORM_PaletteUpdate(void)
{
	JAVANVM_InitPalette((void *)&Colours_table[0]);
}

int PLATFORM_Keyboard(void)
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

	UI_alt_function = -1;
	if (JAVANVM_Kbhits(VK_ALT,KEY_LOCATION_LEFT)) {
		if (key_pressed) {
			switch (lastkey) {
			case VK_F:
				key_pressed = 0;
				/*SwitchFullscreen();*/
				break;
			case VK_G:
				key_pressed = 0;
				/*SwitchWidth();*/
				break;
			case VK_B:
				key_pressed = 0;
				/*SwitchBW();*/
				break;
			case VK_J:
				key_pressed = 0;
				/*SwapJoysticks();*/
				break;
			case VK_R:
				UI_alt_function = UI_MENU_RUN;
				break;
			case VK_Y:
				UI_alt_function = UI_MENU_SYSTEM;
				break;
			case VK_O:
				UI_alt_function = UI_MENU_SOUND;
				break;
			case VK_W:
				UI_alt_function = UI_MENU_SOUND_RECORDING;
				break;
			case VK_A:
				UI_alt_function = UI_MENU_ABOUT;
				break;
			case VK_S:
				UI_alt_function = UI_MENU_SAVESTATE;
				break;
			case VK_D:
				UI_alt_function = UI_MENU_DISK;
				break;
			case VK_L:
				UI_alt_function = UI_MENU_LOADSTATE;
				break;
			case VK_C:
				UI_alt_function = UI_MENU_CARTRIDGE;
				break;
			case VK_T:
				UI_alt_function = UI_MENU_CASSETTE;
				break;
			case VK_BACK_SLASH:
				return AKEY_PBI_BB_MENU;
				break;
			break;
			}
		}
		if (UI_alt_function != -1) {
			key_pressed = 0;
			return AKEY_UI;
		}
	}
	/* SHIFT STATE */
	if ((JAVANVM_Kbhits(VK_SHIFT,KEY_LOCATION_LEFT)) || (JAVANVM_Kbhits(VK_SHIFT,KEY_LOCATION_RIGHT)))
		INPUT_key_shift = 1;
	else
		INPUT_key_shift = 0;

    /* CONTROL STATE */
	if ((JAVANVM_Kbhits(VK_CONTROL,KEY_LOCATION_LEFT)) || (JAVANVM_Kbhits(VK_CONTROL,KEY_LOCATION_RIGHT)))
		key_control = 1;
	else
		key_control = 0;

	/* OPTION / SELECT / START keys */
	INPUT_key_consol = INPUT_CONSOL_NONE;
	if (JAVANVM_Kbhits(VK_F2,KEY_LOCATION_STANDARD))
		INPUT_key_consol &= (~INPUT_CONSOL_OPTION);
	if (JAVANVM_Kbhits(VK_F3,KEY_LOCATION_STANDARD))
		INPUT_key_consol &= (~INPUT_CONSOL_SELECT);
	if (JAVANVM_Kbhits(VK_F4,KEY_LOCATION_STANDARD))
		INPUT_key_consol &= (~INPUT_CONSOL_START);

	if (key_pressed == 0)
		return AKEY_NONE;

	/* Handle movement and special keys. */
	switch (lastkey) {
	case VK_F1:
		key_pressed = 0;
		return AKEY_UI;
	case VK_F5:
		key_pressed = 0;
		return INPUT_key_shift ? AKEY_COLDSTART : AKEY_WARMSTART;
	case VK_F8:
		key_pressed = 0;
		return (PLATFORM_Exit(1) ? AKEY_NONE : AKEY_EXIT);
	case VK_F9:
		return AKEY_EXIT;
	case VK_F10:
		key_pressed = 0;
		return INPUT_key_shift ? AKEY_SCREENSHOT_INTERLACE : AKEY_SCREENSHOT_INTERLACE;
	}
	/* keyboard joysticks: don't pass the keypresses to emulation
	 * as some games pause on a keypress (River Raid, Bruce Lee)
	 */
#define LQE(x) (lastkey == KBD_##x && lastloc == KBD_##x##_LOC)
	if (!UI_is_active && kbd_joy_0_enabled) {
		if (LQE(STICK_0_LEFT) || LQE(STICK_0_RIGHT) ||
			LQE(STICK_0_UP) || LQE(STICK_0_DOWN) ||
			LQE(STICK_0_LEFTUP) || LQE(STICK_0_LEFTDOWN) ||
			LQE(STICK_0_RIGHTUP) || LQE(STICK_0_RIGHTDOWN) ||
			LQE(TRIG_0)) {
			key_pressed = 0;
			return AKEY_NONE;
		}
	}

	if (!UI_is_active && kbd_joy_1_enabled) {
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

	if (INPUT_key_shift)
		shiftctrl ^= AKEY_SHFT;

	if (Atari800_machine_type == Atari800_MACHINE_5200 && !UI_is_active && lastloc == KEY_LOCATION_STANDARD) {
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
		case '#':
		case '=':
			return AKEY_5200_HASH ^ shiftctrl;
		case '*':
			return AKEY_5200_ASTERISK ^ shiftctrl;
		}
		return AKEY_NONE;
	}

	if (key_control)
		shiftctrl ^= AKEY_CTRL;

	if (lastloc == KEY_LOCATION_STANDARD) switch (lastkey) {
	case VK_BACK_QUOTE:
		return AKEY_ATARI ^ shiftctrl;
	case VK_END:
	case VK_F6:
		return AKEY_HELP ^ shiftctrl;
	case VK_PAGE_DOWN:
		return AKEY_F2 | AKEY_SHFT;
	case VK_PAGE_UP:
		return AKEY_F1 | AKEY_SHFT;
	case VK_HOME:
		return (key_control ? AKEY_LESS : AKEY_CLEAR)|shiftctrl;
	case VK_PAUSE:
	case VK_F7:
		return AKEY_BREAK;
	case VK_CAPS_LOCK:
		if (INPUT_key_shift)
			return AKEY_CAPSLOCK|shiftctrl;
		else
			return AKEY_CAPSTOGGLE|shiftctrl;
	case VK_SPACE:
		return AKEY_SPACE ^ shiftctrl;
	case VK_BACK_SPACE:
		return AKEY_BACKSPACE|shiftctrl;
	case VK_ENTER:
		return AKEY_RETURN ^ shiftctrl;
	case VK_LEFT:
		return (INPUT_key_shift ? AKEY_PLUS : AKEY_LEFT) ^ shiftctrl;
	case VK_RIGHT:
		return (INPUT_key_shift ? AKEY_ASTERISK : AKEY_RIGHT) ^ shiftctrl;
	case VK_UP:
		return (INPUT_key_shift ? AKEY_MINUS : AKEY_UP) ^ shiftctrl;
	case VK_DOWN:
		return (INPUT_key_shift ? AKEY_EQUAL : AKEY_DOWN) ^ shiftctrl;
	case VK_ESCAPE:
		return AKEY_ESCAPE ^ shiftctrl;
	case VK_TAB:
		return AKEY_TAB ^ shiftctrl;
	case VK_DELETE:
		if (INPUT_key_shift)
			return AKEY_DELETE_LINE|shiftctrl;
		else
			return AKEY_DELETE_CHAR;
	case VK_INSERT:
		if (INPUT_key_shift)
			return AKEY_INSERT_LINE|shiftctrl;
		else
			return AKEY_INSERT_CHAR;
	}
	if (INPUT_cx85 && lastloc == KEY_LOCATION_NUMPAD) switch (lastkey) {
	case VK_NUMPAD1:
		return AKEY_CX85_1;
	case VK_NUMPAD2:
		return AKEY_CX85_2;
	case VK_NUMPAD3:
		return AKEY_CX85_3;
	case VK_NUMPAD4:
		return AKEY_CX85_4;
	case VK_NUMPAD5:
		return AKEY_CX85_5;
	case VK_NUMPAD6:
		return AKEY_CX85_6;
	case VK_NUMPAD7:
		return AKEY_CX85_7;
	case VK_NUMPAD8:
		return AKEY_CX85_8;
	case VK_NUMPAD9:
		return AKEY_CX85_9;
	case VK_NUMPAD0:
		return AKEY_CX85_0;
	case VK_DECIMAL:
		return AKEY_CX85_PERIOD;
	case VK_SUBTRACT:
		return AKEY_CX85_MINUS;
	case VK_ENTER:
		return AKEY_CX85_PLUS_ENTER;
	case VK_DIVIDE:
		return (key_control ? AKEY_CX85_ESCAPE : AKEY_CX85_NO);
	case VK_MULTIPLY:
		return AKEY_CX85_DELETE;
	case VK_ADD:
		return AKEY_CX85_YES;
	}

	/* Handle CTRL-0 to CTRL-9 and others */
	if (key_control && lastloc == KEY_LOCATION_STANDARD) {
		switch(lastuni) {
		case '.':
			return AKEY_FULLSTOP|shiftctrl;
		case ',':
			return AKEY_COMMA|shiftctrl;
		case ';':
			return AKEY_SEMICOLON|shiftctrl;
		case '/':
			return AKEY_SLASH|shiftctrl;
		}
		switch (lastkey) {
		case VK_BACK_SLASH:
			/* work-around for Windows */
			return AKEY_ESCAPE|shiftctrl;
		case VK_PERIOD:
			return AKEY_FULLSTOP|shiftctrl;
		case VK_COMMA:
			return AKEY_COMMA|shiftctrl;
		case VK_SEMICOLON:
			return AKEY_SEMICOLON|shiftctrl;
		case VK_SLASH:
			return AKEY_SLASH|shiftctrl;
		case VK_0:
			return AKEY_CTRL_0|shiftctrl;
		case VK_1:
			return AKEY_CTRL_1|shiftctrl;
		case VK_2:
			return AKEY_CTRL_2|shiftctrl;
		case VK_3:
			return AKEY_CTRL_3|shiftctrl;
		case VK_4:
			return AKEY_CTRL_4|shiftctrl;
		case VK_5:
			return AKEY_CTRL_5|shiftctrl;
		case VK_6:
			return AKEY_CTRL_6|shiftctrl;
		case VK_7:
			return AKEY_CTRL_7|shiftctrl;
		case VK_8:
			return AKEY_CTRL_8|shiftctrl;
		case VK_9:
			return AKEY_CTRL_9|shiftctrl;
		}
	}

	/* Host Caps Lock will make lastuni switch case, so prevent this*/
    if(lastuni>='A' && lastuni <= 'Z' && !INPUT_key_shift) lastuni += 0x20;
    if(lastuni>='a' && lastuni <= 'z' && INPUT_key_shift) lastuni -= 0x20;
	/* Uses only UNICODE translation, no shift states (this was added to
	 * support non-US keyboard layouts)*/
	if (lastloc == KEY_LOCATION_STANDARD) {
		switch (lastuni) {
		case 1:
			return AKEY_CTRL_a|shiftctrl;
		case 2:
			return AKEY_CTRL_b|shiftctrl;
		case 3:
			return AKEY_CTRL_c|shiftctrl;
		case 4:
			return AKEY_CTRL_d|shiftctrl;
		case 5:
			return AKEY_CTRL_e|shiftctrl;
		case 6:
			return AKEY_CTRL_f|shiftctrl;
		case 7:
			return AKEY_CTRL_g|shiftctrl;
		case 8:
			return AKEY_CTRL_h|shiftctrl;
		case 9:
			return AKEY_CTRL_i|shiftctrl;
		case 10:
			return AKEY_CTRL_j|shiftctrl;
		case 11:
			return AKEY_CTRL_k|shiftctrl;
		case 12:
			return AKEY_CTRL_l|shiftctrl;
		case 13:
			return AKEY_CTRL_m|shiftctrl;
		case 14:
			return AKEY_CTRL_n|shiftctrl;
		case 15:
			return AKEY_CTRL_o|shiftctrl;
		case 16:
			return AKEY_CTRL_p|shiftctrl;
		case 17:
			return AKEY_CTRL_q|shiftctrl;
		case 18:
			return AKEY_CTRL_r|shiftctrl;
		case 19:
			return AKEY_CTRL_s|shiftctrl;
		case 20:
			return AKEY_CTRL_t|shiftctrl;
		case 21:
			return AKEY_CTRL_u|shiftctrl;
		case 22:
			return AKEY_CTRL_v|shiftctrl;
		case 23:
			return AKEY_CTRL_w|shiftctrl;
		case 24:
			return AKEY_CTRL_x|shiftctrl;
		case 25:
			return AKEY_CTRL_y|shiftctrl;
		case 26:
			return AKEY_CTRL_z|shiftctrl;
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
	}

	return AKEY_NONE;
}

#ifdef SOUND
static void SoundSetup(void)
{
	int dsprate = 48000;
	int sound_flags = 0;
	int sconfig[JAVANVM_InitSoundSIZE];
	sound_flags |= POKEYSND_BIT16;
	sconfig[JAVANVM_InitSoundSampleRate] = dsprate;
	sconfig[JAVANVM_InitSoundBitsPerSample] = 16;
	sconfig[JAVANVM_InitSoundChannels] = POKEYSND_stereo_enabled ? 2 : 1;
	sconfig[JAVANVM_InitSoundSigned] = TRUE;
	sconfig[JAVANVM_InitSoundBigEndian] = TRUE;
	line_buffer_size = JAVANVM_InitSound((void *)&sconfig[0]);
	dsp_buffer_size = 4096; /*adjust this to fix skipping/latency*/
	if (POKEYSND_stereo_enabled) dsp_buffer_size *= 2;
	if (line_buffer_size < dsp_buffer_size) dsp_buffer_size = line_buffer_size;
	free(dsp_buffer);
	dsp_buffer = (UBYTE*)malloc(dsp_buffer_size);
	POKEYSND_Init(POKEYSND_FREQ_17_EXACT, dsprate, (POKEYSND_stereo_enabled ? 2 : 1) , sound_flags);
}

void Sound_Reinit(void)
{
	SoundSetup();
}
#endif /* SOUND */

int PLATFORM_Initialise(int *argc, char *argv[])
{
	int i, j;
	int help_only = FALSE;
	int scale = 2;
	for (i = j = 1; i < *argc; i++) {
		int i_a = (i + 1 < *argc);		/* is argument available? */
		int a_m = FALSE;			/* error, argument missing! */
		
		if (strcmp(argv[i], "-scale") == 0) {
			if (i_a)
				scale = Util_sscandec(argv[++i]);
			else a_m = TRUE;
		}
		else {
			if (strcmp(argv[i], "-help") == 0) {
				help_only = TRUE;
				Log_print("\t-scale <n>       Scale width and height by <n>");
			}
			argv[j++] = argv[i];
		}

		if (a_m) {
			Log_print("Missing argument for '%s'", argv[i]);
			return FALSE;
		}
	}
	*argc = j;

	if (!help_only) {
        int config[JAVANVM_InitGraphicsSIZE];
        config[JAVANVM_InitGraphicsScalew] = scale;
        config[JAVANVM_InitGraphicsScaleh] = scale;
        config[JAVANVM_InitGraphicsScreen_WIDTH] = Screen_WIDTH;
        config[JAVANVM_InitGraphicsScreen_HEIGHT] = Screen_HEIGHT;
        config[JAVANVM_InitGraphicsATARI_VISIBLE_WIDTH] = 336;
        config[JAVANVM_InitGraphicsATARI_LEFT_MARGIN] = 24;
		JAVANVM_InitGraphics((void *)&config[0]);
		JAVANVM_InitPalette((void *)&Colours_table[0]);
#ifdef SOUND
		SoundSetup();
#endif
	}
	if (INPUT_cx85) {
		kbd_joy_0_enabled = FALSE;
	}
	return TRUE;
}

int PLATFORM_Exit(int run_monitor){
	int restart;
	if (run_monitor) {
#ifdef SOUND
		Sound_Pause();
#endif
		restart = MONITOR_Run();
#ifdef SOUND
		Sound_Continue();
#endif
	}
   	else {
		restart = FALSE;
	}
	if (restart) {
		return 1;
	}
	return restart;
}

void PLATFORM_DisplayScreen(void){
	JAVANVM_DisplayScreen((void *)Screen_atari);
	return;
}

static void do_platform_PORT(UBYTE *s0, UBYTE *s1)
{
#define KBHITS(x) (JAVANVM_Kbhits(x,x##_LOC))
	int stick0, stick1;
	stick0 = stick1 = INPUT_STICK_CENTRE;

	if (kbd_joy_0_enabled) {
		if (KBHITS(KBD_STICK_0_LEFT))
			stick0 = INPUT_STICK_LEFT;
		if (KBHITS(KBD_STICK_0_RIGHT))
			stick0 = INPUT_STICK_RIGHT;
		if (KBHITS(KBD_STICK_0_UP))
			stick0 = INPUT_STICK_FORWARD;
		if (KBHITS(KBD_STICK_0_DOWN))
			stick0 = INPUT_STICK_BACK;
		if ((KBHITS(KBD_STICK_0_LEFTUP))
			|| ((KBHITS(KBD_STICK_0_LEFT)) && (KBHITS(KBD_STICK_0_UP))))
			stick0 = INPUT_STICK_UL;
		if ((KBHITS(KBD_STICK_0_LEFTDOWN))
			|| ((KBHITS(KBD_STICK_0_LEFT)) && (KBHITS(KBD_STICK_0_DOWN))))
			stick0 = INPUT_STICK_LL;
		if ((KBHITS(KBD_STICK_0_RIGHTUP))
			|| ((KBHITS(KBD_STICK_0_RIGHT)) && (KBHITS(KBD_STICK_0_UP))))
			stick0 = INPUT_STICK_UR;
		if ((KBHITS(KBD_STICK_0_RIGHTDOWN))
			|| ((KBHITS(KBD_STICK_0_RIGHT)) && (KBHITS(KBD_STICK_0_DOWN))))
			stick0 = INPUT_STICK_LR;
	}
	if (kbd_joy_1_enabled) {
		if (KBHITS(KBD_STICK_1_LEFT))
			stick1 = INPUT_STICK_LEFT;
		if (KBHITS(KBD_STICK_1_RIGHT))
			stick1 = INPUT_STICK_RIGHT;
		if (KBHITS(KBD_STICK_1_UP))
			stick1 = INPUT_STICK_FORWARD;
		if (KBHITS(KBD_STICK_1_DOWN))
			stick1 = INPUT_STICK_BACK;
		if ((KBHITS(KBD_STICK_1_LEFTUP))
			|| ((KBHITS(KBD_STICK_1_LEFT)) && (KBHITS(KBD_STICK_1_UP))))
			stick1 = INPUT_STICK_UL;
		if ((KBHITS(KBD_STICK_1_LEFTDOWN))
			|| ((KBHITS(KBD_STICK_1_LEFT)) && (KBHITS(KBD_STICK_1_DOWN))))
			stick1 = INPUT_STICK_LL;
		if ((KBHITS(KBD_STICK_1_RIGHTUP))
			|| ((KBHITS(KBD_STICK_1_RIGHT)) && (KBHITS(KBD_STICK_1_UP))))
			stick1 = INPUT_STICK_UR;
		if ((KBHITS(KBD_STICK_1_RIGHTDOWN))
			|| ((KBHITS(KBD_STICK_1_RIGHT)) && (KBHITS(KBD_STICK_1_DOWN))))
			stick1 = INPUT_STICK_LR;
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

static void do_platform_TRIG(UBYTE *t0, UBYTE *t1)
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

int PLATFORM_PORT(int num)
{
#ifndef DONT_DISPLAY
	if (num == 0) {
		UBYTE a, b;
		do_platform_PORT(&a, &b);
		return (b << 4) | (a & 0x0f);
	}
#endif
	return 0xff;
}

int PLATFORM_TRIG(int num)
{
#ifndef DONT_DISPLAY
	UBYTE a, b;
	do_platform_TRIG(&a, &b);
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

void PLATFORM_Sleep(double s){
	JAVANVM_Sleep((int)(s*1e3));
}

int main(int argc, char **argv)
{
	/* initialise Atari800 core */
	if (!Atari800_Initialise(&argc, argv))
		return 3;

	/* main loop */
	for (;;) {
		INPUT_key_code = PLATFORM_Keyboard();
		Atari800_Frame();
		if (Atari800_display_screen)
			PLATFORM_DisplayScreen();
		if (JAVANVM_CheckThreadStatus()) {
		   	Atari800_Exit(FALSE);
			exit(0);
		}
	}
}

/*
vim:ts=4:sw=4:
*/

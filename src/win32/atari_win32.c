/*
 * atari_win32.c - Win32 port specific code
 *
 * Copyright (C) 2000 Krzysztof Nikiel
 * Copyright (C) 2000-2007 Atari800 development team (see DOC/CREDITS)
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

#define DIRECTINPUT_VERSION	    0x0500

#include "config.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "atari.h"
#include "input.h"
#include "log.h"
#include "monitor.h"
#include "platform.h"
#include "screen.h"
#include "sound.h"
#include "ui.h"
#include "util.h"

#include "main.h"
#include "joystick.h"
#include "keyboard.h"
#include "screen_win32.h"
#include "atari_win32.h"

static int usesnd = 1;

static int kbjoy = 0;
static int win32keys = FALSE;

/* default configuration options */
/* declared in ui.c              */
ALTJOYMODE alternateJoystickMode = JOY_NORMAL_MODE;
KEYJOYMODE keyboardJoystickMode = KEYPAD_MODE;
BOOL mapController1Buttons = FALSE;
BOOL mapController2Buttons = FALSE;

FRAMEPARAMS frameparams;
FSRESOLUTION fsresolution;
SCREENMODE screenmode;
ASPECTMODE aspectmode;
ASPECTRATIO aspectratio;
BOOL usecustomfsresolution;
BOOL showcursor;
int windowscale;
int fullscreenWidth;
int fullscreenHeight;

/* This is an early init called once from main.cpp */
/* before primary system initialization.           */
void Win32_Init() {

	int i;

	/* initialize buttons on joysticks/gamepads */
	for (i = 0; i < MAX_PROG_BUTTONS; i++) {
		joystat.jsbutton[STICK1][i][STATE] = 0;
		joystat.jsbutton[STICK1][i][ASSIGN] = AKEY_NONE;
		
		joystat.jsbutton[STICK2][i][STATE] = 0;
		joystat.jsbutton[STICK2][i][ASSIGN] = AKEY_NONE;
	}
	
	frameparams.scanlinemode = NONE;
	frameparams.filter = NEARESTNEIGHBOR;
	frameparams.d3dRefresh = TRUE;
	frameparams.screensaver = FALSE;
	frameparams.tiltlevel = TILTLEVEL0;
}

int PLATFORM_Configure(char *option, char *parameters)
{
	int i;
	int value = 0;
	char buf[MAXKEYNAMELENGTH];
	
	if (strcmp(option, "DISPLAY_MODE") == 0) {
		if (strcmp(parameters,"GDI_NEARESTNEIGHBOR")==0) {
			SetDisplayMode(GDI_NEARESTNEIGHBOR);
			return TRUE;
		}
		if (strcmp(parameters,"GDIPLUS_NEARESTNEIGHBOR")==0) {
			SetDisplayMode(GDIPLUS_NEARESTNEIGHBOR);
			return TRUE;
		}
		if (strcmp(parameters,"GDIPLUS_BILINEAR")==0) {
			SetDisplayMode(GDIPLUS_BILINEAR);
			return TRUE;
		}
		if (strcmp(parameters,"GDIPLUS_HQBILINEAR")==0) {
			SetDisplayMode(GDIPLUS_HQBILINEAR);
			return TRUE;
		}
		if (strcmp(parameters,"GDIPLUS_HQBICUBIC")==0) {
			SetDisplayMode(GDIPLUS_HQBICUBIC);
			return TRUE;
		}
		if (strcmp(parameters,"DIRECT3D_NEARESTNEIGHBOR")==0) {
			SetDisplayMode(DIRECT3D_NEARESTNEIGHBOR);
			return TRUE;
		}
		if (strcmp(parameters,"DIRECT3D_BILINEAR")==0) {
			SetDisplayMode(DIRECT3D_BILINEAR);
			return TRUE;
		}
		return FALSE;
	}
	
	if (strcmp(option, "WINDOW_SCALE") == 0) {
			windowscale = Util_sscandec(parameters);
			return TRUE;
	}
	
	if (strcmp(option, "SCREEN_MODE") == 0) {
		if (strcmp(parameters,"WINDOW")==0) {
			screenmode = WINDOW;
			return TRUE;
		}
		if (strcmp(parameters,"FULLSCREEN")==0) {
			screenmode = FULLSCREEN;
			return TRUE;
		}
		return FALSE;
	}
	
	if (strcmp(option, "ASPECT_MODE") == 0) {
		if (strcmp(parameters,"OFF")==0) {
			aspectmode = OFF;
			return TRUE;
		}
		if (strcmp(parameters,"NORMAL")==0) {
			aspectmode = NORMAL;
			return TRUE;
		}
		if (strcmp(parameters,"ADAPTIVE")==0) {
			aspectmode = ADAPTIVE;
			return TRUE;
		}
		return FALSE;
	}
	
	if (strcmp(option, "ASPECT_RATIO") == 0) {
		if (strcmp(parameters,"HYBRID")==0) {
			aspectratio = HYBRID;  // normal 7:5 ratio
			return TRUE;
		}
		if (strcmp(parameters,"WIDE")==0) {
			aspectratio = WIDE;  // cropped 4:3 ratio
			return TRUE;
		}
		if (strcmp(parameters,"CROPPED")==0) {
			aspectratio = CROPPED;  // normal 7:5 ratio
			return TRUE;
		}
		if (strcmp(parameters,"COMPRESSED")==0) {
			aspectratio = COMPRESSED;  // cropped 4:3 ratio
			return TRUE;
		}
		return FALSE;
	}
	
	if (strcmp(option, "FULLSCREEN_RESOLUTION") == 0) {
		if (strcmp(parameters,"VGA")==0) {
			fsresolution = VGA;			
			fullscreenWidth = 640;
			fullscreenHeight = 480;
			usecustomfsresolution = TRUE;
			return TRUE;
		}
		if (strcmp(parameters,"SXGA")==0) {
			fsresolution = SXGA;			
			fullscreenWidth = 1280;
			fullscreenHeight = 960;
			usecustomfsresolution = TRUE;
			return TRUE;
		}
		if (strcmp(parameters,"UXGA")==0) {
			fsresolution = UXGA;			
			fullscreenWidth = 1600;
			fullscreenHeight = 1200;
			usecustomfsresolution = TRUE;
			return TRUE;
		}
		if (strcmp(parameters,"DESKTOPRES")==0) {
			fsresolution = DESKTOPRES;			
			usecustomfsresolution = FALSE;
			return TRUE;
		}
		return FALSE;
	}
	
	if (strcmp(option, "SCANLINE_MODE") == 0) {
		if (strcmp(parameters,"OFF")==0) {
			frameparams.scanlinemode = NONE;		
			return TRUE;
		}
		if (strcmp(parameters,"LOW")==0) {
			frameparams.scanlinemode = LOW;		
			return TRUE;
		}
		if (strcmp(parameters,"MEDIUM")==0) {
			frameparams.scanlinemode = MEDIUM;		
			return TRUE;
		}
		if (strcmp(parameters,"HIGH")==0) {
			frameparams.scanlinemode = HIGH;		
			return TRUE;
		}
		return FALSE;
	}
	
	if (strcmp(option, "SHOW_CURSOR") == 0) {
		if (strcmp(parameters,"1") == 0) {
			showcursor = TRUE;
			return TRUE;
		}
		if (strcmp(parameters,"0") == 0) {
			showcursor = FALSE;
			return TRUE;
		}
		return FALSE;
	}
	
	if (strcmp(option, "ALTERNATE_JOYSTICK_MODE") == 0) {
		if (strcmp(parameters,"JOY_NORMAL_MODE")==0) {
			alternateJoystickMode = JOY_NORMAL_MODE;
			return TRUE;
		}
		if (strcmp(parameters,"JOY_DUAL_MODE") == 0) {
			alternateJoystickMode = JOY_DUAL_MODE;
			return TRUE;
		}
		if (strcmp(parameters,"JOY_SHARED_MODE") == 0) {
			alternateJoystickMode = JOY_SHARED_MODE;
			return TRUE;
		}
		return FALSE;
	}
	
	if (strcmp(option, "KEYBOARD_JOYSTICK_MODE") == 0) {
		if (strcmp(parameters,"KEYPAD_MODE")==0) {
			keyboardJoystickMode = KEYPAD_MODE;
			return TRUE;
		}
		if (strcmp(parameters,"KEYPAD_PLUS_MODE") == 0) {
			keyboardJoystickMode = KEYPAD_PLUS_MODE;
			return TRUE;
		}
		if (strcmp(parameters,"ARROW_MODE") == 0) {
			keyboardJoystickMode = ARROW_MODE;
			return TRUE;
		}
		return FALSE;
	}
	
	if (strcmp(option, "MAP_CONTROLLER_1_BUTTONS") == 0) {
		if (strcmp(parameters,"1") == 0) {
			mapController1Buttons = TRUE;
			return TRUE;
		}
		if (strcmp(parameters,"0") == 0) {
			mapController1Buttons = FALSE;
			return TRUE;
		}
		return FALSE;
	}
	
	if (strcmp(option, "MAP_CONTROLLER_2_BUTTONS") == 0) {
		if (strcmp(parameters,"1") == 0) {
			mapController2Buttons = TRUE;
			return TRUE;
		}
		if (strcmp(parameters,"0") == 0) {
			mapController2Buttons = FALSE;
			return TRUE;
		}
		return FALSE;
	}
	
	/* read in controller 1 button assignments */
	for (i = 0; i < MAX_PROG_BUTTONS; i++) {
		sprintf(buf, "JOY_1_BUTTON_%d", i + 2);
		if (strcmp(option, buf) == 0) {
			joystat.jsbutton[STICK1][i][ASSIGN] = getkeyvalue(parameters);
			return TRUE;
		}
	}
	
	/* read in controller 2 button assignments */
	for (i = 0; i < MAX_PROG_BUTTONS; i++) {
		sprintf(buf, "JOY_2_BUTTON_%d", i + 2);
		if (strcmp(option, buf) == 0) {
			joystat.jsbutton[STICK2][i][ASSIGN] = getkeyvalue(parameters);
			return TRUE;
		}
	}
	
	return FALSE;
}

void PLATFORM_ConfigSave(FILE *fp)
{
	int i;
	char buf[MAXKEYNAMELENGTH];
	
	switch (GetDisplayMode()) {
		case GDI_NEARESTNEIGHBOR:
			fprintf(fp, "DISPLAY_MODE=GDI_NEARESTNEIGHBOR\n");
			break;
		case GDIPLUS_NEARESTNEIGHBOR:
			fprintf(fp, "DISPLAY_MODE=GDIPLUS_NEARESTNEIGHBOR\n");
			break;
		case GDIPLUS_BILINEAR:
			fprintf(fp, "DISPLAY_MODE=GDIPLUS_BILINEAR\n");
			break;
		case GDIPLUS_HQBILINEAR:
			fprintf(fp, "DISPLAY_MODE=GDIPLUS_HQBILINEAR\n");
			break;
		case GDIPLUS_HQBICUBIC:
			fprintf(fp, "DISPLAY_MODE=GDIPLUS_HQBICUBIC\n");
			break;
		case DIRECT3D_NEARESTNEIGHBOR:
			fprintf(fp, "DISPLAY_MODE=DIRECT3D_NEARESTNEIGHBOR\n");
			break;
		case DIRECT3D_BILINEAR:
			fprintf(fp, "DISPLAY_MODE=DIRECT3D_BILINEAR\n");
			break;
	}
	
	switch (screenmode) {
		case WINDOW:
			fprintf(fp, "SCREEN_MODE=WINDOW\n");
			break;
		case FULLSCREEN:
			fprintf(fp, "SCREEN_MODE=FULLSCREEN\n");
			break;
	}
	
	fprintf(fp, "WINDOW_SCALE=%d\n", windowscale);
	
	switch (aspectmode) {
		case OFF:
			fprintf(fp, "ASPECT_MODE=OFF\n");
			break;
		case NORMAL:
			fprintf(fp, "ASPECT_MODE=NORMAL\n");
			break;
		case ADAPTIVE:
			fprintf(fp, "ASPECT_MODE=ADAPTIVE\n");
			break;
	}
	
	switch (aspectratio) {
		case HYBRID:
			fprintf(fp, "ASPECT_RATIO=HYBRID\n");
			break;
		case WIDE:
			fprintf(fp, "ASPECT_RATIO=WIDE\n");
			break;
		case CROPPED:
			fprintf(fp, "ASPECT_RATIO=CROPPED\n");
			break;
		case COMPRESSED:
			fprintf(fp, "ASPECT_RATIO=COMPRESSED\n");
			break;
	}
	
	switch (fsresolution) {
		case VGA:
			fprintf(fp, "FULLSCREEN_RESOLUTION=VGA\n");
			break;
		case SXGA:
			fprintf(fp, "FULLSCREEN_RESOLUTION=SXGA\n");
			break;
		case UXGA:
			fprintf(fp, "FULLSCREEN_RESOLUTION=UXGA\n");
			break;
		case DESKTOPRES:
			fprintf(fp, "FULLSCREEN_RESOLUTION=DESKTOPRES\n");
			break;
	}
	
	switch (frameparams.scanlinemode) {
		case NONE:
			fprintf(fp, "SCANLINE_MODE=OFF\n");
			break;
		case LOW:
			fprintf(fp, "SCANLINE_MODE=LOW\n");
			break;
		case MEDIUM:
			fprintf(fp, "SCANLINE_MODE=MEDIUM\n");
			break;
		case HIGH:
			fprintf(fp, "SCANLINE_MODE=HIGH\n");
			break;
	}
	
	fprintf(fp, "SHOW_CURSOR=%d\n", showcursor);
	
	switch (alternateJoystickMode) {
		case JOY_NORMAL_MODE:
			fprintf(fp, "ALTERNATE_JOYSTICK_MODE=JOY_NORMAL_MODE\n");
			break;
		case JOY_DUAL_MODE:
			fprintf(fp, "ALTERNATE_JOYSTICK_MODE=JOY_DUAL_MODE\n");
			break;
		case JOY_SHARED_MODE:
			fprintf(fp, "ALTERNATE_JOYSTICK_MODE=JOY_SHARED_MODE\n");
			break;
	}
	
	switch (keyboardJoystickMode) {
		case KEYPAD_MODE:
			fprintf(fp, "KEYBOARD_JOYSTICK_MODE=KEYPAD_MODE\n");
			break;
		case KEYPAD_PLUS_MODE:
			fprintf(fp, "KEYBOARD_JOYSTICK_MODE=KEYPAD_PLUS_MODE\n");
			break;
		case ARROW_MODE:
			fprintf(fp, "KEYBOARD_JOYSTICK_MODE=ARROW_MODE\n");
			break;
	}
	
	fprintf(fp, "MAP_CONTROLLER_1_BUTTONS=%d\n", mapController1Buttons);
	fprintf(fp, "MAP_CONTROLLER_2_BUTTONS=%d\n", mapController2Buttons);
	
	/* save controller 1 programmable button assignments */
	for (i = 0; i < MAX_PROG_BUTTONS; i++) {
		getkeyname(joystat.jsbutton[STICK1][i][ASSIGN], buf);
		fprintf(fp, "JOY_1_BUTTON_%d=%s\n", i + 2, buf);
	}
	
	/* save controller 2 programmable button assignments */
	for (i = 0; i < MAX_PROG_BUTTONS; i++) {
		getkeyname(joystat.jsbutton[STICK2][i][ASSIGN], buf);
		fprintf(fp, "JOY_2_BUTTON_%d=%s\n", i + 2, buf);
	}
}

/* get the AKEY_ value of a key with the given name */
int getkeyvalue(char *name) {
	
  int i;

	for (i = 0; i < MAXKEYREFS; i++) {	
	  if (strcmp(name, keyref[i].keyname) == 0) {
		return keyref[i].keyvalue;
	  }
	}
	return AKEY_NONE;
}   

/* get the name of a key with the given AKEY_ value */
void getkeyname(int value, char *name) {
	
	int i;

	for (i = 0; i < MAXKEYREFS; i++) {
		if (keyref[i].keyvalue == value)	{
			sprintf(name, "%s", keyref[i].keyname);
			return;
		}
	}
	sprintf(name, "%s", "N/A");
}

static int stick0 = INPUT_STICK_CENTRE;
static int stick1 = INPUT_STICK_CENTRE;

static int trig0 = 1;
static int trig1 = 1;

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
		    if (keyboardJoystickMode != ARROW_MODE)
				keycode |= AKEY_UP;
			else
				keycode |= AKEY_NONE;
			break;
		case DIK_DOWN:
			if (keyboardJoystickMode != ARROW_MODE)
				keycode |= AKEY_DOWN;
			else
				keycode |= AKEY_NONE;
			break;
		case DIK_LEFT:
			if (keyboardJoystickMode != ARROW_MODE)
				keycode |= AKEY_LEFT;
			else
				keycode |= AKEY_NONE;
			break;
		case DIK_RIGHT:
			if (keyboardJoystickMode != ARROW_MODE)
				keycode |= AKEY_RIGHT;
			else
				keycode |= AKEY_NONE;
			break;
		case DIK_NUMPAD8:
			if (keyboardJoystickMode == ARROW_MODE)
				keycode |= AKEY_UP;
			else
				keycode |= AKEY_NONE;
		    break;
		case DIK_NUMPAD2:
			if (keyboardJoystickMode == ARROW_MODE)
				keycode |= AKEY_DOWN;
			else
				keycode |= AKEY_NONE;
		    break;
        case DIK_NUMPAD4:
			if (keyboardJoystickMode == ARROW_MODE)
				keycode |= AKEY_LEFT;
			else
				keycode |= AKEY_NONE;
		    break;
        case DIK_NUMPAD6:
			if (keyboardJoystickMode == ARROW_MODE)
				keycode |= AKEY_RIGHT;
			else
				keycode |= AKEY_NONE;
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

 /* get  keycode for a single console key button press */
 int getbuttonkeycode (int buttondef)
 {
	int retval = AKEY_NONE;
 
	switch (buttondef) {
		case AKEY_OPTION:
			INPUT_key_consol = INPUT_CONSOL_SELECT + INPUT_CONSOL_START;
			break;
		case AKEY_SELECT:
			INPUT_key_consol = INPUT_CONSOL_OPTION + INPUT_CONSOL_START;
			break;
		case AKEY_START:
			INPUT_key_consol = INPUT_CONSOL_SELECT + INPUT_CONSOL_OPTION;
			break;
		default:
			retval = buttondef;
	}

	return retval;
 }
 
 void PLATFORM_GetButtonAssignments(int stick, int button, char *buffer, int bufsize)
 {
    getkeyname(joystat.jsbutton[stick][button][ASSIGN], buffer);
	buffer[bufsize-1] = '\0';
 }
 
 void PLATFORM_SetButtonAssignment(int stick, int button, int value)
{
	if (stick == STICK1) {
		joystat.jsbutton[stick][button][ASSIGN] = value;
	}
	else if (stick == STICK2) {
		joystat.jsbutton[stick][button][ASSIGN] = value;
	}
	
	return;
}

/* capture keystroke for assigning joystick buttons in UI */
int PLATFORM_GetKeyName(void)
{
	int keycode;
	
	clearkb();	
	while(TRUE)	{
		keycode = PLATFORM_Keyboard();
		if (keycode != AKEY_NONE)
		  return keycode;
		if (kbhits[DIK_F2])
		  return AKEY_OPTION;
		if (kbhits[DIK_F3])
		  return AKEY_SELECT;
		if (kbhits[DIK_F4])
		  return AKEY_START;
	}
}
 
 int PLATFORM_Keyboard(void)
{
	int keycode;
	int i;

	prockb();

	stick0 = INPUT_STICK_CENTRE;
	stick1 = INPUT_STICK_CENTRE;
	
	if (kbjoy) { 
		if (keyboardJoystickMode == ARROW_MODE) {
			trig0 = kbhits[DIK_LCONTROL] ? 0 : 1;  /* use left ctrl key for trigger */
			
			for (i = 1; i < sizeof(joydefs_arrow) / sizeof(joydefs_arrow[0]); i++)
			if (kbhits[joydefs_arrow[i]])
				stick0 &= joymask_arrow[i];
		}
		else if (keyboardJoystickMode == KEYPAD_PLUS_MODE) {
			trig0 = (kbhits[joydefs_plus[0]] || kbhits[DIK_LCONTROL]) ? 0 : 1;  /* use left ctrl key or 0 for trigger */
			
			for (i = 1; i < sizeof(joydefs_plus) / sizeof(joydefs_plus[0]); i++)
			if (kbhits[joydefs_plus[i]])
				stick0 &= joymask_plus[i];
		}
		else { /* standard KEYPAD_MODE */
			trig0 = kbhits[joydefs[0]] ? 0 : 1;  /* use 0 for trigger */
			
			for (i = 1; i < sizeof(joydefs) / sizeof(joydefs[0]); i++)
			if (kbhits[joydefs[i]])
				stick0 &= joymask[i];
		}
	}
	else {
		if (!procjoy(STICK1)) {
			stick0 &= joystat.stick;
			trig0 = !joystat.trig;
			
			if (alternateJoystickMode == JOY_SHARED_MODE) {
				stick1 &= joystat.stick;
				trig1 = !joystat.trig;
			}
			
			/* process programmable buttons on stick 1 */
			/* if the feature is enabled    */
			if (mapController1Buttons) {
				for (i = 0; i < MAX_PROG_BUTTONS; i++) {
					if (joystat.jsbutton[STICK1][i][STATE]) 
						return getbuttonkeycode(joystat.jsbutton[STICK1][i][ASSIGN]);
				}
			}

			/* process second stick on player1 controller */
			/* if the feature is enabled               */
			if (alternateJoystickMode == JOY_DUAL_MODE) {			
				stick1 &= joystat.stick_1;		
			}
		}
		if (!procjoy(STICK2)) {
			stick1 &= joystat.stick;
			trig1 = !joystat.trig;
			
			/* process programmable buttons on stick 2 */
			/* if the feature is enabled    */
			if (mapController2Buttons) {
				for (i = 0; i < MAX_PROG_BUTTONS; i++) {
					if (joystat.jsbutton[STICK2][i][STATE]) 
						return getbuttonkeycode(joystat.jsbutton[STICK2][i][ASSIGN]);
				}
			}
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

	// Support Win32 specific hot-key sequences
	if (kbhits[DIK_LMENU] || kbhits[DIK_RMENU]) { /* left or right Alt key is pressed */
		switch (kbcode) {
			case DIK_PGUP:     
				return AKEY32_WINDOWSIZEUP;  		/* ALT+PAGEUP .. Increase window size */
			case DIK_PGDN:
				return AKEY32_WINDOWSIZEDOWN;  		/* ALT+PAGEDOWN .. Decrease window size */
			case DIK_I:
				return AKEY32_TOGGLESCANLINEMODE;  	/* ALT+I  .. Toggle Interleave (scanline) mode*/ 
			case DIK_Z:
				return AKEY32_TOGGLESCREENSAVER;  	/* ALT+Z .. Toggle "screensaver" mode */ 
			case DIK_T:
				return AKEY32_TILTSCREEN;  		/* ALT+T .. Toggle Tilt mode */ 
			case DIK_RETURN:
				return AKEY32_TOGGLEFULLSCREEN;  	/* ALT+ENTER .. Toggle fullscreen mode*/ 
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
		case DIK_F12:
			kbcode = 0;
			return AKEY_TURBO;
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
		if (keyboardJoystickMode != ARROW_MODE  || UI_is_active)
			keycode ^= (INPUT_key_shift ? AKEY_MINUS : AKEY_UP);
		else 
			keycode ^= AKEY_NONE;
		break;
	case DIK_DOWN:
	    if (keyboardJoystickMode != ARROW_MODE || UI_is_active)
			keycode ^= (INPUT_key_shift ? AKEY_EQUAL : AKEY_DOWN);
		else 
			keycode ^= AKEY_NONE;
		break;
	case DIK_LEFT:
	    if (keyboardJoystickMode != ARROW_MODE || UI_is_active)
			keycode ^= (INPUT_key_shift ? AKEY_PLUS : AKEY_LEFT);
		else 
			keycode ^= AKEY_NONE;
		break;
	case DIK_RIGHT:
	    if (keyboardJoystickMode != ARROW_MODE || UI_is_active)
			keycode ^= (INPUT_key_shift ? AKEY_ASTERISK : AKEY_RIGHT);
		else 
			keycode ^= AKEY_NONE;
		break;
	case DIK_NUMPAD8:
		if (keyboardJoystickMode == ARROW_MODE)
			keycode ^= (INPUT_key_shift ? AKEY_MINUS : AKEY_UP);
		else 
			keycode ^= AKEY_NONE;
		break;
	case DIK_NUMPAD2:
		if (keyboardJoystickMode == ARROW_MODE)
			keycode ^= (INPUT_key_shift ? AKEY_EQUAL : AKEY_DOWN);
		else 
			keycode ^= AKEY_NONE;
		break;
	case DIK_NUMPAD4:
		if (keyboardJoystickMode == ARROW_MODE)
			keycode ^= (INPUT_key_shift ? AKEY_PLUS : AKEY_LEFT);
		else 
			keycode ^= AKEY_NONE;	
		break;
	case DIK_NUMPAD6:
		if (keyboardJoystickMode == ARROW_MODE)
			keycode ^= (INPUT_key_shift ? AKEY_ASTERISK : AKEY_RIGHT);
		else 
			keycode ^= AKEY_NONE;
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

// ensure a parameter argument exists and
// does not have a hyphen
BOOL checkparamarg(char arg[])
{
	size_t pos;
	char val[] = "-";

	if (!arg)
		return FALSE;    // no value found (EOL condition)
	
	pos = strcspn(arg, val);
	if (pos != strlen(arg))  // contains a hyphen
		return FALSE;

	return TRUE;	
}

void cmdlineFail(char msg[])
{
	char txt[256];
	sprintf(txt, "There was an invalid or missing argument\nfor commandline parameter: %s\nThe emulator may not run as expected.", msg);
	MessageBox(NULL, txt, "Commandline Error", MB_OK | MB_ICONWARNING);
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
		else if (strcmp(argv[i], "-keyjoy") == 0)
		{
			if (!checkparamarg(argv[++i]))
				cmdlineFail(argv[i-1]);
			else if (strcmp(argv[i], "keypad") == 0) 
				keyboardJoystickMode = KEYPAD_MODE;
			else if (strcmp(argv[i], "keypadplus") == 0) 
				keyboardJoystickMode = KEYPAD_PLUS_MODE;
			else if (strcmp(argv[i], "arrow") == 0)
				keyboardJoystickMode = ARROW_MODE;
		}
		else if (strcmp(argv[i], "-altjoy") == 0)
		{
			if (!checkparamarg(argv[++i]))
				cmdlineFail(argv[i-1]);
			else if (strcmp(argv[i], "normal") == 0) 
				alternateJoystickMode = JOY_NORMAL_MODE;
			else if (strcmp(argv[i], "dual") == 0) 
				alternateJoystickMode = JOY_DUAL_MODE;
			else if (strcmp(argv[i], "shared") == 0)
				alternateJoystickMode = JOY_SHARED_MODE;
		}
		else if (strcmp(argv[i], "-mapjoy1buttons") == 0) {
			mapController1Buttons = TRUE;
			Log_print("using mapped controller 1 buttons");
		}
		else if (strcmp(argv[i], "-mapjoy2buttons") == 0) {
			mapController2Buttons = TRUE;
			Log_print("using mapped controller 2 buttons");
		}
		else if (strcmp(argv[i], "-noconsole") == 0) {
			Log_print("console off");
		}
		else {
			if (strcmp(argv[i], "-help") == 0) {
				help_only = TRUE;
				Log_print("\t-nojoystick      Disable joystick");
				Log_print("\t-win32keys       Support for international keyboards");
				Log_print("\t-keyjoy <mode>   Keyboard joystick mode <keypad>, <keypadplus>, <arrow>");
				Log_print("\t-altjoy <mode>   Alternate joystick mode <normal>, <dual>, <shared>");
				Log_print("\t-mapjoy1buttons  Use mapped controller 1 buttons");
				Log_print("\t-mapjoy2buttons  Use mapped controller 2 buttons");
			}
			argv[j++] = argv[i];
		}
	}
	
	*argc = j;

	if (gron(argc, argv))
		exit(1);
		
	if (help_only)
		return FALSE;

#ifdef SOUND
	if (usesnd)
		Sound_Initialise(argc, argv);
#endif

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
	    if (alternateJoystickMode == JOY_SHARED_MODE)
			return trig1 && trig0;
		else
			return trig1;
	default:
		return 1;
	}
}

void Atari800_Frame32(void)
{
	// Process Win32-specific hot-key presses
	switch (INPUT_key_code)
	{	
		case AKEY32_WINDOWSIZEUP:
			kbcode = 0;
			changewindowsize(STEPUP, 50);
			break;
		case AKEY32_WINDOWSIZEDOWN:
			kbcode = 0;
			changewindowsize(STEPDOWN, 50);
			break;  
		case AKEY32_TOGGLESCANLINEMODE:
			kbcode = 0;
			changescanlinemode();
			break;
		case AKEY32_TOGGLESCREENSAVER:
			kbcode = 0;
			togglescreensaver();
			break;
		case AKEY32_TILTSCREEN:
			kbcode = 0;
			changetiltlevel();
			break;
		case AKEY32_TOGGLEFULLSCREEN:
			kbcode = 0;
			togglewindowstate();
			break;			
	}
}

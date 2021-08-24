/*
 * libatari800/input.c - Atari800 as a library - input device support
 *
 * Copyright (c) 2001-2002 Jacek Poplawski
 * Copyright (C) 2001-2014 Atari800 development team (see DOC/CREDITS)
 * Copyright (c) 2016-2019 Rob McMullen
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
#include "libatari800/init.h"
#include "libatari800/input.h"
#include "akey.h"
#include "atari.h"
#include "binload.h"
#include "../input.h"
#include "log.h"
#include "platform.h"
#include "pokey.h"
#include "libatari800/statesav.h"

static int lastkey = -1, key_control = 0;

input_template_t *LIBATARI800_Input_array = NULL;


int PLATFORM_Keyboard(void)
{
	int shiftctrl = 0;
	int keycode = 0;

	input_template_t *input = LIBATARI800_Input_array;

	lastkey = input->keychar;
	if (lastkey == 0) {
		keycode = TRUE;
		lastkey = input->keycode;
	}
	if (lastkey == 0) {
		keycode = TRUE;
		lastkey = -input->special;
	}

	/* SHIFT STATE */
	INPUT_key_shift = input->shift;

	/* CONTROL STATE */
	key_control = input->control;

	/*
	if (event.type == 2 || event.type == 3) {
		Log_print("E:%x S:%x C:%x K:%x U:%x M:%x",event.type,INPUT_key_shift,key_control,lastkey,event.key.keysym.unicode,event.key.keysym.mod);
	}
	*/

	BINLOAD_pause_loading = FALSE;

	/* OPTION / SELECT / START keys */
	INPUT_key_consol = INPUT_CONSOL_NONE;
	if (input->option)
		INPUT_key_consol &= ~INPUT_CONSOL_OPTION;
	if (input->select)
		INPUT_key_consol &= ~INPUT_CONSOL_SELECT;
	if (input->start)
		INPUT_key_consol &= ~INPUT_CONSOL_START;

	if (!lastkey) {
		return AKEY_NONE;
	}

	/* if the raw keycode has been sent, assume they know what they're doing
	and have all the correct shift & control bits set if needed */
	if (keycode || lastkey < 0)
		return lastkey;

	if (INPUT_key_shift)
		shiftctrl ^= AKEY_SHFT;

	/* FIXME: handle 5200 as in the SDL port*/

	if (key_control)
		shiftctrl ^= AKEY_CTRL;

	/* Handle special keys */
	switch (lastkey) {
	case ' ':
		return AKEY_SPACE ^ shiftctrl;
	case '\n':
		return AKEY_RETURN ^ shiftctrl;
	case '\b':
		return AKEY_BACKSPACE|shiftctrl;
	case '\t':
		return AKEY_TAB ^ shiftctrl;
	case 27:
		return AKEY_ESCAPE ^ shiftctrl;
	case 127:
		return AKEY_DELETE_CHAR;
	}

	/* Handle CTRL-0 to CTRL-9 and other control characters */
	if (key_control) {
		switch(lastkey) {
		case '.':
			return AKEY_FULLSTOP|shiftctrl;
		case ',':
			return AKEY_COMMA|shiftctrl;
		case ';':
			return AKEY_SEMICOLON|shiftctrl;
		case '/':
			return AKEY_SLASH|shiftctrl;
		case '\\':
			/* work-around for Windows */
			return AKEY_ESCAPE|shiftctrl;
		case '0':
			return AKEY_CTRL_0|shiftctrl;
		case '1':
			return AKEY_CTRL_1|shiftctrl;
		case '2':
			return AKEY_CTRL_2|shiftctrl;
		case '3':
			return AKEY_CTRL_3|shiftctrl;
		case '4':
			return AKEY_CTRL_4|shiftctrl;
		case '5':
			return AKEY_CTRL_5|shiftctrl;
		case '6':
			return AKEY_CTRL_6|shiftctrl;
		case '7':
			return AKEY_CTRL_7|shiftctrl;
		case '8':
			return AKEY_CTRL_8|shiftctrl;
		case '9':
			return AKEY_CTRL_9|shiftctrl;
		}
	}

	/* input.c takes care of removing invalid shift+control keys */
	switch (lastkey) {
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
		return AKEY_A|shiftctrl;
	case 'B':
		return AKEY_B|shiftctrl;
	case 'C':
		return AKEY_C|shiftctrl;
	case 'D':
		return AKEY_D|shiftctrl;
	case 'E':
		return AKEY_E|shiftctrl;
	case 'F':
		return AKEY_F|shiftctrl;
	case 'G':
		return AKEY_G|shiftctrl;
	case 'H':
		return AKEY_H|shiftctrl;
	case 'I':
		return AKEY_I|shiftctrl;
	case 'J':
		return AKEY_J|shiftctrl;
	case 'K':
		return AKEY_K|shiftctrl;
	case 'L':
		return AKEY_L|shiftctrl;
	case 'M':
		return AKEY_M|shiftctrl;
	case 'N':
		return AKEY_N|shiftctrl;
	case 'O':
		return AKEY_O|shiftctrl;
	case 'P':
		return AKEY_P|shiftctrl;
	case 'Q':
		return AKEY_Q|shiftctrl;
	case 'R':
		return AKEY_R|shiftctrl;
	case 'S':
		return AKEY_S|shiftctrl;
	case 'T':
		return AKEY_T|shiftctrl;
	case 'U':
		return AKEY_U|shiftctrl;
	case 'V':
		return AKEY_V|shiftctrl;
	case 'W':
		return AKEY_W|shiftctrl;
	case 'X':
		return AKEY_X|shiftctrl;
	case 'Y':
		return AKEY_Y|shiftctrl;
	case 'Z':
		return AKEY_Z|shiftctrl;
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
		return AKEY_a|shiftctrl;
	case 'b':
		return AKEY_b|shiftctrl;
	case 'c':
		return AKEY_c|shiftctrl;
	case 'd':
		return AKEY_d|shiftctrl;
	case 'e':
		return AKEY_e|shiftctrl;
	case 'f':
		return AKEY_f|shiftctrl;
	case 'g':
		return AKEY_g|shiftctrl;
	case 'h':
		return AKEY_h|shiftctrl;
	case 'i':
		return AKEY_i|shiftctrl;
	case 'j':
		return AKEY_j|shiftctrl;
	case 'k':
		return AKEY_k|shiftctrl;
	case 'l':
		return AKEY_l|shiftctrl;
	case 'm':
		return AKEY_m|shiftctrl;
	case 'n':
		return AKEY_n|shiftctrl;
	case 'o':
		return AKEY_o|shiftctrl;
	case 'p':
		return AKEY_p|shiftctrl;
	case 'q':
		return AKEY_q|shiftctrl;
	case 'r':
		return AKEY_r|shiftctrl;
	case 's':
		return AKEY_s|shiftctrl;
	case 't':
		return AKEY_t|shiftctrl;
	case 'u':
		return AKEY_u|shiftctrl;
	case 'v':
		return AKEY_v|shiftctrl;
	case 'w':
		return AKEY_w|shiftctrl;
	case 'x':
		return AKEY_x|shiftctrl;
	case 'y':
		return AKEY_y|shiftctrl;
	case 'z':
		return AKEY_z|shiftctrl;
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

void LIBATARI800_Mouse(void)
{
	int mouse_mode;

	input_template_t *input = LIBATARI800_Input_array;

	mouse_mode = input->mouse_mode;

	if (mouse_mode == LIBATARI800_FLAG_DIRECT_MOUSE) {
		int potx, poty;

		potx = input->mousex;
		poty = input->mousey;
		if(potx < 0) potx = 0;
		if(poty < 0) poty = 0;
		if(potx > 227) potx = 227;
		if(poty > 227) poty = 227;
		POKEY_POT_input[INPUT_mouse_port << 1] = 227 - potx;
		POKEY_POT_input[(INPUT_mouse_port << 1) + 1] = 227 - poty;
	}
	else {
		INPUT_mouse_delta_x = input->mousex;
		INPUT_mouse_delta_y = input->mousey;
	}

	INPUT_mouse_buttons = input->mouse_buttons;
}

int LIBATARI800_Input_Initialise(int *argc, char *argv[])
{
	return TRUE;
}

int PLATFORM_PORT(int num)
{
	input_template_t *input = LIBATARI800_Input_array;

	if (num == 0) {
		return (input->joy0 + (input->joy1 << 4)) ^ 0xff;
	}
	else if (num == 1) {
		return (input->joy2 + (input->joy3 << 4)) ^ 0xff;
	}
	return 0xff;
}

int PLATFORM_TRIG(int num)
{
	input_template_t *input = LIBATARI800_Input_array;

	switch (num) {
	case 0:
		return input->trig0 ? 0 : 1;
	case 1:
		return input->trig1 ? 0 : 1;
	case 2:
		return input->trig2 ? 0 : 1;
	case 3:
		return input->trig3 ? 0 : 1;
	default:
		break;
	}
	return 1;
}

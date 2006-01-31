/*
 * input.c - keyboard, joysticks and mouse emulation
 *
 * Copyright (C) 2001-2002 Piotr Fusik
 * Copyright (C) 2001-2006 Atari800 development team (see DOC/CREDITS)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "config.h"
#include <string.h>
#include "antic.h"
#include "atari.h"
#include "cassette.h"
#include "cpu.h"
#include "gtia.h"
#include "input.h"
#include "log.h"
#include "memory.h"
#include "pia.h"
#include "platform.h"
#include "pokey.h"
#include "util.h"
#ifndef CURSES_BASIC
#include "screen.h" /* for atari_screen */
#endif
#ifdef __PLUS
#include "input_win.h"
#endif

#ifdef DREAMCAST
extern int Atari_POT(int);
#else
#define Atari_POT(x) 228
#endif

int key_code = AKEY_NONE;
int key_shift = 0;
int key_consol = CONSOL_NONE;

int joy_autofire[4] = {AUTOFIRE_OFF, AUTOFIRE_OFF, AUTOFIRE_OFF, AUTOFIRE_OFF};

int joy_block_opposite_directions = 1;

int joy_multijoy = 0;

int joy_5200_min = 6;
int joy_5200_center = 114;
int joy_5200_max = 220;

int mouse_mode = MOUSE_OFF;
int mouse_port = 0;
int mouse_delta_x = 0;
int mouse_delta_y = 0;
int mouse_buttons = 0;
int mouse_speed = 3;
int mouse_pot_min = 1;
int mouse_pot_max = 228;
/* There should be UI or options for light pen/gun offsets.
   Below are best offsets for different programs:
   AtariGraphics: H = 0..32, V = 0 (there's calibration in the program)
   Bug Hunt: H = 44, V = 2
   Barnyard Blaster: H = 40, V = 0
   Operation Blood (light gun version): H = 40, V = 4
 */
int mouse_pen_ofs_h = 42;
int mouse_pen_ofs_v = 2;
int mouse_joy_inertia = 10;

#ifndef MOUSE_SHIFT
#define MOUSE_SHIFT 4
#endif
static int mouse_x = 0;
static int mouse_y = 0;
static int mouse_move_x = 0;
static int mouse_move_y = 0;
static int mouse_pen_show_pointer = 0;

static UBYTE mouse_amiga_codes[16] = {
	0x00, 0x02, 0x0a, 0x08,
	0x01, 0x03, 0x0b, 0x09,
	0x05, 0x07, 0x0f, 0x0d,
	0x04, 0x06, 0x0e, 0x0c
};

static UBYTE mouse_st_codes[16] = {
	0x00, 0x02, 0x03, 0x01,
	0x08, 0x0a, 0x0b, 0x09,
	0x0c, 0x0e, 0x0f, 0x0d,
	0x04, 0x06, 0x07, 0x05
};

static UBYTE STICK[4];
static UBYTE TRIG_input[4];

static int joy_multijoy_no = 0;	/* number of selected joy */

static int max_scanline_counter;
static int scanline_counter;

void INPUT_Initialise(int *argc, char *argv[])
{
	int i;
	int j;

	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-mouse") == 0) {
			char *mode = argv[++i];
			if (strcmp(mode, "off") == 0)
				mouse_mode = MOUSE_OFF;
			else if (strcmp(mode, "pad") == 0)
				mouse_mode = MOUSE_PAD;
			else if (strcmp(mode, "touch") == 0)
				mouse_mode = MOUSE_TOUCH;
			else if (strcmp(mode, "koala") == 0)
				mouse_mode = MOUSE_KOALA;
			else if (strcmp(mode, "pen") == 0)
				mouse_mode = MOUSE_PEN;
			else if (strcmp(mode, "gun") == 0)
				mouse_mode = MOUSE_GUN;
			else if (strcmp(mode, "amiga") == 0)
				mouse_mode = MOUSE_AMIGA;
			else if (strcmp(mode, "st") == 0)
				mouse_mode = MOUSE_ST;
			else if (strcmp(mode, "trak") == 0)
				mouse_mode = MOUSE_TRAK;
			else if (strcmp(mode, "joy") == 0)
				mouse_mode = MOUSE_JOY;
		}
		else if (strcmp(argv[i], "-mouseport") == 0) {
			mouse_port = Util_sscandec(argv[++i]) - 1;
			if (mouse_port < 0 || mouse_port > 3) {
				Aprint("Invalid mouse port, using 1");
				mouse_port = 0;
			}
		}
		else if (strcmp(argv[i], "-mousespeed") == 0) {
			mouse_speed = Util_sscandec(argv[++i]);
			if (mouse_speed < 1 || mouse_speed > 9) {
				Aprint("Invalid mouse speed, using 3");
				mouse_speed = 3;
			}
		}
		else if (strcmp(argv[i], "-multijoy") == 0) {
			joy_multijoy = 1;
		}
		else {
			if (strcmp(argv[i], "-help") == 0) {
				Aprint("\t-mouse off       Do not use mouse");
				Aprint("\t-mouse pad       Emulate paddles");
				Aprint("\t-mouse touch     Emulate Atari Touch Tablet");
				Aprint("\t-mouse koala     Emulate Koala Pad");
				Aprint("\t-mouse pen       Emulate Light Pen");
				Aprint("\t-mouse gun       Emulate Light Gun");
				Aprint("\t-mouse amiga     Emulate Amiga mouse");
				Aprint("\t-mouse st        Emulate Atari ST mouse");
				Aprint("\t-mouse trak      Emulate Atari Trak-Ball");
				Aprint("\t-mouse joy       Emulate joystick using mouse");
				Aprint("\t-mouseport <n>   Set mouse port 1-4 (default 1)");
				Aprint("\t-mousespeed <n>  Set mouse speed 1-9 (default 3)");
				Aprint("\t-multijoy        Emulate MultiJoy4 interface");
			}
			argv[j++] = argv[i];
		}
	}

	INPUT_CenterMousePointer();
	*argc = j;
}

/* mouse_step is used in Amiga, ST, trak-ball and joystick modes.
   It moves mouse_x and mouse_y in the direction given by
   mouse_move_x and mouse_move_y.
   Bresenham's algorithm is used:
   if (abs(deltaX) >= abs(deltaY)) {
     if (deltaX == 0)
       return;
     MoveHorizontally();
     e -= abs(deltaY);
     if (e < 0) {
       e += abs(deltaX);
       MoveVertically();
     }
   }
   else {
     MoveVertically();
     e -= abs(deltaX);
     if (e < 0) {
       e += abs(deltaY);
       MoveHorizontally();
     }
   }
   Returned is stick code for the movement direction.
*/
static UBYTE mouse_step(void)
{
	static int e = 0;
	UBYTE r = STICK_CENTRE;
	int dx = mouse_move_x >= 0 ? mouse_move_x : -mouse_move_x;
	int dy = mouse_move_y >= 0 ? mouse_move_y : -mouse_move_y;
	if (dx >= dy) {
		if (mouse_move_x == 0)
			return r;
		if (mouse_move_x < 0) {
			r &= STICK_LEFT;
			mouse_x--;
			mouse_move_x += 1 << MOUSE_SHIFT;
			if (mouse_move_x > 0)
				mouse_move_x = 0;
		}
		else {
			r &= STICK_RIGHT;
			mouse_x++;
			mouse_move_x -= 1 << MOUSE_SHIFT;
			if (mouse_move_x < 0)
				mouse_move_x = 0;
		}
		e -= dy;
		if (e < 0) {
			e += dx;
			if (mouse_move_y < 0) {
				r &= STICK_FORWARD;
				mouse_y--;
				mouse_move_y += 1 << MOUSE_SHIFT;
				if (mouse_move_y > 0)
					mouse_move_y = 0;
			}
			else {
				r &= STICK_BACK;
				mouse_y++;
				mouse_move_y -= 1 << MOUSE_SHIFT;
				if (mouse_move_y < 0)
					mouse_move_y = 0;
			}
		}
	}
	else {
		if (mouse_move_y < 0) {
			r &= STICK_FORWARD;
			mouse_y--;
			mouse_move_y += 1 << MOUSE_SHIFT;
			if (mouse_move_y > 0)
				mouse_move_y = 0;
		}
		else {
			r &= STICK_BACK;
			mouse_y++;
			mouse_move_y -= 1 << MOUSE_SHIFT;
			if (mouse_move_y < 0)
				mouse_move_y = 0;
		}
		e -= dx;
		if (e < 0) {
			e += dy;
			if (mouse_move_x < 0) {
				r &= STICK_LEFT;
				mouse_x--;
				mouse_move_x += 1 << MOUSE_SHIFT;
				if (mouse_move_x > 0)
					mouse_move_x = 0;
			}
			else {
				r &= STICK_RIGHT;
				mouse_x++;
				mouse_move_x -= 1 << MOUSE_SHIFT;
				if (mouse_move_x < 0)
					mouse_move_x = 0;
			}
		}
	}
	return r;
}

void INPUT_Frame(void)
{
	int i;
	static int last_key_code = AKEY_NONE;
	static int last_key_break = 0;
	static UBYTE last_stick[4] = {STICK_CENTRE, STICK_CENTRE, STICK_CENTRE, STICK_CENTRE};
	static int last_mouse_buttons = 0;

	scanline_counter = 10000;	/* do nothing in INPUT_Scanline() */

	/* handle keyboard */

	/* In Atari 5200 joystick there's a second fire button, which acts
	   like the Shift key in 800/XL/XE (bit 3 in SKSTAT) and generates IRQ
	   like the Break key (bit 7 in IRQST and IRQEN).
	   Note that in 5200 the joystick position and first fire button are
	   separate for each port, but the keypad and 2nd button are common.
	   That is, if you press a key in the emulator, it's like you really pressed
	   it in all the controllers simultaneously. Normally the port to read
	   keypad & 2nd button is selected with the CONSOL register in GTIA
	   (this is simply not emulated).
	   key_code is used for keypad keys and key_shift is used for 2nd button.
	*/
	i = machine_type == MACHINE_5200 ? key_shift : (key_code == AKEY_BREAK);
	if (i && !last_key_break) {
		if (IRQEN & 0x80) {
			IRQST &= ~0x80;
			GenerateIRQ();
		}
	}
	last_key_break = i;

	SKSTAT |= 0xc;
	if (key_shift)
		SKSTAT &= ~8;

	if (key_code < 0) {
		if (press_space) {
			key_code = AKEY_SPACE;
			press_space = 0;
		}
		else {
			last_key_code = AKEY_NONE;
		}
	}
	if (key_code >= 0) {
		SKSTAT &= ~4;
		if ((key_code ^ last_key_code) & ~AKEY_SHFTCTRL) {
		/* ignore if only shift or control has changed its state */
			last_key_code = key_code;
			KBCODE = (UBYTE) key_code;
			if (IRQEN & 0x40) {
				if (IRQST & 0x40) {
					IRQST &= ~0x40;
					GenerateIRQ();
				}
				else {
					/* keyboard over-run */
					SKSTAT &= ~0x40;
					/* assert(IRQ != 0); */
				}
			}
		}
	}

	/* handle joysticks */
	i = Atari_PORT(0);
	STICK[0] = i & 0x0f;
	STICK[1] = (i >> 4) & 0x0f;
	i = Atari_PORT(1);
	STICK[2] = i & 0x0f;
	STICK[3] = (i >> 4) & 0x0f;

	for (i = 0; i < 4; i++) {
		if (joy_block_opposite_directions) {
			if ((STICK[i] & 0x0c) == 0) {	/* right and left simultaneously */
				if (last_stick[i] & 0x04)	/* if wasn't left before, move left */
					STICK[i] |= 0x08;
				else						/* else move right */
					STICK[i] |= 0x04;
			}
			else {
				last_stick[i] &= 0x03;
				last_stick[i] |= STICK[i] & 0x0c;
			}
			if ((STICK[i] & 0x03) == 0) {	/* up and down simultaneously */
				if (last_stick[i] & 0x01)	/* if wasn't up before, move up */
					STICK[i] |= 0x02;
				else						/* else move down */
					STICK[i] |= 0x01;
			}
			else {
				last_stick[i] &= 0x0c;
				last_stick[i] |= STICK[i] & 0x03;
			}
		}
		else
			last_stick[i] = STICK[i];
		TRIG_input[i] = Atari_TRIG(i);
		if ((joy_autofire[i] == AUTOFIRE_FIRE && !TRIG_input[i]) || (joy_autofire[i] == AUTOFIRE_CONT))
			TRIG_input[i] = (nframes & 2) ? 1 : 0;
	}

	/* handle analog joysticks in Atari 5200 */
	if (machine_type != MACHINE_5200) {
		for (i = 0; i < 8; i++)
			POT_input[i] = Atari_POT(i);
	}
	else {
		for (i = 0; i < 4; i++) {
#ifdef DREAMCAST
			/* first get analog js data */
			POT_input[2 * i] = Atari_POT(2 * i);         /* x */
			POT_input[2 * i + 1] = Atari_POT(2 * i + 1); /* y */
			if (POT_input[2 * i] != joy_5200_center
			 || POT_input[2 * i + 1] != joy_5200_center)
				continue;
			/* if analog js is unused, alternatively try keypad */
#endif
			if ((STICK[i] & (STICK_CENTRE ^ STICK_LEFT)) == 0)
				POT_input[2 * i] = joy_5200_min;
			else if ((STICK[i] & (STICK_CENTRE ^ STICK_RIGHT)) == 0)
				POT_input[2 * i] = joy_5200_max;
			else
				POT_input[2 * i] = joy_5200_center;
			if ((STICK[i] & (STICK_CENTRE ^ STICK_FORWARD)) == 0)
				POT_input[2 * i + 1] = joy_5200_min;
			else if ((STICK[i] & (STICK_CENTRE ^ STICK_BACK)) == 0)
				POT_input[2 * i + 1] = joy_5200_max;
			else
				POT_input[2 * i + 1] = joy_5200_center;
		}
	}

	/* handle mouse */
#ifdef __PLUS
	if (g_Input.ulState & IS_CAPTURE_MOUSE)
#endif
	switch (mouse_mode) {
	case MOUSE_PAD:
	case MOUSE_TOUCH:
	case MOUSE_KOALA:
		if (mouse_mode != MOUSE_PAD || machine_type == MACHINE_5200)
			mouse_x += mouse_delta_x * mouse_speed;
		else
			mouse_x -= mouse_delta_x * mouse_speed;
		if (mouse_x < mouse_pot_min << MOUSE_SHIFT)
			mouse_x = mouse_pot_min << MOUSE_SHIFT;
		else if (mouse_x > mouse_pot_max << MOUSE_SHIFT)
			mouse_x = mouse_pot_max << MOUSE_SHIFT;
		if (mouse_mode == MOUSE_KOALA || machine_type == MACHINE_5200)
			mouse_y += mouse_delta_y * mouse_speed;
		else
			mouse_y -= mouse_delta_y * mouse_speed;
		if (mouse_y < mouse_pot_min << MOUSE_SHIFT)
			mouse_y = mouse_pot_min << MOUSE_SHIFT;
		else if (mouse_y > mouse_pot_max << MOUSE_SHIFT)
			mouse_y = mouse_pot_max << MOUSE_SHIFT;
		POT_input[mouse_port << 1] = mouse_x >> MOUSE_SHIFT;
		POT_input[(mouse_port << 1) + 1] = mouse_y >> MOUSE_SHIFT;
		if (machine_type == MACHINE_5200) {
			if (mouse_buttons & 1)
				TRIG_input[mouse_port] = 0;
			if (mouse_buttons & 2)
				SKSTAT &= ~8;
		}
		else
			STICK[mouse_port] &= ~(mouse_buttons << 2);
		break;
	case MOUSE_PEN:
	case MOUSE_GUN:
		mouse_x += mouse_delta_x * mouse_speed;
		if (mouse_x < 0)
			mouse_x = 0;
		else if (mouse_x > 167 << MOUSE_SHIFT)
			mouse_x = 167 << MOUSE_SHIFT;
		mouse_y += mouse_delta_y * mouse_speed;
		if (mouse_y < 0)
			mouse_y = 0;
		else if (mouse_y > 119 << MOUSE_SHIFT)
			mouse_y = 119 << MOUSE_SHIFT;
		PENH_input = 44 + mouse_pen_ofs_h + (mouse_x >> MOUSE_SHIFT);
		PENV_input = 4 + mouse_pen_ofs_v + (mouse_y >> MOUSE_SHIFT);
		if ((mouse_buttons & 1) == (mouse_mode == MOUSE_PEN))
			STICK[mouse_port] &= ~1;
		if ((mouse_buttons & 2) && !(last_mouse_buttons & 2))
			mouse_pen_show_pointer = !mouse_pen_show_pointer;
		break;
	case MOUSE_AMIGA:
	case MOUSE_ST:
	case MOUSE_TRAK:
		mouse_move_x += (mouse_delta_x * mouse_speed) >> 1;
		mouse_move_y += (mouse_delta_y * mouse_speed) >> 1;

		/* i = max(abs(mouse_move_x), abs(mouse_move_y)); */
		i = mouse_move_x >= 0 ? mouse_move_x : -mouse_move_x;
		if (mouse_move_y > i)
			i = mouse_move_y;
		else if (-mouse_move_y > i)
			i = -mouse_move_y;

		{
			UBYTE stick = STICK_CENTRE;
			if (i > 0) {
				i += (1 << MOUSE_SHIFT) - 1;
				i >>= MOUSE_SHIFT;
				if (i > 50)
					max_scanline_counter = scanline_counter = 5;
				else
					max_scanline_counter = scanline_counter = max_ypos / i;
				stick = mouse_step();
			}
			if (mouse_mode == MOUSE_TRAK) {
				/* bit 3 toggles - vertical movement, bit 2 = 0 - up */
				/* bit 1 toggles - horizontal movement, bit 0 = 0 - left */
				STICK[mouse_port] = ((mouse_y & 1) << 3) | ((stick & 1) << 2)
									| ((mouse_x & 1) << 1) | ((stick & 4) >> 2);
			}
			else {
				STICK[mouse_port] = (mouse_mode == MOUSE_AMIGA ? mouse_amiga_codes : mouse_st_codes)
									[(mouse_y & 3) * 4 + (mouse_x & 3)];
			}
		}

		if (mouse_buttons & 1)
			TRIG_input[mouse_port] = 0;
		if (mouse_buttons & 2)
			POT_input[mouse_port << 1] = 1;
		break;
	case MOUSE_JOY:
		if (machine_type == MACHINE_5200) {
			/* 2 * mouse_speed is ok for Super Breakout, for Ballblazer you need more */
			int val = joy_5200_center + ((mouse_delta_x * 2 * mouse_speed) >> MOUSE_SHIFT);
			if (val < joy_5200_min)
				val = joy_5200_min;
			else if (val > joy_5200_max)
				val = joy_5200_max;
			POT_input[mouse_port << 1] = val;
			val = joy_5200_center + ((mouse_delta_y * 2 * mouse_speed) >> MOUSE_SHIFT);
			if (val < joy_5200_min)
				val = joy_5200_min;
			else if (val > joy_5200_max)
				val = joy_5200_max;
			POT_input[(mouse_port << 1) + 1] = val;
			if (mouse_buttons & 2)
				SKSTAT &= ~8;
		}
		else {
			mouse_move_x += mouse_delta_x * mouse_speed;
			mouse_move_y += mouse_delta_y * mouse_speed;
			if (mouse_move_x < -mouse_joy_inertia << MOUSE_SHIFT ||
				mouse_move_x > mouse_joy_inertia << MOUSE_SHIFT ||
				mouse_move_y < -mouse_joy_inertia << MOUSE_SHIFT ||
				mouse_move_y > mouse_joy_inertia << MOUSE_SHIFT) {
				mouse_move_x >>= 1;
				mouse_move_y >>= 1;
			}
			STICK[mouse_port] &= mouse_step();
		}
		if (mouse_buttons & 1)
			TRIG_input[mouse_port] = 0;
		break;
	default:
		break;
	}
	last_mouse_buttons = mouse_buttons;

	if (joy_multijoy && machine_type != MACHINE_5200) {
		PORT_input[0] = 0xf0 | STICK[joy_multijoy_no];
		PORT_input[1] = 0xff;
		TRIG[0] = TRIG_input[joy_multijoy_no];
		TRIG[2] = TRIG[1] = 1;
		TRIG[3] = machine_type == MACHINE_XLXE ? cartA0BF_enabled : 1;
	}
	else {
		TRIG[0] = TRIG_input[0];
		TRIG[1] = TRIG_input[1];
		if (machine_type == MACHINE_XLXE) {
			TRIG[2] = 1;
			TRIG[3] = cartA0BF_enabled;
		}
		else {
			TRIG[2] = TRIG_input[2];
			TRIG[3] = TRIG_input[3];
		}
		PORT_input[0] = (STICK[1] << 4) | STICK[0];
		PORT_input[1] = (STICK[3] << 4) | STICK[2];
	}
}

void INPUT_Scanline(void)
{
	if (--scanline_counter == 0) {
		UBYTE stick = mouse_step();
		if (mouse_mode == MOUSE_TRAK) {
			/* bit 3 toggles - vertical movement, bit 2 = 0 - up */
			/* bit 1 toggles - horizontal movement, bit 0 = 0 - left */
			STICK[mouse_port] = ((mouse_y & 1) << 3) | ((stick & 1) << 2)
								| ((mouse_x & 1) << 1) | ((stick & 4) >> 2);
		}
		else {
			STICK[mouse_port] = (mouse_mode == MOUSE_AMIGA ? mouse_amiga_codes : mouse_st_codes)
								[(mouse_y & 3) * 4 + (mouse_x & 3)];
		}
		PORT_input[0] = (STICK[1] << 4) | STICK[0];
		PORT_input[1] = (STICK[3] << 4) | STICK[2];
		scanline_counter = max_scanline_counter;
	}
}

void INPUT_SelectMultiJoy(int no)
{
	no &= 3;
	joy_multijoy_no = no;
	if (joy_multijoy && machine_type != MACHINE_5200) {
		PORT_input[0] = 0xf0 | STICK[no];
		TRIG[0] = TRIG_input[no];
	}
}

void INPUT_CenterMousePointer(void)
{
	switch (mouse_mode) {
	case MOUSE_PAD:
	case MOUSE_TOUCH:
	case MOUSE_KOALA:
		mouse_x = 114 << MOUSE_SHIFT;
		mouse_y = 114 << MOUSE_SHIFT;
		break;
	case MOUSE_PEN:
	case MOUSE_GUN:
		mouse_x = 84 << MOUSE_SHIFT;
		mouse_y = 60 << MOUSE_SHIFT;
		break;
	case MOUSE_AMIGA:
	case MOUSE_ST:
	case MOUSE_TRAK:
	case MOUSE_JOY:
		mouse_move_x = 0;
		mouse_move_y = 0;
		break;
	}
}

#ifndef CURSES_BASIC

#define PLOT(dx, dy)	do {\
							ptr[(dx) + ATARI_WIDTH * (dy)] ^= 0x0f0f;\
							ptr[(dx) + ATARI_WIDTH * (dy) + ATARI_WIDTH / 2] ^= 0x0f0f;\
						} while (0)

/* draw light pen cursor */
void INPUT_DrawMousePointer(void)
{
	if ((mouse_mode == MOUSE_PEN || mouse_mode == MOUSE_GUN) && mouse_pen_show_pointer) {
		int x = mouse_x >> MOUSE_SHIFT;
		int y = mouse_y >> MOUSE_SHIFT;
		if (x >= 0 && x <= 167 && y >= 0 && y <= 119) {
			UWORD *ptr = & ((UWORD *) atari_screen)[12 + x + ATARI_WIDTH * y];
			PLOT(-2, 0);
			PLOT(-1, 0);
			PLOT(1, 0);
			PLOT(2, 0);
			if (y >= 1) {
				PLOT(0, -1);
				if (y >= 2)
					PLOT(0, -2);
			}
			if (y <= 118) {
				PLOT(0, 1);
				if (y <= 117)
					PLOT(0, 2);
			}
		}
	}
}

#endif /* CURSES_BASIC */

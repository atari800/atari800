/*
 * atari_curses.c - Curses based port code
 *
 * Copyright (c) 1995-1998 David Firth
 * Copyright (c) 1998-2005 Atari800 development team (see DOC/CREDITS)

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
#ifdef USE_NCURSES
#include <ncurses.h>
#else
#include <curses.h>
#endif

/* workaround warnings with PDCurses */
#undef HAVE_LIMITS_H
#undef HAVE_MEMMOVE
#undef HAVE_MEMORY_H
#undef HAVE_UNISTD_H

#include "antic.h" /* ypos */
#include "atari.h"
#include "input.h"
#include "monitor.h"
#include "log.h"

#ifdef SOUND
#include "sound.h"
#endif

#define CURSES_LEFT 0
#define CURSES_CENTRAL 1
#define CURSES_RIGHT 2
#define CURSES_WIDE_1 3
#define CURSES_WIDE_2 4

static int curses_mode = CURSES_LEFT;

UBYTE curses_screen[24][40];

void Atari_Initialise(int *argc, char *argv[])
{
	int i;
	int j;

	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-left") == 0)
			curses_mode = CURSES_LEFT;
		else if (strcmp(argv[i], "-central") == 0)
			curses_mode = CURSES_CENTRAL;
		else if (strcmp(argv[i], "-right") == 0)
			curses_mode = CURSES_RIGHT;
		else if (strcmp(argv[i], "-wide1") == 0)
			curses_mode = CURSES_WIDE_1;
		else if (strcmp(argv[i], "-wide2") == 0)
			curses_mode = CURSES_WIDE_2;
		else {
			if (strcmp(argv[i], "-help") == 0) {
				Aprint("\t-central         Center emulated screen\n"
				       "\t-left            Align left\n"
				       "\t-right           Align right (on 80 columns)\n"
				       "\t-wide1           Use 80 columns\n"
				       "\t-wide2           Use 80 columns, display twice"
				      );
			}
			argv[j++] = argv[i];
		}
	}

	*argc = j;

	initscr();
	noecho();
	cbreak();					/* Don't wait for carriage return */
	keypad(stdscr, TRUE);
	curs_set(0);				/* Disable Cursor */
	nodelay(stdscr, 1);			/* Don't block for keypress */

#ifdef SOUND
	Sound_Initialise(argc, argv);
#endif
}

int Atari_Exit(int run_monitor)
{
	int restart;

	curs_set(1);
	endwin();


	if (run_monitor)
		restart = monitor();
	else
		restart = FALSE;

	if (restart) {
		curs_set(0);
	}
#ifdef SOUND
	else
		Sound_Exit();
#endif
	Aflushlog();
	return restart;
}

void curses_clear_screen(void)
{
	int y;
	for (y = 0; y < 24; y++)
		memset(curses_screen[y], 0, 40);
}

void curses_display_line(int anticmode, const UBYTE *screendata)
{
	UBYTE *p;
	int w;
	if (ypos < 32 || ypos >= 224)
		return;
	p = &(curses_screen[(ypos >> 3) - 4][0]);
	switch (anticmode) {
	case 2:
	case 3:
	case 4:
	case 5:
		switch (DMACTL & 3) {
		case 1:
			p += 4;
			w = 32;
			break;
		case 2:
			w = 40;
			break;
		case 3:
			screendata += 4;
			w = 40;
			break;
		default:
			return;
		}
		memcpy(p, screendata, w);
		break;
	case 6:
	case 7:
		switch (DMACTL & 3) {
		case 1:
			p += 12;
			w = 16;
			break;
		case 2:
			p += 10;
			w = 20;
			break;
		case 3:
			p += 8;
			w = 24;
			break;
		default:
			return;
		}
		do
			*p++ = *screendata++ & 0xbf;
		while (--w);
		break;
	default:
		break;
	}
}

void Atari_DisplayScreen(UBYTE *screen)
{
	int x;
	int y;

	for (y = 0; y < 24; y++) {
		for (x = 0; x < 40; x++) {
			int ch = curses_screen[y][x];

			switch (ch & 0xe0) {
			case 0x00:			/* Numbers + !"$% etc. */
			case 0x20:			/* Upper Case Characters */
				ch = 0x20 + ch;
				break;
			case 0x40:			/* Control Characters */
				ch = ch + A_BOLD;
				break;
			case 0x60:			/* Lower Case Characters */
				break;
			case 0x80:			/* Numbers, !"$% etc. */
			case 0xa0:			/* Upper Case Characters */
				ch = 0x20 + (ch & 0x7f) + A_REVERSE;
				break;
			case 0xc0:			/* Control Characters */
				ch = (ch & 0x7f) + A_REVERSE + A_BOLD;
				break;
			case 0xe0:			/* Lower Case Characters */
				ch = (ch & 0x7f) + A_REVERSE;
				break;
			}

			switch (curses_mode) {
			default:
			case CURSES_LEFT:
				move(y, x);
				break;
			case CURSES_CENTRAL:
				move(y, 20 + x);
				break;
			case CURSES_RIGHT:
				move(y, 40 + x);
				break;
			case CURSES_WIDE_1:
				move(y, x + x);
				break;
			case CURSES_WIDE_2:
				move(y, x + x);
				addch(ch);
				ch = ' ' + (ch & A_REVERSE);
				break;
			}

			addch(ch);
		}
	}

	refresh();
}

int Atari_Keyboard(void)
{
	int keycode = getch();

#if 0
	/* for debugging */
	if (keycode > 0) {
		Atari800_Exit(FALSE);
		printf("keycode == %d (0x%x)\n", keycode, keycode);
		exit(1);
	}
#endif

	key_consol = CONSOL_NONE;

	switch (keycode) {
	case 0x01:
		keycode = AKEY_CTRL_A;
		break;
	case 0x02:
		keycode = AKEY_CTRL_B;
		break;
/*
   case 0x03 :
   keycode = AKEY_CTRL_C;
   break;
 */
	case 0x04:
		keycode = AKEY_CTRL_D;
		break;
	case 0x05:
		keycode = AKEY_CTRL_E;
		break;
	case 0x06:
		keycode = AKEY_CTRL_F;
		break;
	case 0x07:
		keycode = AKEY_CTRL_G;
		break;
#if !defined(DJGPP) && !defined(SOLARIS2)
	case 0x08:
		keycode = AKEY_CTRL_H;
		break;
#endif
	case 0x09:
		keycode = AKEY_TAB;
		break;
/*
   case 0x0a :
   keycode = AKEY_CTRL_J;
   break;
 */
	case 0x0b:
		keycode = AKEY_CTRL_K;
		break;
	case 0x0c:
		keycode = AKEY_CTRL_L;
		break;
/*
   case 0x0d :
   keycode = AKEY_CTRL_M;
   break;
 */
	case 0x0e:
		keycode = AKEY_CTRL_N;
		break;
	case 0x0f:
		keycode = AKEY_CTRL_O;
		break;
	case 0x10:
		keycode = AKEY_CTRL_P;
		break;
	case 0x11:
		keycode = AKEY_CTRL_Q;
		break;
	case 0x12:
		keycode = AKEY_CTRL_R;
		break;
	case 0x13:
		keycode = AKEY_CTRL_S;
		break;
	case 0x14:
		keycode = AKEY_CTRL_T;
		break;
	case 0x15:
		keycode = AKEY_CTRL_U;
		break;
	case 0x16:
		keycode = AKEY_CTRL_V;
		break;
	case 0x17:
		keycode = AKEY_CTRL_W;
		break;
	case 0x18:
		keycode = AKEY_CTRL_X;
		break;
	case 0x19:
		keycode = AKEY_CTRL_Y;
		break;
	case 0x1a:
		keycode = AKEY_CTRL_Z;
		break;
	case '`':
		keycode = AKEY_CAPSTOGGLE;
		break;
	case '!':
		keycode = AKEY_EXCLAMATION;
		break;
	case '"':
		keycode = AKEY_DBLQUOTE;
		break;
	case '#':
		keycode = AKEY_HASH;
		break;
	case '$':
		keycode = AKEY_DOLLAR;
		break;
	case '%':
		keycode = AKEY_PERCENT;
		break;
	case '&':
		keycode = AKEY_AMPERSAND;
		break;
	case '\'':
		keycode = AKEY_QUOTE;
		break;
	case '@':
		keycode = AKEY_AT;
		break;
	case '(':
		keycode = AKEY_PARENLEFT;
		break;
	case ')':
		keycode = AKEY_PARENRIGHT;
		break;
	case '[':
		keycode = AKEY_BRACKETLEFT;
		break;
	case ']':
		keycode = AKEY_BRACKETRIGHT;
		break;
	case '<':
		keycode = AKEY_LESS;
		break;
	case '>':
		keycode = AKEY_GREATER;
		break;
	case '=':
		keycode = AKEY_EQUAL;
		break;
	case '?':
		keycode = AKEY_QUESTION;
		break;
	case '-':
		keycode = AKEY_MINUS;
		break;
	case '+':
		keycode = AKEY_PLUS;
		break;
	case '*':
		keycode = AKEY_ASTERISK;
		break;
	case '/':
		keycode = AKEY_SLASH;
		break;
	case ':':
		keycode = AKEY_COLON;
		break;
	case ';':
		keycode = AKEY_SEMICOLON;
		break;
	case ',':
		keycode = AKEY_COMMA;
		break;
	case '.':
		keycode = AKEY_FULLSTOP;
		break;
	case '_':
		keycode = AKEY_UNDERSCORE;
		break;
	case '^':
		keycode = AKEY_CIRCUMFLEX;
		break;
	case '\\':
		keycode = AKEY_BACKSLASH;
		break;
	case '|':
		keycode = AKEY_BAR;
		break;
	case ' ':
		keycode = AKEY_SPACE;
		break;
	case '0':
		keycode = AKEY_0;
		break;
	case '1':
		keycode = AKEY_1;
		break;
	case '2':
		keycode = AKEY_2;
		break;
	case '3':
		keycode = AKEY_3;
		break;
	case '4':
		keycode = AKEY_4;
		break;
	case '5':
		keycode = AKEY_5;
		break;
	case '6':
		keycode = AKEY_6;
		break;
	case '7':
		keycode = AKEY_7;
		break;
	case '8':
		keycode = AKEY_8;
		break;
	case '9':
		keycode = AKEY_9;
		break;
	case 'a':
		keycode = AKEY_a;
		break;
	case 'b':
		keycode = AKEY_b;
		break;
	case 'c':
		keycode = AKEY_c;
		break;
	case 'd':
		keycode = AKEY_d;
		break;
	case 'e':
		keycode = AKEY_e;
		break;
	case 'f':
		keycode = AKEY_f;
		break;
	case 'g':
		keycode = AKEY_g;
		break;
	case 'h':
		keycode = AKEY_h;
		break;
	case 'i':
		keycode = AKEY_i;
		break;
	case 'j':
		keycode = AKEY_j;
		break;
	case 'k':
		keycode = AKEY_k;
		break;
	case 'l':
		keycode = AKEY_l;
		break;
	case 'm':
		keycode = AKEY_m;
		break;
	case 'n':
		keycode = AKEY_n;
		break;
	case 'o':
		keycode = AKEY_o;
		break;
	case 'p':
		keycode = AKEY_p;
		break;
	case 'q':
		keycode = AKEY_q;
		break;
	case 'r':
		keycode = AKEY_r;
		break;
	case 's':
		keycode = AKEY_s;
		break;
	case 't':
		keycode = AKEY_t;
		break;
	case 'u':
		keycode = AKEY_u;
		break;
	case 'v':
		keycode = AKEY_v;
		break;
	case 'w':
		keycode = AKEY_w;
		break;
	case 'x':
		keycode = AKEY_x;
		break;
	case 'y':
		keycode = AKEY_y;
		break;
	case 'z':
		keycode = AKEY_z;
		break;
	case 'A':
		keycode = AKEY_A;
		break;
	case 'B':
		keycode = AKEY_B;
		break;
	case 'C':
		keycode = AKEY_C;
		break;
	case 'D':
		keycode = AKEY_D;
		break;
	case 'E':
		keycode = AKEY_E;
		break;
	case 'F':
		keycode = AKEY_F;
		break;
	case 'G':
		keycode = AKEY_G;
		break;
	case 'H':
		keycode = AKEY_H;
		break;
	case 'I':
		keycode = AKEY_I;
		break;
	case 'J':
		keycode = AKEY_J;
		break;
	case 'K':
		keycode = AKEY_K;
		break;
	case 'L':
		keycode = AKEY_L;
		break;
	case 'M':
		keycode = AKEY_M;
		break;
	case 'N':
		keycode = AKEY_N;
		break;
	case 'O':
		keycode = AKEY_O;
		break;
	case 'P':
		keycode = AKEY_P;
		break;
	case 'Q':
		keycode = AKEY_Q;
		break;
	case 'R':
		keycode = AKEY_R;
		break;
	case 'S':
		keycode = AKEY_S;
		break;
	case 'T':
		keycode = AKEY_T;
		break;
	case 'U':
		keycode = AKEY_U;
		break;
	case 'V':
		keycode = AKEY_V;
		break;
	case 'W':
		keycode = AKEY_W;
		break;
	case 'X':
		keycode = AKEY_X;
		break;
	case 'Y':
		keycode = AKEY_Y;
		break;
	case 'Z':
		keycode = AKEY_Z;
		break;
	case 0x1b:
		keycode = AKEY_ESCAPE;
		break;
	case KEY_F0 + 1:
		keycode = AKEY_UI;
		break;
	case KEY_F0 + 2:
		key_consol &= ~CONSOL_OPTION;
		keycode = AKEY_NONE;
		break;
	case KEY_F0 + 3:
		key_consol &= ~CONSOL_SELECT;
		keycode = AKEY_NONE;
		break;
	case KEY_F0 + 4:
		key_consol &= ~CONSOL_START;
		keycode = AKEY_NONE;
		break;
	case KEY_F0 + 5:
		keycode = AKEY_WARMSTART;
		break;
	case KEY_F0 + 6:
		keycode = AKEY_HELP;
		break;
	case KEY_F0 + 7:
		keycode = AKEY_BREAK;
		break;
	case KEY_F0 + 8:
		keycode = Atari_Exit(TRUE) ? AKEY_NONE : AKEY_EXIT;
		break;
	case KEY_F0 + 9:
		keycode = AKEY_EXIT;
		break;
	case KEY_DOWN:
		keycode = AKEY_DOWN;
		break;
	case KEY_LEFT:
		keycode = AKEY_LEFT;
		break;
	case KEY_RIGHT:
		keycode = AKEY_RIGHT;
		break;
	case KEY_UP:
		keycode = AKEY_UP;
		break;
#if defined(DJGPP) || defined(SOLARIS2)
	case 8:
	case 127:
#else
	case 127:
	case KEY_BACKSPACE:
#endif
		keycode = AKEY_BACKSPACE;
		break;
	case KEY_ENTER:
	case '\n':
		keycode = AKEY_RETURN;
		break;
	default:
		keycode = AKEY_NONE;
		break;
	}
	return keycode;
}

int Atari_PORT(int num)
{
	return 0xff;
}

int Atari_TRIG(int num)
{
	return 1;
}

int main(int argc, char **argv)
{
	/* initialise Atari800 core */
	if (!Atari800_Initialise(&argc, argv))
		return 3;

	/* main loop */
	while (TRUE) {
		key_code = Atari_Keyboard();
		Atari800_Frame();
		if (display_screen)
			Atari_DisplayScreen(NULL);
	}
}

/*
$Log$
Revision 1.17  2005/08/16 23:05:49  pfusik
#include "config.h" before system headers

Revision 1.16  2005/08/14 08:40:31  pfusik
fixed warnings with PDCurses; fixed wide Atari screen

Revision 1.15  2005/08/13 08:43:35  pfusik
generate curses screen basing on the DL; fixed -wide2;
fixed TAB, F8 and console keys

Revision 1.14  2005/08/10 20:03:38  pfusik
backspace now works on DJGPP/pdcurses

Revision 1.13  2005/03/05 12:26:05  pfusik
support for special AKEY_*, refresh rate control and atari_sync()
moved to Atari800_Frame(); F6 is Atari HELP key

Revision 1.12  2005/02/23 16:45:32  pfusik
PNG screenshots

Revision 1.11  2003/03/03 09:57:32  joy
Ed improved autoconf again plus fixed some little things

Revision 1.10  2003/02/24 09:32:40  joy
header cleanup

Revision 1.9  2003/02/08 23:52:17  joy
little cleanup

Revision 1.8  2002/08/07 08:59:06  joy
last EOL is not necessary

Revision 1.7  2002/08/07 08:40:37  joy
Aflushlog added, -help added

Revision 1.6  2001/12/12 15:24:37  joy
F5 is a warm start on all platforms.

Revision 1.5  2001/10/11 08:39:55  fox
added curses_screen for UI

Revision 1.4  2001/10/10 12:14:10  fox
compilable now

Revision 1.3  2001/04/08 05:54:48  knik
sound_update call removed (moved to atari.c)

*/
